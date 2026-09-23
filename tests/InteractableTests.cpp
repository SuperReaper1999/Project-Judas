// Milestone 16: standalone, headless tests for the interaction system —
// src/HingeTransform.*, src/InteractionSystem.*, src/Door.*,
// src/LightSwitch.* — and the interact key's input-ownership gating
// (mirroring tests/LightingTests.cpp's own Section F pattern). Door needs
// a real PhysicsWorld (no window/GL — see tests/StepClimbTests.cpp's own
// header comment, PhysicsWorld has never needed either); LightSwitch and
// the pure SelectInteractable/HingeTransform math need neither. Actual
// on-screen appearance (does the door visibly swing, does the prompt
// render legibly) is human-validated, per this milestone's own brief.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Door.h"
#include "HingeTransform.h"
#include "Interactable.h"
#include "InteractionSystem.h"
#include "LightSwitch.h"
#include "PauseMenu.h"
#include "PhysicsWorld.h"

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

void CheckNear(float a, float b, float tolerance, const char* description) {
    Check(std::abs(a - b) <= tolerance, description);
}

void CheckVec3Near(const glm::vec3& a, const glm::vec3& b, float tolerance, const char* description) {
    Check(glm::length(a - b) <= tolerance, description);
}

const glm::quat kArbitraryRotation =
    glm::angleAxis(glm::radians(53.0f), glm::normalize(glm::vec3(0.4f, 0.8f, -0.3f)));

// A minimal hand-crafted Interactable for exercising SelectInteractable in
// isolation, without needing a real Door/LightSwitch/PhysicsWorld — pure
// data, same "test double" shape every other standalone suite in this
// project already uses for its own dependencies.
class MockInteractable : public Interactable {
public:
    MockInteractable(glm::vec3 point, float radius, std::string name)
        : m_point(point), m_radius(radius), m_name(std::move(name)) {}

    glm::vec3 GetInteractionPoint() const override { return m_point; }
    float GetInteractionRadius() const override { return m_radius; }
    std::string GetPromptText() const override { return m_name; }
    bool CanInteract() const override { return m_canInteract; }
    void Interact() override { ++m_interactCount; }

    int InteractCount() const { return m_interactCount; }
    void SetCanInteract(bool value) { m_canInteract = value; }

private:
    glm::vec3 m_point;
    float m_radius;
    std::string m_name;
    bool m_canInteract = true;
    int m_interactCount = 0;
};

}  // namespace

// --- Section A: hinge transform math ---
void TestHingeTransformAtZeroAngle() {
    std::printf("Section A: ComputeHingeTransform at zero angle\n");

    const glm::vec3 baseCenter(2.0f, 0.0f, 0.0f);
    const glm::quat baseOrientation(1.0f, 0.0f, 0.0f, 0.0f);
    const glm::vec3 pivot(0.0f, 0.0f, 0.0f);

    glm::vec3 position;
    glm::quat orientation;
    ComputeHingeTransform(baseCenter, baseOrientation, pivot, glm::vec3(0.0f, 1.0f, 0.0f), 0.0f, position,
                          orientation);

    CheckVec3Near(position, baseCenter, 1.0e-5f, "zero angle reproduces the closed-pose center exactly");
    CheckNear(glm::abs(glm::dot(orientation, baseOrientation)), 1.0f, 1.0e-5f,
              "zero angle reproduces the closed-pose orientation exactly");
}

void TestHingeTransformAtNinetyDegrees() {
    std::printf("Section A: ComputeHingeTransform at a 90 degree swing\n");

    const glm::vec3 baseCenter(2.0f, 0.0f, 0.0f);  // 2m from the pivot along +X
    const glm::quat baseOrientation(1.0f, 0.0f, 0.0f, 0.0f);
    const glm::vec3 pivot(0.0f, 0.0f, 0.0f);
    const glm::vec3 hingeAxis(0.0f, 1.0f, 0.0f);  // world +Y in this test, for a simple hand check

    glm::vec3 position;
    glm::quat orientation;
    // A +90 degree rotation about +Y takes +X to -Z (right-handed rotation).
    ComputeHingeTransform(baseCenter, baseOrientation, pivot, hingeAxis, glm::radians(90.0f), position,
                          orientation);

    CheckVec3Near(position, glm::vec3(0.0f, 0.0f, -2.0f), 1.0e-3f,
                  "a 90 degree swing moves the center from +X to -Z around the pivot, preserving distance");
}

void TestHingeTransformRotateTheUniverse() {
    std::printf("Section A: ComputeHingeTransform rotate-the-universe invariance\n");

    const glm::vec3 baseCenter(1.5f, 0.2f, -0.5f);
    const glm::quat baseOrientation = glm::angleAxis(glm::radians(15.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec3 pivot(0.0f, 0.0f, 0.0f);
    const glm::vec3 hingeAxis = baseOrientation * glm::vec3(0.0f, 1.0f, 0.0f);
    constexpr float kAngle = 0.9f;  // radians

    glm::vec3 referencePosition;
    glm::quat referenceOrientation;
    ComputeHingeTransform(baseCenter, baseOrientation, pivot, hingeAxis, kAngle, referencePosition,
                          referenceOrientation);

    const glm::vec3 rotatedBaseCenter = kArbitraryRotation * baseCenter;
    const glm::quat rotatedBaseOrientation = kArbitraryRotation * baseOrientation;
    const glm::vec3 rotatedPivot = kArbitraryRotation * pivot;
    const glm::vec3 rotatedHingeAxis = kArbitraryRotation * hingeAxis;

    glm::vec3 rotatedPosition;
    glm::quat rotatedOrientation;
    ComputeHingeTransform(rotatedBaseCenter, rotatedBaseOrientation, rotatedPivot, rotatedHingeAxis, kAngle,
                          rotatedPosition, rotatedOrientation);

    CheckVec3Near(rotatedPosition, kArbitraryRotation * referencePosition, 1.0e-3f,
                  "rotating the whole scenario rotates the swung position identically — the door works "
                  "relative to its own authored transform, never a fixed world axis");
    CheckNear(glm::abs(glm::dot(rotatedOrientation, kArbitraryRotation * referenceOrientation)), 1.0f,
              1.0e-3f, "rotating the whole scenario rotates the swung orientation identically");
}

// --- Section B: target selection ---
void TestSelectInteractableRangeAndFacing() {
    std::printf("Section B: SelectInteractable range and facing\n");

    MockInteractable inRangeFacing(glm::vec3(0.0f, 0.0f, -2.0f), 3.0f, "near, ahead");
    MockInteractable outOfRange(glm::vec3(0.0f, 0.0f, -50.0f), 3.0f, "far away");
    MockInteractable behindPlayer(glm::vec3(0.0f, 0.0f, 2.0f), 3.0f, "behind");

    const std::vector<Interactable*> candidates = {&inRangeFacing, &outOfRange, &behindPlayer};
    const glm::vec3 playerPosition(0.0f);
    const glm::vec3 lookDirection(0.0f, 0.0f, -1.0f);

    Interactable* selected = SelectInteractable(playerPosition, lookDirection, candidates);
    Check(selected == &inRangeFacing, "selects the one candidate that is both in range and roughly faced");
}

void TestSelectInteractableClosestWins() {
    std::printf("Section B: SelectInteractable picks the CLOSEST qualifying candidate\n");

    MockInteractable near_(glm::vec3(0.0f, 0.0f, -1.0f), 5.0f, "near");
    MockInteractable far_(glm::vec3(0.0f, 0.0f, -3.0f), 5.0f, "far");
    const std::vector<Interactable*> candidates = {&far_, &near_};  // deliberately listed far-first

    Interactable* selected =
        SelectInteractable(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), candidates);
    Check(selected == &near_, "the closer of two qualifying candidates wins, regardless of list order");
}

void TestSelectInteractableNoneQualify() {
    std::printf("Section B: SelectInteractable returns nullptr when nothing qualifies\n");

    MockInteractable tooFar(glm::vec3(0.0f, 0.0f, -100.0f), 2.0f, "too far");
    const std::vector<Interactable*> candidates = {&tooFar};

    Interactable* selected =
        SelectInteractable(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), candidates);
    Check(selected == nullptr, "nothing selected when every candidate is out of range — this is the "
                                "entire 'stop offering interaction' mechanism, no separate state to clear");

    Check(SelectInteractable(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), {}) == nullptr,
          "an empty candidate list selects nothing without crashing");
}

// --- Section D/E: the door ---
void TestDoorOpenCloseAnimationAndCollision() {
    std::printf("Section D/E: door open/close animation and physics collision coherence\n");

    PhysicsWorld physics;
    Check(physics.Init(), "PhysicsWorld initializes for the door test");

    const glm::vec3 hingePosition(5.0f, 0.0f, 0.0f);
    const glm::quat baseOrientation(1.0f, 0.0f, 0.0f, 0.0f);
    const glm::vec3 halfExtents(1.0f, 1.0f, 0.1f);
    const glm::vec3 localHingeAxis(0.0f, 1.0f, 0.0f);
    constexpr float kOpenAngle = glm::radians(90.0f);
    constexpr float kAngularSpeed = glm::radians(90.0f);  // exactly 1 second to fully open
    constexpr float kFixedDeltaTime = 1.0f / 60.0f;

    Door door(physics, hingePosition, baseOrientation, halfExtents, localHingeAxis, kOpenAngle,
              kAngularSpeed, glm::vec3(0.5f));

    Check(!door.IsOpen(), "door starts closed");
    CheckNear(door.GetCurrentAngleRadians(), 0.0f, 1.0e-5f, "door starts at zero swing angle");
    Check(door.GetPromptText() == "Press G to open door", "closed-door prompt reads 'open'");

    door.Interact();
    Check(door.IsOpen(), "Interact() toggles the door open");
    Check(door.GetPromptText() == "Press G to close door", "open-door prompt reads 'close'");

    // Advance exactly enough fixed steps to fully open (60 steps at 1/60s
    // each, given kAngularSpeed = 90 deg/s and kOpenAngle = 90 deg).
    for (int i = 0; i < 60; ++i) {
        door.FixedUpdate(physics, kFixedDeltaTime);
    }
    CheckNear(door.GetCurrentAngleRadians(), kOpenAngle, 1.0e-3f,
              "after enough fixed steps, the door reaches exactly its open angle (clamped, not "
              "overshooting)");

    // Physics collision coherence: the STATIC body's own live transform
    // must match what Door itself thinks its current pose is — this is
    // the actual mechanism that keeps a moving door's collision correct
    // (see docs/ARCHITECTURE.md, "Milestone 16, Door").
    const BodyTransform physicsTransform = physics.GetTransform(BodyHandle{0});
    glm::vec3 expectedPosition;
    glm::quat expectedOrientation;
    ComputeHingeTransform(hingePosition + baseOrientation * glm::vec3(halfExtents.x, 0.0f, 0.0f),
                          baseOrientation, hingePosition, localHingeAxis, kOpenAngle, expectedPosition,
                          expectedOrientation);
    CheckVec3Near(physicsTransform.position, expectedPosition, 1.0e-3f,
                  "the door's physics body position matches its own computed open pose");
    CheckNear(glm::abs(glm::dot(physicsTransform.rotation, expectedOrientation)), 1.0f, 1.0e-3f,
              "the door's physics body orientation matches its own computed open pose");

    door.Interact();
    Check(!door.IsOpen(), "interacting again closes the door");
    for (int i = 0; i < 60; ++i) {
        door.FixedUpdate(physics, kFixedDeltaTime);
    }
    CheckNear(door.GetCurrentAngleRadians(), 0.0f, 1.0e-3f, "the door swings fully closed again");

    door.Destroy(physics);
    physics.Shutdown();
}

void TestDoorInteractionPointMovesWithSwing() {
    std::printf("Section D: door interaction point follows its own swing\n");

    PhysicsWorld physics;
    physics.Init();
    Door door(physics, glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.1f),
              glm::vec3(0.0f, 1.0f, 0.0f), glm::radians(90.0f), glm::radians(90.0f), glm::vec3(0.5f));

    const glm::vec3 closedPoint = door.GetInteractionPoint();
    door.Interact();
    for (int i = 0; i < 30; ++i) door.FixedUpdate(physics, 1.0f / 60.0f);  // half-open
    const glm::vec3 midSwingPoint = door.GetInteractionPoint();

    Check(glm::length(midSwingPoint - closedPoint) > 0.1f,
          "the door's interaction point moves as it swings open, not fixed to its closed pose");

    door.Destroy(physics);
    physics.Shutdown();
}

// --- Section F: the second interactable (light switch) ---
void TestLightSwitchUsesSameInteractionPath() {
    std::printf("Section F: light switch uses the exact same Interactable path as the door\n");

    LightSwitch lightSwitch(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(0.05f, 0.15f, 0.03f),
                             glm::vec3(0.0f, 0.0f, 1.0f), glm::radians(40.0f), glm::radians(200.0f),
                             glm::vec3(0.7f), glm::vec3(2.0f, 1.0f, 0.0f), glm::vec3(3.0f, 2.5f, 1.5f), 10.0f);

    Check(!lightSwitch.IsLampOn(), "lamp starts off");
    Check(lightSwitch.GetPromptText() == "Press G to turn on light", "off-switch prompt reads 'turn on'");

    // The exact same SelectInteractable path the door test above uses —
    // proof this isn't a parallel/secret DoorManager mechanism.
    const std::vector<Interactable*> candidates = {&lightSwitch};
    Interactable* selected = SelectInteractable(glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                                                 candidates);
    Check(selected == &lightSwitch, "the light switch is selectable through the generic interaction path");
    selected->Interact();

    Check(lightSwitch.IsLampOn(), "Interact(), called generically through the Interactable interface, "
                                    "toggles the lamp on");
    Check(lightSwitch.GetPromptText() == "Press G to turn off light", "on-switch prompt reads 'turn off'");

    for (int i = 0; i < 20; ++i) lightSwitch.FixedUpdate(1.0f / 60.0f);
    CheckNear(lightSwitch.GetCurrentAngleRadians(), glm::radians(40.0f), 1.0e-2f,
              "the lever visibly swings toward its toggled angle over several fixed steps");
}

// --- Section G: interact-key input ownership ---
//
// Mirrors tests/LightingTests.cpp's own Section F exactly — the same
// drain-always/act-when-allowed boundary Application::Run's real loop
// uses for the interact key (see docs/ARCHITECTURE.md, "Milestone 16,
// Input ownership").
void TestInteractInputOwnership() {
    std::printf("Section G: interact key obeys the same input-ownership boundary as the torch\n");

    PauseMenu menu;
    MockInteractable target(glm::vec3(0.0f), 5.0f, "target");

    auto handleInteractRequest = [&](bool requested) {
        if (requested && !menu.IsOpen() && target.CanInteract()) target.Interact();
    };

    handleInteractRequest(true);
    Check(target.InteractCount() == 1, "interact fires normally during ordinary gameplay");

    menu.HandleBackRequest();  // open the pause menu
    handleInteractRequest(true);
    Check(target.InteractCount() == 1, "an interact request while the menu owns input does not trigger "
                                         "the target");

    menu.HandleBackRequest();  // close (resume)
    Check(target.InteractCount() == 1, "closing the menu alone does not trigger a stale interact");

    handleInteractRequest(true);
    Check(target.InteractCount() == 2, "gameplay control of interaction is fully restored after resume");
}

int main() {
    TestHingeTransformAtZeroAngle();
    TestHingeTransformAtNinetyDegrees();
    TestHingeTransformRotateTheUniverse();
    TestSelectInteractableRangeAndFacing();
    TestSelectInteractableClosestWins();
    TestSelectInteractableNoneQualify();
    TestDoorOpenCloseAnimationAndCollision();
    TestDoorInteractionPointMovesWithSwing();
    TestLightSwitchUsesSameInteractionPath();
    TestInteractInputOwnership();

    if (g_failures == 0) {
        std::printf("\nALL TESTS PASSED\n");
        return 0;
    }
    std::printf("\n%d TEST(S) FAILED\n", g_failures);
    return 1;
}
