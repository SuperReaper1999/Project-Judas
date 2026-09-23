// Focused geometry tests for releasing a secured pilot to the gravity-facing
// exterior of the spacecraft. These use PhysicsWorld's actual shape support
// distances and the same pure dismount calculation used by PilotControl.
#include <cmath>
#include <cstdio>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include "PhysicsWorld.h"
#include "PilotAttachment.h"

namespace {
int g_failures = 0;

void Check(bool condition, const char* description) {
    if (condition) {
        std::printf("  OK   %s\n", description);
    } else {
        std::printf("  FAIL %s\n", description);
        ++g_failures;
    }
}

void CheckNear(float actual, float expected, float tolerance, const char* description) {
    Check(std::abs(actual - expected) <= tolerance, description);
}

void CheckVecNear(const glm::vec3& actual, const glm::vec3& expected, float tolerance,
                  const char* description) {
    Check(glm::length(actual - expected) <= tolerance, description);
}

const glm::quat kSceneRotation = glm::angleAxis(
    glm::radians(57.0f), glm::normalize(glm::vec3(0.3f, -0.6f, 0.7f)));

}  // namespace

int main() {
    PhysicsWorld physics;
    physics.Init();
    physics.CreatePlayerShape(0.3f, 0.6f);

    const glm::vec3 halfExtents(2.0f, 0.25f, 3.0f);
    const BodyHandle ship = physics.CreateDynamicBox(
        glm::vec3(0.0f), halfExtents, 80.0f, 0.8f, 0.1f);
    constexpr float kSkinMargin = 0.02f;
    constexpr float kPlayerMaximumExtent = 0.9f;

    std::printf("Section A: upright dismount leaves a player already above the hull in place\n");
    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    const glm::vec3 down(0.0f, -9.81f, 0.0f);
    const glm::vec3 shipPosition(3.0f, 10.0f, -4.0f);
    physics.ResetBody(ship, shipPosition, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    const BodyTransform uprightShip{shipPosition, glm::quat(1.0f, 0.0f, 0.0f, 0.0f)};
    const glm::vec3 uprightPlayer = shipPosition + up * (0.25f + kPlayerMaximumExtent + kSkinMargin);
    CheckNear(physics.GetBodySupportDistance(ship, up), 0.25f, 1.0e-5f,
              "box support distance uses its current local half-height");
    CheckNear(physics.GetPlayerShapeMaxSupportDistance(), kPlayerMaximumExtent, 1.0e-5f,
              "player clearance uses capsule radius plus half-segment");
    CheckVecNear(ComputePilotDismountPosition(
                     uprightShip, uprightPlayer, down, physics.GetBodySupportDistance(ship, up),
                     physics.GetPlayerShapeMaxSupportDistance(), kSkinMargin),
                 uprightPlayer, 1.0e-5f, "normal top-side release does not move the player");

    std::printf("Section B: release after rolling clears the hull along local gravity-up\n");
    const glm::quat invertedRotation = glm::angleAxis(glm::pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
    physics.ResetBody(ship, shipPosition, invertedRotation);
    const BodyTransform invertedShip{shipPosition, invertedRotation};
    const glm::vec3 attachedUnderneath = shipPosition + invertedRotation *
        (up * (0.25f + kPlayerMaximumExtent + kSkinMargin));
    const glm::vec3 released = ComputePilotDismountPosition(
        invertedShip, attachedUnderneath, down,
        physics.GetBodySupportDistance(ship, up), physics.GetPlayerShapeMaxSupportDistance(),
        kSkinMargin);
    Check(glm::dot(attachedUnderneath - shipPosition, up) < 0.0f,
          "the secured pilot is underneath the rolled ship before release");
    CheckNear(glm::dot(released - shipPosition, up), 1.17f, 1.0e-4f,
              "release places the capsule just clear on the gravity-facing side");
    CheckVecNear(released - attachedUnderneath, up * 2.34f, 1.0e-4f,
                 "only the positional clearance correction is applied");

    std::printf("Section C: zero gravity does not invent a preferred egress direction\n");
    CheckVecNear(ComputePilotDismountPosition(
                     invertedShip, attachedUnderneath, glm::vec3(0.0f),
                     physics.GetBodySupportDistance(ship, up),
                     physics.GetPlayerShapeMaxSupportDistance(), kSkinMargin),
                 attachedUnderneath, 1.0e-5f, "zero-gravity release preserves the captured position");

    std::printf("Section D: rotated-world dismount equivalence\n");
    const glm::quat tiltedShipRotation = kSceneRotation * invertedRotation;
    const glm::vec3 arbitraryUp = kSceneRotation * up;
    const glm::vec3 arbitraryGravity = kSceneRotation * down;
    const glm::vec3 arbitraryShipPosition = kSceneRotation * shipPosition;
    physics.ResetBody(ship, arbitraryShipPosition, tiltedShipRotation);
    const BodyTransform tiltedShip{arbitraryShipPosition, tiltedShipRotation};
    const glm::vec3 tiltedPilot = arbitraryShipPosition + tiltedShipRotation *
        (up * (0.25f + kPlayerMaximumExtent + kSkinMargin));
    const glm::vec3 tiltedReleased = ComputePilotDismountPosition(
        tiltedShip, tiltedPilot, arbitraryGravity,
        physics.GetBodySupportDistance(ship, arbitraryUp),
        physics.GetPlayerShapeMaxSupportDistance(), kSkinMargin);
    CheckVecNear(glm::conjugate(kSceneRotation) * (tiltedReleased - arbitraryShipPosition),
                 released - shipPosition, 1.0e-4f,
                 "rotating ship, gravity, and position rotates the egress result rigidly");

    physics.Shutdown();
    if (g_failures == 0) {
        std::printf("All pilot dismount tests passed.\n");
        return 0;
    }
    std::printf("%d pilot dismount test(s) failed.\n", g_failures);
    return 1;
}
