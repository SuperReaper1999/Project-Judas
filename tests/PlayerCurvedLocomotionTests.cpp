#include <cmath>
#include <cstdio>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include "PlayerController.h"
#include "PilotControl.h"
#include "RadicalGravity.h"
#include "Window.h"

namespace {
int failures = 0;
void Check(bool ok, const char* label) {
    std::printf("  %s %s\n", ok ? "OK  " : "FAIL", label);
    if (!ok) ++failures;
}

class UniformGravity final : public GravityField {
public:
    explicit UniformGravity(glm::vec3 down) : m_down(glm::normalize(down)) {}
    glm::vec3 Sample(const glm::vec3&) const override { return m_down * 9.81f; }
private:
    glm::vec3 m_down;
};

class ConstantGravity final : public GravityField {
public:
    explicit ConstantGravity(glm::vec3 acceleration) : m_acceleration(acceleration) {}
    glm::vec3 Sample(const glm::vec3&) const override { return m_acceleration; }

private:
    glm::vec3 m_acceleration;
};

float SphereClearance(glm::vec3 center, glm::vec3 position, glm::quat orientation) {
    const glm::vec3 a = position + orientation * glm::vec3(0.0f, -0.6f, 0.0f);
    const glm::vec3 b = position + orientation * glm::vec3(0.0f, 0.6f, 0.0f);
    const glm::vec3 ab = b - a;
    const float t = glm::clamp(glm::dot(center - a, ab) / glm::dot(ab, ab), 0.0f, 1.0f);
    return glm::length(a + t * ab - center) - 20.3f;
}

struct WalkMetrics {
    glm::vec3 position{0.0f};
    glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};
    float minClearance = 1.0e9f;
    float maxClearance = -1.0e9f;
    float maxOrientationStep = 0.0f;
    float maxUpTrackingError = 0.0f;
    int nearZeroOrientationSteps = 0;
    float maxFirstPersonCameraStep = 0.0f;
    float maxThirdPersonCameraStep = 0.0f;
    int airborneSteps = 0;
    int changedDirectionSteps = 0;
};

WalkMetrics WalkSphere(const glm::quat& universeRotation, const char* name) {
    constexpr float dt = 1.0f / 60.0f;
    const glm::vec3 center(0.0f);
    const glm::vec3 spawn = universeRotation * glm::vec3(0.0f, 20.92f, 0.0f);
    PhysicsWorld physics;
    physics.Init();
    physics.CreateStaticSphere(center, 20.0f, 0.8f, 0.0f);
    // PlayerController's body frame initializes upright, then aligns to
    // sampled gravity. Compensate its free-look yaw for the twist between
    // that shortest alignment and the universe rotation so the initial
    // look/movement basis is rotated along with the complete scenario.
    const glm::quat alignedFrame = glm::rotation(glm::vec3(0, 1, 0),
                                                 universeRotation * glm::vec3(0, 1, 0));
    const glm::vec3 localForward = glm::inverse(alignedFrame) *
                                   (universeRotation * glm::vec3(0, 0, -1));
    const float yaw = glm::degrees(std::atan2(-localForward.x, -localForward.z));
    PlayerController player(spawn, yaw);
    player.Spawn(physics);
    Window input;
    input.SetTestInputMode(true);
    RadicalGravity gravity(center, 9.81f);

    // Let arbitrary initial orientation settle against the actual radial field.
    for (int i = 0; i < 360; ++i) player.FixedUpdate(input, physics, gravity, dt);

    input.SetTestActionState(Action::MoveForward, true);
    WalkMetrics result;
    glm::quat previous = player.GetOrientation();
    glm::vec3 previousFirstCamera(0.0f), previousThirdCamera(0.0f);
    bool havePreviousCamera = false;
    for (int i = 0; i < 1800; ++i) {
        if (i == 700) {
            input.SetTestActionState(Action::MoveForward, false);
            input.SetTestActionState(Action::StrafeRight, true);
        }
        if (i == 1200) {
            input.SetTestActionState(Action::StrafeRight, false);
            input.SetTestActionState(Action::MoveForward, true);
        }
        player.FixedUpdate(input, physics, gravity, dt);
        const glm::vec3 position = player.GetPosition();
        const glm::quat orientation = player.GetOrientation();
        const float clearance = SphereClearance(center, position, orientation);
        result.minClearance = std::min(result.minClearance, clearance);
        result.maxClearance = std::max(result.maxClearance, clearance);
        const float orientationStep = glm::degrees(
            glm::angle(glm::normalize(orientation * glm::inverse(previous))));
        result.maxOrientationStep = std::max(result.maxOrientationStep, orientationStep);
        if (orientationStep < 0.01f) ++result.nearZeroOrientationSteps;
        const glm::vec3 actualUp = orientation * glm::vec3(0.0f, 1.0f, 0.0f);
        const glm::vec3 radialUp = glm::normalize(position - center);
        result.maxUpTrackingError = std::max(result.maxUpTrackingError,
            glm::degrees(std::acos(glm::clamp(glm::dot(actualUp, radialUp), -1.0f, 1.0f))));
        previous = orientation;
        const glm::vec3 firstCamera = glm::vec3(glm::inverse(
            player.GetViewMatrix(0.5f, PlayerViewMode::FirstPerson))[3]);
        const glm::vec3 thirdCamera = glm::vec3(glm::inverse(
            player.GetViewMatrix(0.5f, PlayerViewMode::ThirdPerson))[3]);
        if (havePreviousCamera) {
            result.maxFirstPersonCameraStep = std::max(result.maxFirstPersonCameraStep,
                glm::length(firstCamera - previousFirstCamera));
            result.maxThirdPersonCameraStep = std::max(result.maxThirdPersonCameraStep,
                glm::length(thirdCamera - previousThirdCamera));
        }
        previousFirstCamera = firstCamera;
        previousThirdCamera = thirdCamera;
        havePreviousCamera = true;
        if (!player.IsGrounded()) ++result.airborneSteps;
        if (i > 0 && (i == 700 || i == 1200)) ++result.changedDirectionSteps;

        if (i == 1799) {
            result.position = position;
            result.orientation = orientation;
        }
        if (i == 1799) {
            const glm::mat4 first = player.GetViewMatrix(0.5f, PlayerViewMode::FirstPerson);
            const glm::mat4 third = player.GetViewMatrix(0.5f, PlayerViewMode::ThirdPerson);
            Check(std::isfinite(first[0][0]) && std::isfinite(third[0][0]),
                  "first- and third-person presentation transforms remain finite on the sphere");
            glm::vec3 torchPosition, torchDirection;
            player.GetTorchTransform(0.5f, torchPosition, torchDirection);
            Check(glm::length(torchPosition - (player.GetPresentedPosition(0.5f) +
                  player.GetPresentedOrientation(0.5f) * glm::vec3(0.0f, 0.7f, 0.0f))) < 1.0e-4f,
                  "torch origin follows the presented player-local eye while traversing curvature");
        }
    }

    Check(result.airborneSteps == 0, "continuous curved-surface walk has no grounded-state chatter");
    Check(result.maxClearance - result.minClearance < 0.002f,
          "continuous curved-surface support clearance stays within 2 mm");
    Check(result.minClearance > 0.017f && result.maxClearance < 0.023f,
          "capsule remains near its intended 20 mm skin margin");
    Check(result.maxOrientationStep < 0.35f,
          "local frame incorporates each small curvature change without angular snaps");
    Check(result.nearZeroOrientationSteps < 100,
          "local frame does not hold still for repeated steps while walking");
    Check(result.maxUpTrackingError < 0.4f,
          "player local up stays close to current radial gravity while walking");
    Check(result.maxFirstPersonCameraStep < 0.1f && result.maxThirdPersonCameraStep < 0.1f,
          "first- and third-person presented camera positions advance smoothly");
    Check(result.changedDirectionSteps == 2, "walk includes deliberate direction changes");

    // Stop and require the settled pose to remain stationary.
    input.SetTestActionState(Action::MoveForward, false);
    for (int i = 0; i < 300; ++i) player.FixedUpdate(input, physics, gravity, dt);
    const glm::vec3 stillPosition = player.GetPosition();
    for (int i = 0; i < 120; ++i) player.FixedUpdate(input, physics, gravity, dt);
    Check(glm::length(player.GetPosition() - stillPosition) < 1.0e-4f,
          "standing remains stationary after the long curved traversal");

    input.RequestTestJump();
    player.UpdateFrameInput(input);
    player.FixedUpdate(input, physics, gravity, dt);
    bool wasAirborne = !player.IsGrounded();
    bool landedAgain = false;
    for (int i = 0; i < 180; ++i) {
        player.UpdateFrameInput(input);
        player.FixedUpdate(input, physics, gravity, dt);
        wasAirborne = wasAirborne || !player.IsGrounded();
        if (wasAirborne && player.IsGrounded()) {
            landedAgain = true;
            break;
        }
    }
    Check(wasAirborne && landedAgain, "jump leaves and reacquires curved support");
    if (landedAgain) {
        for (int i = 0; i < 3; ++i) player.FixedUpdate(input, physics, gravity, dt);
        const float landedClearance = SphereClearance(center, player.GetPosition(), player.GetOrientation());
        Check(landedClearance > 0.014f && landedClearance < 0.026f,
              "landing returns to the bounded curved-surface support band");
    }

    std::printf("  [%s] clearance range %.6f..%.6f m, max frame rotation %.4f deg, final r %.5f m\n",
        name, result.minClearance, result.maxClearance, result.maxOrientationStep,
        glm::length(result.position));
    player.Destroy(physics);
    physics.Shutdown();
    return result;
}

void TestRotatedUniverse() {
    std::puts("Curved locomotion under a rotated universe");
    const glm::quat q = glm::angleAxis(glm::radians(67.0f),
        glm::normalize(glm::vec3(1.0f, 2.0f, -3.0f)));
    const WalkMetrics baseline = WalkSphere(glm::quat(1, 0, 0, 0), "sphere");
    const WalkMetrics rotated = WalkSphere(q, "rotated sphere");
    Check(std::abs(baseline.minClearance - rotated.minClearance) < 0.002f &&
          std::abs(baseline.maxClearance - rotated.maxClearance) < 0.002f,
          "rotating the full sphere scenario preserves support-clearance behaviour");
    Check(glm::length(rotated.position - q * baseline.position) < 0.15f,
          "rotating the universe rotates the long-walk endpoint equivalently");
    const glm::vec3 expectedUp = glm::normalize(rotated.position);
    Check(glm::dot(rotated.orientation * glm::vec3(0, 1, 0), expectedUp) > 0.995f,
          "player local up follows radial gravity in the rotated scenario");
}

void TestFlatControl() {
    std::puts("Flat-ground locomotion control");
    constexpr float dt = 1.0f / 60.0f;
    PhysicsWorld physics;
    physics.Init();
    physics.CreateStaticBox(glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(100, 1, 100), 0.8f, 0.0f);
    PlayerController player(glm::vec3(0, 0.92f, 0), 0.0f);
    player.Spawn(physics);
    Window input;
    input.SetTestInputMode(true);
    UniformGravity gravity(glm::vec3(0, -1, 0));
    for (int i = 0; i < 30; ++i) player.FixedUpdate(input, physics, gravity, dt);
    input.SetTestActionState(Action::MoveForward, true);
    float minZ = 1.0e9f, maxZ = -1.0e9f;
    float minClearance = 1.0e9f, maxClearance = -1.0e9f;
    bool alwaysGrounded = true;
    for (int i = 0; i < 600; ++i) {
        player.FixedUpdate(input, physics, gravity, dt);
        minZ = std::min(minZ, player.GetPosition().z);
        maxZ = std::max(maxZ, player.GetPosition().z);
        const float clearance = player.GetPosition().y - 0.9f;
        minClearance = std::min(minClearance, clearance);
        maxClearance = std::max(maxClearance, clearance);
        alwaysGrounded = alwaysGrounded && player.IsGrounded();
    }
    Check(alwaysGrounded, "flat walk remains grounded");
    Check(maxZ - minZ > 20.0f, "flat control traverses normally at walking speed");
    Check(maxClearance - minClearance < 0.002f,
          "flat support clearance is also stable during walking");
    std::printf("  flat clearance range %.6f..%.6f m\n", minClearance, maxClearance);
    player.Destroy(physics);
    physics.Shutdown();
}

void TestAscendingPilotRelease(const glm::quat& universeRotation,
                               const glm::vec3& gravityAcceleration, const char* scenario) {
    std::printf("Ascending spacecraft release (%s)\n", scenario);
    constexpr float dt = 1.0f / 60.0f;
    constexpr float shipSpeed = 11.7f;
    const glm::vec3 localUp(0.0f, 1.0f, 0.0f);
    const glm::vec3 up = universeRotation * localUp;
    const glm::quat shipOrientation = universeRotation;
    const glm::vec3 shipPosition(0.0f);
    const glm::vec3 playerPosition = shipPosition + up * 1.17f;

    PhysicsWorld physics;
    Check(physics.Init(), "pilot-release physics world initializes");
    const BodyHandle ship = physics.CreateDynamicBox(
        shipPosition, glm::vec3(2.0f, 0.25f, 3.0f), 80.0f, 0.8f, 0.1f);
    physics.ResetBody(ship, shipPosition, shipOrientation);
    physics.SetLinearVelocity(ship, up * shipSpeed);

    PlayerController player(playerPosition, 0.0f);
    Check(player.Spawn(physics), "pilot-release player shape initializes");
    player.FixedUpdateAttached(playerPosition, shipOrientation);

    Window input;
    input.SetTestInputMode(true);
    ConstantGravity gravity(gravityAcceleration);
    FlyingPrimitiveControl control;
    control.handle = ship;
    control.controlled = true;
    PilotAttachment attachment;
    attachment.attached = true;

    HandlePilotToggleRequest(control, attachment, player, physics, gravity);
    Check(!control.controlled && !attachment.attached,
          "release returns control to ordinary player movement");

    // The live loop advances PhysicsWorld before PlayerController. This
    // reproduces an ascending craft moving into the player's old attached
    // pose before the player consumes its inherited point velocity.
    physics.ApplyLinearAcceleration(ship, gravity.Sample(shipPosition), dt);
    physics.Step(dt);
    AdvancePlayerForPiloting(control, attachment, player, physics, input, gravity, dt);
    const float firstStepGap = glm::dot(
        player.GetPosition() - physics.GetTransform(ship).position, up);
    Check(std::abs(firstStepGap - 1.17f) < 0.004f,
          "first detached step preserves clearance while the ascending hull advances first");
    Check(glm::dot(player.GetVelocity(), up) > shipSpeed - 0.5f,
          "inherited upward point velocity survives release and gravity");
    Check(player.IsGrounded(), "release reacquires the ascending spacecraft as physical support");
    Check(player.GetSupportBodyHandle().id == ship.id,
          "release support is the spacecraft the pilot left");

    input.SetTestActionState(Action::MoveForward, true);
    float minimumGap = firstStepGap;
    float maximumGap = firstStepGap;
    for (int step = 0; step < 120; ++step) {
        if (step == 12) input.SetTestActionState(Action::MoveForward, false);
        physics.ApplyLinearAcceleration(ship, gravity.Sample(physics.GetTransform(ship).position), dt);
        physics.Step(dt);
        AdvancePlayerForPiloting(control, attachment, player, physics, input, gravity, dt);
        const float gap = glm::dot(
            player.GetPosition() - physics.GetTransform(ship).position, up);
        minimumGap = std::min(minimumGap, gap);
        maximumGap = std::max(maximumGap, gap);
    }
    Check(maximumGap - minimumGap < 0.002f,
          "support transform carry preserves clearance during sustained coasting");
    std::printf("  release clearance %.6f..%.6f m, final grounded=%d, final gap=%.6f m\n",
                minimumGap, maximumGap, player.IsGrounded() ? 1 : 0,
                glm::dot(player.GetPosition() - physics.GetTransform(ship).position, up));
    Check(player.IsGrounded(), "player remains grounded while walking on the rising spacecraft");
    const glm::vec3 straightUpCoast = playerPosition + up * shipSpeed * dt * 121.0f;
    const glm::vec3 tangentialMotion = player.GetPosition() - straightUpCoast;
    Check(glm::length(tangentialMotion - up * glm::dot(tangentialMotion, up)) > 0.5f,
          "player movement remains available while coasting with the ascending spacecraft");

    player.Destroy(physics);
    physics.Shutdown();
}
}  // namespace

int main() {
    TestFlatControl();
    TestRotatedUniverse();
    const glm::quat universeRotation(1.0f, 0.0f, 0.0f, 0.0f);
    const glm::vec3 gravityAcceleration(0.0f, -9.81f, 0.0f);
    TestAscendingPilotRelease(universeRotation, gravityAcceleration, "world frame");
    const glm::quat rotatedUniverse = glm::angleAxis(glm::radians(67.0f),
        glm::normalize(glm::vec3(1.0f, 2.0f, -3.0f)));
    TestAscendingPilotRelease(rotatedUniverse, rotatedUniverse * gravityAcceleration,
                              "rotated world frame");
    if (failures) std::printf("\n%d test(s) failed\n", failures);
    else std::puts("\nALL TESTS PASSED");
    return failures == 0 ? 0 : 1;
}
