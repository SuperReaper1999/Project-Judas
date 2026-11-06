#include "WaterFragmentGeometry.h"
#include "WaterGasAperture.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <map>
#include <tuple>
#include <iostream>
#include <iomanip>
namespace judas::volume_grid {
namespace {
using Real=CutCompensated;
struct Rectangle{WaterCoordinate lo,hi;};
Real Min(Real a,Real b){return (a-b).Value()<0.?a:b;}
Real Max(Real a,Real b){return (a-b).Value()>0.?a:b;}
Real Area(const Rectangle& p,int axis){Real a(1.);for(int d=0;d<3;++d)if(d!=axis)a=a*(p.hi[d]-p.lo[d]);return a;}
bool Fail(std::string& e,const char* s){e=s;return false;}
bool Subtract(std::vector<Rectangle>& remaining,Rectangle cut,int axis){
    std::vector<Rectangle> next;Real covered;
    for(auto box:remaining){Rectangle overlap=box;bool positive=true;
        for(int d=0;d<3;++d)if(d!=axis){overlap.lo[d]=Max(box.lo[d],cut.lo[d]);overlap.hi[d]=Min(box.hi[d],cut.hi[d]);if(!((overlap.hi[d]-overlap.lo[d]).Value()>0.))positive=false;}
        if(!positive){next.push_back(box);continue;}
        covered=covered+Area(overlap,axis);
        for(int d=0;d<3;++d)if(d!=axis){
            if((overlap.lo[d]-box.lo[d]).Value()>0.){auto part=box;part.hi[d]=overlap.lo[d];next.push_back(part);box.lo[d]=overlap.lo[d];}
            if((box.hi[d]-overlap.hi[d]).Value()>0.){auto part=box;part.lo[d]=overlap.hi[d];next.push_back(part);box.hi[d]=overlap.hi[d];}
        }
    }
    if(std::abs((covered-Area(cut,axis)).Value())>1e-12*Area(cut,axis).Value())return false;
    remaining=std::move(next);return true;
}
}
bool PrepareWaterFragmentGeometry(const CutSpatialField& field,const WaterMaterialState& state,
    double density,const GridVec& gravity,const WaterGasSettings& settings,
    WaterFragmentGeometry& output,std::string& error){
    WaterFragmentGeometry next;const auto n=state.fragments.size();auto& water=next.liquid;auto& gas=next.air;
    water.density=density;water.gravity=gravity;water.cellWater.assign(n,SIZE_MAX);water.cellCapacity.resize(n);gas.cellNode.assign(n,SIZE_MAX);next.carrier.resize(n);std::iota(next.carrier.begin(),next.carrier.end(),0);
    for(auto port:field.boundaries){
        if(port.fragment>=field.chart.regions.size()||port.axis<0||port.axis>2||(port.sign!=1&&port.sign!=-1)||!port.gasOnly||port.volume!=0.||port.incomingWater!=0.)return Fail(error,"invalid fragment atmospheric boundary");
        const auto& cell=field.chart.regions[port.fragment];double boundary=port.sign>0?cell.hi[port.axis]:cell.lo[port.axis];
        if(boundary!=(port.sign>0?field.chart.layout.cells[port.axis]*field.chart.layout.h:0.))return Fail(error,"fragment atmosphere must be a real exterior opening");
    }
    std::vector<std::array<std::vector<Rectangle>,6>> remaining(n);
    for(std::size_t i=0;i<n;++i){const auto& f=state.fragments[i];if(f.cell>=field.chart.regions.size())return Fail(error,"fragment has no containing cell");
        Real v=MaterialVolume(f);if(!(v.Value()>0.&&std::isfinite(v.Value())))return Fail(error,"invalid fragment volume");
        water.cellCapacity[i]=field.geometry.startCapacity[f.cell];
        for(int axis=0;axis<3;++axis)for(int side=0;side<2;++side){Rectangle face{f.lo,f.hi};face.lo[axis]=face.hi[axis]=side?f.hi[axis]:f.lo[axis];remaining[i][2*axis+side].push_back(face);}
        if(!f.water){if(!(f.gasMass.Value()>0.))return Fail(error,"air fragment has no positive conserved mass");}
    }
    if(!BuildWaterMaterialFaces(field,state,next.faces,error))return false;
    std::vector<std::size_t> gasParent(n);std::iota(gasParent.begin(),gasParent.end(),0);
    auto gasRoot=[&](std::size_t i){while(gasParent[i]!=i){gasParent[i]=gasParent[gasParent[i]];i=gasParent[i];}return i;};
    for(auto f:next.faces){const auto& a=state.fragments[f.left];const auto& b=state.fragments[f.right];
        if(!a.water&&!b.water&&a.cell==b.cell&&a.carrier==b.carrier){auto x=gasRoot(f.left),y=gasRoot(f.right);if(x!=y)gasParent[y]=x;}
    }
    std::vector<std::size_t> gasOwner(n,SIZE_MAX);
    for(std::size_t i=0;i<n;++i)if(!state.fragments[i].water){auto r=gasRoot(i);
        if(gasOwner[r]==SIZE_MAX){gasOwner[r]=gas.nodeCell.size();gas.nodeCell.push_back(i);gas.volume.push_back({});gas.mass.push_back({});gas.containingCapacity.push_back(field.geometry.startCapacity[state.fragments[i].cell]);}
        auto id=gasOwner[r];gas.cellNode[i]=id;next.carrier[i]=gas.nodeCell[id];gas.volume[id]=gas.volume[id]+MaterialVolume(state.fragments[i]);gas.mass[id]=gas.mass[id]+state.fragments[i].gasMass;
    }
    std::vector<WaterCoordinate> gasLo(gas.nodeCell.size()),gasHi(gas.nodeCell.size());
    std::vector<bool> gasBounds(gas.nodeCell.size());
    for(std::size_t i=0;i<n;++i)if(!state.fragments[i].water){auto node=gas.cellNode[i];const auto& f=state.fragments[i];
        if(!gasBounds[node]){gasLo[node]=f.lo;gasHi[node]=f.hi;gasBounds[node]=true;}
        else for(int d=0;d<3;++d){gasLo[node][d]=Min(gasLo[node][d],f.lo[d]);gasHi[node][d]=Max(gasHi[node][d],f.hi[d]);}
    }
    std::vector<std::size_t> parent(n);std::iota(parent.begin(),parent.end(),0);
    auto root=[&](std::size_t i){while(parent[i]!=i){parent[i]=parent[parent[i]];i=parent[i];}return i;};
    for(auto f:next.faces)if(state.fragments[f.left].water&&state.fragments[f.right].water&&state.fragments[f.left].cell==state.fragments[f.right].cell){auto a=root(f.left),b=root(f.right);if(a!=b)parent[b]=a;}
    std::vector<std::size_t> owner(n,SIZE_MAX);std::vector<WaterCoordinate> centre,moment,anchor;
    for(std::size_t i=0;i<n;++i)if(state.fragments[i].water){auto r=root(i);if(owner[r]==SIZE_MAX){owner[r]=water.regions.size();WaterMimeticRegion region;region.sourceCell=i;region.origin=field.chart.regions[state.fragments[i].cell].lo;water.regions.push_back(region);next.waterVolume.push_back({});centre.push_back({});moment.push_back({});anchor.push_back(state.fragments[i].lo);}
        auto id=owner[r];water.cellWater[i]=id;next.carrier[i]=water.regions[id].sourceCell;Real volume=MaterialVolume(state.fragments[i]);next.waterVolume[id]=next.waterVolume[id]+volume;
        for(int a=0;a<3;++a){centre[id][a]=centre[id][a]+volume*((state.fragments[i].lo[a]-anchor[id][a])+(state.fragments[i].hi[a]-anchor[id][a]))*Real(.5);moment[id][a]=moment[id][a]+volume*Real(state.fragments[i].velocity[a]);}
    }
    for(std::size_t r=0;r<water.regions.size();++r)for(int a=0;a<3;++a){centre[r][a]=centre[r][a]/next.waterVolume[r];water.regions[r].centroid[a]=(anchor[r][a]+centre[r][a]).Value();}
    auto append=[&](std::size_t id,int axis,int sign,Rectangle face,std::size_t shared,std::size_t gasNode,bool fixed){
        auto& region=water.regions[water.cellWater[id]];WaterCellPatch patch;patch.axis=axis;patch.sign=sign;patch.fixed=fixed;patch.compensatedGeometry=true;patch.geometricArea=Area(face,axis);patch.area=patch.geometricArea.Value();Real potential;
        for(int a=0;a<3;++a){Real center=(face.lo[a]+face.hi[a])*Real(.5),mid=centre[water.cellWater[id]][a];patch.geometricOffset[a]=((face.lo[a]-anchor[water.cellWater[id]][a])+(face.hi[a]-anchor[water.cellWater[id]][a]))*Real(.5)-mid;patch.offset[a]=patch.geometricOffset[a].Value();potential=potential-Real(density*gravity[a])*(Real(region.origin[a])+center);}
        auto patchId=region.patches.size();region.patches.push_back(patch);region.owners.push_back({shared,gasNode});region.potential.push_back(potential);
        if(fixed)++water.solidFaces;else if(gasNode!=SIZE_MAX)++water.freeSurfaces;return patchId;
    };
    std::map<std::tuple<std::size_t,std::size_t,int>,std::size_t> sharedIndex;
    struct Opening {std::size_t left,right;int axis,sign;double length;std::vector<WaterMaterialFragment> rectangles;};
    std::vector<Opening> openings;
    std::map<std::tuple<std::size_t,std::size_t,int,int,double,double>,std::size_t> openingIndex;
    auto aperture=[&](std::size_t left,std::size_t right,int axis,int sign,Rectangle face,double length){
        Real plane=face.lo[axis];auto key=std::make_tuple(left,right,axis,sign,plane.hi,plane.lo);
        auto entry=openingIndex.emplace(key,openings.size());
        if(entry.second)openings.push_back({left,right,axis,sign,length,{}});
        WaterMaterialFragment footprint;footprint.lo=face.lo;footprint.hi=face.hi;footprint.lo[axis]={};footprint.hi[axis]=Real(1.);
        openings[entry.first->second].rectangles.push_back(footprint);
    };
    for(auto face:next.faces){auto& l=state.fragments[face.left];auto& r=state.fragments[face.right];Rectangle a{face.lo,face.hi},b=a;
        for(int d=0;d<3;++d){Real shift=Real(field.chart.regions[l.cell].lo[d])-Real(field.chart.regions[r.cell].lo[d]);b.lo[d]=b.lo[d]+shift;b.hi[d]=b.hi[d]+shift;}
        if(!Subtract(remaining[face.left][2*face.axis+1],a,face.axis)||!Subtract(remaining[face.right][2*face.axis],b,face.axis))return Fail(error,"material neighbour faces overlap or leave inconsistent ownership");
        if(l.water&&r.water){if(water.cellWater[face.left]==water.cellWater[face.right])continue;auto entry=sharedIndex.emplace(std::make_tuple(water.cellWater[face.left],water.cellWater[face.right],face.axis),water.sharedFaces);auto id=entry.first->second;if(entry.second){++water.sharedFaces;next.sharedWater.push_back({face.left,face.right});}append(face.left,face.axis,1,a,id,SIZE_MAX,false);append(face.right,face.axis,-1,b,id,SIZE_MAX,false);}
        else if(!l.water&&!r.water){auto left=gas.cellNode[face.left],right=gas.cellNode[face.right];if(left==right)continue;
            double length=((gasHi[left][face.axis]-gasLo[left][face.axis]+gasHi[right][face.axis]-gasLo[right][face.axis])*Real(.5)).Value();
            aperture(left,right,face.axis,1,a,length);
        }
        else{auto w=l.water?face.left:face.right,g=l.water?face.right:face.left;Rectangle p=l.water?a:b;int sign=l.water?1:-1;auto patch=append(w,face.axis,sign,p,SIZE_MAX,gas.cellNode[g],false);next.interfaces.push_back({w,g,patch,face.axis,sign,p.lo,p.hi});}
    }
    for(std::size_t i=0;i<n;++i)for(int axis=0;axis<3;++axis)for(int side=0;side<2;++side)for(auto p:remaining[i][2*axis+side]){
        const auto& f=state.fragments[i];int sign=side?1:-1;bool vent=false;
        for(auto port:field.boundaries)if(port.fragment==f.cell&&port.axis==axis&&port.sign==sign){Real coordinate=side?Real(field.chart.regions[f.cell].hi[axis]-field.chart.regions[f.cell].lo[axis]):Real();if((p.lo[axis]-coordinate).Value()==0.)vent=true;}
        if(f.water){if(vent)return Fail(error,"water escape through an exterior air port needs an explicit boundary receiver");append(i,axis,sign,p,SIZE_MAX,SIZE_MAX,true);}
        else if(vent){auto node=gas.cellNode[i];double length=((gasHi[node][axis]-gasLo[node][axis])*Real(.5)).Value();aperture(node,SIZE_MAX,axis,sign,p,length);}

    }
    for(const auto& opening:openings){double area=0.,conductance=0.;
        if(!MeasureWaterGasAperture(opening.rectangles,opening.axis,opening.length,settings,area,conductance,error))return false;
        if(opening.right==SIZE_MAX)gas.vents.push_back({opening.left,conductance,0.});
        else{gas.links.push_back({opening.left,opening.right,conductance});gas.sharedDryArea.push_back(area);}
    }
    // Boundary rectangles are integration pieces, not independent velocity
    // degrees of freedom. One constant normal velocity per oriented boundary
    // and pressure owner retains its exact area and first moment. Hydrostatic
    // pressure is integrated through that same first moment.
    std::vector<std::vector<std::size_t>> patchMap(water.regions.size());
    for(std::size_t r=0;r<water.regions.size();++r){auto& region=water.regions[r];
        std::vector<WaterCellPatch> patches;std::vector<WaterPatchOwner> owners;
        std::vector<Real> potential;std::vector<std::array<Real,3>> firstMoment;
        patchMap[r].resize(region.patches.size());
        for(std::size_t i=0;i<region.patches.size();++i){auto p=region.patches[i];auto o=region.owners[i];std::size_t id=SIZE_MAX;
            for(std::size_t j=0;j<patches.size();++j)if(p.axis==patches[j].axis&&p.sign==patches[j].sign&&p.fixed==patches[j].fixed&&o.shared==owners[j].shared&&o.gas==owners[j].gas){id=j;break;}
            if(id==SIZE_MAX){id=patches.size();auto empty=p;empty.geometricArea={};empty.area=0.;patches.push_back(empty);owners.push_back(o);potential.push_back({});firstMoment.push_back({});}
            patchMap[r][i]=id;patches[id].geometricArea=patches[id].geometricArea+p.Area();
            potential[id]=potential[id]+p.Area()*region.potential[i];
            for(int d=0;d<3;++d)firstMoment[id][d]=firstMoment[id][d]+p.Area()*p.Offset(d);
        }
        for(std::size_t i=0;i<patches.size();++i){auto& p=patches[i];p.area=p.geometricArea.Value();potential[i]=potential[i]/p.geometricArea;for(int d=0;d<3;++d){p.geometricOffset[d]=firstMoment[i][d]/p.geometricArea;p.offset[d]=p.geometricOffset[d].Value();}}
        region.patches=std::move(patches);region.owners=std::move(owners);region.potential=std::move(potential);
    }
    for(auto& face:next.interfaces)face.patch=patchMap[water.cellWater[face.water]][face.patch];
    for(std::size_t r=0;r<water.regions.size();++r){auto& region=water.regions[r];GridVec velocity;for(int a=0;a<3;++a)velocity[a]=(moment[r][a]/next.waterVolume[r]).Value();
        if(!PrepareWaterMimeticCell(next.waterVolume[r].Value(),density,settings.dt,gravity,region.patches,region.response,error,velocity)){error+=" connected material owner="+std::to_string(region.sourceCell);
            if(settings.trace){std::cerr<<std::setprecision(17)<<"BAD_OWNER volume="<<next.waterVolume[r].Value()<<" patches="<<region.patches.size()<<" error="<<error<<'\n';
                for(std::size_t i=0;i<n;++i)if(state.fragments[i].water&&water.cellWater[i]==r){auto f=state.fragments[i];std::cerr<<"PIECE id="<<i<<" cell="<<f.cell<<" volume="<<MaterialVolume(f).Value();for(int a=0;a<3;++a)std::cerr<<" lo"<<a<<"="<<f.lo[a].hi<<","<<f.lo[a].lo<<" hi"<<a<<"="<<f.hi[a].hi<<","<<f.hi[a].lo;std::cerr<<'\n';}
                for(auto p:region.patches)std::cerr<<"PATCH axis="<<p.axis<<" sign="<<p.sign<<" area="<<p.area<<" offset="<<p.offset[0]<<","<<p.offset[1]<<","<<p.offset[2]<<'\n';}
            return false;}
    }
    output=std::move(next);error.clear();return true;
}
}
