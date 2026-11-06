#include "WaterInterfaceEvents.h"
#include <algorithm>
#include <cmath>
#include <sstream>
namespace judas::volume_grid {
namespace {
using Real=CutCompensated;
Real Min(Real a,Real b){return (a-b).Value()<0.?a:b;}
Real Max(Real a,Real b){return (a-b).Value()>0.?a:b;}
bool Equal(Real a,Real b){return (a-b).Value()==0.;}
bool Fail(std::string& e,const char* s){e=s;return false;}
bool Intersection(const WaterMaterialFragment& f,const WaterMaterialFragment& cut,WaterMaterialFragment& part){
    part=f;for(int a=0;a<3;++a){part.lo[a]=Max(f.lo[a],cut.lo[a]);part.hi[a]=Min(f.hi[a],cut.hi[a]);if(!((part.hi[a]-part.lo[a]).Value()>0.))return false;}return true;
}
void Split(const WaterMaterialFragment& source,const WaterMaterialFragment& overlap,
    std::vector<WaterMaterialFragment>& output){
    auto box=source;
    for(int a=0;a<3;++a){
        if((overlap.lo[a]-box.lo[a]).Value()>0.){auto piece=box;piece.hi[a]=overlap.lo[a];output.push_back(piece);box.lo[a]=overlap.lo[a];}
        if((box.hi[a]-overlap.hi[a]).Value()>0.){auto piece=box;piece.lo[a]=overlap.hi[a];output.push_back(piece);box.hi[a]=overlap.hi[a];}
    }
}
void Merge(std::vector<WaterMaterialFragment>& fragments){
    bool changed=true;
    while(changed){changed=false;
        for(std::size_t i=0;i<fragments.size()&&!changed;++i)for(std::size_t j=i+1;j<fragments.size()&&!changed;++j){auto& a=fragments[i];const auto& b=fragments[j];
            if(a.cell!=b.cell||a.water!=b.water||a.carrier!=b.carrier)continue;
            for(int axis=0;axis<3;++axis){bool matching=Equal(a.hi[axis],b.lo[axis])||Equal(b.hi[axis],a.lo[axis]);
                for(int d=0;d<3;++d)if(d!=axis&&(!Equal(a.lo[d],b.lo[d])||!Equal(a.hi[d],b.hi[d])))matching=false;
                if(matching){a.lo[axis]=Min(a.lo[axis],b.lo[axis]);a.hi[axis]=Max(a.hi[axis],b.hi[axis]);fragments.erase(fragments.begin()+j);changed=true;break;}
            }
        }
    }
}
}
bool BoundWaterInterfaceEvents(const WaterMaterialState& state,const std::vector<WaterInterfaceSweep>& sweeps,
    WaterInterfaceEventReport& output,std::string& error){
    WaterInterfaceEventReport next;next.piecesBefore=state.fragments.size();std::vector<Real> consumption(state.fragments.size());
    for(auto sweep:sweeps){auto& face=sweep.face;auto target=sweep.volume.Value()>0.?face.gas:face.water;
        if(target>=state.fragments.size()||face.water>=state.fragments.size()||face.gas>=state.fragments.size()||face.axis<0||face.axis>2||!std::isfinite(sweep.volume.Value()))return Fail(error,"invalid interface event");
        if(sweep.volume.Value()==0.)continue;
        auto& box=state.fragments[target];int direction=sweep.volume.Value()>0.?face.sign:-face.sign;
        Real depth=box.hi[face.axis]-box.lo[face.axis],area(1.);
        for(int a=0;a<3;++a)if(a!=face.axis)area=area*(face.hi[a]-face.lo[a]);
        if(!(depth.Value()>0.&&area.Value()>0.)||(direction!=1&&direction!=-1))return Fail(error,"interface has no finite receiver support");
        Real amount(std::abs(sweep.volume.Value()));consumption[target]=consumption[target]+amount;
        double fraction=(area*depth/amount).Value();if(fraction<next.maximumFraction){next.maximumFraction=fraction;next.limitingFragment=target;}
    }
    for(std::size_t i=0;i<consumption.size();++i)if(consumption[i].Value()>0.){double f=(MaterialVolume(state.fragments[i])/consumption[i]).Value();if(f<next.maximumFraction){next.maximumFraction=f;next.limitingFragment=i;}}
    if(!(next.maximumFraction>0.&&std::isfinite(next.maximumFraction)))return Fail(error,"interface event time is outside representable range");
    output=std::move(next);error.clear();return true;
}
bool ApplyWaterInterfaceEvents(const CutSpatialField& field,const WaterMaterialState& source,
    const std::vector<WaterInterfaceSweep>& sweeps,const std::vector<Real>& carrierMass,
    WaterMaterialState& output,WaterInterfaceEventReport& report,std::string& error,const std::vector<std::size_t>* connectedCarriers){
    const auto n=source.fragments.size();if(carrierMass.size()!=n||(connectedCarriers&&connectedCarriers->size()!=n))return Fail(error,"air carriers do not match the interface epoch");
    for(std::size_t i=0;i<n;++i){const auto& f=source.fragments[i];
        if(f.cell>=field.chart.regions.size()||!(MaterialVolume(f).Value()>0.)||
           !std::isfinite(MaterialVolume(f).Value())||!std::isfinite(carrierMass[i].Value())||
           carrierMass[i].Value()<0.||(connectedCarriers&&(*connectedCarriers)[i]>=n))
            return Fail(error,"invalid interface source or conserved air carrier");
    }
    for(const auto& sweep:sweeps){const auto& f=sweep.face;
        if(f.water>=n||f.gas>=n||!source.fragments[f.water].water||source.fragments[f.gas].water||
           f.axis<0||f.axis>2||(f.sign!=1&&f.sign!=-1)||!std::isfinite(sweep.volume.Value()))
            return Fail(error,"invalid material interface sweep");
        for(int d=0;d<3;++d)if(!std::isfinite(f.lo[d].Value())||!std::isfinite(f.hi[d].Value())||
            (d!=f.axis&&!((f.hi[d]-f.lo[d]).Value()>0.)))
            return Fail(error,"invalid material interface bounds");
    }
    WaterMaterialState next=source;WaterInterfaceEventReport diagnostic;diagnostic.piecesBefore=n;
    std::vector<std::vector<WaterMaterialFragment>> cells(field.chart.regions.size());
    std::vector<std::vector<std::size_t>> carrierCells(n);
    std::vector<std::vector<const CutRegionFace*>> openings(field.chart.regions.size());
    for(const auto& f:field.geometry.faces)if(f.meanArea>0.){openings[f.left].push_back(&f);openings[f.right].push_back(&f);}
    for(std::size_t i=0;i<n;++i){auto piece=source.fragments[i];piece.carrier=connectedCarriers?(*connectedCarriers)[i]:i;cells[piece.cell].push_back(piece);if(std::find(carrierCells[piece.carrier].begin(),carrierCells[piece.carrier].end(),piece.cell)==carrierCells[piece.carrier].end())carrierCells[piece.carrier].push_back(piece.cell);}
    // Recession first leaves newly occupied air space available before growth.
    // The finite-volume Lie split is explicit; distinct gas labels never merge.
    for(bool growth:{false,true})for(std::size_t sweepIndex=0;sweepIndex<sweeps.size();++sweepIndex){auto sweep=sweeps[sweepIndex];
        if(sweep.volume.Value()==0.||(sweep.volume.Value()>0.)!=growth)continue;
        auto face=sweep.face;auto target=growth?face.gas:face.water,donor=growth?face.water:face.gas;
        if(target>=n||donor>=n)return Fail(error,"invalid interface carrier");
        const auto& receiver=source.fragments[target];target=connectedCarriers?(*connectedCarriers)[target]:target;donor=connectedCarriers?(*connectedCarriers)[donor]:donor;int axis=face.axis,direction=growth?face.sign:-face.sign;
        const auto frame=source.fragments[face.water].cell;Real wanted=growth?sweep.volume:Real()-sweep.volume;
        int u=(axis+1)%3,v=(axis+2)%3;Real plane=face.lo[axis];
        struct Candidate{WaterMaterialFragment box;std::size_t cell;Real near,far;};
        std::vector<Candidate> candidates;std::vector<Real> cutU{face.lo[u],face.hi[u]},cutV{face.lo[v],face.hi[v]};
        for(auto cell:carrierCells[target])for(auto p:cells[cell])if(p.carrier==target&&p.water==receiver.water){
            for(int a=0;a<3;++a){Real shift=Real(field.chart.regions[cell].lo[a])-Real(field.chart.regions[frame].lo[a]);p.lo[a]=p.lo[a]+shift;p.hi[a]=p.hi[a]+shift;}
            p.lo[u]=Max(p.lo[u],face.lo[u]);p.hi[u]=Min(p.hi[u],face.hi[u]);p.lo[v]=Max(p.lo[v],face.lo[v]);p.hi[v]=Min(p.hi[v],face.hi[v]);
            if(!((p.hi[u]-p.lo[u]).Value()>0.&&(p.hi[v]-p.lo[v]).Value()>0.))continue;
            Real near=direction>0?p.lo[axis]-plane:plane-p.hi[axis],far=direction>0?p.hi[axis]-plane:plane-p.lo[axis];
            if(far.Value()<=0.)continue;
            candidates.push_back({p,cell,near,far});cutU.push_back(p.lo[u]);cutU.push_back(p.hi[u]);cutV.push_back(p.lo[v]);cutV.push_back(p.hi[v]);
        }
        for(auto cell:carrierCells[target])for(auto aperture:openings[cell])if(aperture->axis==axis){
            for(int a:{u,v})for(double coordinate:{aperture->lo[a],aperture->hi[a]}){Real value=Real(coordinate)-Real(field.chart.regions[frame].lo[a]);if((value-face.lo[a]).Value()>0.&&(face.hi[a]-value).Value()>0.)(a==u?cutU:cutV).push_back(value);}
        }
        auto sortCuts=[](std::vector<Real>& cuts){std::sort(cuts.begin(),cuts.end(),[](Real a,Real b){return (a-b).Value()<0.;});cuts.erase(std::unique(cuts.begin(),cuts.end(),Equal),cuts.end());};sortCuts(cutU);sortCuts(cutV);
        struct Column{Real lu,hu,lv,hv,depth;std::vector<std::size_t> chain;};std::vector<Column> columns;std::vector<Real> depths{Real()};
        for(std::size_t x=1;x<cutU.size();++x)for(std::size_t y=1;y<cutV.size();++y){Column col{cutU[x-1],cutU[x],cutV[y-1],cutV[y],Real(),{}};std::size_t last=SIZE_MAX;
            for(std::size_t pass=0;pass<candidates.size();++pass){std::size_t found=SIZE_MAX;
                for(std::size_t j=0;j<candidates.size();++j){auto& c=candidates[j];if(((col.lu-c.box.lo[u])+(col.hu-col.lu)*Real(.5)).Value()<0.||((c.box.hi[u]-col.lu)-(col.hu-col.lu)*Real(.5)).Value()<=0.||((col.lv-c.box.lo[v])+(col.hv-col.lv)*Real(.5)).Value()<0.||((c.box.hi[v]-col.lv)-(col.hv-col.lv)*Real(.5)).Value()<=0.||(col.depth-c.near).Value()<0.||!((c.far-col.depth).Value()>0.))continue;
                    bool connected=last==SIZE_MAX||c.cell==candidates[last].cell;
                    if(!connected)for(const auto* openingPtr:openings[c.cell])if(const auto& opening=*openingPtr;opening.axis==axis&&((opening.left==c.cell&&opening.right==candidates[last].cell)||(opening.right==c.cell&&opening.left==candidates[last].cell))){
                        Real baseU=Real(field.chart.regions[frame].lo[u]),baseV=Real(field.chart.regions[frame].lo[v]);
                        if(((baseU-Real(opening.lo[u]))+col.lu+(col.hu-col.lu)*Real(.5)).Value()>=0.&&
                           ((Real(opening.hi[u])-baseU)-col.lu-(col.hu-col.lu)*Real(.5)).Value()>0.&&
                           ((baseV-Real(opening.lo[v]))+col.lv+(col.hv-col.lv)*Real(.5)).Value()>=0.&&
                           ((Real(opening.hi[v])-baseV)-col.lv-(col.hv-col.lv)*Real(.5)).Value()>0.)connected=true;
                    }
                    if(connected){found=j;break;}
                }
                if(found==SIZE_MAX)break;
                col.chain.push_back(found);col.depth=candidates[found].far;last=found;
            }
            if(col.depth.Value()>0.){depths.push_back(col.depth);columns.push_back(std::move(col));}
        }
        sortCuts(depths);Real remaining=wanted,depth;bool found=false;
        for(std::size_t k=1;k<depths.size();++k){Real area;for(auto& c:columns)if((c.depth-depths[k-1]).Value()>0.)area=area+(c.hu-c.lu)*(c.hv-c.lv);Real available=area*(depths[k]-depths[k-1]);
            if(area.Value()>0.&&(available-remaining).Value()>=0.){depth=(available-remaining).Value()==0.?depths[k]:depths[k-1]+remaining/area;found=true;break;}remaining=remaining-available;
        }
        if(!found){diagnostic.failedInterface=sweepIndex;diagnostic.availableVolume=wanted-remaining;report=diagnostic;std::ostringstream d;d.precision(17);d<<"interface reaches another material boundary target="<<target<<" wanted="<<wanted.Value()<<" unsupported="<<remaining.Value()<<" receiverVolume="<<MaterialVolume(receiver).Value();error=d.str();return false;}
        Real converted;
        for(auto& column:columns)for(auto id:column.chain){auto& candidate=candidates[id];Real far=Min(depth,candidate.far),near=Max(Real(),candidate.near);if(!((far-near).Value()>0.))continue;
            auto cell=candidate.cell;WaterMaterialFragment prism=candidate.box;prism.lo[u]=column.lu;prism.hi[u]=column.hu;prism.lo[v]=column.lv;prism.hi[v]=column.hv;prism.lo[axis]=direction>0?plane+near:plane-far;prism.hi[axis]=direction>0?plane+far:plane-near;
            for(int a=0;a<3;++a){Real shift=Real(field.chart.regions[frame].lo[a])-Real(field.chart.regions[cell].lo[a]);prism.lo[a]=prism.lo[a]+shift;prism.hi[a]=prism.hi[a]+shift;}
            std::vector<WaterMaterialFragment> fragments;
            for(auto p:cells[cell]){WaterMaterialFragment part;if(p.carrier!=target||p.water!=receiver.water||!Intersection(p,prism,part)){fragments.push_back(p);continue;}
                Split(p,part,fragments);part.water=growth;part.carrier=donor;part.velocity=source.fragments[face.water].velocity;part.gasMass={};converted=converted+MaterialVolume(part);fragments.push_back(part);
            }
            cells[cell]=std::move(fragments);Merge(cells[cell]);if(std::find(carrierCells[donor].begin(),carrierCells[donor].end(),cell)==carrierCells[donor].end())carrierCells[donor].push_back(cell);
        }
        Real difference=converted-wanted;double tolerance=std::min(1e-14,1e-10*field.geometry.startCapacity[receiver.cell]);
        if(std::abs(difference.Value())>tolerance)return Fail(error,"geometric sweep lost its prescribed material volume");
        if(std::abs(difference.Value())>diagnostic.maximumSweepError.Value())diagnostic.maximumSweepError=Real(std::abs(difference.Value()));
    }
    next.fragments.clear();for(auto& cell:cells)for(auto& f:cell)next.fragments.push_back(std::move(f));
    std::vector<std::vector<std::size_t>> gas(n);std::vector<Real> volume(n);
    for(std::size_t i=0;i<next.fragments.size();++i){const auto& f=next.fragments[i];if(!f.water){gas[f.carrier].push_back(i);volume[f.carrier]=volume[f.carrier]+MaterialVolume(f);}}
    for(std::size_t carrier=0;carrier<n;++carrier)if(!source.fragments[carrier].water){
        if(gas[carrier].empty()&&carrierMass[carrier].Value()>0.)return Fail(error,"a positive-mass trapped air carrier cannot disappear at an interface event");
        std::sort(gas[carrier].begin(),gas[carrier].end(),[&](auto a,auto b){return (MaterialVolume(next.fragments[a])-MaterialVolume(next.fragments[b])).Value()<0.;});Real mass=carrierMass[carrier];
        for(std::size_t j=0;j<gas[carrier].size();++j){auto id=gas[carrier][j];Real m=j+1==gas[carrier].size()?mass:carrierMass[carrier]*MaterialVolume(next.fragments[id])/volume[carrier];next.fragments[id].gasMass=m;mass=mass-m;if(!(m.Value()>0.))return Fail(error,"air remap erased a positive material fragment");}
    }
    Real before,after;for(auto& f:source.fragments)if(f.water)before=before+MaterialVolume(f);for(auto& f:next.fragments)if(f.water)after=after+MaterialVolume(f);
    diagnostic.waterChange=after-before;diagnostic.piecesAfter=next.fragments.size();
    if(std::abs(diagnostic.waterChange.Value())>1e-14)return Fail(error,"interface sweep violates whole-water conservation");
    output=std::move(next);report=std::move(diagnostic);error.clear();return true;
}
}
