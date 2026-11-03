// Prototype consumer test. The grid supplies observations to the native
// PlayerController on ordinary fixed ticks, outside the engine scene pipeline.
#include "GridWaterQuery.h"
#include "PlanarMovingVolume.h"

#include "GravityField.h"
#include "PlayerController.h"
#include "SimulationTiming.h"
#include "Window.h"

#include <iomanip>
#include <iostream>
#include <numeric>

namespace {

using namespace judas::volume_grid;
constexpr float tick = SimulationTiming::kFixedTimestep;
constexpr int phaseAxis = 1;

struct Checks {
    int count = 0;
    int failures = 0;

    void Check(bool passed, const char* description) {
        ++count;
        if (!passed) {
            ++failures;
            std::cerr << "FAIL " << description << '\n';
        }
    }
};

struct Frame {
    glm::quat rotation{1, 0, 0, 0};
    glm::vec3 offset{};
    glm::vec3 Vector(glm::vec3 v) const { return rotation * v; }
    glm::vec3 Point(glm::vec3 p) const { return offset + Vector(p); }
    glm::vec3 Chart(glm::vec3 p) const { return glm::conjugate(rotation) * (p - offset); }
};

class UniformGravity final : public GravityField {
public:
    explicit UniformGravity(glm::vec3 gravity) : gravity_(gravity) {}
    glm::vec3 Sample(const glm::vec3&) const override { return gravity_; }

private:
    glm::vec3 gravity_;
};

bool FillTank(const GridLayout& layout, double level, MovingVolumeState& water,
              std::string& error) {
    GridPhaseGeometry geometry;
    if (!BuildGridPhaseGeometry(layout, {}, phaseAxis, level, geometry, error))
        return false;
    water.water = std::move(geometry.water);
    for (int axis = 0; axis < 3; ++axis)
        water.velocity[axis].assign(layout.FaceCount(axis), 0.);
    return true;
}

bool AdvanceTank(const GridLayout& layout, MovingVolumeState& water, double gravity,
                 std::string& error) {
    PlanarMovingStep step;
    if (!AdvanceEvolvingVolume(layout, {}, water, tick, phaseAxis, gravity, step, error))
        return false;
    water = std::move(step.fluid.state);
    return true;
}

bool ObservePlayer(const GridLayout& layout, const MovingVolumeState& water, const Frame& frame,
                   const PlayerController& player, PlayerFluidSample& sample, std::string& error) {
    GridCellPhase phase;
    if (!ReconstructGridCellPhase(layout, {}, phaseAxis, water.water, phase, error))
        return false;

    // Same chart-axis AABB extents as the legacy fluid controller probe. This
    // does not replace the native capsule used for actual collision sweeps.
    const auto center = frame.Chart(player.GetPosition());
    const double radius = PlayerController::CapsuleRadius();
    const GridVec half{radius, radius + PlayerController::CapsuleHalfHeight(), radius};
    GridVec lo{}, hi{};
    double total = 1.;
    for (int axis = 0; axis < 3; ++axis) {
        lo[axis] = double(center[axis]) - half[axis];
        hi[axis] = double(center[axis]) + half[axis];
        total *= 2. * half[axis];
    }
    GridWaterSample geometry;
    if (!QueryGridWaterVolume(layout, {}, phaseAxis, phase.waterPlane, lo, hi, geometry, error))
        return false;

    sample = {};
    if (geometry.wetVolume == 0.)
        return true;
    const double ratio = geometry.wetVolume / total;
    if (ratio > 1. + 128. * std::numeric_limits<double>::epsilon()) {
        error = "native player probe observed more water than its AABB volume";
        return false;
    }
    sample.immersion = float(std::min(1., ratio));
    sample.density = 1000.f;

    // A first-order, cell-centred observation of the actual staggered flow
    // near the wet centroid. The prototype does not provide acceleration.
    std::array<int, 3> cell{};
    for (int axis = 0; axis < 3; ++axis)
        cell[axis] = std::clamp(int(std::floor(geometry.centroid[axis] / layout.h)),
                                0, layout.cells[axis] - 1);
    glm::vec3 flow(0.f);
    for (int axis = 0; axis < 3; ++axis) {
        auto right = cell;
        ++right[axis];
        const double a = water.velocity[axis][layout.Face(axis, cell[0], cell[1], cell[2])];
        const double b = water.velocity[axis][layout.Face(axis, right[0], right[1], right[2])];
        flow[axis] = float(.5 * (a + b));
    }
    sample.velocity = frame.Vector(flow);
    return true;
}

void PlayerTick(PlayerController& player, PhysicsWorld& physics, const UniformGravity& gravity,
                Window& window, const PlayerFluidSample& sample, bool jump = false) {
    if (jump)
        window.RequestTestJump();
    player.UpdateFrameInput(window, tick);
    player.SetFluidSample(sample);
    physics.Step(tick);
    player.FixedUpdate(window, physics, gravity, tick);
}

bool NativeReady(PhysicsWorld& physics, PlayerController& player, Window& window,
                 const Frame& frame, glm::vec3 start) {
    window.SetTestInputMode(true);
    if (!physics.Init() || !player.Spawn(physics))
        return false;
    player.FixedUpdateAttached(frame.Point(start), frame.rotation);
    return true;
}

float DensityTank(Checks& checks, const Frame& frame, float density) {
    const GridLayout layout{{4, 7, 4}, .5};
    constexpr double gravityAcceleration = -9.81;
    const glm::vec3 spawn(1.f, 1.35f, 1.f);
    const UniformGravity gravity(frame.Vector({0.f, float(gravityAcceleration), 0.f}));
    PhysicsWorld physics;
    PlayerController player(frame.Point(spawn), 0.f);
    Window window;
    MovingVolumeState water;
    std::string error;
    const bool ready = NativeReady(physics, player, window, frame, spawn) &&
                       FillTank(layout, 2.7, water, error) &&
                       physics.CreateStaticBox(frame.Point({1.f, -.25f, 1.f}), frame.rotation,
                                               {1.f, .25f, 1.f}, .5f, 0.f).IsValid();
    checks.Check(ready, "native density tank is ready");
    if (!ready)
        return 0.f;
    player.SetFluidParameters(density, 2.f, 4.f);
    const auto initialWater = water.water;
    float firstWet = 0.f, peak = 0.f;
    bool advanced = true;
    for (int step = 0; step < 180; ++step) {
        PlayerFluidSample sample;
        if (!AdvanceTank(layout, water, gravityAcceleration, error) ||
            !ObservePlayer(layout, water, frame, player, sample, error)) {
            advanced = false;
            break;
        }
        PlayerTick(player, physics, gravity, window, sample);
        if (step == 0)
            firstWet = player.GetFluidSample().immersion;
        peak = std::max(peak, glm::length(player.GetVelocity()));
    }
    checks.Check(advanced, "gravity tank runs the evolving grid and native fixed player");
    if (!advanced)
        std::cerr << error << '\n';
    checks.Check(firstWet > .999f && peak < 5.f, "density tank starts immersed and stays bounded");
    const float dy = frame.Chart(player.GetPosition()).y - spawn.y;
    const double massChange = std::accumulate(water.water.begin(), water.water.end(), 0.) -
                              std::accumulate(initialWater.begin(), initialWater.end(), 0.);
    checks.Check(std::abs(massChange) < 2e-12, "gravity tank conserves water volume");
    std::cout << "density " << density << " displacement " << dy << " m, first immersion "
              << firstWet << ", peak speed " << peak << " m/s\n";
    player.Destroy(physics);
    return dy;
}

void ZeroGravitySwim(Checks& checks, const Frame& frame) {
    const GridLayout layout{{4, 6, 16}, .5};
    const glm::vec3 spawn(1.f, 1.3f, 6.2f);
    const UniformGravity gravity(glm::vec3(0.f));
    PhysicsWorld physics;
    PlayerController player(frame.Point(spawn), 0.f);
    Window window;
    MovingVolumeState water;
    std::string error;
    const bool ready = NativeReady(physics, player, window, frame, spawn) &&
                       FillTank(layout, 2.6, water, error);
    checks.Check(ready, "native swimming tank is ready");
    if (!ready)
        return;
    player.SetFluidParameters(950.f, 2.f, 4.f);
    for (int k = 0; k < layout.cells[2]; ++k) {
        if (k >= 4 && k < 10)
            continue;
        for (int j = 0; j < layout.cells[1]; ++j)
            for (int i = 0; i < layout.cells[0]; ++i)
                water.water[layout.Cell(i, j, k)] = 0.;
    }
    // A divergence-free circulation crosses the edge of the liquid patch,
    // making the authoritative cell water volumes evolve at zero gravity.
    water.velocity[0][layout.Face(0, 2, 2, 9)] = .05;
    water.velocity[2][layout.Face(2, 2, 2, 10)] = .05;
    water.velocity[0][layout.Face(0, 2, 2, 10)] = -.05;
    water.velocity[2][layout.Face(2, 1, 2, 10)] = -.05;

    const auto initial = water.water;
    MovingVolumeState playerless = water;
    const auto volume = [](const auto& values) {
        return std::accumulate(values.begin(), values.end(), 0.);
    };
    PlayerFluidSample sample;
    bool complete = ObservePlayer(layout, water, frame, player, sample, error);
    const bool startsDry = sample.immersion == 0.f;
    bool entered = false, exited = false, jumped = false, replayMatches = true;
    float jumpImmersion = 0.f, jumpSpeed = 0.f, maximumImmersion = 0.f;
    window.SetTestActionState(Action::MoveForward, true);
    for (int step = 0; step < 270 && complete; ++step) {
        if (!AdvanceTank(layout, water, 0., error) ||
            !AdvanceTank(layout, playerless, 0., error) ||
            !ObservePlayer(layout, water, frame, player, sample, error)) {
            complete = false;
            break;
        }
        const glm::vec3 up = frame.Vector({0.f, 1.f, 0.f});
        const float before = glm::dot(player.GetVelocity(), up);
        const bool jump = !jumped && sample.immersion > .65f;
        PlayerTick(player, physics, gravity, window, sample, jump);
        const float immersion = player.GetFluidSample().immersion;
        maximumImmersion = std::max(maximumImmersion, immersion);
        entered = entered || immersion > .20f;
        if (entered && immersion == 0.f && frame.Chart(player.GetPosition()).z < 2.f)
            exited = true;
        if (jump) {
            jumped = true;
            jumpImmersion = immersion;
            jumpSpeed = glm::dot(player.GetVelocity(), up) - before;
        }
        replayMatches = replayMatches && playerless.water == water.water &&
                        playerless.velocity == water.velocity;
    }
    checks.Check(complete, "zero-gravity game input runs with the evolving grid");
    if (!complete)
        std::cerr << error << '\n';
    checks.Check(startsDry && entered && exited && player.GetFluidSample().immersion == 0.f,
                 "native controller enters and exits the reconstructed liquid");
    checks.Check(jumped && jumpImmersion > .65f && jumpSpeed > 0.f && jumpSpeed <= 4.f * tick + 1e-4f,
                 "native jump input gives bounded upward swim acceleration while wet");
    checks.Check(water.water != initial && replayMatches && std::abs(volume(water.water) - volume(initial)) < 2e-12,
                 "water truly evolves, conserves volume and matches the playerless replay");
    const auto end = frame.Chart(player.GetPosition());
    std::cout << "swim jump immersion " << jumpImmersion << ", upward delta " << jumpSpeed
              << " m/s, maximum immersion " << maximumImmersion << ", end chart z " << end.z
              << " m, volume error " << volume(water.water) - volume(initial) << " m³\n";
    player.Destroy(physics);
}

} // namespace

int main() {
    std::cout << std::setprecision(10);
    Checks checks;
    const Frame aligned;
    const Frame oblique{glm::angleAxis(.73f, glm::normalize(glm::vec3(.4f, .8f, .2f))),
                        {2.f, -1.f, 3.f}};
    const float light = DensityTank(checks, aligned, 500.f);
    const float neutral = DensityTank(checks, aligned, 1000.f);
    const float heavy = DensityTank(checks, aligned, 2000.f);
    const float rotatedLight = DensityTank(checks, oblique, 500.f);
    checks.Check(light > .40f && std::abs(neutral) < .002f && heavy < -.25f,
                 "native buoyancy rises, holds or sinks according to player density");
    checks.Check(std::abs(light - rotatedLight) < .002f,
                 "oblique whole-chart gravity reproduces the rise");
    ZeroGravitySwim(checks, aligned);
    ZeroGravitySwim(checks, oblique);
    std::cout << checks.count << " native grid player checks, " << checks.failures << " failures\n";
    return checks.failures ? 1 : 0;
}
