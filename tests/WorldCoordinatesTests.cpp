#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <glm/gtc/quaternion.hpp>

#include "BoxVolume.h"
#include "CelestialGravity.h"
#include "DynamicBody.h"
#include "FaithfulGravity.h"
#include "FlyingPrimitiveControl.h"
#include "GravityContextMap.h"
#include "InteractionSystem.h"
#include "ObjectManipulation.h"
#include "PlayerController.h"
#include "RadicalGravity.h"
#include "ReferenceFrame.h"
#include "ShadowTransforms.h"
#include "SphericalVolume.h"
#include "Window.h"
#include "WorldCoordinates.h"

namespace {
constexpr float kDt = 1.0f / 60.0f;
const glm::dvec3 kFarOrigin(1.0e9, -2.0e9, 3.0e9);
int failures = 0;

void Check(bool condition, const char* label) {
    std::printf("  %s %s\n", condition ? "OK  " : "FAIL", label);
    if (!condition) ++failures;
}

bool Near(glm::vec3 a, glm::vec3 b, float tolerance) {
    return glm::length(a - b) <= tolerance;
}

bool Finite(glm::vec3 v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

// The source position is an ABSOLUTE double coordinate. This conversion
// deliberately crosses the real global/local boundary before every scenario
// is handed to the existing float simulation.
glm::vec3 Placed(const WorldCoordinates& coordinates, glm::vec3 authoredLocal) {
    return coordinates.ToLocal(coordinates.Origin() + glm::dvec3(authoredLocal));
}

double RunSlowBody(const WorldCoordinates& coordinates) {
    PhysicsWorld physics;
    physics.Init();
    const glm::vec3 start = Placed(coordinates, glm::vec3(2.0f, -3.0f, 4.0f));
    const BodyHandle body = physics.CreateDynamicSphere(start, 0.2f, 1.0f, 0.0f, 0.0f);
    physics.SetLinearVelocity(body, glm::vec3(0.04f, 0.0f, 0.0f));
    for (int step = 0; step < 60; ++step) physics.Step(kDt);
    const double displacement = coordinates.ToGlobal(physics.GetTransform(body).position).x -
                                coordinates.ToGlobal(start).x;
    physics.Shutdown();
    return displacement;
}

glm::vec3 SampleContext(const WorldCoordinates& coordinates, glm::vec3 localShift,
                        glm::vec3 sampleFromCenter) {
    const glm::vec3 center = Placed(coordinates, localShift + glm::vec3(3, -2, 5));
    RadicalGravity radial(center, 9.81f);
    FaithfulGravity uniform;
    const SphericalVolume sphere(center, 5.0f);
    const BoxVolume box(center + glm::vec3(0, 6, 0), glm::vec3(2, 1, 2));
    GravityContextMap contexts;
    contexts.AddRegion(uniform, box);
    contexts.AddRegion(radial, sphere);
    return contexts.Sample(center + sampleFromCenter);
}

struct BodyResult {
    glm::vec3 position;
    glm::vec3 velocity;
    float contactDistance = 0.0f;
};

BodyResult RunBody(const WorldCoordinates& coordinates, const glm::quat& rotation,
                   glm::vec3 localShift = glm::vec3(0.0f)) {
    PhysicsWorld physics;
    Check(physics.Init(), "physics initializes");
    const glm::vec3 center = Placed(coordinates, localShift +
                                     rotation * glm::vec3(6.0f, -4.0f, 9.0f));
    const glm::vec3 direction = rotation * glm::vec3(0.0f, 1.0f, 0.0f);
    physics.CreateStaticSphere(center, 2.0f, 0.8f, 0.0f);
    const BodyHandle body = physics.CreateDynamicSphere(center + direction * 3.0f,
                                                        0.5f, 2.0f, 0.4f, 0.0f);
    const glm::vec3 acceleration = rotation * glm::vec3(0.0f, -9.81f, 0.0f);
    for (int step = 0; step < 120; ++step) {
        physics.ApplyLinearAcceleration(body, acceleration, kDt);
        physics.Step(kDt);
    }
    const BodyResult result{physics.GetTransform(body).position,
                            physics.GetLinearVelocity(body),
                            glm::length(physics.GetTransform(body).position - center)};
    physics.Shutdown();
    return result;
}

struct OrbitResult {
    glm::vec3 a;
    glm::vec3 b;
    glm::vec3 ship;
    glm::vec3 shipVelocity;
    glm::vec3 shipAngularVelocity;
    float minSeparation = 1.0e9f;
    float maxSeparation = 0.0f;
};

OrbitResult RunOrbit(const WorldCoordinates& coordinates, const glm::quat& rotation,
                     glm::vec3 localShift = glm::vec3(0.0f)) {
    constexpr float mass = 1.0e14f;
    constexpr float separation = 30.0f;
    const float relativeSpeed = std::sqrt(CelestialGravity::kGravitationalConstant *
                                           (2.0f * mass) / separation);
    const glm::vec3 center = Placed(coordinates, localShift +
                                     rotation * glm::vec3(4.0f, 11.0f, -7.0f));
    const glm::vec3 axis = rotation * glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 tangent = rotation * glm::vec3(0.0f, 0.0f, 1.0f);
    PhysicsWorld physics;
    Check(physics.Init(), "orbit physics initializes");
    const BodyHandle a = physics.CreateDynamicSphere(center - axis * 15.0f, 1.0f, mass, 0.0f, 0.0f);
    const BodyHandle b = physics.CreateDynamicSphere(center + axis * 15.0f, 1.0f, mass, 0.0f, 0.0f);
    const BodyHandle ship = physics.CreateDynamicBox(center + rotation * glm::vec3(0.0f, 0.0f, 90.0f),
                                                     glm::vec3(0.4f), 80.0f, 0.0f, 0.0f);
    physics.SetLinearVelocity(a, -tangent * relativeSpeed * 0.5f);
    physics.SetLinearVelocity(b, tangent * relativeSpeed * 0.5f);
    physics.SetLinearVelocity(ship, rotation * glm::vec3(
        -std::sqrt(CelestialGravity::kGravitationalConstant * (2.0f * mass) / 90.0f), 0.0f, 0.0f));
    physics.SetAngularVelocity(ship, rotation * glm::vec3(0.4f, -0.3f, 0.2f));
    CelestialGravity gravity({a, b, ship});
    FlyingPrimitiveControl control;
    control.handle = ship;
    Window input;
    input.SetTestInputMode(true);
    OrbitResult result;
    for (int step = 0; step < 900; ++step) {
        gravity.ApplyForces(physics);
        if (step == 180) SetSpacecraftSasEnabled(control, true, physics);
        ApplyFlyingPrimitiveControl(control, input, physics);
        if (step >= 200 && step < 260)
            physics.ApplyForce(ship, rotation * glm::vec3(0.0f, 0.0f, 120.0f));
        physics.Step(kDt);
        const float distance = glm::length(physics.GetTransform(b).position -
                                            physics.GetTransform(a).position);
        result.minSeparation = std::min(result.minSeparation, distance);
        result.maxSeparation = std::max(result.maxSeparation, distance);
    }
    result.a = physics.GetTransform(a).position - center;
    result.b = physics.GetTransform(b).position - center;
    result.ship = physics.GetTransform(ship).position - center;
    result.shipVelocity = physics.GetLinearVelocity(ship);
    result.shipAngularVelocity = physics.GetAngularVelocity(ship);
    physics.Shutdown();
    return result;
}

struct PlayerResult {
    glm::vec3 position;
    glm::vec3 firstEye;
    glm::vec3 thirdEye;
    glm::vec3 torchPosition;
    glm::vec3 torchDirection;
    bool grounded = false;
    int airborneSteps = 0;
};

PlayerResult RunPlayer(const WorldCoordinates& coordinates,
                       glm::vec3 localShift = glm::vec3(0.0f)) {
    PhysicsWorld physics;
    Check(physics.Init(), "player physics initializes");
    const glm::vec3 center = Placed(coordinates, localShift);
    physics.CreateStaticSphere(center, 20.0f, 0.8f, 0.0f);
    PlayerController player(Placed(coordinates, localShift + glm::vec3(0.0f, 20.92f, 0.0f)), 180.0f);
    Check(player.Spawn(physics), "player shape initializes");
    RadicalGravity gravity(center, 9.81f);
    Window input;
    input.SetTestInputMode(true);
    for (int step = 0; step < 240; ++step) player.FixedUpdate(input, physics, gravity, kDt);
    PlayerResult result;
    input.SetTestActionState(Action::MoveForward, true);
    for (int step = 0; step < 360; ++step) {
        if (step == 160) {
            input.RequestTestJump();
            player.UpdateFrameInput(input);
        }
        player.FixedUpdate(input, physics, gravity, kDt);
        if (!player.IsGrounded()) ++result.airborneSteps;
    }
    result.position = player.GetPosition();
    result.firstEye = glm::vec3(glm::inverse(player.GetViewMatrix(0.5f,
                                            PlayerViewMode::FirstPerson))[3]);
    result.thirdEye = glm::vec3(glm::inverse(player.GetViewMatrix(0.5f,
                                            PlayerViewMode::ThirdPerson))[3]);
    player.GetTorchTransform(0.5f, result.torchPosition, result.torchDirection);
    result.grounded = player.IsGrounded();
    player.Destroy(physics);
    physics.Shutdown();
    return result;
}

struct InteractionResult {
    glm::vec3 bodyPosition;
    glm::vec3 bodyVelocity;
    glm::vec3 carryTarget;
    glm::vec3 framePoint;
    glm::vec3 framePointVelocity;
};

InteractionResult TestInteractionAndFrames(const WorldCoordinates& coordinates) {
    PhysicsWorld physics;
    Check(physics.Init(), "interaction physics initializes");
    const glm::vec3 player = Placed(coordinates, glm::vec3(3.0f, -2.0f, 5.0f));
    const glm::vec3 objectPosition = player + glm::vec3(0.0f, 0.0f, -2.0f);
    const BodyHandle handle = physics.CreateDynamicSphere(objectPosition, 0.4f, 3.0f, 0.5f, 0.0f);
    DynamicBody::Visual visual;
    visual.shape = DynamicBody::Shape::Sphere;
    visual.radius = 0.4f;
    DynamicBody object(handle, visual, objectPosition, glm::quat(1, 0, 0, 0));
    ObjectManipulation manipulation({handle});
    PickupInteractable pickup(object, manipulation, physics);
    Check(SelectInteractable(player, glm::vec3(0, 0, -1), {&pickup}) == &pickup,
          "translated object can be targeted at ordinary range");
    pickup.Interact();
    Check(manipulation.IsHolding(), "translated object can be picked up");
    const glm::quat orientation = glm::angleAxis(0.6f, glm::normalize(glm::vec3(1, 2, -3)));
    const glm::vec3 look = orientation * glm::vec3(0, 0, -1);
    const glm::vec3 target = ComputeCarryTarget(player, orientation, look, 0.7f, 2.0f);
    for (int step = 0; step < 40; ++step) {
        manipulation.ApplyCarryForce(physics, target, glm::vec3(0.0f));
        physics.Step(kDt);
    }
    const glm::vec3 beforeThrow = physics.GetLinearVelocity(handle);
    Check(manipulation.Throw(physics, look, 7.0f), "translated held object can be thrown");
    Check(Near(physics.GetLinearVelocity(handle) - beforeThrow, glm::normalize(look) * 7.0f,
               1.0e-5f), "throw impulse stays local and direction correct");

    ReferenceFrame frame{player, orientation, glm::vec3(1.0f, -2.0f, 3.0f),
                         glm::vec3(0.2f, -0.1f, 0.4f)};
    const glm::vec3 frameLocal(2.0f, -0.4f, 1.5f);
    const glm::vec3 worldPoint = PositionToWorld(frame, frameLocal);
    Check(Near(PositionFromWorld(frame, worldPoint), frameLocal, 1.0e-5f),
          "translated moving reference frame round trips");
    Check(Near(VelocityFromWorld(frame, worldPoint,
                                VelocityToWorld(frame, frameLocal, glm::vec3(0.5f, 0.2f, -0.1f))),
               glm::vec3(0.5f, 0.2f, -0.1f), 1.0e-5f),
          "translated rotating-frame velocity round trips");
    Check(glm::length(coordinates.ToGlobal(worldPoint) - coordinates.Origin() -
                      glm::dvec3(worldPoint)) < 1.0e-6,
          "reference-frame point has correct absolute placement");
    const InteractionResult result{physics.GetTransform(handle).position,
                                   physics.GetLinearVelocity(handle), target,
                                   worldPoint, FramePointVelocity(frame, worldPoint)};
    physics.Shutdown();
    return result;
}
}  // namespace

int main() {
    const WorldCoordinates near;
    const WorldCoordinates far(kFarOrigin);
    const glm::vec3 centimetres(0.01f, -0.02f, 0.03f);
    std::printf("centimetre round-trip error: %.9g m\n",
                glm::length(far.ToLocal(far.ToGlobal(centimetres)) - centimetres));
    Check(Near(far.ToLocal(far.ToGlobal(centimetres)), centimetres, 3.0e-7f),
          "centimetre displacement survives a billion-metre world offset");
    Check(glm::vec3(kFarOrigin + glm::dvec3(centimetres)) == glm::vec3(kFarOrigin),
          "direct float conversion is diagnosed");
    const double nearSlowDistance = RunSlowBody(near);
    const double farSlowDistance = RunSlowBody(far);
    std::printf("slow-body displacement near/far: %.9g / %.9g m (expected 0.04)\n",
                nearSlowDistance, farSlowDistance);
    Check(std::abs(nearSlowDistance - 0.04) < 1.0e-5 &&
          std::abs(farSlowDistance - nearSlowDistance) < 1.0e-6,
          "4 cm of authoritative motion survives billion-metre translation");

    const glm::quat identity(1, 0, 0, 0);
    const glm::quat rotation = glm::angleAxis(0.83f, glm::normalize(glm::vec3(1, -2, 3)));
    const glm::vec3 smallShift(32.0f, -48.0f, 16.0f);
    const BodyResult localBody = RunBody(near, identity);
    const BodyResult smallShiftBody = RunBody(near, identity, smallShift);
    const BodyResult farBody = RunBody(far, identity);
    Check(Near(smallShiftBody.position - smallShift, localBody.position, 0.003f) &&
          Near(smallShiftBody.velocity, localBody.velocity, 0.003f),
          "raw float rigid-body physics has no special zero at small translation");
    Check(Near(localBody.position, farBody.position, 1.0e-4f) &&
          Near(localBody.velocity, farBody.velocity, 1.0e-4f) &&
          std::abs(localBody.contactDistance - farBody.contactDistance) < 1.0e-4f &&
          localBody.contactDistance >= 2.49f,
          "rigid-body integration and contact match after global translation");
    const BodyResult rotatedBody = RunBody(far, rotation);
    Check(Near(glm::inverse(rotation) * rotatedBody.position, localBody.position, 0.03f) &&
          Near(glm::inverse(rotation) * rotatedBody.velocity, localBody.velocity, 0.03f),
          "rigid-body result matches after combined rotation and translation");

    RadicalGravity nearGravity(Placed(near, glm::vec3(3, -4, 5)), 9.81f);
    RadicalGravity farGravity(Placed(far, glm::vec3(3, -4, 5)), 9.81f);
    Check(Near(nearGravity.Sample(Placed(near, glm::vec3(7, 2, 9))),
               farGravity.Sample(Placed(far, glm::vec3(7, 2, 9))), 1.0e-5f),
          "local radial gravity matches after large translation");
    for (const glm::vec3 sample : {glm::vec3(1, 2, 1), glm::vec3(0, 6, 0),
                                   glm::vec3(5, 0, 0), glm::vec3(5.01f, 0, 0),
                                   glm::vec3(30, 0, 0)}) {
        Check(Near(SampleContext(near, glm::vec3(0), sample),
                   SampleContext(near, smallShift, sample), 1.0e-5f) &&
              Near(SampleContext(near, glm::vec3(0), sample),
                   SampleContext(far, glm::vec3(0), sample), 1.0e-5f),
              "gravity context and boundary classification survive translation");
    }

    const OrbitResult localOrbit = RunOrbit(near, identity);
    const OrbitResult smallShiftOrbit = RunOrbit(near, identity, smallShift);
    const OrbitResult farOrbit = RunOrbit(far, identity);
    Check(Near(smallShiftOrbit.a, localOrbit.a, 0.01f) &&
          Near(smallShiftOrbit.b, localOrbit.b, 0.01f) &&
          Near(smallShiftOrbit.ship, localOrbit.ship, 0.03f),
          "raw float orbit and spacecraft remain equivalent at small translation");
    Check(Near(localOrbit.a, farOrbit.a, 0.01f) && Near(localOrbit.b, farOrbit.b, 0.01f) &&
          Near(localOrbit.ship, farOrbit.ship, 0.01f) &&
          Near(localOrbit.shipVelocity, farOrbit.shipVelocity, 0.01f) &&
          Near(localOrbit.shipAngularVelocity, farOrbit.shipAngularVelocity, 1.0e-4f),
          "two-body orbit, thrusting spacecraft and SAS match after large translation");
    Check(localOrbit.minSeparation > 29.0f && localOrbit.maxSeparation < 31.0f &&
          Finite(localOrbit.ship) && Finite(localOrbit.shipVelocity) &&
          glm::length(localOrbit.shipAngularVelocity) < 0.01f,
          "orbit remains bounded, spacecraft state finite, and SAS settles");
    const OrbitResult rotatedOrbit = RunOrbit(far, rotation);
    std::printf("rotated orbit differences A/B/ship: %.6f / %.6f / %.6f m\n",
                glm::length(glm::inverse(rotation) * rotatedOrbit.a - localOrbit.a),
                glm::length(glm::inverse(rotation) * rotatedOrbit.b - localOrbit.b),
                glm::length(glm::inverse(rotation) * rotatedOrbit.ship - localOrbit.ship));
    Check(Near(glm::inverse(rotation) * rotatedOrbit.a, localOrbit.a, 0.07f) &&
          Near(glm::inverse(rotation) * rotatedOrbit.b, localOrbit.b, 0.07f) &&
          Near(glm::inverse(rotation) * rotatedOrbit.ship, localOrbit.ship, 0.2f),
          "orbit and spacecraft match after combined rotation and translation");

    const PlayerResult localPlayer = RunPlayer(near);
    const PlayerResult smallShiftPlayer = RunPlayer(near, smallShift);
    const PlayerResult farPlayer = RunPlayer(far);
    Check(Near(smallShiftPlayer.position - smallShift, localPlayer.position, 0.02f) &&
          smallShiftPlayer.grounded == localPlayer.grounded,
          "raw float player locomotion has no special zero at small translation");
    Check(localPlayer.grounded && farPlayer.grounded &&
          localPlayer.airborneSteps > 0 &&
          localPlayer.airborneSteps == farPlayer.airborneSteps &&
          glm::length(localPlayer.position - glm::vec3(0, 20.92f, 0)) > 2.0f,
          "player walks, jumps and lands on the curved support in both placements");
    Check(Near(localPlayer.position, farPlayer.position, 1.0e-4f) &&
          Near(localPlayer.firstEye, farPlayer.firstEye, 1.0e-4f) &&
          Near(localPlayer.thirdEye, farPlayer.thirdEye, 1.0e-4f) &&
          Near(localPlayer.torchPosition, farPlayer.torchPosition, 1.0e-4f) &&
          Near(localPlayer.torchDirection, farPlayer.torchDirection, 1.0e-5f),
          "player, both cameras, and torch match after large translation");
    Check(glm::length(far.ToGlobal(farPlayer.position) - near.ToGlobal(localPlayer.position) -
                      kFarOrigin) < 1.0e-5,
          "absolute player results differ by exactly the universe translation");

    const InteractionResult localInteraction = TestInteractionAndFrames(near);
    const InteractionResult farInteraction = TestInteractionAndFrames(far);
    Check(Near(localInteraction.bodyPosition, farInteraction.bodyPosition, 1.0e-5f) &&
          Near(localInteraction.bodyVelocity, farInteraction.bodyVelocity, 1.0e-5f) &&
          Near(localInteraction.carryTarget, farInteraction.carryTarget, 1.0e-5f) &&
          Near(localInteraction.framePoint, farInteraction.framePoint, 1.0e-5f) &&
          Near(localInteraction.framePointVelocity, farInteraction.framePointVelocity, 1.0e-5f),
          "interaction, carried object, throw, and reference frame match after translation");
    const glm::mat4 nearShadow = ComputeDirectionalShadowMatrix(localPlayer.position,
                                      glm::vec3(1, 2, 3), 35.0f, 50.0f);
    const glm::mat4 farShadow = ComputeDirectionalShadowMatrix(farPlayer.position,
                                      glm::vec3(1, 2, 3), 35.0f, 50.0f);
    Check(std::abs(nearShadow[3][0] - farShadow[3][0]) < 1.0e-5f,
          "shadow camera uses precise local focus after global translation");

    std::printf("M23 world-coordinate failures: %d\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
