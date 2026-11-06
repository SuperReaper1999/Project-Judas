#include "WaterVolumeAdvance.h"
#include "CutDirectedTransport.h"
#include "WaterGasRemap.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <sstream>
namespace judas::volume_grid {
namespace {
using Real=CutCompensated;
bool Fail(std::string& e,const char* why){e=why;return false;}
struct WaterTransfer {std::size_t from,to;Real volume;};
}
bool AdvanceWaterVolume(const CutSpatialField& authored,const WaterGasSettings& settings,double density,
    const GridVec& gravity,const std::vector<GridVec>& sourceVelocity,WaterVolumeStep& output,std::string& error,const WaterVolumeStep* previous){
    auto started=std::chrono::steady_clock::now();const auto n=authored.water.size();
    CutSpatialField field=authored;std::uint64_t stamp=1469598103934665603ull;
    auto hash=[&](double v){std::uint64_t bits=0;std::memcpy(&bits,&v,8);stamp=(stamp^bits)*1099511628211ull;};
    for(const auto& r:field.chart.regions){for(double v:r.lo)hash(v);for(double v:r.hi)hash(v);}
    hash(field.chart.layout.h);for(int v:field.chart.layout.cells)hash(double(v));
    for(double v:field.geometry.startCapacity)hash(v);
    for(double v:field.geometry.endCapacity)hash(v);
    for(const auto& f:field.geometry.faces){hash(double(f.left));hash(double(f.right));hash(double(f.axis));hash(f.meanArea);hash(f.length);for(double v:f.lo)hash(v);for(double v:f.hi)hash(v);}
    for(const auto& p:field.boundaries){hash(double(p.fragment));hash(double(p.axis));hash(double(p.sign));hash(p.volume);hash(p.incomingWater);hash(p.gasOnly?1.:0.);}
    auto velocity=sourceVelocity;
    if(previous){
        if(previous->chartStamp!=stamp||previous->water.size()!=n||previous->gas.size()!=n||previous->gasMass.size()!=n||previous->velocity.size()!=n)
            return Fail(error,"previous water state does not belong to this frozen chart");
        for(std::size_t i=0;i<n;++i){field.water[i]=previous->water[i].Value();field.gas[i]=previous->gas[i].Value();field.gasMass[i]=previous->gasMass[i].Value();}
        if(velocity.empty())velocity=previous->velocity;
    }
    std::vector<CutRegionWaterPlane> planes;WaterGasGeometry gas;WaterMimeticGeometry water;WaterVolumeStep next;
    if(!ReconstructCutRegionPhasePlanes(field.chart,field.geometry,1,field.water,field.gas,{},planes,error)||
       !PrepareWaterGasGeometry(field,planes,settings,1.8e-5,gas,error))return false;
    if(previous)for(std::size_t node=0;node<gas.nodeCell.size();++node){auto cell=gas.nodeCell[node];gas.mass[node]=previous->gasMass[cell];gas.volume[node]=previous->gas[cell];}
    if(!PrepareWaterMimeticGeometry(field,gas,density,settings.dt,gravity,water,error,velocity)||
       !ProjectWaterGasPressure(water,gas,settings,next.pressure,error))return false;
    next.chartStamp=stamp;
    next.pressureMilliseconds=next.pressure.elapsedMilliseconds;
    if(previous)next.water=previous->water;else next.water.assign(field.water.begin(),field.water.end());next.gas.resize(n);next.gasMass.resize(n);next.momentum.resize(n);next.velocity.resize(n);
    std::vector<std::size_t> first(water.sharedFaces,SIZE_MAX);std::vector<WaterGasDeparture> gasDepartures;
    std::vector<Real> firstFlux(water.sharedFaces);std::vector<WaterTransfer> transfers;
    auto transfer=[&](std::size_t from,std::size_t to,Real amount){if(amount.Value()<0.){std::swap(from,to);amount=Real()-amount;}if(amount.Value()>0.)transfers.push_back({from,to,amount});};
    for(std::size_t index=0;index<water.regions.size();++index){
        const auto& region=water.regions[index];const auto cell=region.sourceCell;const auto& flux=next.pressure.liquidFlux[index];
        Real offset=next.pressure.liquidReferencePressure[index];
        if(!region.response.free.empty()){auto ref=region.response.free[region.response.reference];for(int a=0;a<3;++a)offset=offset-Real(density*gravity[a])*region.patches[ref].Offset(a);}
        WaterCellAudit audit;
        if(!AuditWaterCell(field.water[cell],density,settings.dt,gravity,region.patches,region.response,flux,next.pressure.liquidHeadDifference[index],audit,error,true,offset)){error+=" in source cell "+std::to_string(cell);return false;}
        next.maximumMomentumResidual=std::max(next.maximumMomentumResidual,audit.maximumMomentumResidual);
        next.velocity[cell]=audit.meanVelocity;
        for(std::size_t a=0;a<region.response.free.size();++a){auto o=region.owners[region.response.free[a]];Real amount=Real(settings.dt)*flux[a];
            if(o.shared!=SIZE_MAX){
                if(first[o.shared]==SIZE_MAX){first[o.shared]=cell;firstFlux[o.shared]=amount;}
                else transfer(first[o.shared],cell,(firstFlux[o.shared]-amount)*Real(.5));
            }else{auto other=gas.nodeCell[o.gas];if(other==cell)continue;
                if(amount.Value()>0.)transfer(cell,other,amount);
                else if(amount.Value()<0.)gasDepartures.push_back({o.gas,cell,Real()-amount});
            }
        }
    }
    for(auto t:transfers){next.water[t.from]=next.water[t.from]-t.volume;next.water[t.to]=next.water[t.to]+t.volume;}
    for(std::size_t cell=0;cell<n;++cell){double tolerance=std::min(settings.massTolerance/settings.referenceDensity,settings.relativeTolerance*field.geometry.startCapacity[cell]);
        if(next.water[cell].Value()<0.||(next.water[cell]-Real(field.geometry.startCapacity[cell])).Value()>tolerance)return Fail(error,"water departure crosses its represented phase front; a geometry event is required");}
    WaterGasRemapResult remapped;
    if(!RemapWaterGas(field,gas,next.pressure,next.water,gasDepartures,settings,remapped,error))return false;
    next.gas=std::move(remapped.volume);next.gasMass=std::move(remapped.mass);
    Real waterLedger,gasLedger=next.pressure.externalGasMass;
    for(std::size_t cell=0;cell<n;++cell){
        Real partition=Real(field.geometry.startCapacity[cell])-next.water[cell]-next.gas[cell];
        double tolerance=std::min(settings.massTolerance/settings.referenceDensity,settings.relativeTolerance*field.geometry.startCapacity[cell]);
        next.maximumPartitionResidual=std::max(next.maximumPartitionResidual,std::abs(partition.Value())/tolerance);
        if(next.gas[cell].Value()<0.||next.gasMass[cell].Value()<0.)return Fail(error,"gas remap exhausted a conserved quantity");
        waterLedger=waterLedger+next.water[cell]-(previous?previous->water[cell]:Real(field.water[cell]));gasLedger=gasLedger+next.gasMass[cell]-(previous?previous->gasMass[cell]:Real(field.gasMass[cell]));
    }
    if(next.maximumPartitionResidual>1.||std::abs(waterLedger.Value())>settings.massTolerance/settings.referenceDensity||std::abs(gasLedger.Value())>settings.massTolerance)return Fail(error,"water/air original partition or quantity ledger failed");
    // Conservative implicit upwind momentum transport uses precisely the same
    // water transfers; no gas inertia or duplicated gravity is introduced.
    std::vector<CutDirectedEdge> edges;std::vector<double> mass;std::vector<std::size_t> index(n,SIZE_MAX),active;
    for(std::size_t cell=0;cell<n;++cell)if(next.water[cell].Value()>0.){index[cell]=active.size();active.push_back(cell);mass.push_back((Real(density)*next.water[cell]).Value());}
    for(auto t:transfers){if(index[t.from]==SIZE_MAX||index[t.to]==SIZE_MAX)return Fail(error,"depleted water cell requires a phase-front event before momentum transport");edges.push_back({index[t.from],index[t.to],Real(density)*t.volume});}
    if(!active.empty()){
        CutDirectedTransport transport;if(!transport.Build(mass,edges,std::vector<Real>(mass.size()),error))return false;
        for(int axis=0;axis<3;++axis){std::vector<Real> rhs(active.size()),velocity;Real before;
            for(std::size_t a=0;a<active.size();++a){auto cell=active[a];rhs[a]=Real(density)*(previous?previous->water[cell]:Real(field.water[cell]))*Real(next.velocity[cell][axis]);before=before+rhs[a];}
            if(!transport.Solve(rhs,1e-12,velocity,error))return false;
            Real after;
            for(std::size_t a=0;a<active.size();++a){auto cell=active[a];next.velocity[cell][axis]=velocity[a].Value();next.momentum[cell][axis]=Real(density)*next.water[cell]*velocity[a];after=after+next.momentum[cell][axis];}
            if(std::abs((after-before).Value())>1e-10*std::max(std::abs(before.Value()),1.))return Fail(error,"water momentum transport ledger failed");
        }
    }
    for(auto v:next.velocity){double speed=0.;for(double x:v)speed+=x*x;next.maximumSpeed=std::max(next.maximumSpeed,std::sqrt(speed));}
    next.waterLedger=waterLedger.Value();next.gasLedger=gasLedger.Value();next.totalMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();output=std::move(next);error.clear();return true;
}
}
