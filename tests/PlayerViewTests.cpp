#include <cmath>
#include <cstdio>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "LightTransforms.h"
#include "PlayerView.h"

namespace {
int failures = 0;

void Check(bool condition, const char* label) {
    std::printf("  %s %s\n", condition ? "OK  " : "FAIL", label);
    if (!condition) ++failures;
}

void CheckVec(const glm::vec3& actual, const glm::vec3& expected, const char* label,
              float tolerance = 1.0e-4f) {
    Check(glm::length(actual - expected) <= tolerance, label);
}

const glm::quat kUniverseRotation =
    glm::angleAxis(glm::radians(71.0f), glm::normalize(glm::vec3(-0.3f, 0.8f, 0.5f)));

void TestEyeTransformAndTorchConsistency() {
    std::puts("First-person eye follows player-local up and look");
    const glm::vec3 position(2.0f, -1.0f, 4.0f);
    const glm::quat orientation =
        glm::angleAxis(glm::radians(83.0f), glm::normalize(glm::vec3(1.0f, 0.4f, -0.2f)));
    constexpr float yaw = 31.0f;
    constexpr float pitch = -17.0f;
    constexpr float eyeHeight = 0.7f;
    const auto camera = ComputePlayerCameraPose(position, orientation, yaw, pitch,
                                                PlayerViewMode::FirstPerson, eyeHeight, 4.0f, 1.0f);
    CheckVec(camera.position, position + orientation * glm::vec3(0.0f, eyeHeight, 0.0f),
             "eye offset is transformed by the arbitrary player orientation");

    glm::vec3 torchPosition, torchDirection;
    ComputeTorchTransform(position, orientation, yaw, pitch, eyeHeight, torchPosition, torchDirection);
    CheckVec(camera.position, torchPosition, "camera eye and existing torch origin coincide");
    CheckVec(camera.front, torchDirection, "camera look and existing torch direction coincide");
}

void TestThirdPersonCompatibilityAndViewOnlySwitching() {
    std::puts("Third-person compatibility and presentation-only switching");
    const glm::vec3 position(-3.0f, 5.0f, 1.0f);
    const glm::quat orientation = glm::angleAxis(0.7f, glm::normalize(glm::vec3(0.2f, 0.9f, 0.3f)));
    constexpr float yaw = -24.0f;
    constexpr float pitch = 9.0f;
    const auto before = ComputePlayerCameraPose(position, orientation, yaw, pitch,
                                                 PlayerViewMode::ThirdPerson, 0.7f, 4.0f, 1.0f);
    const glm::vec3 localUp = orientation * glm::vec3(0.0f, 1.0f, 0.0f);
    CheckVec(before.position, position + localUp * 0.7f - before.front * 4.0f + localUp,
             "third-person camera retains its prior follow and height offsets");

    PlayerViewMode mode = PlayerViewMode::ThirdPerson;
    ApplyPlayerViewToggle(mode, true, true);
    Check(mode == PlayerViewMode::FirstPerson, "gameplay view request selects first person");
    const auto first = ComputePlayerCameraPose(position, orientation, yaw, pitch, mode,
                                                0.7f, 4.0f, 1.0f);
    CheckVec(first.position, position + localUp * 0.7f,
             "first-person camera is at the player eye, without changing its pose input");
    ApplyPlayerViewToggle(mode, true, true);
    Check(mode == PlayerViewMode::ThirdPerson, "second gameplay request restores third person");
    const auto after = ComputePlayerCameraPose(position, orientation, yaw, pitch, mode,
                                                0.7f, 4.0f, 1.0f);
    CheckVec(after.position, before.position,
             "switching back restores the same camera transform from unchanged player inputs");
    CheckVec(after.front, before.front, "switching views does not alter look direction");
}

void TestRotateTheUniverse() {
    std::puts("First-person camera rotate-the-universe equivalence");
    const glm::vec3 position(2.0f, -1.0f, 4.0f);
    const glm::quat orientation = glm::angleAxis(0.4f, glm::normalize(glm::vec3(0.3f, 0.6f, 0.8f)));
    const auto reference = ComputePlayerCameraPose(position, orientation, 28.0f, -11.0f,
                                                    PlayerViewMode::FirstPerson, 0.7f, 4.0f, 1.0f);
    const auto rotated = ComputePlayerCameraPose(kUniverseRotation * position,
        kUniverseRotation * orientation, 28.0f, -11.0f, PlayerViewMode::FirstPerson,
        0.7f, 4.0f, 1.0f);
    CheckVec(rotated.position, kUniverseRotation * reference.position,
             "rotating the universe rotates the eye position identically");
    CheckVec(rotated.front, kUniverseRotation * reference.front,
             "rotating the universe rotates the camera look direction identically");
    CheckVec(rotated.up, kUniverseRotation * reference.up,
             "rotating the universe rotates camera up identically");
}

void TestMenuOwnership() {
    std::puts("View-toggle input ownership");
    PlayerViewMode mode = PlayerViewMode::ThirdPerson;
    ApplyPlayerViewToggle(mode, true, false);
    Check(mode == PlayerViewMode::ThirdPerson,
          "a view-toggle request while a menu owns input has no effect");
    ApplyPlayerViewToggle(mode, false, true);
    Check(mode == PlayerViewMode::ThirdPerson,
          "closing a menu without a fresh request cannot cause a stale toggle");
}
}  // namespace

int main() {
    TestEyeTransformAndTorchConsistency();
    TestThirdPersonCompatibilityAndViewOnlySwitching();
    TestRotateTheUniverse();
    TestMenuOwnership();
    if (failures) std::printf("\n%d test(s) failed\n", failures);
    else std::puts("\nALL TESTS PASSED");
    return failures == 0 ? 0 : 1;
}
