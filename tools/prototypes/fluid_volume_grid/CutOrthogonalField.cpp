#include "CutOrthogonalField.h"
#include "CutCondensedPressure.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace judas::volume_grid {
namespace {
using Real=CutCompensated;
bool Fail(std::string& e,const char* text){e=text;return false;}
}
bool BuildCutOrthogonalField(const CutSpatialField& source,CutSpatialField& output,
    CutOrthogonalRemap& report,std::string& error,std::size_t budget){
    const auto n=source.parent.size();const auto& layout=source.chart.layout;
    if(&source==&output||!n||source.chart.regions.size()!=n||source.water.size()!=n||source.gas.size()!=n||source.gasMass.size()!=n||
        source.geometry.startCapacity.size()!=n||source.geometry.endCapacity!=source.geometry.startCapacity||budget==0||
        source.flux.size()!=source.geometry.faces.size()||!layout.CheckedCellCount()||
        (source.geometry.motion&&source.geometry.motion->translation!=GridVec{}))return Fail(error,"invalid frozen orthogonal source");
    for(std::size_t i=0;i<n;++i){
        if(!(source.gasMass[i]>=0.&&std::isfinite(source.gasMass[i])))return Fail(error,"invalid source gas mass");
        for(int a=0;a<3;++a)if(!(source.chart.regions[i].lo[a]>=0.&&source.chart.regions[i].hi[a]>source.chart.regions[i].lo[a]&&
            source.chart.regions[i].hi[a]<=layout.cells[a]*layout.h))return Fail(error,"source region lies outside its finite chart");
    }
    for(double q:source.flux)if(q!=0.)return Fail(error,"orthogonal compiler cannot remap an existing velocity state");
    std::array<std::vector<double>,3> cuts;
    for(int a=0;a<3;++a){
        for(int i=0;i<=layout.cells[a];++i)cuts[a].push_back(i*layout.h);
        for(const auto& r:source.chart.regions){cuts[a].push_back(r.lo[a]);cuts[a].push_back(r.hi[a]);}
        if(source.geometry.motion)for(double x:{source.geometry.motion->lo[a],source.geometry.motion->hi[a]})
            if(x>0.&&x<layout.cells[a]*layout.h)cuts[a].push_back(x);
        auto& v=cuts[a];std::sort(v.begin(),v.end());v.erase(std::unique(v.begin(),v.end()),v.end());
    }
    CutOrthogonalRemap stats;stats.originalCells=n;std::size_t total=1;
    for(int a=0;a<3;++a){stats.intervals[a]=cuts[a].size()-1;if(total>budget/stats.intervals[a])return Fail(error,"conforming cut-grid budget exceeded without coarsening");total*=stats.intervals[a];}
    std::vector<std::vector<std::size_t>> bins(layout.CheckedCellCount());
    for(std::size_t i=0;i<n;++i){auto parent=source.chart.regions[i].parent;if(parent>=bins.size())return Fail(error,"source coarse-cell parent is invalid");bins[parent].push_back(i);}
    std::vector<CutRegionWaterPlane> planes;
    if(!ReconstructCutRegionPhasePlanes(source.chart,source.geometry,1,source.water,source.gas,{},planes,error))return false;
    std::vector<Real> remappedWater(n),remappedMass(n);
    std::vector<std::size_t> index(total,SIZE_MAX),original;CutSpatialField next;next.chart.layout=layout;
    auto logical=[&](std::size_t x,std::size_t y,std::size_t z){return (z*stats.intervals[1]+y)*stats.intervals[0]+x;};
    for(std::size_t z=0;z<stats.intervals[2];++z)for(std::size_t y=0;y<stats.intervals[1];++y)for(std::size_t x=0;x<stats.intervals[0];++x){
        const std::array<std::size_t,3> cell{x,y,z};GridVec lo,hi;std::array<int,3> parent;
        for(int a=0;a<3;++a){lo[a]=cuts[a][cell[a]];hi[a]=cuts[a][cell[a]+1];parent[a]=std::clamp(int(std::floor((lo[a]+.5*(hi[a]-lo[a]))/layout.h)),0,layout.cells[a]-1);}
        const auto coarse=layout.Cell(parent[0],parent[1],parent[2]);std::size_t owner=SIZE_MAX;
        for(auto i:bins[coarse]){const auto& r=source.chart.regions[i];bool contains=true;for(int a=0;a<3;++a)contains=contains&&r.lo[a]<=lo[a]&&r.hi[a]>=hi[a];if(contains){if(owner!=SIZE_MAX)return Fail(error,"overlapping source regions");owner=i;}}
        if(owner==SIZE_MAX)continue;
        const auto& region=source.chart.regions[owner];const double actual=CutRegionClippedVolume(region,source.geometry.motion,lo,hi);
        if(actual==0.)continue;
        const double capacity=(hi[0]-lo[0])*(hi[1]-lo[1])*(hi[2]-lo[2]);
        if(std::abs(actual-capacity)>256.*std::numeric_limits<double>::epsilon()*capacity)return Fail(error,"conforming cell still contains an unresolved solid cut");
        auto volume=MeasureCutRegionPhaseVolumes(region,source.geometry.motion,lo,hi,planes[owner]);
        if(source.gas[owner]==0.&&source.gasMass[owner]!=0.)return Fail(error,"source gas mass has no geometric owner");
        const double mass=source.gas[owner]>0.?volume[1]*(source.gasMass[owner]/source.gas[owner]):0.;
        const auto j=next.parent.size();index[logical(x,y,z)]=j;original.push_back(owner);
        next.parent.push_back(owner);next.chart.regions.push_back({lo,hi,coarse});
        next.water.push_back(volume[0]);next.gas.push_back(volume[1]);next.gasMass.push_back(mass);
        next.geometry.startCapacity.push_back(capacity);next.geometry.endCapacity.push_back(capacity);
        remappedWater[owner]=remappedWater[owner]+Real(volume[0]);remappedMass[owner]=remappedMass[owner]+Real(mass);
    }
    for(std::size_t z=0;z<stats.intervals[2];++z)for(std::size_t y=0;y<stats.intervals[1];++y)for(std::size_t x=0;x<stats.intervals[0];++x){
        const auto l=index[logical(x,y,z)];if(l==SIZE_MAX)continue;const std::array<std::size_t,3> at{x,y,z};
        for(int a=0;a<3;++a){auto neighbour=at;if(++neighbour[a]>=stats.intervals[a])continue;
            const auto r=index[logical(neighbour[0],neighbour[1],neighbour[2])];if(r==SIZE_MAX)continue;
            auto lo=next.chart.regions[l].lo,hi=next.chart.regions[l].hi;lo[a]=hi[a];
            const double area=(hi[(a+1)%3]-lo[(a+1)%3])*(hi[(a+2)%3]-lo[(a+2)%3]);
            next.geometry.faces.push_back({l,r,a,lo,hi,area,area,area,0.,.5*((next.chart.regions[l].hi[a]-next.chart.regions[l].lo[a])+(next.chart.regions[r].hi[a]-next.chart.regions[r].lo[a]))});
            next.flux.push_back(0.);next.mobility.push_back(area/next.geometry.faces.back().length);next.originalFace.push_back(SIZE_MAX);
        }
    }
    std::vector<std::vector<CutPhaseBoundary>> boundaries(n);
    for(const auto& b:source.boundaries){if(b.fragment>=n||b.axis<0||b.axis>2||(b.sign!=1&&b.sign!=-1))return Fail(error,"invalid orthogonal source boundary");boundaries[b.fragment].push_back(b);}
    for(std::size_t j=0;j<next.parent.size();++j)for(const auto& b:boundaries[original[j]]){
        const auto& r=next.chart.regions[j];const auto& old=source.chart.regions[b.fragment];
        if((b.sign>0?r.hi[b.axis]:r.lo[b.axis])!=(b.sign>0?old.hi[b.axis]:old.lo[b.axis]))continue;
        const int a=(b.axis+1)%3,c=(b.axis+2)%3;auto port=b;
        port.fragment=j;port.volume*=((r.hi[a]-r.lo[a])*(r.hi[c]-r.lo[c]))/((old.hi[a]-old.lo[a])*(old.hi[c]-old.lo[c]));next.boundaries.push_back(port);
    }
    Real waterError,massError;
    for(std::size_t i=0;i<n;++i){const Real w=remappedWater[i]-Real(source.water[i]),m=remappedMass[i]-Real(source.gasMass[i]);
        const double limit=1e-12*source.geometry.startCapacity[i];
        if(std::abs(w.Value())>limit||std::abs(m.Value())>1.2*limit)return Fail(error,"orthogonal source remap fails local quantity ledger");
        waterError=waterError+w;massError=massError+m;
    }
    if(std::abs(waterError.Value())>1e-14||std::abs(massError.Value())>1.2e-14)return Fail(error,"orthogonal source remap fails original cold quantity ledger");
    stats.orthogonalCells=next.parent.size();stats.waterError=waterError.Value();stats.gasMassError=massError.Value();
    next.chart.faces=next.geometry.faces;output=std::move(next);report=stats;error.clear();return true;
}
} // namespace judas::volume_grid
