#include "WaterMimeticGeometry.h"
#include <algorithm>
#include <cmath>
#include <sstream>
namespace judas::volume_grid {
namespace {
using Real=CutCompensated;
bool Fail(std::string& e,const char* why){e=why;return false;}
}
bool PrepareWaterMimeticGeometry(const CutSpatialField& field,const WaterGasGeometry& gas,
    double density,double dt,const GridVec& gravity,WaterMimeticGeometry& output,std::string& error,const std::vector<GridVec>& sourceVelocity){
    const auto n=field.parent.size();
    if((!sourceVelocity.empty()&&sourceVelocity.size()!=n)||!n||field.chart.regions.size()!=n||field.water.size()!=n||field.gas.size()!=n||gas.cellNode.size()!=n||field.geometry.motion)
        return Fail(error,"liquid geometry requires a matching frozen gas/water chart");
    WaterMimeticGeometry next;next.density=density;next.gravity=gravity;next.cellCapacity=field.geometry.startCapacity;next.cellWater.assign(n,SIZE_MAX);std::vector<Real> height(n);
    std::vector<std::array<std::size_t,6>> neighbour(n),shared(n);
    for(auto& p:neighbour)p.fill(SIZE_MAX);
    for(auto& p:shared)p.fill(SIZE_MAX);
    for(std::size_t i=0;i<n;++i){auto r=field.chart.regions[i];double area=(r.hi[0]-r.lo[0])*(r.hi[2]-r.lo[2]);
        if(!(field.water[i]>=0.&&std::isfinite(field.water[i])&&field.gas[i]>=0.&&std::isfinite(field.gas[i])&&area>0.))return Fail(error,"invalid liquid prism inventory");
        if(field.water[i]==0.)continue;
        height[i]=field.water[i]<=field.gas[i]?Real(field.water[i])/Real(area):Real(r.hi[1]-r.lo[1])-Real(field.gas[i])/Real(area);
        if(!(height[i].Value()>0.))return Fail(error,"liquid has quantity without positive geometric depth");
        next.cellWater[i]=next.regions.size();WaterMimeticRegion region;region.sourceCell=i;region.origin=r.lo;
        region.centroid={.5*(r.hi[0]-r.lo[0]),(height[i]*Real(.5)).Value(),.5*(r.hi[2]-r.lo[2])};next.regions.push_back(std::move(region));
    }
    for(const auto& f:field.geometry.faces){
        if(f.left>=n||f.right>=n||f.axis<0||f.axis>2)return Fail(error,"invalid liquid shared face");
        if(f.meanArea==0.)continue;
        auto l=field.chart.regions[f.left],r=field.chart.regions[f.right];
        for(int a=0;a<3;++a)if(a!=f.axis&&(l.lo[a]!=r.lo[a]||l.hi[a]!=r.hi[a]))return Fail(error,"liquid prisms require conforming shared faces");
        if(neighbour[f.left][2*f.axis+1]!=SIZE_MAX||neighbour[f.right][2*f.axis]!=SIZE_MAX)return Fail(error,"duplicate liquid face geometry");
        neighbour[f.left][2*f.axis+1]=f.right;neighbour[f.right][2*f.axis]=f.left;
        bool wet=next.cellWater[f.left]!=SIZE_MAX&&next.cellWater[f.right]!=SIZE_MAX;
        if(f.axis==1)wet=wet&&field.gas[f.left]==0.;
        if(wet){auto id=next.sharedFaces++;shared[f.left][2*f.axis+1]=id;shared[f.right][2*f.axis]=id;}
    }
    for(auto& region:next.regions){auto cell=region.sourceCell;auto r=field.chart.regions[cell];
        auto append=[&](int axis,int sign,Real loY,Real hiY,std::size_t sharedId,std::size_t gasId,bool fixed){
            Real area;std::array<Real,3> center{Real(region.centroid[0]),height[cell]*Real(.5),Real(region.centroid[2])};
            if(axis==1){area=Real(r.hi[0]-r.lo[0])*Real(r.hi[2]-r.lo[2]);center[1]=loY;}
            else{int widthAxis=axis==0?2:0;area=Real(r.hi[widthAxis]-r.lo[widthAxis])*(hiY-loY);center[1]=(loY+hiY)*Real(.5);center[axis]=sign<0?Real():Real(r.hi[axis]-r.lo[axis]);}
            if(area.Value()<=0.)return;
            WaterCellPatch patch;patch.axis=axis;patch.sign=sign;patch.area=area.Value();patch.fixed=fixed;
            for(int a=0;a<3;++a){Real centroid=a==1?height[cell]*Real(.5):Real(region.centroid[a]);patch.offset[a]=(center[a]-centroid).Value();}
            Real potential;for(int a=0;a<3;++a)potential=potential-Real(density*gravity[a])*(Real(region.origin[a])+center[a]);
            region.patches.push_back(patch);region.owners.push_back({sharedId,gasId});region.potential.push_back(potential);
            if(fixed)++next.solidFaces;else if(gasId!=SIZE_MAX)++next.freeSurfaces;
        };
        for(int axis=0;axis<3;++axis)for(int side=0;side<2;++side){int sign=side?1:-1;
            if(axis==1&&side&&field.gas[cell]>0.){if(gas.cellNode[cell]==SIZE_MAX)return Fail(error,"free liquid surface lost its gas owner");append(axis,sign,height[cell],height[cell],SIZE_MAX,gas.cellNode[cell],false);continue;}
            auto other=neighbour[cell][2*axis+side],connection=shared[cell][2*axis+side];
            if(axis==1){Real coordinate=side?height[cell]:Real();
                if(connection!=SIZE_MAX)append(axis,sign,coordinate,coordinate,connection,SIZE_MAX,false);
                else if(other!=SIZE_MAX){auto owner=gas.cellNode[other];if(owner==SIZE_MAX)return Fail(error,"liquid boundary has neither shared water nor gas space");append(axis,sign,coordinate,coordinate,SIZE_MAX,owner,false);}
                else append(axis,sign,coordinate,coordinate,SIZE_MAX,SIZE_MAX,true);
            }else{
                if(other==SIZE_MAX){append(axis,sign,{},height[cell],SIZE_MAX,SIZE_MAX,true);continue;}
                Real common=(height[cell]-height[other]).Value()<0.?height[cell]:height[other];
                if(connection!=SIZE_MAX)append(axis,sign,{},common,connection,SIZE_MAX,false);
                if((height[cell]-common).Value()>0.){auto owner=gas.cellNode[other];if(owner==SIZE_MAX)return Fail(error,"exposed liquid face lost the adjacent gas owner");append(axis,sign,common,height[cell],SIZE_MAX,owner,false);}
            }
        }
        if(!PrepareWaterMimeticCell(field.water[cell],density,dt,gravity,region.patches,region.response,error,sourceVelocity.empty()?GridVec{}:sourceVelocity[cell])){
            std::ostringstream detail;detail.precision(17);detail<<error<<" cell="<<cell<<" W="<<field.water[cell]<<" height="<<height[cell].hi<<"+"<<height[cell].lo;
            for(auto p:region.patches)detail<<" [axis="<<p.axis<<" sign="<<p.sign<<" A="<<p.area<<" r="<<p.offset[0]<<","<<p.offset[1]<<","<<p.offset[2]<<" fixed="<<p.fixed<<"]";
            error=detail.str();return false;
        }
    }
    output=std::move(next);error.clear();return true;
}
}
