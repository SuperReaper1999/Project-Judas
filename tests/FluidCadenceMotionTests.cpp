// A constant-speed body must not accelerate a particle beyond its actual wall
// speed just because a fluid interval contains two rigid-motion segments.
#include "GameSession.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "Simulation.h"
#include "Window.h"
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool value, const std::string& message) {
    ++checks;
    failures += !value;
    std::printf("%s %s\n", value ? "PASS" : "FAIL", message.c_str());
}

void Run(float fluidHz, const glm::quat& rotation, bool compound) {
    const std::string label = std::to_string(int(fluidHz)) + "Hz/" +
        (rotation == glm::quat(1, 0, 0, 0) ? "identity/" : "rotated/") +
        (compound ? "compound" : "box");
    Scene scene;
    scene.Settings().fluidUpdateRateHz = fluidHz;
    scene.Settings().fluidHydrostaticDragRate = 0;
    scene.Settings().fluidScale = 1;
    auto& gravity = scene.CreateObject("Zero gravity");
    gravity.gravity = SceneGravityComponent{};
    gravity.gravity->magnitude = 0;
    gravity.gravity->regionHalfExtents = glm::vec3(100);
    gravity.transform.rotation = rotation;
    auto& observer = scene.CreateObject("Distant observer");
    observer.playerStart = ScenePlayerStartComponent{};
    observer.transform.position = {50, 50, 50};
    auto& body = scene.CreateObject("Constant-speed physical wall");
    const auto bodyId = body.id;
    body.body = SceneBodyComponent{};
    body.body->motion = SceneBodyMotion::Dynamic;
    body.body->halfExtents = {.1f, .1f, .1f};
    body.body->mass = 1000;
    body.body->friction = 0;
    body.body->restitution = 0;
    body.body->initialLinearVelocity = rotation * glm::vec3(1, 0, 0);
    body.transform.rotation = rotation;
    if (compound) {
        body.body->shape = SceneShape::Compound;
        body.body->compoundBoxes = {{{0, .25f, 0}, {.1f, .1f, .1f}},
                                    {{0, -.25f, 0}, {.1f, .1f, .1f}}};
    }
    auto& water = scene.CreateObject("Single physical particle");
    water.fluidVolume = SceneFluidVolumeComponent{};
    water.fluidVolume->spacing = .1f;
    water.fluidVolume->countX = water.fluidVolume->countY = water.fluidVolume->countZ = 1;
    water.transform.position = rotation * glm::vec3(.16f, compound ? .25f : 0, 0);
    water.transform.rotation = rotation;

    RuntimeWorld world;
    ProjectSettings settings;
    settings.advancedFluidDynamics = true;
    world.ConfigureProject(settings);
    GameSession session;
    Window window;
    window.SetTestInputMode(true);
    std::string error;
    const bool built = world.Build(scene, nullptr, error) && session.Begin(world, error);
    Check(built, label + " ordinary runtime constructs: " + error);
    if (!built) return;
    const auto handle = world.DynamicBodies()[world.FindEntity(bodyId)->slot].Handle();
    const auto direction = rotation * glm::vec3(1, 0, 0);
    float maximumSpeed = 0, maximumWallError = 0;
    bool finite = true;
    for (int step = 0; step < 24; ++step) {
        session.HandleFrameInput(window, false, false, false, false, false);
        StepPlayedWorld(session, window, 1.f / 60);
        const auto velocity = world.Fluid().Particles().at(0).velocity;
        maximumSpeed = std::max(maximumSpeed, glm::length(velocity));
        maximumWallError = std::max(maximumWallError,
            glm::length(world.Physics().GetLinearVelocity(handle) - direction));
        for (int axis = 0; axis < 3; ++axis)
            finite &= std::isfinite(velocity[axis]);
    }
    const auto particle = world.Fluid().Particles().at(0);
    const auto localVelocity = glm::inverse(rotation) * particle.velocity;
    std::printf("MEASURE %s max_particle_speed=%.9g final_local_velocity=(%.9g,%.9g,%.9g) wall_error=%.9g\n",
        label.c_str(), maximumSpeed, localVelocity.x, localVelocity.y, localVelocity.z, maximumWallError);
    Check(world.Fluid().Particles().size() == 1 && particle.mass == 1.f,
          label + " particle count and mass unchanged");
    Check(finite, label + " finite velocity");
    Check(maximumWallError < 1e-5f, label + " rigid body retains prescribed physical speed");
    Check(maximumSpeed <= 1.001f, label + " particle contact never exceeds actual constant wall speed");
    Check(std::abs(localVelocity.x - 1) < .001f &&
          std::abs(localVelocity.y) < .001f && std::abs(localVelocity.z) < .001f,
          label + " positive contact transfers the actual wall-normal velocity");
}
} // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    const auto rotated = glm::angleAxis(.73f, glm::normalize(glm::vec3(1, 2, -3)));
    for (float rate : {60.f, 30.f})
        for (const auto& rotation : {glm::quat(1, 0, 0, 0), rotated})
            for (bool compound : {false, true}) Run(rate, rotation, compound);
    std::printf("SUMMARY %u checks %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
