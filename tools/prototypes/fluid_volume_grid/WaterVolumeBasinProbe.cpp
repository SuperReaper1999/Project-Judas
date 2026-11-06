#include "BasinFixture.h"
#include "WaterVolumeAdvance.h"
#include "CutOrthogonalField.h"
#include "CutTransparentGrid.h"
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
using namespace judas::volume_grid;
using namespace judas::volume_grid::basin_probe;
using Real=CutCompensated;
bool BuildFixture(bool rotated, CutSpatialField& field, std::string& error) {
    const auto fixture = Fixture::Authored(rotated);
    CutRegionChart chart;
    CutRegionGeometry geometry;
    if (!PrepareCutRegionChart(fixture.layout, fixture.Walls(), fixture.InitialBox(),
        {-.005,-.015,-.005}, {.005,.025,.005}, chart, error) ||
        !BuildCutRegionGeometry(chart, {}, geometry, error)) return false;
    CutDynamicState source;
    SeedImport imported;
    if (!WaterByRegion(fixture, chart, geometry, rotated, source.water, imported, error)) return false;
    std::vector<double> gas;
    for (std::size_t i = 0; i < source.water.size(); ++i) {
        gas.push_back(geometry.startCapacity[i] - source.water[i]);
        source.gasMass.push_back(1.2 * gas.back());
    }
    CutConnectedOwners owners;
    if (!BuildCutConnectedOwners(chart, geometry, .125, owners, error)) return false;
    std::vector<CutRegionWaterPlane> planes;
    if (!ReconstructCutRegionPhasePlanes(chart, geometry, 1, source.water, gas, {}, planes, error)) return false;
    auto vent = FullCutRegionGasVent(chart, 1, 1);
    std::vector<CutPhaseBoundary> ports;
    for (std::size_t i = 0; i < chart.regions.size(); ++i)
        if (MeasureCutRegionGasVent(chart.regions[i], geometry.motion, vent, planes[i]).mean > 0.)
            ports.push_back({i,1,1,0.,0.,true});
    return BuildCutSpatialField(chart, geometry, owners, source,
        std::vector<double>(geometry.faces.size()), ports, field, error, false);
}



int main(int argc,char** argv){
    std::cout<<std::setprecision(17);std::string mode=argc>1?argv[1]:"advance";int repeats=argc>2?std::stoi(argv[2]):1;int failures=0;
    if(mode!="advance"&&mode!="pressure"){std::cerr<<"mode must be advance or pressure\n";return 2;}
    for(bool rotated:{false,true}){
        CutSpatialField original,orthogonal,field;CutOrthogonalRemap remap;CutTransparentGridReport compact;std::string error;
        if(!BuildFixture(rotated,original,error)||!BuildCutOrthogonalField(original,orthogonal,remap,error)||!CoarsenCutTransparentGrid(orthogonal,field,compact,error)){std::cerr<<error;return 2;}
        WaterGasSettings settings;settings.dt=double(tick);
        for(int repeat=0;repeat<repeats;++repeat){auto started=std::chrono::steady_clock::now();bool ok=false;WaterGasProjectionResult projected;WaterVolumeStep step;
            if(mode=="advance")ok=AdvanceWaterVolume(field,settings,1000.,{0.,-double(gravity),0.},{},step,error);
            else{std::vector<CutRegionWaterPlane> planes;WaterGasGeometry gas;WaterMimeticGeometry water;
                ok=ReconstructCutRegionPhasePlanes(field.chart,field.geometry,1,field.water,field.gas,{},planes,error)&&PrepareWaterGasGeometry(field,planes,settings,1.8e-5,gas,error)&&PrepareWaterMimeticGeometry(field,gas,1000.,settings.dt,{0.,-double(gravity),0.},water,error)&&ProjectWaterGasPressure(water,gas,settings,projected,error);
            }
            std::uint64_t digest=1469598103934665603ull;
            auto hash=[&](Real v){for(double part:{v.hi,v.lo}){std::uint64_t bits;std::memcpy(&bits,&part,8);digest=(digest^bits)*1099511628211ull;}};
            if(mode=="pressure"&&ok){for(auto v:projected.head)hash(v);for(auto v:projected.gasAbsolutePressure)hash(v);for(auto v:projected.gasMass)hash(v);for(auto v:projected.gasVolume)hash(v);for(const auto& f:projected.liquidFlux)for(auto v:f)hash(v);}
            std::cout<<"WATER_BASIN mode="<<mode<<" rotated="<<rotated<<" repeat="<<repeat<<" accepted="<<ok<<" elapsedMs="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()<<" pressureMs="<<(mode=="pressure"?projected.elapsedMilliseconds:step.pressureMilliseconds)<<" linear="<<projected.linearApplications<<" fallbacks="<<projected.linearFallbacks<<" stateHash="<<digest<<" strictMinority="<<projected.strictMinorityResidual<<" partition="<<step.maximumPartitionResidual<<" momentum="<<step.maximumMomentumResidual<<" water="<<step.waterLedger<<" gas="<<step.gasLedger<<" speed="<<step.maximumSpeed<<" reason="<<error<<std::endl;
            if(!ok)++failures;
        }
    }
    return failures?1:0;
}
