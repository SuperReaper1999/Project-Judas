#include "CoupledVolumeBox.h"
#include "OwnedEvolvingVolume.h"
#include "GridWaterQuery.h"
#include "FluidSettlingContract.h"
#include <iomanip>
#include <iostream>
#include <numeric>

using namespace judas::volume_grid;

int main(int argc, char** argv) {
    const int hz = argc == 2 ? std::stoi(argv[1]) : 60;
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
        if (!AdvanceOwnedEvolvingVolumeInterval(grid, {shape}, fluid, owned,
                                               i == 0 ? 1. / 60 : 1. / 30, 1, gravity, 7, next, error)) {
            std::cout << "warmup rejected at " << i << " " << error << '\n';
            return 2;
        }
        fluid = std::move(next.final.phase.fluid.state);
        owned = std::move(next.final.halfVelocity);
    }
    const double solidVolume = 8 * half * half * half;
    CoupledBoxState body{shape.owner, shape.lo, shape.hi, {}, double(float(500 * solidVolume))};
    const auto contract = FluidTest::SettlingContract::Make(2, .5, dt);
    FluidTest::WindowMotion early, late;
    std::cout << std::setprecision(12) << "dt " << dt << " water " << water << '\n';
    for (std::size_t i = 0; i < contract.late.end; ++i) {
        OwnedCoupledBoxStep next;
        if (!AdvanceOwnedEvolvingCoupledVolumeBox(grid, body, fluid, owned, dt, 1, gravity, next, error)) {
            std::cout << "release rejected at " << i << " t " << i*dt << " y " << body.lo[1]+half
                      << " vy " << body.velocity[1] << " " << error << '\n';
            return 3;
        }
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
        double exterior = 0.;
        int columns = 0;
        for (int k = 0; k < 8; ++k)
            for (int x = 0; x < 8; ++x) {
                if (x*.25 <= body.hi[0]+.25 && (x+1)*.25 >= body.lo[0]-.25 &&
                    k*.25 <= body.hi[2]+.25 && (k+1)*.25 >= body.lo[2]-.25)
                    continue;
                double height = 0.;
                for (int y = 0; y < 12; ++y) {
                    const auto cell = grid.Cell(x,y,k);
                    if (fluid.water[cell] > 1e-12)
                        height = std::max(height, phase.waterPlane[cell]);
                }
                exterior += height;
                ++columns;
            }
        const double fraction = std::clamp((exterior / columns - body.lo[1]) / (2 * half), 0., 1.);
        const double y = body.lo[1] + half, vy = body.velocity[1];
        if (contract.early.Contains(i))
            early.Add(y, vy, fraction);
        if (contract.late.Contains(i))
            late.Add(y, vy, fraction, true, fraction);
        if (i % (hz/6) == 0 || i == contract.late.end-1)
            std::cout << i << ',' << (i+1)*dt << ',' << y << ',' << vy << ',' << fraction << ','
                      << next.box.geometryEvaluations << ',' << next.box.pressureEnergyResidual << ','
                      << std::accumulate(fluid.water.begin(), fluid.water.end(), 0.)-water << '\n';
    }
    const auto verdict = FluidTest::Assess(contract, early, late, std::sqrt(-gravity * half));
    std::cout << "early rms " << early.Rms() << " range " << early.Range()
              << " late rms " << late.Rms() << " range " << late.Range()
              << " peak " << late.peakSpeed << " exterior-plane mean " << late.MeanIndependent()
              << " peak/decay/range/exterior gates " << verdict.peak << verdict.rmsDecay << verdict.rangeDecay
              << verdict.independentImmersion << '\n';
}
