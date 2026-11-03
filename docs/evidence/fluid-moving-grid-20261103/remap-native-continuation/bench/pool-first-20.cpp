#include "CoupledVolumeBox.h"
#include "OwnedEvolvingVolume.h"
#include "GridWaterQuery.h"
#include "FluidSettlingContract.h"
#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string_view>

using namespace judas::volume_grid;

int main(int argc, char** argv) {
    const int hz = argc >= 2 ? std::stoi(argv[1]) : 60;
    const bool strictWarmup = argc == 3 && std::string_view(argv[2]) == "--strict-warmup";
    if ((hz != 60 && hz != 30) || argc > 3 || (argc == 3 && !strictWarmup))
        return 1;
    const double dt = 1. / hz, gravity = -9.81;
    const GridLayout grid{{8,12,8}, .25};
    const double half = double(.3f);
    const GridVec center{1., double(.7f), 1.};
    BoxMotion shape{1};
    for (int a = 0; a < 3; ++a) {
        shape.lo[a] = center[a] - half;
        shape.hi[a] = center[a] + half;
    }
    MovingVolumeState fluid;
    HalfDualVolumeState owned;
    for (int a = 0; a < 3; ++a) {
        fluid.velocity[a].resize(grid.FaceCount(a));
        owned.velocity[a].resize(2 * grid.CellCount());
    }
    fluid.water.resize(grid.CellCount());
    for (int j = 0; j < 10; ++j)
        for (int k = 0; k < 8; ++k)
            for (int i = 0; i < 8; ++i) {
                const GridVec point{(i+.5)*.25,(j+.5)*.25,(k+.5)*.25};
                bool omitted = true;
                for (int a = 0; a < 3; ++a)
                    omitted = omitted && std::abs(point[a] - center[a]) < half + .25 * double(.36f);
                if (!omitted)
                    fluid.water[grid.Cell(i,j,k)] = .25*.25*.25;
            }
    const double water = std::accumulate(fluid.water.begin(), fluid.water.end(), 0.);
    std::string error;
    for (int i = 0; i < 120; ++i) {
        OwnedEvolvingVolumeInterval next;
        if (!AdvanceGeometricEvolvingVolumeInterval(grid, {shape}, fluid, owned,
                                               i == 0 ? 1. / 60 : 1. / 30, 1, gravity, 7, next, error)) {
            std::cout << "warmup rejected at " << i << " " << error << '\n';
            return 2;
        }
        fluid = std::move(next.final.phase.fluid.state);
        owned = std::move(next.final.halfVelocity);
    }
    double warmupSpeed = 0.;
    for (int z = 0; z < 8; ++z) {
        for (int y = 0; y < 12; ++y) {
            for (int x = 0; x < 8; ++x) {
                const std::array<int, 3> i{x, y, z};
                GridVec u{};
                for (int a = 0; a < 3; ++a) {
                    auto hi = i;
                    ++hi[a];
                    u[a] = .5 * (fluid.velocity[a][grid.Face(a, x, y, z)] +
                        fluid.velocity[a][grid.Face(a, hi[0], hi[1], hi[2])]);
                }
                warmupSpeed += fluid.water[grid.Cell(x,y,z)] *
                    std::sqrt(std::inner_product(u.begin(),u.end(),u.begin(),0.));
            }
        }
    }
    const double solidVolume = 8 * half * half * half;
    CoupledBoxState body{shape.owner, shape.lo, shape.hi, {}, double(float(500 * solidVolume))};
    double releaseHeight = 0.;
    int releaseColumns = 0;
    for (int z = 0; z < 8; ++z) {
        for (int x = 0; x < 8; ++x) {
            if (x * grid.h <= shape.hi[0] + grid.h && (x + 1) * grid.h >= shape.lo[0] - grid.h &&
                z * grid.h <= shape.hi[2] + grid.h && (z + 1) * grid.h >= shape.lo[2] - grid.h)
                continue;
            for (int y = 0; y < 12; ++y)
                releaseHeight += fluid.water[grid.Cell(x, y, z)] / (grid.h * grid.h);
            ++releaseColumns;
        }
    }
    const double clearance = releaseHeight / releaseColumns - shape.hi[1];
    const auto contract = FluidTest::SettlingContract::Make(2, .5, dt);
    FluidTest::WindowMotion early, late;
    std::cout << std::setprecision(14) << "dt " << dt << " water " << water
              << " warmup mean water speed " << warmupSpeed / water << " release clearance " << clearance
              << " warmup diagnostic " << (warmupSpeed / water < .05 ? "PASS" : "FAIL") << '\n';
    std::cout << "step,time,y,vy,side_immersion,exterior_immersion,maximum_trace_immersion,substeps,"
                 "rejections,last_geometry_calls,pressure_work_error,volume_error,body_impulse_error,step_ms\n";
    std::vector<double> times;
    double maxWork = 0., maxVolume = 0., maxImpulse = 0., sideMean = 0., exteriorMean = 0.;
    double phaseFluxRoundoff = 0., legacySpeed = 0.;
    int originalSamples = 0, splitFrames = 0, largestSubdivision = 1;
    const auto skip = std::getenv("GRID_PROFILE_SKIP") ? std::stoull(std::getenv("GRID_PROFILE_SKIP")) : 0;
    const auto count = std::getenv("GRID_PROFILE_COUNT") ? std::stoull(std::getenv("GRID_PROFILE_COUNT")) : 20;
    for (std::size_t i = 0; i < skip + count; ++i) {
        GeometricCoupledBoxInterval interval;
        const auto started = std::chrono::steady_clock::now();
        if (!AdvanceGeometricCoupledBoxInterval(grid, body, fluid, owned, dt, 1, gravity, 7,
                                               interval, error)) {
            std::cout << "release rejected at " << i << " t " << i*dt << " y " << body.lo[1]+half
                      << " vy " << body.velocity[1] << " " << error << '\n';
            return 3;
        }
        const double elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        times.push_back(elapsed);
        if (interval.substeps > 1)
            ++splitFrames;
        largestSubdivision = std::max(largestSubdivision, interval.substeps);
        auto& next = interval.final;
        double impulseError = 0.;
        for (int a = 0; a < 3; ++a) {
            const double free = a == 1 ? dt * gravity : 0.;
            impulseError = std::max(impulseError, std::abs(
                body.mass * (next.box.body.velocity[a] - body.velocity[a] - free) - interval.bodyPressureImpulse[a]));
        }
        maxImpulse = std::max(maxImpulse, impulseError);
        phaseFluxRoundoff += interval.geometricPhaseFluxRoundoff;
        body = next.box.body;
        fluid = std::move(next.box.fluid.state);
        owned = std::move(next.halfVelocity);
        const BoxMotion stationary{body.owner,body.lo,body.hi,{}};
        GridCellPhase phase;
        if (!ReconstructGridCellPhase(grid, {stationary}, 1, fluid.water, phase, error)) {
            std::cout << "observation rejected at " << i << " " << error << '\n';
            return 4;
        }
        // Observe exterior columns separated from the body by at least one
        // whole chart cell. Report the mean measured free-surface height.
        double exterior = 0., massEquivalent = 0.;
        int columns = 0;
        for (int k = 0; k < 8; ++k)
            for (int x = 0; x < 8; ++x) {
                if (x*.25 <= body.hi[0]+.25 && (x+1)*.25 >= body.lo[0]-.25 &&
                    k*.25 <= body.hi[2]+.25 && (k+1)*.25 >= body.lo[2]-.25)
                    continue;
                double height = 0.;
                for (int y = 0; y < 12; ++y) {
                    const auto cell = grid.Cell(x,y,k);
                    massEquivalent += fluid.water[cell] / (.25 * .25);
                    if (fluid.water[cell] > 1e-12)
                        height = std::max(height, phase.waterPlane[cell]);
                }
                exterior += height;
                ++columns;
            }
        MovingGridGeometry geometry;
        if (!BuildMovingGridGeometry(grid, {stationary}, geometry, error)) {
            std::cout << "side wet observation rejected at " << i << ' ' << error << '\n';
            return 4;
        }
        double wetArea = 0., sideArea = 0.;
        for (const auto& patch : geometry.startPatches) {
            if (patch.axis == 1)
                continue;
            const int row = int((patch.cell / std::size_t(grid.cells[0])) % grid.cells[1]);
            const double lo = std::max(body.lo[1], row * grid.h);
            const double hi = std::min(body.hi[1], (row + 1) * grid.h);
            const double wet = std::clamp((phase.waterPlane[patch.cell] - lo) / (hi - lo), 0., 1.);
            wetArea += patch.area * wet;
            sideArea += patch.area;
        }
        const double sampled = wetArea / sideArea;
        const auto exteriorFraction = [&](double level) {
            return std::clamp((level / columns - body.lo[1]) / (2 * half), 0., 1.);
        };
        const double independent = exteriorFraction(massEquivalent);
        const double y = body.lo[1] + half, vy = body.velocity[1];
        if (contract.early.Contains(i))
            early.Add(y, vy, sampled);
        if (contract.late.Contains(i))
            late.Add(y, vy, sampled, true, independent);
        if (i >= std::size_t(3 * hz) && i < std::size_t(4 * hz)) {
            sideMean += sampled;
            exteriorMean += independent;
            ++originalSamples;
        }
        if (i + 1 == std::size_t(4 * hz))
            legacySpeed = std::abs(vy);
        const double volumeError = std::accumulate(fluid.water.begin(), fluid.water.end(), 0.) - water;
        if (!std::isfinite(y) || !std::isfinite(vy) || !std::isfinite(volumeError) ||
            std::abs(vy) >= 100. || std::abs(volumeError) > .001 * water) {
            std::cout << "trajectory violated the finite state or material gate at " << i << '\n';
            return 4;
        }
        maxVolume = std::max(maxVolume, std::abs(volumeError));
        maxWork = std::max(maxWork, interval.maxPressureEnergyResidual);
        std::cout << i << ',' << (i+1)*dt << ',' << y << ',' << vy << ',' << sampled << ','
                  << independent << ',' << exteriorFraction(exterior) << ',' << interval.substeps << ','
                  << interval.rejectedAttempts << ',' << next.box.geometryEvaluations << ','
                  << interval.maxPressureEnergyResidual << ',' << volumeError << ',' << impulseError
                  << ',' << elapsed << '\n';
    }
    std::cout << "PROFILE window completed with " << skip + count << " release frames\n";
    return 0;
    const auto verdict = FluidTest::Assess(contract, early, late, std::sqrt(-gravity * half));
    std::cout << "early rms " << early.Rms() << " range " << early.Range()
              << " late rms " << late.Rms() << " range " << late.Range()
              << " peak " << late.peakSpeed << " side-wetted mean " << late.MeanImmersion()
              << " exterior mass-equivalent mean " << late.MeanIndependent()
              << " peak/decay/range/exterior gates " << verdict.peak << verdict.rmsDecay << verdict.rangeDecay
              << verdict.independentImmersion << " side " << verdict.immersion
              << " coverage " << verdict.coverage << '\n';
    std::sort(times.begin(), times.end());
    std::cout << "ORIGINAL_WINDOW side " << sideMean / originalSamples
              << " exterior " << exteriorMean / originalSamples << " speed at 4 s " << legacySpeed << '\n';
    std::cout << "LEDGER max_volume " << maxVolume << " max_pressure_work " << maxWork
              << " max_body_impulse " << maxImpulse << " half_flux_roundoff " << phaseFluxRoundoff << '\n';
    std::cout << "TIMING median " << times[times.size() / 2]
              << " p95 " << times[(95 * times.size() + 99) / 100 - 1] << " worst " << times.back()
              << " split_frames " << splitFrames << " max_substeps " << largestSubdivision << '\n';
    const bool originalWindow = std::abs(sideMean / originalSamples - .5) <= .1 &&
        std::abs(exteriorMean / originalSamples - .5) <= .1 && legacySpeed < .5;
    const bool accepted = verdict.Passed() && originalWindow && clearance >= grid.h &&
        maxVolume <= 1e-9 && maxWork <= 1e-6 && maxImpulse <= 2e-6;
    std::cout << "PROXY mandatory " << (accepted ? "PASS" : "FAIL")
              << " strict_warmup " << strictWarmup << '\n';
    return accepted && (!strictWarmup || warmupSpeed / water < .05) ? 0 : 5;
}
