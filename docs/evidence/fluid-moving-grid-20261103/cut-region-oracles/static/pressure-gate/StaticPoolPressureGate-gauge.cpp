#include "../phase-geometry/GridStaticMovingPhase.h"
// Compile the unmodified pinned pressure source in this disposable TU so the
// adapter can reach its private AdvanceVolume geometry boundary.
#include "MovingVolumeGrid.cpp"

#include <chrono>
#include <cstdio>

namespace {

using namespace judas::volume_grid;
using Index = std::array<int, 3>;

const GridLayout layout{{24, 32, 24}, .125};
constexpr double dt = double(1.f / 60), gravity = -double(9.81f);
constexpr double height = 0.;
constexpr double h = .125, cellVolume = h*h*h;

struct Checks {
    int total = 0, failed = 0;
    void Expect(bool pass, const char* label, double value = 0.) {
        ++total;
        failed += !pass;
        std::printf("%s %s %.17g\n", pass ? "PASS" : "FAIL", label, value);
    }
};

std::vector<BoxMotion> Walls(bool sealed) {
    constexpr GridVec origin{-1.5, -.5, -1.5};
    std::vector<BoxMotion> boxes;
    const auto box = [&](const std::array<float, 3>& center, const std::array<float, 3>& half) {
        BoxMotion item{std::uint32_t(boxes.size() + 1)};
        for (int axis = 0; axis < 3; ++axis) {
            item.lo[axis] = double(center[axis]) - half[axis] - origin[axis];
            item.hi[axis] = double(center[axis]) + half[axis] - origin[axis];
        }
        boxes.push_back(item);
    };
    box({0.f, -.1f, 0.f}, {1.1f, .1f, 1.1f});
    box({-1.1f, 1.5f, 0.f}, {.1f, 1.5f, 1.1f});
    box({1.1f, 1.5f, 0.f}, {.1f, 1.5f, 1.1f});
    const float zWall = sealed ? 1.5f : 1.499f;
    box({0.f, 1.5f, -1.1f}, {1.f, zWall, .1f});
    box({0.f, 1.5f, 1.1f}, {1.f, zWall, .1f});
    return boxes;
}

BoxMotion Body() {
    constexpr double half = double(.3f), y = double(.7f) + .5;
    return {6, {1.5-half, y-half, 1.5-half}, {1.5+half, y+half, 1.5+half}, {}};
}

bool BoundedTopology(const GridStaticUnion& walls, const BoxMotion& moving, std::string& error) {
    for (int axis = 0; axis < 3; ++axis) {
        if (moving.hi[axis] - moving.lo[axis] < h) {
            error = "pool gate requires a box at least one cell wide on every chart axis";
            return false;
        }
    }
    // The admitted one body cannot create an interior dividing slab by
    // itself. Mixed static/dynamic cuts are rejected for this first gate.
    bool valid = true;
    grid_phase_detail::VisitGrid(layout, -1, [&](const Index& i) {
        if (walls.cellPartition[layout.Cell(i[0], i[1], i[2])] == GridStaticUnion::uncut)
            return;
        bool intersects = true;
        for (int axis = 0; axis < 3; ++axis) {
            const double lo = std::min(moving.lo[axis], moving.lo[axis] + moving.translation[axis]);
            const double hi = std::max(moving.hi[axis], moving.hi[axis] + moving.translation[axis]);
            intersects = intersects && std::min(hi, (i[axis]+1)*h) > std::max(lo, i[axis]*h);
        }
        valid = valid && !intersects;
    });
    if (!valid)
        error = "pool gate refuses a coarse cell cut by both fixed and moving solids";
    return valid;
}

MovingVolumeState AuthoredParcels() {
    MovingVolumeState state;
    state.water.resize(layout.CellCount());
    for (int axis = 0; axis < 3; ++axis)
        state.velocity[axis].resize(layout.FaceCount(axis));
    constexpr float spacing = .25f, clearance = .3f + spacing * .36f;
    const auto xz = [](int i) { return (i + .5f) * .25f - 1.f; };
    for (int y = 0; y < 10; ++y) {
        for (int z = 0; z < 8; ++z) {
            for (int x = 0; x < 8; ++x) {
                if (std::abs(xz(x)) < clearance && std::abs((y+.5f)*spacing-.7f) < clearance &&
                    std::abs(xz(z)) < clearance)
                    continue;
                for (int dz = 0; dz < 2; ++dz)
                    for (int dy = 0; dy < 2; ++dy)
                        for (int dx = 0; dx < 2; ++dx)
                            state.water[layout.Cell(4+2*x+dx, 4+2*y+dy, 4+2*z+dz)] = cellVolume;
            }
        }
    }
    return state;
}

struct Outcome {
    MovingVolumeStep step;
    MovingGridGeometry geometry;
    std::array<std::vector<GridHalfCellAperture>, 3> open;
    double ms = 0.;
    double workError = 0.;
};

bool Pressure(const GridStaticUnion& fixed, const BoxMotion& moving, const MovingVolumeState& material,
              double signedGravity, Outcome& output, std::string& error) {
    const auto begin = std::chrono::steady_clock::now();
    Outcome next;
    if (!BoundedTopology(fixed, moving, error) ||
        !BuildGridStaticMovingUnion(fixed, {moving}, next.geometry, error))
        return false;
    auto snapshot = moving;
    snapshot.translation = {};
    GridCellPhase phase;
    if (!ReconstructStaticMovingPhase(fixed, {snapshot}, 1, material.water, phase, error) ||
        !AverageStaticMovingWetApertures(fixed, {moving}, phase, next.open, error))
        return false;
    std::array<std::vector<double>, 3> leftWet, rightWet;
    GridGeometricMomentumStart ownedStart;
    ownedStart.predictor = material;
    HalfDualVolumeState owned;
    for (int axis = 0; axis < 3; ++axis) {
        const auto count = layout.FaceCount(axis);
        const auto& apertures = next.geometry.averageAperture[axis];
        leftWet[axis].resize(count);
        rightWet[axis].resize(count);
        owned.velocity[axis].resize(2 * layout.CellCount());
        auto& mass = ownedStart.faceMass[axis];
        auto& participation = ownedStart.faceParticipation[axis];
        mass.resize(count);
        participation.resize(count);
        if (!BuildStaticMovingHalfStorage(fixed, {snapshot}, phase, axis, ownedStart.storage[axis], error))
            return false;
        grid_phase_detail::VisitGrid(layout, axis, [&](const Index& i) {
            const auto face = layout.Face(axis, i[0], i[1], i[2]);
            leftWet[axis][face] = next.open[axis][face].leftWet;
            rightWet[axis][face] = next.open[axis][face].rightWet;
            if (i[axis] == 0 || i[axis] == layout.cells[axis] || apertures[face] == 0.)
                return;
            auto neighbor = i;
            --neighbor[axis];
            const auto lo = layout.Cell(neighbor[0], neighbor[1], neighbor[2]);
            const auto hi = layout.Cell(i[0], i[1], i[2]);
            const auto& store = ownedStart.storage[axis];
            const double left = store.capacity[lo][1] + 999.*store.water[lo][1];
            const double right = store.capacity[hi][0] + 999.*store.water[hi][0];
            participation[face] = apertures[face] / std::max({apertures[face],
                next.geometry.startAperture[axis][face], next.geometry.endAperture[axis][face]});
            mass[face] = (left + right) * participation[face];
            // The zero initial speed is shared by all owned halves in the
            // original seed and in the separate filled control.
        });
    }
    const PlanarPressureReference reference{1, height, signedGravity, leftWet, rightWet, &phase.waterPlane};
    bool retry = false;
    if (!AdvanceVolume(layout, next.geometry, ownedStart.predictor, dt, {}, &reference, PressureMetric::OwnedDual,
                       next.step, error, &retry, 0., std::nullopt, &ownedStart)) {
        error = "projection: " + error + (retry ? " [subdivision eligible]" : "");
        return false;
    }
    HalfDualPressureUpdate applied;
    if (!ApplyGridGeometricPressure(layout, next.geometry, ownedStart, next.step, owned, dt, applied, error)) {
        error = "pressure mapping: " + error;
        return false;
    }
    const auto loads = AccumulateGridPressureReaction(next.geometry.averagePatches, next.step.pressure);
    if (loads.size() != 1 || loads.front().owner != moving.owner) {
        error = "pressure reaction lost moving owner";
        return false;
    }
    next.workError = next.step.pressureFluidWork;
    for (int axis = 0; axis < 3; ++axis)
        next.workError += loads.front().force[axis] * moving.translation[axis];
    next.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-begin).count();
    output = std::move(next);
    error.clear();
    return true;
}

double Total(const std::vector<double>& volumes) {
    return double(std::accumulate(volumes.begin(), volumes.end(), 0.L));
}

void Validate(Checks& checks, const Outcome& outcome, double before, bool sealed, bool demandFlow) {
    const auto& step = outcome.step;
    const double change = Total(step.state.water) - before;
    checks.Expect(std::abs(change) < 1e-11 && step.residual < 1e-12 && std::abs(outcome.workError) < 1e-9,
                  "qualified pressure step conserves water, cell volumes and wall work", change);
    bool valid = true;
    double wetMagnitude = 0.;
    for (int z : {3,21}) {
        double mixture = 0., wet = 0., absoluteMixture = 0., absoluteWet = 0., open = 0.;
        for (int x = 4; x < 20; ++x) {
            const auto face = layout.Face(2, x, 4, z);
            const double q = step.flux[2][face], water = step.waterFlux[2][face];
            const auto& support = outcome.open[2][face];
            open += support.open;
            mixture += (z == 3 ? -q : q);
            wet += (z == 3 ? -water : water);
            absoluteMixture += std::abs(q);
            absoluteWet += std::abs(water);
            valid = valid && (water == 0. || ((q > 0. ? support.leftWet : support.rightWet) > 0.));
        }
        wetMagnitude += absoluteWet;
        std::printf("SEAM %s z=%g area=%.17g outward_mixture=%.17g outward_water=%.17g l1_mixture=%.17g l1_water=%.17g\n",
            sealed ? "sealed" : "authored", z*h-1.5, open, mixture, wet, absoluteMixture, absoluteWet);
        if (sealed)
            valid = valid && open == 0. && absoluteMixture == 0. && absoluteWet == 0.;
    }
    checks.Expect(valid && (!demandFlow || sealed || wetMagnitude > 1e-15),
                  "seam water crosses only genuinely supported open faces", wetMagnitude);
    std::printf("PRESSURE iterations=%d residual=%.17g work_error=%.17g phase_roundoff=%.17g cfl=%.17g time_ms=%.6f\n",
        step.iterations, step.residual, outcome.workError, step.waterRoundoffCorrection, step.maxCfl, outcome.ms);
}

void Run(Checks& checks, bool sealed, bool filledControl) {
    GridStaticUnion walls;
    std::string error;
    const bool built = BuildGridStaticUnion(layout, Walls(sealed), walls, error);
    checks.Expect(built, built ? "build exact fixed-wall partition" : error.c_str());
    if (!built)
        return;
    auto material = AuthoredParcels();
    auto body = Body();
    if (filledControl) {
        MovingGridGeometry geometry;
        if (!BuildGridStaticMovingUnion(walls, {body}, geometry, error)) {
            checks.Expect(false, error.c_str());
            return;
        }
        material.water = geometry.startCapacity;
    } else {
        checks.Expect(Total(material.water) == 9.25, "exact original authored parcel inventory", Total(material.water));
    }
    for (bool moving : {false,true}) {
        if (filledControl && !moving)
            continue;
        body.translation[1] = moving ? .25 * dt : 0.;
        std::printf("CASE %s %s %s dt=%.17g water=%.17g dy=%.17g\n", sealed ? "sealed" : "authored",
            filledControl ? "uniform-wet control" : "original seed", moving ? "prescribed motion" : "stationary geometry",
            dt, Total(material.water), body.translation[1]);
        Outcome outcome;
        const bool accepted = Pressure(walls, body, material, filledControl ? 0. : gravity, outcome, error);
        checks.Expect(accepted, accepted ? "prototype projects the authored wall geometry" : error.c_str());
        if (accepted)
            Validate(checks, outcome, Total(material.water), sealed, filledControl);
    }
    if (!sealed && !filledControl) {
        auto nearWall = Body();
        nearWall.hi[0] = 2.5 + 1e-8;
        nearWall.lo[0] = nearWall.hi[0] - 2.*double(.3f);
        MovingGridGeometry permissibleGeometry;
        const bool disjoint = BuildGridStaticMovingUnion(walls, {nearWall}, permissibleGeometry, error);
        Outcome outcome;
        outcome.ms = 42.;
        checks.Expect(disjoint && !Pressure(walls, nearWall, material, gravity, outcome, error) &&
                      error.find("both fixed and moving") != std::string::npos && outcome.ms == 42.,
                      "unqualified mixed-cell topology rejects even for physically disjoint solids");
    }
}

} // namespace

int main() {
    Checks checks;
    Run(checks, false, false);
    Run(checks, true, false);
    std::printf("SUMMARY authored pressure handoff %d checks %d failures\n", checks.total, checks.failed);
    return checks.failed ? 1 : 0;
}
