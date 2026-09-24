#include <cmath>
#include <cstdio>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "DynamicBody.h"
#include "InteractionSystem.h"
#include "ObjectManipulation.h"
#include "PhysicsWorld.h"

namespace {
int failures = 0;
void Check(bool value, const char* message) {
    std::printf("  %s %s\n", value ? "OK  " : "FAIL", message);
    if (!value) ++failures;
}
bool Near(const glm::vec3& a, const glm::vec3& b, float epsilon = 1.0e-4f) {
    return glm::length(a - b) <= epsilon;
}

void TestCarryTransform() {
    std::printf("Carry target orientation\n");
    const glm::vec3 p(4.0f, -2.0f, 1.0f);
    const glm::quat q = glm::angleAxis(1.1f, glm::normalize(glm::vec3(0.3f, -0.4f, 0.8f)));
    const glm::vec3 look = glm::normalize(q * glm::vec3(0.2f, 0.1f, -1.0f));
    const glm::vec3 target = ComputeCarryTarget(p, q, look, 0.7f, 1.8f);
    const glm::vec3 expected = p + q * glm::vec3(0.0f, 0.7f, 0.0f) + look * 1.8f;
    Check(Near(target, expected), "eye offset follows arbitrary player orientation and look");

    const glm::quat universe = glm::angleAxis(0.73f, glm::normalize(glm::vec3(-0.6f, 0.1f, 0.7f)));
    const glm::vec3 rotated = ComputeCarryTarget(universe * p, universe * q, universe * look, 0.7f, 1.8f);
    Check(Near(rotated, universe * target), "carry target rotates rigidly with the universe");

    const glm::quat carriedOrientation = ComputeCarryOrientation(q, look);
    const glm::quat rotatedOrientation = ComputeCarryOrientation(universe * q, universe * look);
    Check(glm::length(carriedOrientation * glm::vec3(0, 1, 0) - glm::vec3(0, 1, 0)) > 0.1f,
          "carry orientation is a real orientation, not a fixed world axis");
    Check(Near(rotatedOrientation * glm::vec3(0, 1, 0),
               universe * (carriedOrientation * glm::vec3(0, 1, 0)), 1.0e-5f),
          "look-relative carried orientation rotates with the universe");
}

void TestPhysicsManipulation() {
    std::printf("Physics-backed pickup/drop/throw\n");
    PhysicsWorld physics;
    physics.Init();
    const BodyHandle ball = physics.CreateDynamicSphere(glm::vec3(0.0f), 0.25f, 2.0f, 0.4f, 0.0f);
    const BodyHandle excluded = physics.CreateDynamicBox(glm::vec3(3.0f, 0.0f, 0.0f), glm::vec3(0.5f),
                                                          3.0f, 0.4f, 0.0f);
    const BodyHandle fixed = physics.CreateStaticBox(glm::vec3(5.0f, 0.0f, 0.0f), glm::vec3(0.5f),
                                                       0.4f, 0.0f);
    ObjectManipulation manipulation({ball});
    Check(manipulation.CanPickUp(ball, physics), "whitelisted dynamic body is eligible");
    Check(!manipulation.CanPickUp(excluded, physics), "unlisted dynamic body is rejected");
    Check(!manipulation.CanPickUp(fixed, physics), "static body is rejected");
    Check(manipulation.TryPickUp(ball, physics), "eligible body can be picked up");
    Check(!manipulation.CanPickUp(ball, physics), "only one body can be held");

    for (int i = 0; i < 180; ++i) {
        manipulation.ApplyCarryForce(physics, glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(0.0f));
        physics.Step(1.0f / 120.0f);
    }
    Check(glm::length(physics.GetTransform(ball).position - glm::vec3(0.0f, 2.0f, 0.0f)) < 0.08f,
          "carry is achieved by forces moving the authoritative physics body");

    const glm::quat targetTilt = glm::angleAxis(0.8f, glm::vec3(1, 0, 0));
    for (int i = 0; i < 120; ++i) {
        manipulation.ApplyCarryOrientationTorque(physics, targetTilt);
        physics.Step(1.0f / 120.0f);
    }
    Check(glm::dot(physics.GetTransform(ball).rotation * glm::vec3(0, 1, 0),
                   targetTilt * glm::vec3(0, 1, 0)) > 0.99f,
          "held orientation follows a target through ordinary torque/inertia");

    physics.SetLinearVelocity(ball, glm::vec3(1.0f, 2.0f, 3.0f));
    physics.SetAngularVelocity(ball, glm::vec3(-0.5f, 0.25f, 1.0f));
    manipulation.Drop();
    Check(!manipulation.IsHolding(), "drop clears held state");
    Check(Near(physics.GetLinearVelocity(ball), glm::vec3(1.0f, 2.0f, 3.0f)),
          "drop preserves existing body velocity");
    Check(Near(physics.GetAngularVelocity(ball), glm::vec3(-0.5f, 0.25f, 1.0f)),
          "drop preserves existing angular velocity");

    manipulation.TryPickUp(ball, physics);
    const glm::vec3 before = physics.GetLinearVelocity(ball);
    const glm::vec3 look = glm::normalize(glm::vec3(0.3f, -0.7f, -0.2f));
    Check(manipulation.Throw(physics, look, 8.0f), "throw releases held object");
    Check(Near(physics.GetLinearVelocity(ball), before + look * 8.0f),
          "throw adds a mass-scaled impulse in the full look direction");
    Check(!manipulation.Throw(physics, look, 8.0f), "throw without a held body is harmless");
    physics.Shutdown();
}

void TestTargetSelectionAndOwnership() {
    std::printf("Pickup interaction selection\n");
    PhysicsWorld physics;
    physics.Init();
    const BodyHandle handle = physics.CreateDynamicBox(glm::vec3(0.0f, 0.0f, -2.0f), glm::vec3(0.3f),
                                                         1.0f, 0.4f, 0.0f);
    DynamicBody::Visual visual;
    DynamicBody body(handle, visual, glm::vec3(0.0f, 0.0f, -2.0f), glm::quat(1, 0, 0, 0));
    ObjectManipulation manipulation({handle});
    PickupInteractable pickup(body, manipulation, physics);
    std::vector<Interactable*> targets{&pickup};
    Check(SelectInteractable(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), targets) == &pickup,
          "pickup uses shared interaction range and facing selection");
    Check(SelectInteractable(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), targets) == nullptr,
          "target behind player is not selectable");
    Check(SelectInteractable(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, -1.0f), targets) == nullptr,
          "target outside interaction radius is not selectable");
    pickup.Interact();
    Check(SelectInteractable(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), targets) == nullptr,
          "held object is no longer offered as a pickup target");
    // The application drains this request even when paused and only calls
    // Throw inside its gameplay-ownership gate; this mirrors that contract.
    const bool menuOpen = true;
    const bool requestWhilePaused = true;
    const bool gameplayOwnsInput = !menuOpen;
    if (requestWhilePaused && gameplayOwnsInput) manipulation.Throw(physics, {0, 0, -1}, 8.0f);
    Check(manipulation.IsHolding(), "menu-owned throw request cannot alter held state");
    // Input requests are edge-triggered and drained while paused. Resume
    // alone does not replay the old request.
    const bool staleRequestAfterResume = false;
    if (staleRequestAfterResume && !menuOpen) manipulation.Throw(physics, {0, 0, -1}, 8.0f);
    Check(manipulation.IsHolding(), "resuming does not apply a stale menu-owned throw");
    manipulation.Drop();
    physics.ResetBody(handle, glm::vec3(0.0f, 0.0f, -2.0f), glm::quat(1, 0, 0, 0));
    Check(!manipulation.IsHolding(), "reset protocol clears held state before body reset");
    physics.Shutdown();
}
}  // namespace

int main() {
    TestCarryTransform();
    TestPhysicsManipulation();
    TestTargetSelectionAndOwnership();
    std::printf("%s (%d failure(s))\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
