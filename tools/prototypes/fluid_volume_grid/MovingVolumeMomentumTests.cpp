#include "MovingVolumeMomentum.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

using namespace judas::volume_grid;

struct Checks {
    int total = 0, failures = 0;

    void Expect(bool passed, const std::string& label, double value = 0) {
        ++total;
        failures += !passed;
        std::cout << (passed ? "PASS " : "FAIL ") << label << " value=" << value << '\n';
    }
};

struct Fixture {
    MovingGridGeometry geometry;
    MovingVolumeState before;
    MovingVolumeStep projected;
};

bool Project(Checks& checks, const GridLayout& layout, const BoxMotion& box,
             int waterLayers, Fixture& fixture) {
    std::string error;
    const bool geometry = BuildMovingGridGeometry(layout, {box}, fixture.geometry, error);
    checks.Expect(geometry, "motion geometry " + error);
    if (!geometry) return false;
    fixture.before.water = fixture.geometry.startCapacity;
    for (int k = 0; k < layout.cells[2]; ++k) {
        for (int j = waterLayers; j < layout.cells[1]; ++j) {
            for (int i = 0; i < layout.cells[0]; ++i) {
                fixture.before.water[layout.Cell(i, j, k)] = 0;
            }
        }
    }
    for (int axis = 0; axis < 3; ++axis) {
        fixture.before.velocity[axis].resize(layout.FaceCount(axis));
    }
    const bool projected = AdvanceMovingVolume(layout, fixture.geometry, fixture.before,
        1. / 30., {}, fixture.projected, error);
    checks.Expect(projected, "moving-box pressure and material flux " + error);
    return projected;
}

struct Totals {
    GridVec momentum{};
    double kineticEnergy = 0;
};

Totals Observe(const GridLayout& layout, const std::vector<double>& capacity,
               const std::vector<double>& water, const std::array<std::vector<double>, 3>& velocity) {
    Totals out;
    for (int k = 0; k < layout.cells[2]; ++k) {
        for (int j = 0; j < layout.cells[1]; ++j) {
            for (int i = 0; i < layout.cells[0]; ++i) {
                const auto c = layout.Cell(i, j, k);
                const double halfMass = .5 * ((capacity[c] - water[c]) + 1000 * water[c]);
                for (int axis = 0; axis < 3; ++axis) {
                    std::array<int, 3> high{i, j, k};
                    ++high[axis];
                    const double lowVelocity = velocity[axis][layout.Face(axis, i, j, k)];
                    const double highVelocity = velocity[axis][layout.Face(axis, high[0], high[1], high[2])];
                    // Observe each primal cell's contribution to its two duals.
                    out.momentum[axis] += halfMass * (lowVelocity + highVelocity);
                    out.kineticEnergy += .5 * halfMass *
                        (lowVelocity * lowVelocity + highVelocity * highVelocity);
                }
            }
        }
    }
    return out;
}

double MaxDifference(const GridVec& first, const GridVec& second) {
    return std::max({std::abs(first[0] - second[0]), std::abs(first[1] - second[1]),
                     std::abs(first[2] - second[2])});
}

void TestAnalyticFlux(Checks& checks) {
    const GridLayout layout{{2, 1, 1}, 1.};
    const BoxMotion box{1, {.5, .25, .25}, {1.25, .75, .75}, {.10, 0, 0}};
    Fixture fixture;
    if (!Project(checks, layout, box, 1, fixture)) return;
    // The left primal volume gains .025 m³. This fully wet step therefore
    // passes 25 kg right-to-left, independently of pressure coefficients.
    checks.Expect(std::abs(fixture.projected.materialMassFlux[0][layout.Face(0, 1, 0, 0)] + 25.) < 1e-9,
                  "two-cell translation carries its analytical primal mass flux");
    // Manufactured advection-only velocity samples. Their boundary values
    // are intentionally nonzero to test the half-dual averaging operator.
    fixture.projected.state.velocity[0] = {1., -.5, 2.};
    fixture.projected.state.velocity[1] = {3., 4., 3., 4.};
    MovingVolumeMomentumStep result;
    std::string error;
    const bool ok = AdvectMovingVolumeMomentum(layout, fixture.geometry, fixture.before,
        fixture.projected, result, error);
    checks.Expect(ok, "two-cell analytic momentum transport " + error);
    if (!ok) return;
    const std::array<double, 3> x{431.25 / 450., -421.875 / 906.25, 912.5 / 456.25};
    const std::array<double, 2> y{1362.5 / 450., 1825. / 456.25};
    const GridVec measured{result.velocity[0][0], result.velocity[0][1], result.velocity[0][2]};
    const double xError = MaxDifference(measured, x);
    const double yError = std::max({std::abs(result.velocity[1][0] - y[0]),
        std::abs(result.velocity[1][1] - y[1]), std::abs(result.velocity[1][2] - y[0]),
        std::abs(result.velocity[1][3] - y[1])});
    checks.Expect(xError < 1e-11, "longitudinal half-duals move analytical momentum", xError);
    checks.Expect(yError < 1e-11, "transverse boundary half-duals move analytical momentum", yError);
}

bool SameStep(const MovingVolumeMomentumStep& first, const MovingVolumeMomentumStep& second) {
    return first.velocity == second.velocity && first.beforeMomentum == second.beforeMomentum &&
        first.afterMomentum == second.afterMomentum &&
        first.beforeKineticEnergy == second.beforeKineticEnergy &&
        first.afterKineticEnergy == second.afterKineticEnergy &&
        first.maximumPrimalMassResidual == second.maximumPrimalMassResidual &&
        first.maximumDualMassResidual == second.maximumDualMassResidual &&
        first.maximumDonorCfl == second.maximumDonorCfl;
}

void TestTransportAndRejection(Checks& checks) {
    const GridLayout layout{{8, 7, 7}, .25};
    const BoxMotion box{17, {.61, .56, .60}, {1.01, .74, .93}, {.035, -.02, .024}};
    Fixture fixture;
    if (!Project(checks, layout, box, 5, fixture)) return;
    MovingVolumeMomentumStep result;
    std::string error;
    const bool ok = AdvectMovingVolumeMomentum(layout, fixture.geometry, fixture.before,
        fixture.projected, result, error);
    checks.Expect(ok, "advect actual two-phase moving-box momentum " + error);
    if (!ok) return;
    const auto initial = Observe(layout, fixture.geometry.startCapacity, fixture.before.water,
                                 fixture.projected.state.velocity);
    const auto final = Observe(layout, fixture.geometry.endCapacity, fixture.projected.state.water,
                               result.velocity);
    const double momentumError = MaxDifference(initial.momentum, final.momentum);
    checks.Expect(momentumError < 2e-9, "paired dual fluxes preserve all momentum components", momentumError);
    checks.Expect(MaxDifference(initial.momentum, result.beforeMomentum) < 2e-9 &&
        MaxDifference(final.momentum, result.afterMomentum) < 2e-9,
        "reported momentum agrees with independent primal-to-dual observation");
    checks.Expect(result.maximumPrimalMassResidual < 2e-10,
        "actual two-phase flux closes primal material mass", result.maximumPrimalMassResidual);
    checks.Expect(result.maximumDualMassResidual < 2e-10,
        "paired averages close the dual material mass", result.maximumDualMassResidual);
    checks.Expect(final.kineticEnergy >= 0 && final.kineticEnergy < initial.kineticEnergy,
        "donor momentum mixing dissipates rather than adding energy", final.kineticEnergy - initial.kineticEnergy);
    checks.Expect(std::abs(initial.kineticEnergy - result.beforeKineticEnergy) < 2e-9 &&
        std::abs(final.kineticEnergy - result.afterKineticEnergy) < 2e-9,
        "reported kinetic energy agrees with independent dual mass");
    checks.Expect(result.maximumDonorCfl > 0 && result.maximumDonorCfl < 1,
        "actual moving flux obeys dual material donor budget", result.maximumDonorCfl);
    const auto saved = result;

    auto constant = fixture.projected;
    constexpr GridVec field{.17, -.31, .57};
    for (int axis = 0; axis < 3; ++axis) {
        std::fill(constant.state.velocity[axis].begin(), constant.state.velocity[axis].end(), field[axis]);
    }
    MovingVolumeMomentumStep uniform;
    const bool admitted = AdvectMovingVolumeMomentum(layout, fixture.geometry, fixture.before,
        constant, uniform, error);
    checks.Expect(admitted, "manufactured uniform velocity over actual moving material flux " + error);
    if (admitted) {
        double speedError = 0;
        for (int axis = 0; axis < 3; ++axis) {
            for (double speed : uniform.velocity[axis]) {
                speedError = std::max(speedError, std::abs(speed - field[axis]));
            }
        }
        checks.Expect(speedError < 2e-10, "constant advected velocity survives changing cell capacities", speedError);
    }

    auto bad = fixture.projected;
    bad.materialMassFlux[0][layout.Face(0, 1, 1, 1)] += 1.;
    checks.Expect(!AdvectMovingVolumeMomentum(layout, fixture.geometry, fixture.before, bad, result, error) &&
        error.find("published phases") != std::string::npos && SameStep(saved, result),
        "inconsistent material flux rejects without partial publication");

    bad = fixture.projected;
    bad.materialMassFlux[0][layout.Face(0, 0, 1, 1)] = 1.;
    checks.Expect(!AdvectMovingVolumeMomentum(layout, fixture.geometry, fixture.before, bad, result, error) &&
        !error.empty() && SameStep(saved, result),
        "material flux through a sealed wall rejects without publication");

    bad = fixture.projected;
    bad.state.velocity[0][layout.Face(0, 1, 1, 1)] = std::numeric_limits<double>::quiet_NaN();
    checks.Expect(!AdvectMovingVolumeMomentum(layout, fixture.geometry, fixture.before, bad, result, error) &&
        !error.empty() && SameStep(saved, result),
        "non-finite advected momentum rejects without publication");

    auto excessive = fixture.projected;
    // A large conservative circulation leaves the primal mass ledgers
    // unchanged but requires more donor mass than a dual cell owns.
    constexpr double kilograms = 100;
    excessive.materialMassFlux[0][layout.Face(0, 2, 1, 1)] += kilograms;
    excessive.materialMassFlux[1][layout.Face(1, 2, 2, 1)] += kilograms;
    excessive.materialMassFlux[0][layout.Face(0, 2, 2, 1)] -= kilograms;
    excessive.materialMassFlux[1][layout.Face(1, 1, 2, 1)] -= kilograms;
    checks.Expect(!AdvectMovingVolumeMomentum(layout, fixture.geometry, fixture.before, excessive, result, error) &&
        error.find("donor capacity") != std::string::npos && SameStep(saved, result),
        "conservative but excessive dual donor flux rejects transactionally");

    const GridLayout topologyLayout{{3, 3, 3}, 1.};
    const BoxMotion opening{42, {.55, .85, .85}, {1.07, 2.15, 2.15}, {-.10, 0, 0}};
    Fixture topology;
    if (!Project(checks, topologyLayout, opening, 3, topology)) return;
    const auto face = topologyLayout.Face(0, 1, 1, 1);
    checks.Expect(topology.geometry.startAperture[0][face] == 0 && topology.geometry.endAperture[0][face] > 0,
        "topology fixture actually opens a closed face");
    checks.Expect(!AdvectMovingVolumeMomentum(topologyLayout, topology.geometry, topology.before,
        topology.projected, result, error) && error.find("open interior") != std::string::npos && SameStep(saved, result),
        "moving aperture topology change rejects without inventing face momentum");
}

} // namespace

int main() {
    std::cout << std::setprecision(17);
    Checks checks;
    TestAnalyticFlux(checks);
    TestTransportAndRejection(checks);
    std::cout << "SUMMARY checks=" << checks.total << " failures=" << checks.failures << '\n';
    return checks.failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
