#include "BasinFixture.h"
#include "CutHydroacousticStep.h"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>

using namespace judas::volume_grid;
using namespace judas::volume_grid::basin_probe;

int main() {
    std::cout << std::setprecision(17);
    bool current = true;
    for (const bool rotated : {false, true}) {
        const auto fixture = Fixture::Authored(rotated);
        const auto body = fixture.InitialBox();
        CutRegionChart chart;
        CutRegionGeometry geometry;
        std::string error;
        if (!PrepareCutRegionChart(fixture.layout, fixture.Walls(), body,
                                   {-.005, -.015, -.005}, {.005, .025, .005}, chart, error) ||
            !BuildCutRegionGeometry(chart, {}, geometry, error)) {
            std::cerr << error << '\n';
            return 1;
        }
        CutHydroacousticState state;
        SeedImport seed;
        if (!WaterByRegion(fixture, chart, geometry, rotated, state.waterVolume, seed, error)) {
            std::cerr << error << '\n';
            return 1;
        }
        const CutHydroacousticSettings settings{double(tick)};
        for (std::size_t i = 0; i < chart.regions.size(); ++i)
            state.gasMass.push_back(settings.gasDensity * (geometry.startCapacity[i] - state.waterVolume[i]));
        std::vector<CutRegionWaterPlane> planes;
        if (!ReconstructCutRegionWaterPlanes(chart, geometry, 1, state.waterVolume, {}, planes, error)) {
            std::cerr << error << '\n';
            return 1;
        }
        unsigned shared = 0, inverted = 0, partial = 0;
        for (const auto& face : geometry.faces) {
            if (face.axis != 1 || face.startArea == 0.)
                continue;
            const auto& a = planes[face.left];
            const auto& b = planes[face.right];
            if (MeasureCutRegionSharedPhase(face, geometry.motion, a, b, true, true).start != 0. ||
                MeasureCutRegionSharedPhase(face, geometry.motion, a, b, false, false).start != 0.)
                continue;
            ++shared;
            if (MeasureCutRegionSharedPhase(face, geometry.motion, a, b, false, true).start > 0.)
                ++inverted;
            const auto area = [](const CutRegion& r) { return (r.hi[0]-r.lo[0]) * (r.hi[2]-r.lo[2]); };
            if (face.startArea < area(chart.regions[face.left]) ||
                face.startArea < area(chart.regions[face.right]))
                ++partial;
        }
        CutHydroacousticStep candidate;
        candidate.state.waterVolume = {-7.};
        const auto begin = std::chrono::steady_clock::now();
        const bool admitted = AdvanceCutHydroacoustic(chart, geometry, state,
            {FullCutRegionGasVent(chart, 1, 1)}, 1, 1.5, settings, candidate, error);
        const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-begin).count();
        const bool unchanged = candidate.state.waterVolume == std::vector<double>{-7.};
        std::cout << "AUTHORED rotated=" << rotated << " regions=" << chart.regions.size()
                  << " water_m3=" << std::accumulate(state.waterVolume.begin(), state.waterVolume.end(), 0.)
                  << " opposed_cross_face_traces=" << shared << " wet_above_dry=" << inverted
                  << " smaller_than_cell_section=" << partial
                  << " admitted=" << admitted << " output_unchanged=" << unchanged
                  << " ms=" << elapsed << " reason=" << error << '\n';
        current = current && !admitted && unchanged && shared > 0;
    }
    return current ? 0 : 1;
}
