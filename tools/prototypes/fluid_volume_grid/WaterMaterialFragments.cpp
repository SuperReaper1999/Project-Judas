#include "WaterMaterialFragments.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace judas::volume_grid {
namespace {
using Real=CutCompensated;
bool Fail(std::string& e,const char* why){e=why;return false;}
Real Minimum(Real a,Real b){return (a-b).Value()<0.?a:b;}
Real Maximum(Real a,Real b){return (a-b).Value()>0.?a:b;}
bool Equal(Real a,Real b){auto d=a-b;return d.hi==0.&&d.lo==0.;}
}
CutCompensated MaterialVolume(const WaterMaterialFragment& f){
    Real volume(1.);for(int a=0;a<3;++a)volume=volume*(f.hi[a]-f.lo[a]);return volume;
}
bool ImportWaterMaterial(const CutSpatialField& field,const std::vector<GridVec>& velocity,
    WaterMaterialState& output,std::string& error){
    const auto n=field.chart.regions.size();
    if(!n||field.water.size()!=n||field.gas.size()!=n||field.gasMass.size()!=n||field.geometry.startCapacity.size()!=n||field.geometry.startCapacity!=field.geometry.endCapacity||field.geometry.motion||(!velocity.empty()&&velocity.size()!=n))return Fail(error,"material import requires matching frozen rectangular cells");
    WaterMaterialState next;
    for(std::size_t i=0;i<n;++i){auto r=field.chart.regions[i];WaterMaterialFragment box;box.cell=i;
        for(int a=0;a<3;++a){double length=r.hi[a]-r.lo[a];if(!(length>0.&&std::isfinite(length)))return Fail(error,"invalid material cell bounds");box.hi[a]=Real(length);}
        Real capacity=MaterialVolume(box),w(field.water[i]),g(field.gas[i]);
        if(!(w.Value()>=0.&&g.Value()>=0.&&field.gasMass[i]>=0.&&std::isfinite(w.Value())&&std::isfinite(g.Value())&&std::isfinite(field.gasMass[i])))return Fail(error,"invalid imported material quantity");
        if(std::abs((capacity-w-g).Value())>256.*std::numeric_limits<double>::epsilon()*capacity.Value())return Fail(error,"material import does not partition its physical cell");
        Real area=box.hi[0]*box.hi[2];Real height=w.Value()<=g.Value()?w/area:box.hi[1]-g/area;
        if(w.Value()>0.){auto liquid=box;liquid.water=true;liquid.hi[1]=height;liquid.carrier=next.fragments.size();if(!velocity.empty())liquid.velocity=velocity[i];Real actual=MaterialVolume(liquid);if(!(actual.Value()>0.))return Fail(error,"positive imported water lost geometric support");next.waterImportError=next.waterImportError+actual-w;next.fragments.push_back(liquid);}
        if(g.Value()>0.){auto air=box;air.lo[1]=height;air.gasMass=Real(field.gasMass[i]);air.carrier=next.fragments.size();if(!(MaterialVolume(air).Value()>0.)||!(air.gasMass.Value()>0.))return Fail(error,"positive imported air lost volume or mass");next.fragments.push_back(air);}
        else if(field.gasMass[i]!=0.)return Fail(error,"air mass has no imported dry volume");
        next.importedWater=next.importedWater+w;next.importedGasMass=next.importedGasMass+Real(field.gasMass[i]);
    }
    if(std::abs(next.waterImportError.Value())>1e-14)return Fail(error,"material import exceeds original water-quantity budget");
    output=std::move(next);error.clear();return true;
}
bool BuildWaterMaterialFaces(const CutSpatialField& field,const WaterMaterialState& state,
    std::vector<WaterMaterialFace>& output,std::string& error){
    const auto n=field.chart.regions.size();std::vector<std::vector<std::size_t>> cells(n);std::vector<WaterMaterialFace> next;
    for(std::size_t i=0;i<state.fragments.size();++i){auto& f=state.fragments[i];if(f.cell>=n||!(MaterialVolume(f).Value()>0.))return Fail(error,"invalid local material fragment");cells[f.cell].push_back(i);}
    auto pair=[&](std::size_t l,std::size_t r,int axis,const CutRegionFace* containing){
        auto& a=state.fragments[l];auto& b=state.fragments[r];WaterCoordinate shift;
        for(int d=0;d<3;++d)shift[d]=Real(field.chart.regions[b.cell].lo[d])-Real(field.chart.regions[a.cell].lo[d]);
        if(!Equal(a.hi[axis],b.lo[axis]+shift[axis]))return;
        WaterMaterialFace face;face.left=l;face.right=r;face.axis=axis;
        for(int d=0;d<3;++d){face.lo[d]=Maximum(a.lo[d],b.lo[d]+shift[d]);face.hi[d]=Minimum(a.hi[d],b.hi[d]+shift[d]);if(d!=axis&&!((face.hi[d]-face.lo[d]).Value()>0.))return;}
        if(containing)for(int d=0;d<3;++d)if(d!=axis){face.lo[d]=Maximum(face.lo[d],Real(containing->lo[d])-Real(field.chart.regions[a.cell].lo[d]));face.hi[d]=Minimum(face.hi[d],Real(containing->hi[d])-Real(field.chart.regions[a.cell].lo[d]));if(!((face.hi[d]-face.lo[d]).Value()>0.))return;}
        face.lo[axis]=face.hi[axis]=a.hi[axis];next.push_back(face);
    };
    for(auto& cell:cells)for(std::size_t i=0;i<cell.size();++i)for(std::size_t j=i+1;j<cell.size();++j)for(int a=0;a<3;++a){pair(cell[i],cell[j],a,nullptr);pair(cell[j],cell[i],a,nullptr);}
    for(auto& face:field.geometry.faces){
        if(face.left>=n||face.right>=n||face.axis<0||face.axis>2)return Fail(error,"invalid containing-cell adjacency");
        if(face.meanArea==0.)continue;
        double area=1.;for(int a=0;a<3;++a)if(a!=face.axis)area*=face.hi[a]-face.lo[a];
        if(!(face.meanArea>0.)||std::abs(face.meanArea-area)>256.*std::numeric_limits<double>::epsilon()*area)return Fail(error,"material adjacency needs explicitly carved solid apertures");
        for(auto l:cells[face.left])for(auto r:cells[face.right])pair(l,r,face.axis,&face);
    }
    output=std::move(next);error.clear();return true;
}
}
