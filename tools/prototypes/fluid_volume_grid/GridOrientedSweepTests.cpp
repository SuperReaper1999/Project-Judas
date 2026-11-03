#include "GridOrientedSweep.h"

#include <iomanip>
#include <iostream>
#include <numeric>

namespace {

using namespace judas::volume_grid;
constexpr GridLayout grid{{5, 5, 5}, .25};

struct Checks {
    int count = 0, failures = 0;
    void Check(bool passed, const char* name) {
        ++count;
        if (!passed) {
            ++failures;
            std::cerr << "FAIL " << name << '\n';
        }
    }
};

double Dot(const GridVec& a, const GridVec& b) {
    return std::inner_product(a.begin(), a.end(), b.begin(), 0.);
}
GridVec Cross(const GridVec& a, const GridVec& b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0]};
}
GridVec Subtract(const GridVec& a, const GridVec& b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

GridOrientedBox ReferencePose(const GridOrientedMotion& motion, double time) {
    auto box = motion.start;
    for (int a = 0; a < 3; ++a)
        box.center[a] += time * motion.translation[a];
    // Independent angle-axis quaternion rotation of the original basis.
    const double cosHalf = std::cos(.5 * time * motion.angle);
    const double sinHalf = std::sin(.5 * time * motion.angle);
    const GridVec q{sinHalf * motion.rotationAxis[0], sinHalf * motion.rotationAxis[1],
                    sinHalf * motion.rotationAxis[2]};
    for (auto& axis : box.axes) {
        const auto first = Cross(q, axis), second = Cross(q, first);
        for (int a = 0; a < 3; ++a)
            axis[a] += 2. * (cosHalf * first[a] + second[a]);
    }
    return box;
}

GridOrientedBox Oblique() {
    const double x = .37, y = .61;
    const std::array<GridVec, 3> basis{{
        {std::cos(y), 0., -std::sin(y)},
        {std::sin(y) * std::sin(x), std::cos(x), std::cos(y) * std::sin(x)},
        {std::sin(y) * std::cos(x), -std::sin(x), std::cos(y) * std::cos(x)},
    }};
    return {11, {.62, .59, .64}, {.16, .09, .11}, basis};
}

void Translation(Checks& checks) {
    const GridOrientedMotion motion{Oblique(), {.12, -.06, .04}};
    GridOrientedSweep sweep;
    std::string error;
    const bool admitted = BuildGridOrientedSweep(grid, motion, sweep, error);
    checks.Check(admitted, "arbitrarily oriented linear sweep is admitted");
    if (!admitted) {
        std::cerr << error << '\n';
        return;
    }
    checks.Check(sweep.maxFaceResidual < 8e-11 && sweep.maxPatchResidual < 8e-11,
                 "translation changes capacity by shared face flux and oriented wall work");
    std::cout << std::setprecision(17) << "translation evaluations " << sweep.evaluations
              << ", face residual " << sweep.maxFaceResidual << " m^3, wall residual "
              << sweep.maxPatchResidual << " m^3\n";

    const GridOrientedBox aligned{6, {.48, .58, .60}, {.12, .16, .14}};
    const GridOrientedMotion crossing{aligned, {.16, 0., 0.}};
    const BoxMotion reference{6, {.36, .42, .46}, {.60, .74, .74}, {.16, 0., 0.}};
    MovingGridGeometry analytic;
    const bool compared = BuildMovingGridGeometry(grid, {reference}, analytic, error) &&
                          BuildGridOrientedSweep(grid, crossing, sweep, error);
    checks.Check(compared, "aligned sweep with shared-plane jumps is admitted");
    if (!compared)
        return;
    double apertureError = 0., sweepError = 0.;
    for (int axis = 0; axis < 3; ++axis)
        for (std::size_t i = 0; i < grid.FaceCount(axis); ++i) {
            apertureError = std::max(apertureError,
                std::abs(sweep.averageAperture[axis][i] - analytic.averageAperture[axis][i]));
            sweepError = std::max(sweepError,
                std::abs(sweep.solidSweep[axis][i] - analytic.solidSweep[axis][i]));
        }
    checks.Check(apertureError < 2e-15 && sweepError < 2e-15,
                 "open quadrature and vertex times recover exact AABB aperture and sweep");
}

void Rotation(Checks& checks) {
    const GridVec axis{.6, .0, .8};
    const GridOrientedMotion motion{Oblique(), {}, axis, .29};
    GridOrientedSweep sweep;
    std::string error;
    const bool admitted = BuildGridOrientedSweep(grid, motion, sweep, error);
    checks.Check(admitted, "constant oblique angular sweep is admitted");
    if (!admitted) {
        std::cerr << error << '\n';
        return;
    }
    checks.Check(sweep.maxFaceResidual < 8e-11 && sweep.maxPatchResidual < 8e-11,
                 "angular face velocity and wall torque independently close cell volume");
    std::cout << "angular evaluations " << sweep.evaluations << ", face residual "
              << sweep.maxFaceResidual << " m^3, wall residual " << sweep.maxPatchResidual << " m^3\n";

    std::vector<double> frozen(grid.CellCount()), integrated(grid.CellCount());
    for (const auto& patch : sweep.start.patches) {
        const auto arm = Subtract(patch.centroid, motion.start.center);
        frozen[patch.cell] += Dot(axis, Cross(arm, patch.normal)) * patch.area * motion.angle;
    }
    for (const auto& patch : sweep.patches)
        integrated[patch.cell] += Dot(axis, patch.torqueArea) * motion.angle;
    double frozenError = 0., work = 0., expected = 0.;
    for (std::size_t cell = 0; cell < grid.CellCount(); ++cell) {
        const double change = sweep.end.capacity[cell] - sweep.start.capacity[cell];
        const double pressure = std::sin(double(cell) * .49);
        frozenError = std::max(frozenError, std::abs(frozen[cell] + change));
        work -= pressure * integrated[cell];
        expected += pressure * change;
    }
    checks.Check(std::abs(work - expected) < 5e-10 && std::abs(expected) > 1e-5,
                 "nonconstant pressure torque work matches endpoint occupied-volume work");
    checks.Check(frozenError > 1e-4,
                 "frozen start-wall torque fails the same moving angular work identity");

    const auto endpoint = ReferencePose(motion, 1.);
    GridOrientedSweep reverse;
    const bool reversed = BuildGridOrientedSweep(grid, {endpoint, {}, axis, -.29}, reverse, error);
    checks.Check(reversed, "time-reversed angular sweep is admitted");
    if (!reversed) {
        std::cerr << error << '\n';
        return;
    }
    double directionError = 0.;
    for (int a = 0; a < 3; ++a)
        for (std::size_t i = 0; i < grid.FaceCount(a); ++i)
            directionError = std::max({directionError,
                std::abs(sweep.averageAperture[a][i] - reverse.averageAperture[a][i]),
                std::abs(sweep.solidSweep[a][i] + reverse.solidSweep[a][i])});
    checks.Check(directionError < 2e-9, "reversal retains apertures and negates signed angular sweep");

    // Independently integrate static cuts at composite midpoint times using
    // the quaternion pose, rather than the adaptive open Gauss rules.
    constexpr int samples = 1024;
    std::array<std::vector<double>, 3> aperture;
    for (int a = 0; a < 3; ++a)
        aperture[a].resize(grid.FaceCount(a));
    for (int i = 0; i < samples; ++i) {
        GridOrientedGeometry sampled;
        if (!BuildGridOrientedGeometry(grid, {ReferencePose(motion, (i + .5) / samples)},
                                       sampled, error)) {
            checks.Check(false, "independent sampled angular geometry is admitted");
            return;
        }
        for (int a = 0; a < 3; ++a)
            for (std::size_t f = 0; f < grid.FaceCount(a); ++f)
                aperture[a][f] += sampled.aperture[a][f] / samples;
    }
    double apertureError = 0.;
    for (int a = 0; a < 3; ++a)
        for (std::size_t f = 0; f < grid.FaceCount(a); ++f)
            apertureError = std::max(apertureError,
                                     std::abs(aperture[a][f] - sweep.averageAperture[a][f]));
    std::cout << "angular refined midpoint aperture error " << apertureError
              << " m^2, reversal " << directionError << ", frozen volume work error "
              << frozenError << " m^3\n";
    checks.Check(apertureError < 1e-8, "adaptive angular apertures agree with refined independent timing");

    auto combined = motion;
    combined.translation = {-.06, .07, .03};
    checks.Check(BuildGridOrientedSweep(grid, combined, reverse, error) &&
                     reverse.maxFaceResidual < 8e-11 && reverse.maxPatchResidual < 8e-11,
                 "combined translation and rotation close both geometric balances");
}

void InvalidMotion(Checks& checks) {
    GridOrientedSweep sweep;
    std::string error;
    const auto start = Oblique();
    checks.Check(BuildGridOrientedSweep(grid, {start}, sweep, error) &&
                     sweep.start.aperture == sweep.end.aperture &&
                     sweep.averageAperture == sweep.start.aperture && sweep.evaluations == 1 &&
                     sweep.maxFaceResidual == 0. && sweep.maxPatchResidual == 0.,
                 "stationary swept geometry has no swept volume");
    const auto preserved = sweep.averageAperture;
    const auto evaluations = sweep.evaluations;
    checks.Check(!BuildGridOrientedSweep(grid, {start, {.26, 0., 0.}}, sweep, error) &&
                     sweep.averageAperture == preserved && sweep.evaluations == evaluations,
                 "unsupported step travel is rejected without publication");
    checks.Check(!BuildGridOrientedSweep(grid, {start, {}, {.6, 0., .1}, .2}, sweep, error) &&
                     sweep.averageAperture == preserved,
                 "invalid rotation axis is rejected without publication");

    const double pi = std::acos(-1.);
    const double c = std::cos(pi / 8.), s = std::sin(pi / 8.);
    const GridOrientedBox nearBoundary{7, {.162, .6, .6}, {.12, .12, .08},
                                       {{{c, s, 0.}, {-s, c, 0.}, {0., 0., 1.}}}};
    const GridOrientedMotion leavesBetween{nearBoundary, {}, {0., 0., 1.}, pi / 4.};
    GridOrientedGeometry pose;
    checks.Check(BuildGridOrientedGeometry(grid, {nearBoundary}, pose, error) &&
                     BuildGridOrientedGeometry(grid, {ReferencePose(leavesBetween, 1.)}, pose, error) &&
                     !BuildGridOrientedGeometry(grid, {ReferencePose(leavesBetween, .5)}, pose, error) &&
                     !BuildGridOrientedSweep(grid, leavesBetween, sweep, error) &&
                     sweep.averageAperture == preserved,
                 "valid endpoints cannot admit a box that crosses the domain between them");
}

} // namespace

int main() {
    Checks checks;
    Translation(checks);
    Rotation(checks);
    InvalidMotion(checks);
    std::cout << checks.count << " oriented sweep checks, " << checks.failures << " failures\n";
    return checks.failures ? 1 : 0;
}
