#include "OrientedMovingVolume.h"

#include <cstdio>
#include <numeric>

namespace {

using namespace judas::volume_grid;
constexpr GridLayout layout{{5, 5, 5}, .25};
constexpr double dt = 1. / 30.;

struct Checks {
    int count = 0;
    int failures = 0;
    void Expect(bool pass, const char* name, double measurement = 0) {
        ++count;
        if (!pass)
            ++failures;
        std::printf("%s %s %.17g\n", pass ? "PASS" : "FAIL", name, measurement);
    }
};

double Sum(const std::vector<double>& values) {
    return std::accumulate(values.begin(), values.end(), 0.);
}

void TestMovingPressure(Checks& checks) {
    const GridOrientedBox body{11, {.62, .59, .64}, {.16, .09, .11}};
    GridOrientedGeometry geometry;
    std::string error;
    if (!BuildGridOrientedGeometry(layout, {body}, geometry, error)) {
        checks.Expect(false, error.c_str());
        return;
    }
    MovingVolumeState water;
    water.water = geometry.capacity;
    for (int axis = 0; axis < 3; ++axis)
        water.velocity[axis].resize(layout.FaceCount(axis));
    auto air = water;
    std::fill(air.water.begin(), air.water.end(), 0);
    constexpr GridVec gravity{1.2, -9.81, .7};
    const GridOrientedMotion motions[]{
        {body, {.03, -.01, .015}},
        {body, {}, {.6, 0, .8}, .12},
        {body, {.012, -.003, .004}, {.6, 0, .8}, .12},
    };
    OrientedMovingVolumeStep wet;
    for (const auto& motion : motions) {
        OrientedMovingVolumeStep dry;
        const bool solved = AdvanceOrientedMovingVolume(layout, motion, water, dt, {}, wet, error) &&
            AdvanceOrientedMovingVolume(layout, motion, air, dt, {}, dry, error);
        checks.Expect(solved, solved ? "same swept geometry solves in water and air" : error.c_str());
        if (!solved)
            return;
        const double work = wet.fluid.pressureFluidWork;
        checks.Expect(work > .01 && std::abs(work - 1000 * dry.fluid.pressureFluidWork) < 1e-9,
                      "prescribed motion transfers pressure work proportional to phase density", work);
        checks.Expect(std::abs(wet.pressureWorkResidual) < 1e-8 && wet.maximumSweepResidual < 1e-10,
                      "fluid pressure work closes with swept rigid force and torque",
                      wet.pressureWorkResidual);
        checks.Expect(std::abs(Sum(wet.fluid.state.water) - Sum(water.water)) < 1e-12 &&
                      Sum(dry.fluid.state.water) == 0, "moving OBB conserves both phase inventories");
        if (motion.angle != 0)
            checks.Expect(std::abs(std::inner_product(wet.load.torque.begin(), wet.load.torque.end(),
                                                      motion.rotationAxis.begin(), 0.)) > .1,
                          "rotational motion creates real pressure torque");
    }

    const GridOrientedMotion still{body, {}};
    const bool rested = AdvanceOrientedMovingVolume(layout, still, water, dt, gravity, wet, error);
    checks.Expect(rested, rested ? "fully wet hydrostatic OBB solves" : error.c_str());
    if (!rested)
        return;
    const double volume = 8 * body.half[0] * body.half[1] * body.half[2];
    double gravityError = 0, maxTorque = 0;
    for (int axis = 0; axis < 3; ++axis) {
        gravityError = std::max(gravityError, std::abs(wet.load.force[axis] + 1000 * volume * gravity[axis]));
        maxTorque = std::max(maxTorque, std::abs(wet.load.torque[axis]));
    }
    checks.Expect(gravityError < 1e-12 && maxTorque < 1e-12 && wet.fluid.iterations == 0,
                  "oblique gravity applies displaced weight without flow or centre torque", gravityError);
    const auto previous = wet;
    auto mixed = water;
    mixed.water[0] *= .5;
    const bool accepted = AdvanceOrientedMovingVolume(layout, still, mixed, dt, gravity, wet, error);
    checks.Expect(!accepted && error.find("uniform phase") != std::string::npos &&
                  wet.fluid.state.water == previous.fluid.state.water &&
                  wet.load.force == previous.load.force, "unsupported mixed OBB phase leaves output intact");
}

} // namespace

int main() {
    Checks checks;
    TestMovingPressure(checks);
    std::printf("SUMMARY oriented moving-volume pressure %d checks %d failures\n",
                checks.count, checks.failures);
    return checks.failures ? 1 : 0;
}
