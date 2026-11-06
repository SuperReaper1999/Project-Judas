#include "WaterGasRemap.h"
#include <algorithm>
#include <cmath>
#include <sstream>
namespace judas::volume_grid {
namespace {
using Real=CutCompensated;
bool Fail(std::string& e,const char* why){e=why;return false;}
double Energy(Real mass,Real volume,const WaterGasSettings& s){return s.referencePressure/s.referenceDensity*mass.Value()*(std::log(mass.Value()/(s.referenceDensity*volume.Value()))-1.)+s.referencePressure*volume.Value();}
}
bool RemapWaterGas(const CutSpatialField& field,const WaterGasGeometry& gas,
    const WaterGasProjectionResult& pressure,const std::vector<Real>& water,
    const std::vector<WaterGasDeparture>& departures,const WaterGasSettings& settings,
    WaterGasRemapResult& output,std::string& error){
    const auto n=water.size(),ng=gas.nodeCell.size();
    if(field.water.size()!=n||field.gas.size()!=n||gas.cellNode.size()!=n||pressure.gasVolume.size()!=ng||pressure.gasMass.size()!=ng||field.geometry.startCapacity.size()!=n)return Fail(error,"gas remap dimensions disagree");
    std::vector<std::vector<std::pair<std::size_t,Real>>> pieces(ng);std::vector<Real> incoming(n),weights(ng);
    std::vector<std::size_t> newOwner(n,SIZE_MAX);
    for(auto d:departures){
        if(d.node>=ng||d.cell>=n||!(d.volume.Value()>0.&&std::isfinite(d.volume.Value())))return Fail(error,"invalid receding gas parcel");
        if(gas.cellNode[d.cell]==SIZE_MAX){if(newOwner[d.cell]!=SIZE_MAX&&newOwner[d.cell]!=d.node){std::ostringstream detail;detail.precision(17);detail<<"disconnected receding gas fronts require separate geometric fragments cell="<<d.cell<<" oldGas="<<field.gas[d.cell]<<" firstOwner="<<newOwner[d.cell]<<" otherOwner="<<d.node<<" newVolume="<<d.volume.Value();error=detail.str();return false;}newOwner[d.cell]=d.node;}
        incoming[d.cell]=incoming[d.cell]+d.volume;
        pieces[d.node].push_back({d.cell,d.volume});weights[d.node]=weights[d.node]+d.volume;
    }
    WaterGasRemapResult next;next.volume.resize(n);next.mass.resize(n);double initialEnergy=0.,parcelEnergy=0.;
    for(std::size_t node=0;node<ng;++node){auto cell=gas.nodeCell[node];
        if(cell>=n||!(pressure.gasVolume[node].Value()>0.&&pressure.gasMass[node].Value()>0.))return Fail(error,"gas remap has no positive independently owned quantity");
        Real native=Real(field.geometry.startCapacity[cell])-water[cell]-incoming[cell];
        // This is a geometric weight, not a mass/volume clipping operation.
        // Below reconstruction resolution retain the known native dry parcel.
        // Exact positive node volume and mass are distributed afterward; any
        // unacceptable spatial partition rejects the complete step below.
        if(native.Value()<=0.)native=Real(field.gas[cell]);
        if(!(native.Value()>0.))return Fail(error,"native gas owner lost its geometric support");
        pieces[node].push_back({cell,native});weights[node]=weights[node]+native;
        initialEnergy+=Energy(pressure.gasMass[node],pressure.gasVolume[node],settings);
        // Form the small parcels first. Put the largest last so ledger
        // subtraction cannot erase a real sub-ULP native air film.
        std::stable_sort(pieces[node].begin(),pieces[node].end(),[](const auto& a,const auto& b){return (a.second-b.second).Value()<0.;});
        Real remainingV=pressure.gasVolume[node],remainingM=pressure.gasMass[node];
        for(std::size_t index=0;index<pieces[node].size();++index){auto p=pieces[node][index];Real fraction=p.second/weights[node];
            Real v=index+1==pieces[node].size()?remainingV:pressure.gasVolume[node]*fraction;
            Real m=index+1==pieces[node].size()?remainingM:pressure.gasMass[node]*fraction;
            if(!(v.Value()>0.&&m.Value()>0.))return Fail(error,"gas remap exhausted a positive parcel");
            next.volume[p.first]=next.volume[p.first]+v;next.mass[p.first]=next.mass[p.first]+m;
            parcelEnergy+=Energy(m,v,settings);remainingV=remainingV-v;remainingM=remainingM-m;
        }
    }
    // Gas mass is conservative; gas volume is the void left by conservative
    // water. Reconcile the pressure proposal's admitted geometric error now,
    // rather than carrying and accumulating that error into the next frame.
    // Never erase a positive minority parcel when C-W is below resolution.
    for(std::size_t cell=0;cell<n;++cell)if(next.volume[cell].Value()>0.){
        Real voidVolume=Real(field.geometry.startCapacity[cell])-water[cell];
        Real correction=voidVolume-next.volume[cell];
        double tolerance=std::min(settings.massTolerance/settings.referenceDensity,settings.relativeTolerance*field.geometry.startCapacity[cell]);
        if(std::abs(correction.Value())>tolerance)return Fail(error,"gas remap geometric reconstruction exceeds the original cell budget");
        if(voidVolume.Value()>0.){
            double gauge=settings.referencePressure*(next.mass[cell]/(Real(settings.referenceDensity)*next.volume[cell])-Real(1.)).Value();
            double afterGauge=settings.referencePressure*(next.mass[cell]/(Real(settings.referenceDensity)*voidVolume)-Real(1.)).Value();
            next.reconstructionWorkBound+=std::max(std::abs(gauge),std::abs(afterGauge))*std::abs(correction.Value());
            next.maximumGeometricCorrection=std::max(next.maximumGeometricCorrection,std::abs(correction.Value()));
            next.volume[cell]=voidVolume;
        }
    }
    Real globalPartition,totalMass,totalVolume;
    double finalEnergy=0.;
    for(std::size_t cell=0;cell<n;++cell){
        Real r=Real(field.geometry.startCapacity[cell])-water[cell]-next.volume[cell];
        double tolerance=std::min(settings.massTolerance/settings.referenceDensity,settings.relativeTolerance*field.geometry.startCapacity[cell]);
        next.maximumPartitionResidual=std::max(next.maximumPartitionResidual,std::abs(r.Value())/tolerance);globalPartition=globalPartition+r;
        totalMass=totalMass+next.mass[cell];totalVolume=totalVolume+next.volume[cell];
        if(next.volume[cell].Value()>0.)finalEnergy+=Energy(next.mass[cell],next.volume[cell],settings);
    }
    for(std::size_t node=0;node<ng;++node){totalMass=totalMass-pressure.gasMass[node];totalVolume=totalVolume-pressure.gasVolume[node];}
    next.freeEnergyChange=finalEnergy-initialEnergy;
    const double energyRoundoff=1e-10*std::max({std::abs(initialEnergy),std::abs(parcelEnergy),1.});
    if(next.maximumPartitionResidual>1.||std::abs(globalPartition.Value())>settings.massTolerance/settings.referenceDensity||std::abs(totalMass.Value())>settings.massTolerance||std::abs(totalVolume.Value())>settings.massTolerance/settings.referenceDensity)return Fail(error,"gas remap original spatial/quantity contract failed");
    if(std::abs(parcelEnergy-initialEnergy)>energyRoundoff||next.freeEnergyChange>energyRoundoff+next.reconstructionWorkBound)return Fail(error,"gas remap created isothermal free energy");
    output=std::move(next);error.clear();return true;
}
}
