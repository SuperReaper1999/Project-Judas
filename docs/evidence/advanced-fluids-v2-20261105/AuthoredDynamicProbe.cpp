#include "BasinFixture.h"
#include "CutDynamicTransport.h"
#include "CutRectangularOwners.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>

using namespace judas::volume_grid;
using namespace judas::volume_grid::basin_probe;

int main(int argc, char** argv) {
    const int samples = argc == 2 ? std::stoi(argv[1]) : 1;
    if (samples < 1 || samples > 20)
        return 2;
    std::cout << std::setprecision(17);
    int failures = 0;
    for (const bool rotated : {false, true}) {
        const auto fixture = Fixture::Authored(rotated);
        CutRegionChart source;
        CutRegionGeometry geometry;
        std::string error;
        if (!PrepareCutRegionChart(fixture.layout, fixture.Walls(), fixture.InitialBox(),
                                   {-.005, -.015, -.005}, {.005, .025, .005}, source, error) ||
            !BuildCutRegionGeometry(source, {}, geometry, error)) {
            std::cerr << error << '\n';
            return 2;
        }
        CutDynamicState seed;
        SeedImport import;
        if (!WaterByRegion(fixture, source, geometry, rotated, seed.water, import, error)) {
            std::cerr << error << '\n';
            return 2;
        }
        for (std::size_t i = 0; i < seed.water.size(); ++i)
            seed.gasMass.push_back(1.2 * (geometry.startCapacity[i] - seed.water[i]));
        for (bool merge : {false, true}) {
            auto chart = source;
            auto active = geometry;
            auto state = seed;
            if (merge) {
                CutRectangularOwners owners;
                if (!BuildCutRectangularOwners(source, geometry, .125, owners, error)) {
                    std::cerr << error << '\n';
                    return 2;
                }
                state.water.assign(owners.chart.regions.size(), 0.);
                state.gasMass.assign(owners.chart.regions.size(), 0.);
                for (std::size_t i = 0; i < seed.water.size(); ++i) {
                    state.water[owners.owner[i]] += seed.water[i];
                    state.gasMass[owners.owner[i]] += seed.gasMass[i];
                }
                chart = std::move(owners.chart);
                active = std::move(owners.geometry);
            }
            double smallest = std::numeric_limits<double>::infinity();
            double width = smallest;
            unsigned tiny = 0, hiddenGas = 0;
            for (std::size_t i = 0; i < chart.regions.size(); ++i) {
                const auto capacity = active.startCapacity[i];
                if (capacity > 0.) {
                    smallest = std::min(smallest, capacity);
                    tiny += capacity < fixture.layout.h * fixture.layout.h * fixture.layout.h * 1e-6;
                    for (int axis = 0; axis < 3; ++axis)
                        width = std::min(width, chart.regions[i].hi[axis] - chart.regions[i].lo[axis]);
                }
                hiddenGas += state.gasMass[i] > 0. && state.water[i] == capacity;
            }
            const long double waterDelta = std::accumulate(state.water.begin(), state.water.end(), 0.L) -
                std::accumulate(seed.water.begin(), seed.water.end(), 0.L);
            const long double gasDelta = std::accumulate(state.gasMass.begin(), state.gasMass.end(), 0.L) -
                std::accumulate(seed.gasMass.begin(), seed.gasMass.end(), 0.L);
            std::cout << "GEOMETRY rotated=" << rotated << " merged=" << merge
                      << " regions=" << chart.regions.size() << " faces=" << active.faces.size()
                      << " min_capacity=" << smallest << " min_width=" << width
                      << " tiny_regions=" << tiny << " retained_sub_ulp_gas_owners=" << hiddenGas
                      << " water_delta=" << waterDelta << " gas_mass_delta=" << gasDelta << '\n';
            // Admission without gravity is a separate control, not a basin
            // physics success or a replacement for the unchanged gravity run.
            CutDynamicSettings zero;
            zero.gravity = {};
            CutDynamicStep quiet;
            const bool staticOK = AdvanceCutDynamic(chart, active, state, zero, quiet, error,
                {FullCutRegionGasVent(chart, 1, 1)});
            std::cout << "ZERO_GRAVITY rotated=" << rotated << " merged=" << merge
                      << " admitted=" << staticOK << " reason=" << error << '\n';
            for (double dt : {double(tick), double(tick) / 100., double(tick) / 10000.}) {
                std::vector<double> timings;
                int admitted = 0, unchanged = 0;
                for (int sample = 0; sample < samples; ++sample) {
                    CutDynamicSettings settings;
                    settings.dt = dt;
                    settings.gravity = {0., -double(gravity), 0.};
                    CutDynamicStep result;
                    result.state.water = {-7.};
                    result.state.gasMass = {-9.};
                    const auto begin = std::chrono::steady_clock::now();
                    const bool ok = AdvanceCutDynamic(chart, active, state, settings, result, error,
                        {FullCutRegionGasVent(chart, 1, 1)});
                    const double elapsed = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - begin).count();
                    timings.push_back(elapsed);
                    admitted += ok;
                    const bool intact = result.state.water == std::vector<double>{-7.} &&
                        result.state.gasMass == std::vector<double>{-9.};
                    unchanged += !ok && intact;
                    failures += !ok;
                    std::cout << "TRIAL rotated=" << rotated << " merged=" << merge
                              << " dt=" << dt << " sample=" << sample << " admitted=" << ok
                              << " rejection_transactional=" << intact << " ms=" << elapsed
                              << " reason=" << error << std::endl;
                }
                std::sort(timings.begin(), timings.end());
                std::cout << "TIMING rotated=" << rotated << " merged=" << merge << " dt=" << dt
                          << " samples=" << samples << " admitted=" << admitted
                          << " unchanged_rejections=" << unchanged
                          << " mean_ms=" << std::accumulate(timings.begin(), timings.end(), 0.) / samples
                          << " median_ms=" << timings[timings.size() / 2]
                          << " p95_ms=" << timings[std::size_t(std::ceil(.95 * samples)) - 1]
                          << " worst_ms=" << timings.back() << std::endl;
            }
        }
    }
    std::cout << "QUALIFICATION failed_trials=" << failures << '\n';
    return failures ? 1 : 0;
}
