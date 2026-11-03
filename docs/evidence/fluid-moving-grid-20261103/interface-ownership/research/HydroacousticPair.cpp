#include "CutRegionGeometry.h"

#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>

namespace {

using namespace judas::volume_grid;
using Vector = std::array<double, 4>;
using Matrix = std::array<Vector, 4>;

Vector Solve(Matrix matrix, Vector rhs) {
    for (std::size_t k = 0; k < rhs.size(); ++k) {
        for (std::size_t i = k + 1; i < rhs.size(); ++i) {
            const double weight = matrix[i][k] / matrix[k][k];
            for (std::size_t j = k; j < rhs.size(); ++j)
                matrix[i][j] -= weight * matrix[k][j];
            rhs[i] -= weight * rhs[k];
        }
    }
    for (std::size_t i = rhs.size(); i-- > 0;) {
        for (std::size_t j = i + 1; j < rhs.size(); ++j)
            rhs[i] -= matrix[i][j] * rhs[j];
        rhs[i] /= matrix[i][i];
    }
    return rhs;
}

struct Checks {
    int count = 0, failures = 0;

    void Expect(bool condition, const char* label, double value) {
        ++count;
        failures += !condition;
        std::cout << (condition ? "PASS " : "FAIL ") << label << " value=" << value << '\n';
    }
};

void Pair(Checks& checks, int up, double dt, double dryPortFraction) {
    constexpr double g = 9.81, waterDensity = 1000., gasDensity = 1.2, soundSpeed = 343.;
    constexpr double area = 1., compression = .001, baseWater = .5;
    const double scale = waterDensity * g;
    GridLayout layout{{1, 1, 1}, 1.};
    layout.cells[(up + 1) % 3] = 2;
    CutRegionChart chart;
    CutRegionGeometry geometry;
    std::string error;
    if (!PrepareCutRegionChart(layout, {}, {}, {}, {}, chart, error) ||
        !BuildCutRegionGeometry(chart, {}, geometry, error) || geometry.faces.size() != 1) {
        checks.Expect(false, "two chambers share one actual face", 0.);
        return;
    }
    const auto& face = geometry.faces.front();
    const CutRegionWaterPlane water{{up, false}, .5};
    const double Aw = MeasureCutRegionSharedPhase(face, geometry.motion, water, water, true, true).start;
    // Manufactured impedance controls retain the same storage geometry.
    // The actual full port is the exact jointly dry portion of that face.
    const double Ag = dryPortFraction * MeasureCutRegionSharedPhase(face, geometry.motion, water, water, false, false).start;
    const double L = face.length;
    const double V = MeasureCutRegionPhaseVolumes(chart.regions[0], geometry.motion,
        chart.regions[0].lo, chart.regions[0].hi, water)[1];
    const double capacitance = scale * V / (gasDensity * soundSpeed * soundSpeed);
    const double Gw = dt * dt * Aw * g / (2. * L);
    const double Gg = dt * dt * Ag * scale / (2. * gasDensity * L);
    const double storage = 2. * area;
    const Matrix coefficients{{
        {storage + Gw, -Gw, -storage, 0.},
        {-Gw, storage + Gw, 0., -storage},
        {-storage, 0., storage + 2.*capacitance + Gg, -Gg},
        {0., -storage, -Gg, storage + 2.*capacitance + Gg},
    }};
    const auto p = Solve(coefficients, {0., 0., compression, 0.});
    const double qw = Gw * (p[0] - p[1]);
    const double qg = Gg * (p[2] - p[3]);
    const double volumeLeft = storage * (p[0] - p[2]);
    const double volumeRight = storage * (p[1] - p[3]);
    const double massLeft = gasDensity * (2. * capacitance * p[2] - compression - volumeLeft);
    const double massRight = gasDensity * (2. * capacitance * p[3] - volumeRight);
    const double common = compression / (4. * capacitance);
    const double differential = compression / (4. * (capacitance + Gg + area * Gw / (area + Gw)));
    const double pressureError = scale * std::max(std::abs(p[2] - (common + differential)),
                                                  std::abs(p[3] - (common - differential)));
    const double waterError = std::max(std::abs(volumeLeft + qw), std::abs(volumeRight - qw));
    const double gasError = std::max(std::abs(massLeft + gasDensity * qg),
                                    std::abs(massRight - gasDensity * qg));
    const double waterVelocity = 2. * qw / (Aw * dt);
    const double gasVelocity = Ag > 0. ? 2. * qg / (Ag * dt) : 0.;
    const double waterPotential = .5 * scale * (volumeLeft*volumeLeft + volumeRight*volumeRight) / area;
    const double gasCompression = 2. * scale * capacitance * (p[2]*p[2] + p[3]*p[3]);
    const double kinetic = .5 * L * (waterDensity * Aw * waterVelocity*waterVelocity +
                                    gasDensity * Ag * gasVelocity*gasVelocity);
    const double wallArea = .25, displacement = compression / wallArea;
    const double wallForce = scale * p[2] * wallArea;
    const double work = wallForce * displacement;
    const double workResidual = (waterPotential + gasCompression + kinetic) - work;
    checks.Expect(Aw == .5 && V == .5 && pressureError < 2e-10,
                  "analytic symmetric and antisymmetric pressures agree", pressureError);
    checks.Expect(waterError < 1e-16 && gasError < 1e-16 && baseWater + volumeLeft > 0. &&
                  baseWater + volumeRight < 1.,
                  "paired water volume and gas mass close independently", std::max(waterError, gasError));
    checks.Expect(std::abs(workResidual) < 1e-13 && std::abs(workResidual - work) > 1e-3,
                  "midpoint wall reaction closes energy and endpoint traction fails the control", workResidual);
    std::cout << "MEASURE axis=" << up << " dt=" << dt << " dry_area=" << Ag << " gas_delta_Pa="
              << 2.*scale*(p[2]-p[3]) << " water_transfer_m3=" << qw
              << " gas_transfer_kg=" << gasDensity*qg << " wall_work_J=" << work << '\n';
}

} // namespace

int main() {
    std::cout << std::setprecision(17);
    Checks checks;
    for (const int up : {1, 2})
        for (const double dt : {1. / 60., 1. / 240.})
            for (const double dry : {0., 1e-16, 1.})
                Pair(checks, up, dt, dry);
    std::cout << "SUMMARY " << checks.count << " checks " << checks.failures << " failures\n";
    return checks.failures ? 1 : 0;
}
