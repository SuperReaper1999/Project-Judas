#include "WaterVolumeAdvance.h"
#include <cmath>
#include <iostream>
#include <map>
using namespace judas::volume_grid;
namespace {
int checks=0,failures=0;void Check(bool ok,const std::string& why){++checks;if(!ok){++failures;std::cerr<<why<<'\n';}}
}
int main(){
    CutSpatialField f;f.chart.layout={{3,3,3},1.};std::map<std::array<int,3>,std::size_t> cells;
    for(int z=0;z<3;++z)for(int y=0;y<3;++y)for(int x=0;x<3;++x){
        if(x==1&&y==1&&z==1)continue;
        auto i=f.parent.size();cells[{x,y,z}]=i;f.parent.push_back(i);
        f.chart.regions.push_back({{double(x),double(y),double(z)},{double(x+1),double(y+1),double(z+1)},i});
        f.geometry.startCapacity.push_back(1.);f.geometry.endCapacity.push_back(1.);
        double water=y==2?.4:1.;f.water.push_back(water);f.gas.push_back(1.-water);f.gasMass.push_back(1.2*(1.-water));
    }
    for(auto entry:cells)for(int axis=0;axis<3;++axis){auto target=entry.first;++target[axis];auto other=cells.find(target);if(other==cells.end())continue;
        const auto& r=f.chart.regions[entry.second];auto lo=r.lo,hi=r.hi;lo[axis]=hi[axis];f.geometry.faces.push_back({entry.second,other->second,axis,lo,hi,1.,1.,1.,0.,1.});f.flux.push_back(0.);
    }
    std::vector<CutRegionWaterPlane> planes;WaterGasGeometry gas;WaterMimeticGeometry water;WaterGasProjectionResult p;std::string e;WaterGasSettings settings;
    bool prepared=ReconstructCutRegionPhasePlanes(f.chart,f.geometry,1,f.water,f.gas,{},planes,e)&&PrepareWaterGasGeometry(f,planes,settings,1.8e-5,gas,e)&&PrepareWaterMimeticGeometry(f,gas,1000.,settings.dt,{0.,-9.81,0.},water,e)&&ProjectWaterGasPressure(water,gas,settings,p,e);
    Check(prepared,"body-excluding hydrostatic pool prepares: "+e);if(!prepared)return 1;
    GridVec force{};int bodyFaces=0;double speed=0.;
    for(std::size_t index=0;index<water.regions.size();++index){auto& r=water.regions[index];std::vector<CutCompensated> pressure;
        for(auto patch:r.response.free){auto owner=r.owners[patch];pressure.push_back(owner.gas!=SIZE_MAX?p.gasPressure[owner.gas]:p.head[owner.shared]-r.potential[patch]+p.headDatum);}
        WaterCellAudit audit;if(!AuditWaterCell(f.water[r.sourceCell],1000.,settings.dt,{0.,-9.81,0.},r.patches,r.response,p.liquidFlux[index],pressure,audit,e)){Check(false,e);continue;}
        for(double v:audit.meanVelocity)speed=std::max(speed,std::abs(v));
        auto box=f.chart.regions[r.sourceCell];
        for(std::size_t patch=0;patch<r.patches.size();++patch){auto b=r.patches[patch];if(!b.fixed)continue;auto point=r.centroid;
            for(int a=0;a<3;++a)point[a]+=box.lo[a]+b.offset[a];
            bool body=true;for(int a=0;a<3;++a)if(point[a]<1.||point[a]>2.)body=false;
            if(body){force[b.axis]+=b.sign*b.area*audit.wallPressure[patch];++bodyFaces;}
        }
    }
    Check(bodyFaces==6,"all six actual solid boundary faces own their pressure reactions");
    Check(std::abs(force[0])<1e-9&&std::abs(force[1]-9810.)<1e-9&&std::abs(force[2])<1e-9,"integrated buoyancy equals displaced water weight, independently of the air model");
    Check(speed<1e-12,"body exclusion does not disturb a hydrostatic pool");
    WaterVolumeStep step;Check(AdvanceWaterVolume(f,settings,1000.,{0.,-9.81,0.},{},step,e),"hydrostatic body pool completes quantity/momentum advance: "+e);
    Check(step.maximumSpeed<1e-12&&std::abs(step.waterLedger)<1e-20&&std::abs(step.gasLedger)<1e-20,"body-pool advance conserves both materials and remains at rest");
    std::cout<<"BODY_BUOYANCY force="<<force[0]<<","<<force[1]<<","<<force[2]<<" speed="<<speed<<'\n';
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
