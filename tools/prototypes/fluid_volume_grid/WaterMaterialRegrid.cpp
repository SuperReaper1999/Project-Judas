#include "WaterMaterialRegrid.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>

namespace judas::volume_grid {
namespace {
using Real = CutCompensated;
using Clock = std::chrono::steady_clock;
bool Less(Real a, Real b) { return (a-b).Value()<0.; }
bool Equal(Real a, Real b) { return (a-b).Value()==0.; }
Real Min(Real a, Real b) { return Less(a,b)?a:b; }
Real Max(Real a, Real b) { return Less(a,b)?b:a; }
bool Fail(std::string& error, const char* why) { error=why; return false; }
void Unique(std::vector<Real>& cuts) {
    std::sort(cuts.begin(),cuts.end(),Less);
    cuts.erase(std::unique(cuts.begin(),cuts.end(),Equal),cuts.end());
}
struct Budget {
    std::size_t used=0, maximum;
    bool Charge(std::size_t amount) {if(amount>maximum-used)return false;used+=amount;return true;}
    bool Inspect() {return Charge(1);}
};
struct Rectangle { Real loU,hiU,loV,hiV; };
bool SameRectangle(const Rectangle& a,const Rectangle& b) {
    return Equal(a.loU,b.loU)&&Equal(a.hiU,b.hiU)&&
        Equal(a.loV,b.loV)&&Equal(a.hiV,b.hiV);
}
// Sweep one coordinate. Union intervals in the other two coordinates before
// extruding; internal cuts cannot survive merely because boxes form T junctions.
bool Retile(const std::vector<WaterMaterialFragment>& source,int axis,
    std::vector<WaterMaterialFragment>& result,Budget& budget) {
    int u=(axis+1)%3,v=(axis+2)%3;
    std::vector<Real> cuts;
    for(const auto& box:source){cuts.push_back(box.lo[axis]);cuts.push_back(box.hi[axis]);}
    Unique(cuts);
    std::vector<std::pair<Rectangle,std::size_t>> previous;
    for(std::size_t x=1;x<cuts.size();++x){
        std::vector<const WaterMaterialFragment*> active;
        std::vector<Real> horizontal;
        for(const auto& box:source){
            if(!budget.Inspect())return false;
            if(!Less(cuts[x-1],box.lo[axis])&&!Less(box.hi[axis],cuts[x])){
                active.push_back(&box);horizontal.push_back(box.lo[u]);horizontal.push_back(box.hi[u]);
            }
        }
        Unique(horizontal);
        std::vector<Rectangle> rectangles;
        std::vector<std::size_t> lastBand;
        for(std::size_t y=1;y<horizontal.size();++y){
            std::vector<std::pair<Real,Real>> intervals;
            for(const auto* box:active){
                if(!budget.Inspect())return false;
                if(!Less(horizontal[y-1],box->lo[u])&&!Less(box->hi[u],horizontal[y]))
                    intervals.push_back({box->lo[v],box->hi[v]});
            }
            std::sort(intervals.begin(),intervals.end(),[](auto a,auto b){return Less(a.first,b.first);});
            std::vector<std::pair<Real,Real>> merged;
            for(auto interval:intervals){
                if(merged.empty()||Less(merged.back().second,interval.first))merged.push_back(interval);
                else merged.back().second=Max(merged.back().second,interval.second);
            }
            std::vector<std::size_t> thisBand;
            for(auto interval:merged){
                std::size_t id=SIZE_MAX;
                for(auto prior:lastBand){
                    const auto& r=rectangles[prior];
                    if(Equal(r.hiU,horizontal[y-1])&&Equal(r.loV,interval.first)&&Equal(r.hiV,interval.second)){id=prior;break;}
                }
                if(id==SIZE_MAX){id=rectangles.size();rectangles.push_back({horizontal[y-1],horizontal[y],interval.first,interval.second});}
                else rectangles[id].hiU=horizontal[y];
                thisBand.push_back(id);
            }
            lastBand=std::move(thisBand);
        }
        std::vector<std::pair<Rectangle,std::size_t>> current;
        for(const auto& rectangle:rectangles){
            std::size_t id=SIZE_MAX;
            for(const auto& prior:previous)if(SameRectangle(rectangle,prior.first)&&Equal(result[prior.second].hi[axis],cuts[x-1])){id=prior.second;break;}
            if(id==SIZE_MAX){
                auto box=source.front();box.lo[axis]=cuts[x-1];box.hi[axis]=cuts[x];
                box.lo[u]=rectangle.loU;box.hi[u]=rectangle.hiU;box.lo[v]=rectangle.loV;box.hi[v]=rectangle.hiV;
                id=result.size();result.push_back(box);
            }else result[id].hi[axis]=cuts[x];
            current.push_back({rectangle,id});
        }
        previous=std::move(current);
    }
    return true;
}
bool Canonical(const std::vector<WaterMaterialFragment>& source,
    std::vector<WaterMaterialFragment>& best,Budget& budget) {
    for(int axis=0;axis<3;++axis){std::vector<WaterMaterialFragment> candidate;
        if(!Retile(source,axis,candidate,budget))return false;
        if(axis==0||candidate.size()<best.size())best=std::move(candidate);
    }
    return true;
}
struct Measures {
    Real volume,mass,kinetic,gasPotential;
    std::array<Real,3> moment,momentum;
};
Measures Measure(const std::vector<WaterMaterialFragment>& pieces) {
    Measures m;
    for(const auto& p:pieces){Real volume=MaterialVolume(p);m.volume=m.volume+volume;m.mass=m.mass+p.gasMass;
        if(!p.water&&p.gasMass.Value()>0.)m.gasPotential=m.gasPotential+p.gasMass*Real(std::log((p.gasMass/volume).Value()));
        for(int a=0;a<3;++a){
            m.moment[a]=m.moment[a]+volume*(p.lo[a]+(p.hi[a]-p.lo[a])*Real(.5));
            m.momentum[a]=m.momentum[a]+volume*Real(p.velocity[a]);
            m.kinetic=m.kinetic+Real(.5)*volume*Real(p.velocity[a])*Real(p.velocity[a]);
        }
    }
    return m;
}
Real OverlapVolume(const WaterMaterialFragment& a,const WaterMaterialFragment& b) {
    Real volume(1.);
    for(int d=0;d<3;++d){Real width=Min(a.hi[d],b.hi[d])-Max(a.lo[d],b.lo[d]);if(!(width.Value()>0.))return {};volume=volume*width;}
    return volume;
}
}

bool CanonicalWaterMaterialBoxes(const std::vector<WaterMaterialFragment>& source,
    std::vector<WaterMaterialFragment>& output,std::string& error,std::size_t workBudget) {
    if(source.empty()||!workBudget)return Fail(error,"canonical material union requires positive work and nonempty support");
    for(const auto& p:source)for(int d=0;d<3;++d)
        if(!std::isfinite(p.lo[d].Value())||!std::isfinite(p.hi[d].Value())||!((p.hi[d]-p.lo[d]).Value()>0.))return Fail(error,"invalid canonical union bounds");
    Budget budget{0,workBudget};std::vector<WaterMaterialFragment> result;
    if(!Canonical(source,result,budget))return Fail(error,"canonical material union exhausted its work budget");
    output=std::move(result);error.clear();return true;
}

bool RegridWaterMaterial(const CutSpatialField& field,const WaterMaterialState& source,
    WaterMaterialState& output,WaterMaterialRegridStats& stats,std::string& error,std::size_t workBudget) {
    auto started=Clock::now();WaterMaterialRegridStats work;work.piecesBefore=source.fragments.size();
    if(!workBudget||source.fragments.empty())return Fail(error,"material regrid requires nonempty geometry and an explicit work budget");
    const auto n=source.fragments.size();
    if(field.geometry.startCapacity.size()!=field.chart.regions.size())return Fail(error,"regrid geometry capacity dimensions disagree");
    Budget budget{0,workBudget};std::vector<std::size_t> before(field.chart.regions.size()),after(before.size());
    for(const auto& p:source.fragments){if(p.cell>=before.size()||!(MaterialVolume(p).Value()>0.)||!std::isfinite(MaterialVolume(p).Value()))return Fail(error,"invalid local regrid material bounds");++before[p.cell];}
    // Charge the existing face discovery before invoking its quadratic local
    // loops. A pathological input must reject before doing unbounded work.
    for(auto count:before){if(count<2)continue;if(count>workBudget||(count>1&&(count-1)>workBudget/(3*count)))return Fail(error,"material adjacency exceeds the regrid work budget");if(!budget.Charge(3*count*(count-1)))return Fail(error,"material adjacency exceeds the regrid work budget");}
    for(const auto& face:field.geometry.faces)if(face.meanArea>0.){
        if(face.left>=before.size()||face.right>=before.size())return Fail(error,"invalid regrid containing-cell adjacency");
        auto a=before[face.left],b=before[face.right];if(a&&b>workBudget/a)return Fail(error,"cross-cell adjacency exceeds the regrid work budget");if(!budget.Charge(a*b))return Fail(error,"cross-cell adjacency exceeds the regrid work budget");
    }
    std::vector<std::size_t> parent(n);std::iota(parent.begin(),parent.end(),0);
    auto root=[&](std::size_t i){while(parent[i]!=i){parent[i]=parent[parent[i]];i=parent[i];}return i;};
    std::vector<WaterMaterialFace> faces;
    if(!BuildWaterMaterialFaces(field,source,faces,error))return false;
    for(const auto& face:faces){const auto& a=source.fragments[face.left];const auto& b=source.fragments[face.right];
        if(a.cell==b.cell&&a.water==b.water&&(a.water||a.carrier==b.carrier)){
            auto left=root(face.left),right=root(face.right);if(left!=right)parent[right]=left;
        }
    }
    std::vector<std::vector<WaterMaterialFragment>> components(n);
    for(std::size_t i=0;i<n;++i){const auto& p=source.fragments[i];
        if(p.cell>=before.size()||!std::isfinite(p.gasMass.Value())||(p.gasMass.Value()<0.||(!p.water&&p.gasMass.Value()==0.)))return Fail(error,"invalid conserved regrid input");
        for(double v:p.velocity)if(!std::isfinite(v))return Fail(error,"nonfinite material regrid velocity");
        components[root(i)].push_back(p);
    }
    WaterMaterialState next=source;next.fragments.clear();
    for(const auto& component:components)if(!component.empty()){
        ++work.components;auto old=Measure(component);std::vector<WaterMaterialFragment> best;
        if(!Canonical(component,best,budget)){work.work=budget.used;stats=work;return Fail(error,"exact local material regrid exhausted its work budget");}
        auto spatial=Measure(best);Real volumeError=spatial.volume-old.volume;
        work.volumeError=work.volumeError+volumeError;
        double tolerance=std::min(1e-14,1e-10*field.geometry.startCapacity[component.front().cell]);
        if(std::abs(volumeError.Value())>tolerance)return Fail(error,"material union changed its owned volume or contained overlapping input");
        // Independently measure set coverage, not just equal total volume.
        for(const auto& box:best){Real covered;
            for(const auto& original:component){if(!budget.Inspect())return Fail(error,"material regrid coverage audit exhausted its work budget");covered=covered+OverlapVolume(box,original);}
            if(std::abs((covered-MaterialVolume(box)).Value())>tolerance)return Fail(error,"material regrid changed physical support");
        }
        for(int a=0;a<3;++a){Real difference=spatial.moment[a]-old.moment[a];
            if(std::abs(difference.Value())>tolerance*std::max(field.chart.regions[component.front().cell].hi[a]-field.chart.regions[component.front().cell].lo[a],1e-30))return Fail(error,"material regrid changed its first moment");
            work.momentError=work.momentError+Real(std::abs(difference.Value()));
        }
        auto carrier=next.fragments.size();
        // All water in this connected pressure cell is already represented by
        // this mean in PrepareWaterFragmentGeometry. Make that existing finite-
        // volume averaging explicit, conservative and energy-audited here.
        for(auto& box:best){box.carrier=carrier;box.gasMass={};
            for(int a=0;a<3;++a)box.velocity[a]=component.front().water?(old.momentum[a]/old.volume).Value():0.;
        }
        if(!component.front().water){
            if(!(old.mass.Value()>0.))return Fail(error,"positive air support has no conserved mass");
            auto largest=std::max_element(best.begin(),best.end(),[](const auto& a,const auto& b){return Less(MaterialVolume(a),MaterialVolume(b));});std::iter_swap(largest,best.end()-1);
            Real remaining=old.mass;
            for(std::size_t i=0;i<best.size();++i){Real mass=i+1==best.size()?remaining:old.mass*MaterialVolume(best[i])/old.volume;
                if(!(mass.Value()>0.))return Fail(error,"material regrid cannot represent a positive air parcel");
                best[i].gasMass=mass;remaining=remaining-mass;
            }
        }
        auto published=Measure(best);work.airMassError=work.airMassError+published.mass-old.mass;
        for(int a=0;a<3;++a){Real change=published.momentum[a]-old.momentum[a];
            if(component.front().water&&std::abs(change.Value())>1e-12*std::max(std::abs(old.momentum[a].Value()),old.volume.Value()))return Fail(error,"material regrid lost water momentum");
            if(component.front().water)work.momentumError=work.momentumError+Real(std::abs(change.Value()));
        }
        if(component.front().water){double change=(published.kinetic-old.kinetic).Value();
            if(change>1e-12*std::max(old.kinetic.Value(),1e-30))return Fail(error,"connected-cell material averaging added kinetic energy");
            work.mixingKineticPerDensityLoss+=std::max(0.,-change);
        }
        if(!component.front().water){double change=(published.gasPotential-old.gasPotential).Value();
            if(!std::isfinite(change)||change>1e-12*std::max(std::abs(old.gasPotential.Value()),1e-30))return Fail(error,"conservative gas regrid added isothermal free energy");
            work.gasMixingPotentialLoss+=std::max(0.,-change);
        }
        for(auto& p:best){++after[p.cell];next.fragments.push_back(std::move(p));}
    }
    if(std::abs(work.volumeError.Value())>1e-14||std::abs(work.airMassError.Value())>1e-14)return Fail(error,"material regrid cold quantity ledger failed");
    work.piecesAfter=next.fragments.size();work.work=budget.used;
    for(std::size_t c=0;c<before.size();++c){work.maximumCellBefore=std::max(work.maximumCellBefore,before[c]);work.maximumCellAfter=std::max(work.maximumCellAfter,after[c]);}
    work.milliseconds=std::chrono::duration<double,std::milli>(Clock::now()-started).count();
    output=std::move(next);stats=work;error.clear();return true;
}
}
