#include "BasinFixture.h"
#include "CutOrthogonalField.h"
#include "CutTransparentGrid.h"
#include "WaterBulkProjection.h"
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
using namespace judas::volume_grid;
using namespace judas::volume_grid::basin_probe;
using Real = CutCompensated;
bool BuildFixture(bool rotated, CutSpatialField &field, std::string &error) {
    const auto fixture = Fixture::Authored(rotated);
    CutRegionChart chart;
    CutRegionGeometry geometry;
    if (!PrepareCutRegionChart(fixture.layout, fixture.Walls(), fixture.InitialBox(),
                               {-.005, -.015, -.005}, {.005, .025, .005}, chart, error) ||
        !BuildCutRegionGeometry(chart, {}, geometry, error))
        return false;
    CutDynamicState source;
    SeedImport imported;
    if (!WaterByRegion(fixture, chart, geometry, rotated, source.water, imported, error))
        return false;
    std::vector<double> gas;
    for (std::size_t i = 0; i < source.water.size(); ++i) {
        gas.push_back(geometry.startCapacity[i] - source.water[i]);
        source.gasMass.push_back(1.2 * gas.back());
    }
    CutConnectedOwners owners;
    if (!BuildCutConnectedOwners(chart, geometry, .125, owners, error))
        return false;
    std::vector<CutRegionWaterPlane> planes;
    if (!ReconstructCutRegionPhasePlanes(chart, geometry, 1, source.water, gas, {}, planes, error))
        return false;
    auto vent = FullCutRegionGasVent(chart, 1, 1);
    std::vector<CutPhaseBoundary> ports;
    for (std::size_t i = 0; i < chart.regions.size(); ++i)
        if (MeasureCutRegionGasVent(chart.regions[i], geometry.motion, vent, planes[i]).mean > 0.)
            ports.push_back({i, 1, 1, 0., 0., true});
    return BuildCutSpatialField(chart, geometry, owners, source,
                                std::vector<double>(geometry.faces.size()), ports, field, error, false);
}

int main(int argc, char **argv) {
    std::cout << std::setprecision(17);
    int steps = 1;
    try {
        std::size_t consumed = 0;
        if (argc > 1) {
            steps = std::stoi(argv[1], &consumed);
            if (consumed != std::string(argv[1]).size())
                steps = 0;
        }
    } catch (const std::exception &) {
        steps = 0;
    }
    if (steps < 1 || steps > 240 || argc > 3 ||
        (argc > 2 && std::string(argv[2]) != "level" && std::string(argv[2]) != "rotated" &&
         std::string(argv[2]) != "both")) {
        std::cerr << "Usage: WaterBulkBasinProbe [1..240] [level|rotated|both]\n";
        return 2;
    }
    bool failed = false;
    for (bool rotated : {false, true}) {
        if (argc > 2 && ((std::string(argv[2]) == "level" && rotated) ||
                         (std::string(argv[2]) == "rotated" && !rotated)))
            continue;
        CutSpatialField original, orthogonal, field;
        CutOrthogonalRemap remap;
        CutTransparentGridReport compact;
        std::string error;
        if (!BuildFixture(rotated, original, error) ||
            !BuildCutOrthogonalField(original, orthogonal, remap, error) ||
            !CoarsenCutTransparentGrid(orthogonal, field, compact, error)) {
            std::cerr << error;
            return 2;
        }
        WaterBulkGeometry geometry;
        WaterBulkState state;
        WaterGasSettings settings;
        settings.trace = true;
        settings.dt = double(tick);
        if (!PrepareWaterBulkStorage(field, 1000., {0., -double(gravity), 0.}, geometry, state,
                                     error)) {
            std::cerr << error;
            return 2;
        }
        std::cout << "BULK_IMPORT rotated=" << rotated << " originalCells=" << original.water.size()
                  << " cells=" << field.water.size() << " minCapacity="
                  << *std::min_element(field.geometry.startCapacity.begin(),
                                       field.geometry.startCapacity.end())
                  << " faces=" << field.geometry.faces.size() << std::endl;
        for (int i = 0; i < steps; ++i) {
            WaterBulkState next;
            WaterBulkProjectionStats stats;
            bool ok = ProjectWaterBulk(geometry, state, settings, next, stats, error);
            std::cout << "BULK_BASIN rotated=" << rotated << " step=" << i << " ok=" << ok
                      << " ms=" << stats.elapsedMs << " Newton=" << stats.newtonIterations
                      << " KrylovApplications=" << stats.linearApplications
                      << " residual=" << stats.maximumVolumeResidual
                      << " energyChange=" << stats.endEnergy - stats.sourceEnergy
                      << " waterReconciliation=" << stats.maximumWaterReconciliation
                      << " massReconciliation=" << stats.maximumMassReconciliation << " error=" << error
                      << std::endl;
            if (!ok) {
                failed = true;
                break;
            }
            state = std::move(next);
        }
    }
    return failed ? 1 : 0;
}
