#include "GridPhaseGeometry.h"
#include "PlanarMovingVolume.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <string_view>

namespace {

using namespace judas::volume_grid;

constexpr GridLayout kLayout{{12, 12, 12}, .2};
constexpr double kHeight = 1.1;
constexpr double kGravity = -9.81;
constexpr double kDt = 1. / 60.;

struct Checks {
    int total = 0, failed = 0;
    void Expect(bool passed, const char* label, double measurement = 0) {
        ++total;
        if (!passed) ++failed;
        std::printf("%s %s %.17g\n", passed ? "PASS" : "FAIL", label, measurement);
    }
};

double Maximum(const GridVec& v) {
    return std::max({std::abs(v[0]), std::abs(v[1]), std::abs(v[2])});
}

GridVec Difference(const GridVec& a, const GridVec& b) {
    return {a[0]-b[0], a[1]-b[1], a[2]-b[2]};
}

double Dot(const GridVec& a, const GridVec& b) {
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}

GridVec Cross(const GridVec& a, const GridVec& b) {
    return {a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]};
}

double MaxField(const std::array<std::vector<double>, 3>& field) {
    double maximum = 0;
    for (const auto& axis : field)
        for (double value : axis)
            maximum = std::max(maximum, std::abs(value));
    return maximum;
}

MovingVolumeState InitialState(const BoxMotion& box, int axis, double height) {
    MovingVolumeState state;
    state.water.resize(kLayout.CellCount());
    for (int a = 0; a < 3; ++a)
        state.velocity[a].resize(kLayout.FaceCount(a));

    // Independent axis-aligned box/water clipping. No production phase
    // geometry calls seed the input accepted by the solved-state controls.
    for (int k = 0; k < kLayout.cells[2]; ++k)
        for (int j = 0; j < kLayout.cells[1]; ++j)
            for (int i = 0; i < kLayout.cells[0]; ++i) {
                const int coordinate[] = {i,j,k};
                double water = 1, displaced = 1;
                for (int a = 0; a < 3; ++a) {
                    const double lo = coordinate[a]*kLayout.h;
                    double hi = (coordinate[a]+1)*kLayout.h;
                    if (a == axis) hi = std::min(hi, height);
                    water *= std::max(0., hi-lo);
                    displaced *= std::max(0., std::min(hi,box.hi[a])-std::max(lo,box.lo[a]));
                }
                state.water[kLayout.Cell(i,j,k)] = water-displaced;
            }
    return state;
}

bool Advance(Checks& checks, const BoxMotion& box, const MovingVolumeState& state, int axis,
             double height, PlanarMovingStep& result) {
    std::string error;
    const bool advanced = AdvancePlanarMovingVolume(kLayout, {box}, state, kDt, axis, height,
                                                    kGravity, result, error);
    if (!advanced)
        std::printf("advance error %s\n", error.c_str());
    checks.Expect(advanced, "certified planar phase advances");
    if (advanced && (result.loads.size() != 1 || result.loads[0].owner != box.owner)) {
        checks.Expect(false, "certified planar phase returns the solid owner");
        return false;
    }
    return advanced;
}

GridPressureLoad Archimedes(const BoxMotion& box, int axis, double height) {
    const int first = (axis+1)%3, second = (axis+2)%3;
    const double area = (box.hi[first]-box.lo[first])*(box.hi[second]-box.lo[second]);
    const double waterDepth = std::clamp(height-box.lo[axis], 0., box.hi[axis]-box.lo[axis]);
    GridPressureLoad expected;
    expected.owner = box.owner;
    expected.force[axis] = -kGravity*area*(box.hi[axis]-box.lo[axis]+999*waterDepth);
    GridVec center{};
    for (int a = 0; a < 3; ++a) center[a] = (box.lo[a]+box.hi[a])/2;
    expected.torque = Cross(center, expected.force);
    return expected;
}

void StaticAndDryControls(Checks& checks) {
    for (int axis = 0; axis < 3; ++axis) {
        BoxMotion box{13, {.43,.66,.61}, {.91,1.08,1.47}, {}};
        const double dryHeight = kHeight+.004;
        box.lo[axis] = dryHeight;
        box.hi[axis] = dryHeight+.473;
        const auto dry = InitialState(box, axis, kHeight);
        PlanarMovingStep dryWater, dryAir;
        if (!Advance(checks, box, dry, axis, kHeight, dryWater) ||
            !Advance(checks, box, InitialState(box,axis,0), axis, 0, dryAir))
            continue;
        const auto expectedAir = Archimedes(box, axis, 0);
        const auto& waterLoad = dryWater.loads.at(0);
        const auto& airLoad = dryAir.loads.at(0);
        checks.Expect(waterLoad.owner == box.owner && airLoad.owner == box.owner,
                      "solved planar loads retain their owning box");
        const double dryForceError = Maximum(Difference(waterLoad.force, airLoad.force));
        const double dryTorqueError = Maximum(Difference(waterLoad.torque, airLoad.torque));
        checks.Expect(dryForceError < 2e-10, "4 mm same-cell dry gap transfers no water force", dryForceError);
        checks.Expect(dryTorqueError < 2e-10, "4 mm same-cell dry gap transfers no water torque", dryTorqueError);
        const double airForceError = Maximum(Difference(airLoad.force, expectedAir.force));
        checks.Expect(airForceError < 2e-10, "actual dry reaction retains only physical air buoyancy", airForceError);
        checks.Expect(MaxField(dryWater.fluid.state.velocity) < 1e-13 &&
                      MaxField(dryWater.fluid.flux) < 1e-15,
                      "dry cut box and horizontal water stay at rest");

        box.lo[axis] = kHeight-.004;
        const auto wet = InitialState(box, axis, kHeight);
        PlanarMovingStep supported;
        if (!Advance(checks, box, wet, axis, kHeight, supported))
            continue;
        const auto expected = Archimedes(box, axis, kHeight);
        const auto& measured = supported.loads.at(0);
        const double forceError = Maximum(Difference(expected.force, measured.force));
        const double torqueError = Maximum(Difference(expected.torque, measured.torque));
        checks.Expect(forceError < 2e-10 && measured.force[axis] > 2,
                      "4 mm positive immersion gives nonzero exact buoyancy", forceError);
        checks.Expect(torqueError < 2e-10, "partial immersion gives exact off-centre torque", torqueError);
        const double massDifference = std::abs(
            std::accumulate(wet.water.begin(), wet.water.end(), 0.)-
            std::accumulate(supported.fluid.state.water.begin(), supported.fluid.state.water.end(), 0.));
        checks.Expect(MaxField(supported.fluid.state.velocity) < 1e-13 &&
                      MaxField(supported.fluid.flux) < 1e-15 &&
                      massDifference < 1e-12,
                      "supported partially wet cut box retains rest and water volume", massDifference);
    }
}

void PerturbationAndRejection(Checks& checks) {
    const BoxMotion box{17, {.43,1.104,.61}, {.91,1.58,1.47}, {}};
    auto state = InitialState(box, 1, kHeight);
    constexpr double speed = .19;
    // A closed face loop in four full cells gives exactly zero divergence.
    state.velocity[0][kLayout.Face(0,1,0,0)] = speed;
    state.velocity[1][kLayout.Face(1,1,1,0)] = speed;
    state.velocity[0][kLayout.Face(0,1,1,0)] = -speed;
    state.velocity[1][kLayout.Face(1,0,1,0)] = -speed;
    PlanarMovingStep output;
    if (!Advance(checks, box, state, 1, kHeight, output))
        return;
    double velocityError = 0, fluxError = 0;
    for (int a = 0; a < 3; ++a)
        for (std::size_t f = 0; f < state.velocity[a].size(); ++f) {
            velocityError = std::max(velocityError, std::abs(state.velocity[a][f]-output.fluid.state.velocity[a][f]));
            const double exactFlux = kDt*kLayout.h*kLayout.h*state.velocity[a][f];
            fluxError = std::max(fluxError, std::abs(exactFlux-output.fluid.flux[a][f]));
        }
    checks.Expect(velocityError < 2e-13, "well-balanced solve preserves a nonzero divergence-free velocity", velocityError);
    checks.Expect(fluxError < 2e-15 && MaxField(output.fluid.flux) > 1e-4,
                  "velocity perturbation produces actual conservative transport", fluxError);

    // Equal-volume rearrangement is bounded and conserves total water, but
    // cannot be certified by this flat hydrostatic reference.
    auto changed = state;
    changed.water[kLayout.Cell(0,0,0)] -= .0001;
    changed.water[kLayout.Cell(0,7,0)] += .0001;
    const auto unchangedWater = output.fluid.state.water;
    const auto unchangedPressure = output.fluid.pressure;
    const auto unchangedForce = output.loads.at(0).force;
    std::string error;
    const bool accepted = AdvancePlanarMovingVolume(kLayout, {box}, changed, kDt, 1, kHeight,
                                                    kGravity, output, error);
    checks.Expect(!accepted && !error.empty(), "same-total nonplanar water fails geometry admission");
    checks.Expect(output.fluid.state.water == unchangedWater &&
                  output.fluid.pressure == unchangedPressure && output.loads.at(0).force == unchangedForce,
                  "nonplanar rejection preserves the previous fluid and body load");
}

void PrescribedMotion(Checks& checks) {
    const BoxMotion box{23, {.43,.35,.61}, {.91,.89,1.47}, {.012,-.006,.009}};
    const auto state = InitialState(box, 1, kHeight);
    PlanarMovingStep output;
    if (!Advance(checks, box, state, 1, kHeight, output))
        return;
    const auto expected = Archimedes(box, 1, kHeight);
    const double hydroWork = Dot(expected.force, box.translation);
    const double solidWork = Dot(output.loads.at(0).force, box.translation);
    const double dynamicWork = solidWork-hydroWork;
    const double workError = std::abs(dynamicWork+output.fluid.pressureFluidWork);
    checks.Expect(dynamicWork < -1e-4, "prescribed moving submerged box does work on real fluid", dynamicWork);
    checks.Expect(workError < 1e-9, "dynamic solid and fluid pressure work are reciprocal", workError);
    const double waterDifference = std::abs(
        std::accumulate(state.water.begin(), state.water.end(), 0.)-
        std::accumulate(output.fluid.state.water.begin(), output.fluid.state.water.end(), 0.));
    checks.Expect(waterDifference < 1e-10, "moving submerged body conserves water volume", waterDifference);
}

void CrossWaterline(Checks& checks, bool strictPhaseFlux) {
    const BoxMotion box{37, {.43,kHeight+.004,.61}, {.91,1.58,1.47}, {.003,-.008,.002}};
    const auto state = InitialState(box, 1, kHeight);
    PlanarMovingStep output;
    if (!Advance(checks, box, state, 1, kHeight, output))
        return;

    const double tangentArea = (box.hi[0]-box.lo[0])*(box.hi[2]-box.lo[2]);
    const double travel = -box.translation[1];
    const double wetDepthAtEnd = travel-(box.lo[1]-kHeight);
    const double averageWetDepth = .5*wetDepthAtEnd*(wetDepthAtEnd/travel);
    const double meanHydro = -kGravity*tangentArea*(box.hi[1]-box.lo[1]+999*averageWetDepth);
    const double solidWork = Dot(output.loads.at(0).force, box.translation);
    const double dynamicWork = solidWork-meanHydro*box.translation[1];
    const double workError = std::abs(dynamicWork+output.fluid.pressureFluidWork);
    checks.Expect(dynamicWork < -1e-4, "actual waterline crossing produces pressure-driven fluid work", dynamicWork);
    checks.Expect(workError < 1e-9, "crossing pressure work separates exact hydrostatics and reciprocal dynamics", workError);
    const double volumeDifference = std::abs(
        std::accumulate(state.water.begin(),state.water.end(),0.)-
        std::accumulate(output.fluid.state.water.begin(),output.fluid.state.water.end(),0.));
    checks.Expect(volumeDifference < 1e-10, "waterline crossing conserves material volume", volumeDifference);

    MovingGridGeometry motion;
    std::string error;
    const bool built = BuildMovingGridGeometry(kLayout,{box},motion,error);
    checks.Expect(built, "waterline crossing has verifiable finite face geometry");
    if (!built)
        return;
    double upwardWater = 0, unreachableWater = 0;
    for (int k = 0; k < kLayout.cells[2]; ++k) {
        for (int j = 1; j < kLayout.cells[1]; ++j) {
            const double faceHeight = j*kLayout.h;
            if (faceHeight <= kHeight)
                continue;
            for (int i = 0; i < kLayout.cells[0]; ++i) {
                const auto face = kLayout.Face(1,i,j,k);
                const double volume = output.fluid.flux[1][face];
                const double area = motion.averageAperture[1][face];
                if (volume <= 0 || area <= 0)
                    continue;
                const double water = (output.fluid.materialMassFlux[1][face]-volume)/999;
                upwardWater += water;
                // A face-aligned departure slab shorter than the known dry
                // gap cannot contain water from the certified initial plane.
                if (volume/area < faceHeight-kHeight)
                    unreachableWater += water;
            }
        }
    }
    std::printf("MEASURE crossing mean_hydro_N=%.17g above_plane_water_m3=%.17g "
                "outside_departure_support_m3=%.17g max_donor_cfl=%.17g\n",
                meanHydro,upwardWater,unreachableWater,output.fluid.maxCfl);
    if (strictPhaseFlux)
        checks.Expect(unreachableWater < 1e-12,
                      "phase flux excludes unreachable dry departure slabs", unreachableWater);

    BoxMotion end = box;
    for (int a = 0; a < 3; ++a) {
        end.lo[a] += box.translation[a];
        end.hi[a] += box.translation[a];
        end.translation[a] = 0;
    }
    const auto endWater = output.fluid.state.water;
    const bool canContinue = AdvancePlanarMovingVolume(kLayout,{end},output.fluid.state,kDt,1,
                                                        kHeight,kGravity,output,error);
    checks.Expect(!canContinue && output.fluid.state.water == endWater,
                  "evolved nonplanar state requires a new phase reconstruction");
    auto highCfl = box;
    highCfl.translation[1] = -.012;
    const bool advanced = AdvancePlanarMovingVolume(kLayout,{highCfl},state,kDt,1,
                                                      kHeight,kGravity,output,error);
    checks.Expect(!advanced && error.find("donor capacity") != std::string::npos &&
                  output.fluid.state.water == endWater,
                  "faster crossing rejects exhausted cut-cell donor without publication");
}

} // namespace

int main(int argc, char** argv) {
    const bool strictPhaseFlux = argc == 2 && std::string_view(argv[1]) == "--strict-phase-flux";
    if (argc != 1 && !strictPhaseFlux)
        return 2;
    Checks checks;
    StaticAndDryControls(checks);
    PerturbationAndRejection(checks);
    PrescribedMotion(checks);
    CrossWaterline(checks, strictPhaseFlux);
    std::printf("SUMMARY solved planar volume %d checks %d failures\n", checks.total, checks.failed);
    return checks.failed ? 1 : 0;
}
