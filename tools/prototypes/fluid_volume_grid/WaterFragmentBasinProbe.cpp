#include "BasinFixture.h"
#include "WaterFragmentAdvance.h"
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




int main(int argc,char** argv){bool failed=false;std::cout<<std::setprecision(17);for(bool rotated:{false,true}){if(argc>1&&((std::string(argv[1])=="level"&&rotated)||(std::string(argv[1])=="rotated"&&!rotated)))continue;CutSpatialField original,orthogonal,field;CutOrthogonalRemap remap;CutTransparentGridReport compact;std::string error;
 if(!BuildFixture(rotated,original,error)||!BuildCutOrthogonalField(original,orthogonal,remap,error)||!CoarsenCutTransparentGrid(orthogonal,field,compact,error)){std::cerr<<error;return 2;}
 WaterMaterialState state,after;WaterGasSettings settings;settings.dt=double(tick);settings.trace=true;
 if(!ImportWaterMaterial(field,{},state,error)){std::cerr<<error;return 2;}
 WaterFragmentAdvanceStats stats;auto start=std::chrono::steady_clock::now();bool ok=AdvanceWaterFragments(field,state,1000.,{0.,-double(gravity),0.},settings,after,stats,error,128);
 if(!ok)failed=true;
 std::cout<<"WHOLE_INTERVAL rotated="<<rotated<<" ok="<<ok<<" time="<<stats.advancedTime.Value()<<" intervals="<<stats.intervals<<" rejected="<<stats.rejected<<" constraints="<<stats.geometricConstraints<<" pieces="<<after.fragments.size()<<" geometryMs="<<stats.geometryMs<<" pressureMs="<<stats.pressureMs<<" transportMs="<<stats.transportMs<<" elapsedMs="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<" error="<<error<<std::endl;
}return failed?1:0;}
