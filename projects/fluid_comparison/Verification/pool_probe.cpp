#include "SceneSerialization.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "GameSession.h"
#include "Window.h"
#include "Simulation.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

int main() {
    Scene scene;
    std::string error;
    if (!LoadSceneFromFile("projects/fluid_comparison/Scenes/rectangular_pool.judas", scene, error))
        throw std::runtime_error(error);
    for (auto& object : scene.Objects()) { object.scripts.clear(); object.ui.reset(); }
    ProjectSettings settings;
    settings.advancedFluidDynamics = true;
    RuntimeWorld world;
    world.ConfigureProject(settings);
    GameSession session;
    Window window;
    window.SetTestInputMode(true);
    if (!world.Build(scene, nullptr, error) || !session.Begin(world, error))
        throw std::runtime_error(error);
    auto mass = [&]() { double sum = 0; for (const auto& p : world.Fluid().Particles()) sum += p.mass; return sum; };
    const auto count = world.Fluid().Particles().size();
    const auto initialMass = mass();
    bool finite = true;
    float maxImmersion = 0;
    auto tick = [&](int frames) {
        for (int i = 0; i < frames; ++i) {
            session.HandleFrameInput(window, false, false, false, false, false);
            StepPlayedWorld(session, window, 1.f / 60);
            maxImmersion = std::max(maxImmersion, session.Player().GetFluidSample().immersion);
            for (const auto& p : world.Fluid().Particles())
                for (int axis = 0; axis < 3; ++axis)
                    finite &= std::isfinite(p.position[axis]) && std::isfinite(p.velocity[axis]);
        }
        int escapes = 0;
        for (const auto& p : world.Fluid().Particles())
            escapes += std::abs(p.position.x) > 6.25f || std::abs(p.position.z) > 4.25f || p.position.y < 0;
        std::cout << "phase_end frames=" << frames << " outside=" << escapes << '\n';
    };
    tick(60);
    const float dry = session.Player().GetFluidSample().immersion;
    window.SetTestActionState(Action::MoveForward, true);
    tick(120);
    window.SetTestActionState(Action::MoveForward, false);
    window.RequestTestJump();
    tick(45);
    const auto entered = session.Player().GetPosition();
    window.SetTestActionState(Action::MoveBackward, true);
    tick(150);
    window.SetTestActionState(Action::MoveBackward, false);
    tick(30);
    const auto exited = session.Player().GetPosition();
    const auto exitImmersion = session.Player().GetFluidSample().immersion;
    const bool exitGrounded = session.Player().IsGrounded();
    for (auto& body : world.DynamicBodies()) {
        ObjectManipulation hand({body.Handle()});
        if (!hand.TryPickUp(body.Handle(), world.Physics())) throw std::runtime_error("prop pickup failed");
        hand.Throw(world.Physics(), glm::normalize(glm::vec3(0, .4f, -1)), 8);
    }
    tick(240);
    int outside = 0;
    for (const auto& p : world.Fluid().Particles())
        if (std::abs(p.position.x) > 6.25f || std::abs(p.position.z) > 4.25f || p.position.y < 0) {
            ++outside;
            std::cout << "outside_particle " << p.position.x << ' ' << p.position.y << ' ' << p.position.z << '\n';
        }
    for (const auto& body : world.DynamicBodies()) {
        const auto pose = world.Physics().GetTransform(body.Handle());
        std::cout << "prop " << pose.position.x << ' ' << pose.position.y << ' ' << pose.position.z << '\n';
    }
    std::cout << "count=" << count << " final_count=" << world.Fluid().Particles().size()
              << " initial_mass=" << initialMass << " final_mass=" << mass()
              << " finite=" << finite << " outside=" << outside
              << " initial_dry=" << dry << " maximum_immersion=" << maxImmersion
              << " entered_z=" << entered.z << " exit=" << exited.x << ',' << exited.y << ',' << exited.z
              << " exit_immersion=" << exitImmersion << " exit_grounded=" << exitGrounded << '\n';
    return !(count == 1440 && world.Fluid().Particles().size() == count && mass() == initialMass && finite &&
             dry == 0 && maxImmersion > .5f && entered.z < 0 && exited.z > 4.5f && exited.z < 7.5f &&
             exitImmersion == 0 && exitGrounded && outside == 0);
}
