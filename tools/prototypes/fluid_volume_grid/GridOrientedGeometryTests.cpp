#include "GridOrientedGeometry.h"

#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>

namespace {

using namespace judas::volume_grid;

struct Checks {
    int count = 0;
    int failures = 0;
    void Check(bool passed, const char* name) {
        ++count;
        if (!passed) {
            ++failures;
            std::cerr << "FAIL " << name << '\n';
        }
    }
};

GridVec Add(const GridVec& a, const GridVec& b) {
    return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}

GridVec Subtract(const GridVec& a, const GridVec& b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

GridVec Scale(const GridVec& a, double s) {
    return {s * a[0], s * a[1], s * a[2]};
}

double Dot(const GridVec& a, const GridVec& b) {
    return std::inner_product(a.begin(), a.end(), b.begin(), 0.);
}

GridVec Cross(const GridVec& a, const GridVec& b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0]};
}

double Error(const GridVec& a, const GridVec& b) {
    const auto d = Subtract(a, b);
    return std::sqrt(Dot(d, d));
}

std::array<GridVec, 3> Axes(double yaw, double pitch, double roll) {
    const auto rotate = [&](const GridVec& p) {
        const double cp = std::cos(pitch), sp = std::sin(pitch);
        const double cy = std::cos(yaw), sy = std::sin(yaw);
        const double cr = std::cos(roll), sr = std::sin(roll);
        const GridVec aroundX{p[0], cp * p[1] - sp * p[2], sp * p[1] + cp * p[2]};
        const GridVec aroundY{cy * aroundX[0] + sy * aroundX[2], aroundX[1],
                              -sy * aroundX[0] + cy * aroundX[2]};
        return GridVec{cr * aroundY[0] - sr * aroundY[1],
                       sr * aroundY[0] + cr * aroundY[1], aroundY[2]};
    };
    return {rotate({1., 0., 0.}), rotate({0., 1., 0.}), rotate({0., 0., 1.})};
}

double Sum(const std::vector<double>& values) {
    return std::accumulate(values.begin(), values.end(), 0.);
}

struct Load {
    GridVec force{}, torque{};
};

Load AffineLoad(const std::vector<GridOrientedPatch>& patches, const GridVec& origin,
                double offset, const GridVec& gradient, bool exactTorque = true) {
    Load load;
    for (const auto& patch : patches) {
        const auto arm = Subtract(patch.centroid, origin);
        const double pressure = offset + Dot(gradient, arm);
        const auto force = Scale(patch.normal, -pressure * patch.area);
        load.force = Add(load.force, force);
        load.torque = Add(load.torque, Cross(arm, force));
        if (exactTorque) {
            GridVec moment{};
            for (int axis = 0; axis < 3; ++axis)
                moment[axis] = Dot(patch.centeredMoment[axis], gradient);
            load.torque = Subtract(load.torque, Cross(moment, patch.normal));
        }
    }
    return load;
}

void AlignedAndFaceControls(Checks& checks) {
    const GridLayout layout{{6, 5, 5}, .25};
    const GridOrientedBox aligned{1, {.50, .50, .50}, {.25, .18, .125}};
    const BoxMotion aabb{1, {.25, .32, .375}, {.75, .68, .625}, {}};
    GridOrientedGeometry obb;
    MovingGridGeometry regular;
    std::string error;
    const bool built = BuildGridOrientedGeometry(layout, {aligned}, obb, error) &&
                       BuildMovingGridGeometry(layout, {aabb}, regular, error);
    checks.Check(built, "aligned OBB and existing AABB are both admitted");
    if (!built)
        return;
    double cellError = 0., apertureError = 0.;
    for (std::size_t c = 0; c < layout.CellCount(); ++c)
        cellError = std::max(cellError, std::abs(obb.capacity[c] - regular.startCapacity[c]));
    for (int axis = 0; axis < 3; ++axis)
        for (std::size_t f = 0; f < layout.FaceCount(axis); ++f)
            apertureError = std::max(apertureError,
                                     std::abs(obb.aperture[axis][f] - regular.startAperture[axis][f]));
    checks.Check(cellError < 2e-15 && apertureError < 2e-15,
                 "aligned cut volumes and common face apertures match AABB geometry");
    bool outwardOwner = true;
    double wallArea = 0.;
    for (const auto& patch : obb.patches) {
        if (patch.normal[0] == -1.)
            outwardOwner = outwardOwner && patch.cell % layout.cells[0] == 0;
        if (patch.normal[0] == 1.)
            outwardOwner = outwardOwner && patch.cell % layout.cells[0] == 3;
        wallArea += patch.area;
    }
    checks.Check(outwardOwner && std::abs(wallArea - 8. * (.25 * .18 + .18 * .125 + .125 * .25)) < 2e-15,
                 "coincident solid wall patches belong once to the fluid-facing cell");

    const GridOrientedBox diamond{5, {.75, .54, .63}, {.17, .13, .17},
                                   Axes(std::acos(-1.) / 4., 0., 0.)};
    checks.Check(BuildGridOrientedGeometry(layout, {diamond}, obb, error),
                 "rotated analytic face-section case builds");
    double blocked = 0.;
    for (int k = 0; k < layout.cells[2]; ++k)
        for (int j = 0; j < layout.cells[1]; ++j)
            blocked += .25 * .25 - obb.aperture[0][layout.Face(0, 3, j, k)];
    checks.Check(std::abs(blocked - (2. * .13) * (2. * std::sqrt(2.) * .17)) < 2e-15,
                 "45-degree box section closes exact independently known area on a common face plane");
}

void WholeBoxAffineControls(Checks& checks) {
    const GridLayout layout{{7, 6, 5}, .23};
    const GridVec origin{.14, -.19, .27}, gradient{-3.1, 5.2, 7.4};
    std::mt19937 rng(0x5dc111cb);
    const auto random = [&] { return (double(rng()) + .5) / 4294967296.; };
    double maxVolume = 0., maxArea = 0., maxForce = 0., maxTorque = 0., loss = 0.;
    for (int sample = 0; sample < 128; ++sample) {
        const GridOrientedBox box{17, {.72 + .08 * random(), .65 + .08 * random(),
                                      .52 + .08 * random()},
                                  {.08 + .16 * random(), .08 + .16 * random(), .08 + .16 * random()},
                                  Axes(3. * random(), 3. * random(), 3. * random())};
        GridOrientedGeometry geometry;
        std::string error;
        if (!BuildGridOrientedGeometry(layout, {box}, geometry, error)) {
            checks.Check(false, "all independently rotated box cuts are admitted");
            std::cerr << error << '\n';
            return;
        }
        const double x = box.half[0], y = box.half[1], z = box.half[2];
        const double expectedVolume = 8. * x * y * z;
        const double cellVolume = layout.h * layout.h * layout.h;
        double displaced = 0., area = 0.;
        for (const auto capacity : geometry.capacity)
            displaced += cellVolume - capacity;
        for (const auto& patch : geometry.patches)
            area += patch.area;
        const GridVec expectedForce = Scale(gradient, -expectedVolume);
        const GridVec expectedTorque = Cross(Subtract(box.center, origin), expectedForce);
        const auto load = AffineLoad(geometry.patches, origin, 3.5, gradient);
        const auto pointLoad = AffineLoad(geometry.patches, origin, 3.5, gradient, false);
        maxVolume = std::max(maxVolume, std::abs(displaced - expectedVolume));
        maxArea = std::max(maxArea, std::abs(area - 8. * (x * y + y * z + z * x)));
        maxForce = std::max(maxForce, Error(load.force, expectedForce));
        maxTorque = std::max(maxTorque, Error(load.torque, expectedTorque));
        loss = std::max(loss, Error(pointLoad.torque, expectedTorque));
    }
    std::cout << std::setprecision(17) << "128 oblique boxes, maximum volume error "
              << maxVolume << " m^3, area " << maxArea << " m^2, force " << maxForce
              << " N, torque " << maxTorque << " N m, centroid-only torque error " << loss
              << " N m\n";
    checks.Check(maxVolume < 2e-14 && maxArea < 2e-14,
                 "arbitrary oriented cuts conserve whole solid volume and patch area");
    checks.Check(maxForce < 2e-13 && maxTorque < 2e-13,
                 "affine pressure traction matches independent divergence-theorem force and torque");
    checks.Check(loss > 1e-4, "negative control detects omitted patch second moments");
}

void WetControls(Checks& checks) {
    const GridLayout layout{{5, 5, 5}, .25};
    GridOrientedGeometry with, without;
    std::string error;
    constexpr double level = .62;
    GridOrientedBox dry{3, {.61, .62, .67}, {.09, .06, .09}, Axes(.41, .31, .57)};
    for (int axis = 0; axis < 3; ++axis)
        dry.center[1] += std::abs(dry.axes[axis][1]) * dry.half[axis];
    dry.center[1] += .004;
    checks.Check(BuildGridOrientedGeometry(layout, {dry}, with, error, GridOrientedPlane{1, level}) &&
                     BuildGridOrientedGeometry(layout, {}, without, error, GridOrientedPlane{1, level}),
                 "oblique box 4 mm above the water builds");
    bool sharesWetCell = false;
    for (std::size_t i = 0; i < with.capacity.size(); ++i) {
        if (with.capacity[i] < .25 * .25 * .25 && with.wetCapacity[i] > 0.)
            sharesWetCell = true;
    }
    checks.Check(sharesWetCell && with.wetPatches.empty() &&
                     with.wetCapacity == without.wetCapacity && with.wetAperture == without.wetAperture,
                 "same-cell oblique dry gap has exactly zero wet solid geometry");

    const GridOrientedBox yaw{4, {.61, .63, .67}, {.16, .17, .11}, Axes(.58, 0., 0.)};
    checks.Check(BuildGridOrientedGeometry(layout, {yaw}, with, error, GridOrientedPlane{1, level}),
                 "partly submerged rotated box builds");
    const double depth = level - (yaw.center[1] - yaw.half[1]);
    const double displaced = (2. * yaw.half[0]) * depth * (2. * yaw.half[2]);
    const GridVec origin{.04, .11, -.19};
    const GridVec wetCenter{yaw.center[0], level - .5 * depth, yaw.center[2]};
    const GridVec hydro{0., -9810., 0.};
    const auto load = AffineLoad(with.wetPatches, origin, 9810. * (level - origin[1]), hydro);
    const auto expectedForce = Scale(hydro, -displaced);
    checks.Check(std::abs(Sum(with.wetCapacity) - (Sum(without.wetCapacity) - displaced)) < 2e-14 &&
                     Error(load.force, expectedForce) < 2e-10 &&
                     Error(load.torque, Cross(Subtract(wetCenter, origin), expectedForce)) < 2e-10,
                 "clipped wet volume and water-gauge hydrostatic load match a submerged prism");

    const auto prior = with.capacity;
    auto invalid = yaw;
    invalid.axes[0][0] *= .5;
    checks.Check(!BuildGridOrientedGeometry(layout, {invalid}, with, error) && prior == with.capacity,
                 "invalid rotation frame is rejected transactionally");
    invalid = yaw;
    invalid.owner = 8;
    checks.Check(!BuildGridOrientedGeometry(layout, {yaw, invalid}, with, error) && prior == with.capacity,
                 "overlapping solid owners are rejected transactionally");
    invalid.center[1] = .02;
    checks.Check(!BuildGridOrientedGeometry(layout, {invalid}, with, error) && prior == with.capacity,
                 "domain crossing is rejected transactionally");

    GridOrientedBox first{8, {.61, .63, .67}, {.22, .02, .12}, Axes(0., 0., .73)};
    GridOrientedBox second = first;
    second.owner = 9;
    second.center = Add(second.center, Scale(second.axes[1], .09));
    checks.Check(BuildGridOrientedGeometry(layout, {first, second}, with, error),
                 "disjoint oriented boxes may have overlapping broad-phase bounds");
}

} // namespace

int main() {
    Checks checks;
    AlignedAndFaceControls(checks);
    WholeBoxAffineControls(checks);
    WetControls(checks);
    std::cout << checks.count << " oriented cut geometry checks, " << checks.failures << " failures\n";
    return checks.failures ? 1 : 0;
}
