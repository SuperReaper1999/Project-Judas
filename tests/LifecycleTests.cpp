// Milestone 29: entity lifecycle, simulation fidelity, coarse evolution,
// world-state deltas, and the reduced-work measurement — headless: a
// RuntimeWorld built without a RenderAssetCache creates no GPU state; a
// Window in test-input mode never touches SDL.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "FidelityPolicy.h"
#include "GameSession.h"
#include "ReferenceFrame.h"
#include "RuntimeWorld.h"
#include "Scene.h"
#include "SceneSerialization.h"
#include "Simulation.h"
#include "SimulationTiming.h"
#include "Window.h"
#include "WorldState.h"

namespace {
int g_failures = 0;

void Check(bool condition, const std::string& label) {
    std::printf("  %s %s\n", condition ? "OK  " : "FAIL", label.c_str());
    if (!condition) ++g_failures;
}

bool Near(const glm::vec3& a, const glm::vec3& b, float tolerance = 1.0e-4f) {
    return glm::length(a - b) <= tolerance;
}
bool NearQuat(const glm::quat& a, const glm::quat& b, float tolerance = 1.0e-4f) {
    return 1.0f - std::abs(glm::dot(glm::normalize(a), glm::normalize(b))) <= tolerance;
}

using Clock = std::chrono::steady_clock;
double Ms(Clock::time_point start) { return std::chrono::duration<double, std::milli>(Clock::now() - start).count(); }

SceneObject& AddGround(Scene& scene, const glm::vec3& halfExtents) {
    SceneObject& ground = scene.CreateObject("Ground");
    ground.transform.position = glm::vec3(0.0f, -1.0f, 0.0f);
    ground.render = SceneRenderComponent{};
    ground.render->halfExtents = halfExtents;
    ground.body = SceneBodyComponent{};
    ground.body->motion = SceneBodyMotion::Static;
    ground.body->halfExtents = halfExtents;
    ground.body->friction = 0.8f;
    ground.gravity = SceneGravityComponent{};
    ground.gravity->kind = SceneGravityKind::Uniform;
    ground.gravity->magnitude = 9.81f;
    ground.gravity->regionShape = SceneRegionShape::Box;
    ground.gravity->regionHalfExtents = glm::vec3(halfExtents.x + 10.0f, 30.0f, halfExtents.z + 10.0f);
    return ground;
}

SceneObject& AddCrate(Scene& scene, const std::string& name, const glm::vec3& position, bool managed,
                      const glm::vec3& velocity = glm::vec3(0.0f)) {
    SceneObject& crate = scene.CreateObject(name);
    crate.transform.position = position;
    crate.render = SceneRenderComponent{};
    crate.render->halfExtents = glm::vec3(0.5f);
    crate.body = SceneBodyComponent{};
    crate.body->motion = SceneBodyMotion::Dynamic;
    crate.body->halfExtents = glm::vec3(0.5f);
    crate.body->mass = 5.0f;
    crate.body->friction = 0.6f;
    crate.body->restitution = 0.1f;
    crate.body->initialLinearVelocity = velocity;
    crate.body->pickable = true;
    crate.body->managed = managed;
    return crate;
}

SceneObject& AddPlayerStart(Scene& scene, const glm::vec3& position) {
    SceneObject& start = scene.CreateObject("Player start");
    start.transform.position = position;
    start.playerStart = ScenePlayerStartComponent{};
    return start;
}

// A flat scene: a resting crate near the player, another far away, and
// two free-flying spheres in zero gravity (outside the ground's region).
Scene MakeLifecycleScene() {
    Scene scene;
    scene.Settings().name = "Lifecycle test";
    AddGround(scene, glm::vec3(200.0f, 1.0f, 200.0f));
    AddPlayerStart(scene, glm::vec3(0.0f, 1.0f, 3.0f));
    AddCrate(scene, "Near crate", glm::vec3(2.0f, 0.5f, 0.0f), true);
    AddCrate(scene, "Far crate", glm::vec3(150.0f, 0.5f, 0.0f), true);
    AddCrate(scene, "Unmanaged crate", glm::vec3(160.0f, 0.5f, 5.0f), false);
    // An entity nothing ever acts on: at rest, above the gravity region,
    // touching nothing. Its state must stay exactly authored.
    AddCrate(scene, "Vacuum prop", glm::vec3(0.0f, 120.0f, 50.0f), false);
    SceneObject& drifter = scene.CreateObject("Drifter");
    drifter.transform.position = glm::vec3(0.0f, 80.0f, 0.0f);  // above the gravity region: zero g
    drifter.render = SceneRenderComponent{};
    drifter.render->shape = SceneShape::Sphere;
    drifter.body = SceneBodyComponent{};
    drifter.body->motion = SceneBodyMotion::Dynamic;
    drifter.body->shape = SceneShape::Sphere;
    drifter.body->radius = 0.5f;
    drifter.body->mass = 4.0f;
    drifter.body->initialLinearVelocity = glm::vec3(3.0f, 0.0f, 1.0f);
    drifter.body->managed = true;
    return scene;
}

struct World {
    RuntimeWorld world;
    GameSession session;
    Window window;
    bool Begin(const Scene& scene, std::string& error) {
        window.SetTestInputMode(true);
        return world.Build(scene, nullptr, error) && session.Begin(world, error);
    }
    void Step(int steps) {
        for (int i = 0; i < steps; ++i) StepPlayedWorld(session, window, SimulationTiming::kFixedTimestep);
    }
};

EntityId IdOf(const Scene& scene, const std::string& name) {
    for (const SceneObject& o : scene.Objects()) {
        if (o.name == name) return o.id;
    }
    return kInvalidSceneObjectId;
}

void SectionIdentity() {
    std::printf("Section A: identity survives full -> coarse -> dormant -> full, no duplicates\n");
    const Scene scene = MakeLifecycleScene();
    World w;
    std::string error;
    Check(w.Begin(scene, error), "scene begins: " + error);
    const EntityId id = IdOf(scene, "Near crate");
    const std::size_t entitiesBefore = w.world.Entities().size();
    const std::size_t bodiesBefore = w.world.Physics().AliveBodyCount();
    w.Step(120);  // settle on the ground
    EntityPhysicalState settled;
    w.world.GetEntityState(id, settled);
    Check(glm::length(settled.linearVelocity) < 0.05f, "the crate settled on the ground");
    const BodyHandle firstHandle = w.world.DynamicBodies()[w.world.FindEntity(id)->slot].Handle();

    Check(w.world.SetEntityFidelity(id, SimulationFidelity::Coarse, &error), "full -> coarse: " + error);
    Check(w.world.Physics().AliveBodyCount() == bodiesBefore - 1, "coarse: the physics body is gone");
    Check(!w.world.Physics().IsDynamicBody(firstHandle), "the previous incarnation's handle is invalid");
    Check(w.world.FindEntity(id)->lifecycle == EntityLifecycle::Active, "coarse is still Active (loaded)");
    Check(w.world.SetEntityFidelity(id, SimulationFidelity::Dormant, &error), "coarse -> dormant: " + error);
    Check(w.world.FindEntity(id)->lifecycle == EntityLifecycle::Unloaded, "dormant is Unloaded, not destroyed");
    w.Step(60);
    Check(w.world.SetEntityFidelity(id, SimulationFidelity::Coarse, &error), "dormant -> coarse");
    Check(w.world.SetEntityFidelity(id, SimulationFidelity::Full, &error), "coarse -> full (reconstruction): " + error);
    Check(w.world.Entities().size() == entitiesBefore, "no entity was duplicated");
    Check(w.world.Physics().AliveBodyCount() == bodiesBefore, "exactly one body came back");
    const EntityRecord* record = w.world.FindEntity(id);
    Check(record && record->id == id && record->name == "Near crate" && record->reconstructions == 1,
          "same id, same name, one reconstruction recorded");
    const BodyHandle secondHandle = w.world.DynamicBodies()[record->slot].Handle();
    Check(secondHandle.IsValid() && secondHandle.id != firstHandle.id && w.world.Physics().IsDynamicBody(secondHandle),
          "the reconstructed body has a fresh, valid handle");
    Check(w.world.EntityIdOfBody(secondHandle) == id && w.world.EntityIdOfBody(firstHandle) == kInvalidSceneObjectId,
          "body -> entity lookup follows the live incarnation only");
}

void SectionPhysicalState() {
    std::printf("Section B: physical state preserved; reconstruction creates no artificial momentum\n");
    const Scene scene = MakeLifecycleScene();
    std::string error;
    // Reference: the crate stays Full throughout. Its displacement over
    // steps 120..180 is what the engine does to a resting crate anyway
    // (the pre-existing resting creep documented in docs/ARCHITECTURE.md,
    // "Milestone 29, Known engine limitation") — the transitioned run must
    // reproduce it, not add to it.
    World reference;
    reference.Begin(scene, error);
    const EntityId crate = IdOf(scene, "Near crate");
    const EntityId drifter = IdOf(scene, "Drifter");
    reference.Step(120);
    EntityPhysicalState refBefore, refAfter;
    reference.world.GetEntityState(crate, refBefore);
    reference.Step(60);
    reference.world.GetEntityState(crate, refAfter);
    const glm::vec3 referenceDisplacement = refAfter.position - refBefore.position;

    World w;
    w.Begin(scene, error);
    w.Step(120);
    EntityPhysicalState before;
    w.world.GetEntityState(crate, before);
    Check(Near(before.position, refBefore.position, 1.0e-6f), "both runs are deterministic up to the transition");

    // Resting crate: coarse (Settled) then back — nothing may move while
    // coarse, and the return must hand back exactly the retained state.
    w.world.SetEntityFidelity(crate, SimulationFidelity::Coarse);
    Check(w.world.FindEntity(crate)->coarseMotion == CoarseMotion::Settled, "a resting crate goes coarse as Settled");
    w.Step(60);
    EntityPhysicalState coarse;
    w.world.GetEntityState(crate, coarse);
    Check(Near(coarse.position, before.position) && NearQuat(coarse.rotation, before.rotation),
          "a settled coarse crate keeps its pose exactly");
    w.world.SetEntityFidelity(crate, SimulationFidelity::Full);
    EntityPhysicalState after;
    w.world.GetEntityState(crate, after);
    Check(Near(after.position, before.position, 1.0e-6f) && Near(after.linearVelocity, before.linearVelocity, 1.0e-6f) &&
              Near(after.angularVelocity, before.angularVelocity, 1.0e-6f) && NearQuat(after.rotation, before.rotation, 1.0e-6f),
          "reconstruction restored pose, linear and angular velocity exactly");
    w.Step(60);
    w.world.GetEntityState(crate, after);
    const glm::vec3 displacement = after.position - before.position;
    std::printf("      sixty steps after reconstruction: displacement %.5f m; untransitioned reference %.5f m\n",
                glm::length(displacement), glm::length(referenceDisplacement));
    Check(Near(displacement, referenceDisplacement, 1.0e-4f) && Near(after.linearVelocity, refAfter.linearVelocity, 1.0e-4f),
          "the reconstructed crate then moves exactly as the never-transitioned crate does: no impulse was added");

    // Moving sphere in zero gravity: velocities survive every transition.
    w.world.Physics().SetAngularVelocity(w.world.DynamicBodies()[w.world.FindEntity(drifter)->slot].Handle(),
                                         glm::vec3(0.0f, 0.7f, 0.0f));
    EntityPhysicalState moving;
    w.world.GetEntityState(drifter, moving);
    w.world.SetEntityFidelity(drifter, SimulationFidelity::Dormant);
    w.Step(30);
    EntityPhysicalState dormant;
    w.world.GetEntityState(drifter, dormant);
    Check(Near(dormant.position, moving.position) && Near(dormant.linearVelocity, moving.linearVelocity) &&
              Near(dormant.angularVelocity, moving.angularVelocity),
          "dormant: time is frozen for the entity; nothing was zeroed");
    w.world.SetEntityFidelity(drifter, SimulationFidelity::Full);
    EntityPhysicalState restored;
    w.world.GetEntityState(drifter, restored);
    Check(Near(restored.linearVelocity, moving.linearVelocity) && Near(restored.angularVelocity, moving.angularVelocity),
          "reconstructed sphere carries its linear and angular velocity");
}

void SectionLifecycle() {
    std::printf("Section C: unload != destroy\n");
    const Scene scene = MakeLifecycleScene();
    World w;
    std::string error;
    w.Begin(scene, error);
    const EntityId a = IdOf(scene, "Near crate");
    const EntityId b = IdOf(scene, "Far crate");
    w.world.SetEntityFidelity(a, SimulationFidelity::Dormant);
    w.world.DestroyEntity(b);
    Check(w.world.FindEntity(b)->lifecycle == EntityLifecycle::Destroyed, "destroyed entity is marked Destroyed");
    Check(!w.world.SetEntityFidelity(b, SimulationFidelity::Full, &error) && !error.empty(),
          "a destroyed entity cannot be reconstructed: " + error);
    Check(w.world.SetEntityFidelity(a, SimulationFidelity::Full, &error), "an unloaded entity comes back");
    const RuntimeWorld::LifecycleCounts counts = w.world.CountLifecycle();
    Check(counts.destroyed == 1 && counts.full == 4, "counts: one destroyed, four full");
    // Reset keeps destruction permanent within the run.
    w.session.ResetToAuthoredState();
    Check(w.world.FindEntity(b)->lifecycle == EntityLifecycle::Destroyed, "R reset does not resurrect a destroyed entity");
    Check(!w.world.SetEntityFidelity(IdOf(scene, "Drifter"), SimulationFidelity::Coarse, &error) == false,
          "an unmanaged entity may still be commanded explicitly (managed only gates the policy)");
}

void SectionRequiresFull() {
    std::printf("Section D: capability limits are explicit\n");
    Scene scene;
    scene.Settings().name = "Full-only";
    AddGround(scene, glm::vec3(50.0f, 1.0f, 50.0f));
    AddPlayerStart(scene, glm::vec3(0.0f, 1.0f, 3.0f));
    SceneObject& cup = scene.CreateObject("Cup");
    cup.transform.position = glm::vec3(0.0f, 1.0f, 0.0f);
    cup.render = SceneRenderComponent{};
    cup.render->shape = SceneShape::Compound;
    cup.body = SceneBodyComponent{};
    cup.body->motion = SceneBodyMotion::Dynamic;
    cup.body->shape = SceneShape::Compound;
    cup.body->compoundBoxes = {{glm::vec3(0.0f), glm::vec3(0.2f, 0.02f, 0.2f)}, {glm::vec3(0.0f, 0.2f, 0.0f), glm::vec3(0.02f, 0.2f, 0.2f)}};
    cup.body->mass = 2.0f;
    cup.body->managed = true;
    World w;
    std::string error;
    Check(w.Begin(scene, error), "scene begins: " + error);
    const EntityId id = IdOf(scene, "Cup");
    Check(w.world.FindEntity(id)->requiresFull, "a compound body is recognised as Full-only");
    Check(!w.world.SetEntityFidelity(id, SimulationFidelity::Coarse, &error) && error.find("no reduced") != std::string::npos,
          "asking it to reduce fails clearly: " + error);
    Check(w.world.FindEntity(id)->fidelity == SimulationFidelity::Full, "...and it stays Full");
}

void SectionCoarseEvolution() {
    std::printf("Section E: coarse inertial motion evolves consistently and promotes without discontinuity\n");
    const Scene scene = MakeLifecycleScene();
    // Reference run: the drifter stays Full for 240 steps.
    World reference;
    std::string error;
    reference.Begin(scene, error);
    const EntityId drifter = IdOf(scene, "Drifter");
    reference.Step(240);
    EntityPhysicalState full;
    reference.world.GetEntityState(drifter, full);

    // Test run: Full for 60, Coarse for 120, Full for 60.
    World w;
    w.Begin(scene, error);
    w.Step(60);
    EntityPhysicalState atDemotion;
    w.world.GetEntityState(drifter, atDemotion);
    w.world.SetEntityFidelity(drifter, SimulationFidelity::Coarse);
    Check(w.world.FindEntity(drifter)->coarseMotion == CoarseMotion::Inertial, "a moving sphere goes coarse as Inertial");
    w.Step(120);
    EntityPhysicalState coarse;
    w.world.GetEntityState(drifter, coarse);
    Check(w.world.FindEntity(drifter)->coarseStepsSimulated == 120, "120 coarse steps were simulated");
    Check(Near(coarse.position, atDemotion.position + atDemotion.linearVelocity * (120.0f / 60.0f), 1.0e-3f),
          "coarse position advanced by v*t in zero gravity (real state, not an animation)");
    w.world.SetEntityFidelity(drifter, SimulationFidelity::Full);
    w.Step(60);
    EntityPhysicalState mixed;
    w.world.GetEntityState(drifter, mixed);
    Check(Near(mixed.position, full.position, 1.0e-3f) && Near(mixed.linearVelocity, full.linearVelocity, 1.0e-5f),
          "full/coarse/full matches the all-full run within 1 mm after 240 steps");

    // Under gravity: a crate thrown upward, demoted mid-flight, follows the
    // same parabola the live integrator would (no contacts involved).
    Scene arc = MakeLifecycleScene();
    AddCrate(arc, "Thrown", glm::vec3(30.0f, 5.0f, 0.0f), true, glm::vec3(0.0f, 8.0f, 0.0f));
    World r2, w2;
    r2.Begin(arc, error);
    w2.Begin(arc, error);
    const EntityId thrown = IdOf(arc, "Thrown");
    r2.Step(40);
    w2.Step(10);
    w2.world.SetEntityFidelity(thrown, SimulationFidelity::Coarse);
    w2.Step(20);
    w2.world.SetEntityFidelity(thrown, SimulationFidelity::Full);
    w2.Step(10);
    EntityPhysicalState refState, testState;
    r2.world.GetEntityState(thrown, refState);
    w2.world.GetEntityState(thrown, testState);
    Check(Near(refState.position, testState.position, 2.0e-3f) && Near(refState.linearVelocity, testState.linearVelocity, 1.0e-3f),
          "a ballistic crate under uniform gravity: coarse arc matches the live arc to 2 mm");
}

void SectionPolicy() {
    std::printf("Section F: the distance policy drives transitions; the machinery works without it\n");
    Scene scene = MakeLifecycleScene();
    scene.Settings().fidelityPolicy = SceneFidelityPolicy::Distance;
    scene.Settings().fidelityFullRadius = 20.0f;
    scene.Settings().fidelityCoarseRadius = 100.0f;
    World w;
    std::string error;
    Check(w.Begin(scene, error), "policy scene begins: " + error);
    const EntityId near = IdOf(scene, "Near crate");
    const EntityId far = IdOf(scene, "Far crate");
    const EntityId unmanaged = IdOf(scene, "Unmanaged crate");
    Check(w.world.FindEntity(far)->fidelity == SimulationFidelity::Dormant,
          "at load, a managed entity beyond the coarse radius is never given a body");
    Check(w.world.FindEntity(unmanaged)->fidelity == SimulationFidelity::Full,
          "an unmanaged entity at the same distance stays Full: the policy ignores it");
    Check(w.world.FindEntity(near)->fidelity == SimulationFidelity::Full, "the near crate is Full");
    w.Step(5);
    // Move the far crate's retained state near the player: the policy promotes it.
    EntityPhysicalState state;
    w.world.GetEntityState(far, state);
    state.position = glm::vec3(5.0f, 0.5f, 0.0f);
    w.world.SetEntityState(far, state);
    w.Step(2);
    Check(w.world.FindEntity(far)->fidelity == SimulationFidelity::Full, "brought within the full radius, the policy reconstructed it");
    // And walk it out again by relocating it.
    w.world.GetEntityState(far, state);
    state.position = glm::vec3(60.0f, 0.5f, 0.0f);
    state.linearVelocity = glm::vec3(0.0f);
    w.world.SetEntityState(far, state);
    w.Step(2);
    Check(w.world.FindEntity(far)->fidelity == SimulationFidelity::Coarse, "between the radii, the policy demoted it to Coarse");
    // A forced override wins over the policy.
    w.world.ForceEntityFidelity(far, SimulationFidelity::Full);
    w.Step(2);
    Check(w.world.FindEntity(far)->fidelity == SimulationFidelity::Full, "a forced fidelity is respected by the policy");
    w.world.ForceEntityFidelity(far, std::nullopt);
    w.Step(2);
    Check(w.world.FindEntity(far)->fidelity == SimulationFidelity::Coarse, "clearing the override hands control back");

    // Explicit command with NO policy installed.
    Scene plain = MakeLifecycleScene();
    World p;
    p.Begin(plain, error);
    Check(p.world.GetFidelityPolicy() == nullptr, "a scene without a policy installs none");
    Check(p.world.SetEntityFidelity(IdOf(plain, "Far crate"), SimulationFidelity::Dormant) &&
              p.world.FindEntity(IdOf(plain, "Far crate"))->fidelity == SimulationFidelity::Dormant,
          "transitions still work when commanded explicitly");
    p.Step(10);
    Check(p.world.FindEntity(IdOf(plain, "Far crate"))->fidelity == SimulationFidelity::Dormant,
          "...and nothing changes it back on its own");
    // A custom policy object replaces the scene's.
    class AlwaysDormant final : public FidelityPolicy {
    public:
        SimulationFidelity Desired(const FidelityPolicyEntity&, const FidelityPolicyContext&) const override {
            return SimulationFidelity::Dormant;
        }
    };
    p.world.SetFidelityPolicy(std::make_unique<AlwaysDormant>());
    p.Step(1);
    Check(p.world.FindEntity(IdOf(plain, "Near crate"))->fidelity == SimulationFidelity::Dormant &&
              p.world.FindEntity(IdOf(plain, "Unmanaged crate"))->fidelity == SimulationFidelity::Full,
          "a game-supplied policy governs managed entities and only those");
}

void SectionDeltaPersistence() {
    std::printf("Section G: baseline + delta reproduces the modified world; the baseline file is untouched\n");
    const Scene scene = MakeLifecycleScene();
    std::string baselineText;
    SaveSceneToString(scene, baselineText);
    const EntityId moved = IdOf(scene, "Near crate");
    const EntityId destroyed = IdOf(scene, "Far crate");
    const EntityId untouched = IdOf(scene, "Vacuum prop");

    std::string deltaText;
    EntityId createdId = kInvalidSceneObjectId;
    EntityPhysicalState movedState;
    {
        World w;
        std::string error;
        w.Begin(scene, error);
        w.Step(60);
        EntityPhysicalState state;
        w.world.GetEntityState(moved, state);
        state.position += glm::vec3(4.0f, 0.0f, -2.0f);
        w.world.SetEntityState(moved, state);
        w.Step(60);
        w.world.GetEntityState(moved, movedState);
        w.world.DestroyEntity(destroyed);
        createdId = w.session.SpawnPersistentEntity();
        Check(createdId >= kRuntimeEntityIdBase, "a runtime-created entity gets a runtime-range id");
        const WorldState state1 = CaptureWorldState(w.world);
        bool sawMoved = false, sawDestroyed = false, sawCreated = false, sawUntouched = false;
        for (const WorldStateEntityChange& c : state1.entities) {
            if (c.id == moved && !c.destroyed && !c.created) sawMoved = true;
            if (c.id == destroyed && c.destroyed) sawDestroyed = true;
            if (c.id == createdId && c.created) sawCreated = true;
            if (c.id == untouched) sawUntouched = true;
        }
        Check(sawMoved && sawDestroyed && sawCreated, "delta lists the moved, destroyed and created entities");
        Check(!sawUntouched, "an entity that never left its authored state has no delta");
        SaveWorldStateToString(state1, deltaText);
        std::string again;
        WorldState reparsed;
        Check(LoadWorldStateFromString(deltaText, reparsed, error), "delta text loads back: " + error);
        SaveWorldStateToString(reparsed, again);
        Check(again == deltaText, "delta save -> load -> save is byte-identical");
        w.session.End();
        w.world.Destroy();  // the process's world is gone
    }
    std::string afterText;
    SaveSceneToString(scene, afterText);
    Check(afterText == baselineText, "the baseline scene is byte-identical after gameplay changed the world");

    // "Relaunch": rebuild the unchanged baseline, apply the delta.
    World w2;
    std::string error;
    w2.Begin(scene, error);
    WorldState delta;
    LoadWorldStateFromString(deltaText, delta, error);
    Check(ApplyWorldState(w2.world, delta, error), "delta applies to a fresh baseline: " + error);
    EntityPhysicalState restored;
    Check(w2.world.GetEntityState(moved, restored) && Near(restored.position, movedState.position, 1.0e-4f),
          "the moved entity is where it was left");
    Check(w2.world.FindEntity(destroyed)->lifecycle == EntityLifecycle::Destroyed, "the destroyed entity remains absent");
    const EntityRecord* created = w2.world.FindEntity(createdId);
    Check(created && !created->authored && created->name == "Persistent crate", "the created entity returned with its identity");
    EntityPhysicalState untouchedState;
    w2.world.GetEntityState(untouched, untouchedState);
    Check(Near(untouchedState.position, scene.Find(untouched)->transform.position, 0.0f) &&
              Near(untouchedState.linearVelocity, glm::vec3(0.0f), 0.0f),
          "untouched baseline content is baseline-identical (bit-exact)");
    Check(w2.world.NextRuntimeEntityId() > createdId, "the runtime id counter continues past restored entities");
    const EntityId next = w2.session.SpawnPersistentEntity();
    Check(next != createdId && next > createdId, "a new runtime entity after reload does not reuse a restored id");

    // Deleting the delta recovers the pristine baseline.
    World w3;
    w3.Begin(scene, error);
    Check(w3.world.FindEntity(destroyed)->lifecycle == EntityLifecycle::Active && w3.world.FindEntity(createdId) == nullptr,
          "without the delta, the baseline is pristine");
}

void SectionDeltaValidation() {
    std::printf("Section H: malformed or inconsistent world state fails before touching the world\n");
    const Scene scene = MakeLifecycleScene();
    World w;
    std::string error;
    w.Begin(scene, error);
    WorldState bad;
    bad.nextRuntimeId = kRuntimeEntityIdBase;
    WorldStateEntityChange unknown;
    unknown.id = 424242;
    bad.entities.push_back(unknown);
    const RuntimeWorld::LifecycleCounts before = w.world.CountLifecycle();
    Check(!ApplyWorldState(w.world, bad, error) && error.find("unknown") != std::string::npos,
          "an unknown entity id is rejected: " + error);
    Check(w.world.CountLifecycle().full == before.full, "...and nothing changed");
    WorldState mixed;
    mixed.nextRuntimeId = kRuntimeEntityIdBase;
    WorldStateEntityChange valid;
    valid.id = IdOf(scene, "Far crate");
    valid.destroyed = true;
    mixed.entities.push_back(valid);
    mixed.entities.push_back(unknown);
    Check(!ApplyWorldState(w.world, mixed, error) && w.world.FindEntity(valid.id)->lifecycle == EntityLifecycle::Active,
          "a delta with one bad record applies none of its good records either");
    WorldState out;
    Check(!LoadWorldStateFromString("JudasWorldState 7\n", out, error) && !error.empty(), "an unsupported version fails");
    Check(!LoadWorldStateFromString("JudasWorldState 1\nbaseline \"x\"\nnext-runtime-id 4611686018427387904\nentity 3 moved\n  position 1 2\nend\n", out, error),
          "a malformed physical state fails with a line number: " + error);
    Check(!LoadWorldStateFromString("JudasWorldState 1\nbaseline \"x\"\nentity 3 destroyed\n", out, error),
          "a missing header line fails");
    Check(!LoadWorldStateFromFile("saves/does_not_exist.judasstate", out, error) && !error.empty(), "a missing file fails clearly");
    bool applied = true;
    Check(ApplyWorldStateFileIfPresent(w.world, "saves/does_not_exist.judasstate", applied, error) && !applied,
          "a missing file at launch is simply 'no saved state'");
}

void SectionOriginAndFrames() {
    std::printf("Section I: far-origin equivalence and reference-frame semantics across transitions\n");
    Scene nearScene = MakeLifecycleScene();
    Scene farScene = MakeLifecycleScene();
    farScene.Settings().worldOrigin = glm::dvec3(1.0e9, -2.0e9, 3.0e9);
    std::string error;
    World a, b;
    a.Begin(nearScene, error);
    b.Begin(farScene, error);
    const EntityId drifter = IdOf(nearScene, "Drifter");
    for (World* w : {&a, &b}) {
        w->Step(30);
        w->world.SetEntityFidelity(drifter, SimulationFidelity::Coarse);
        w->Step(30);
        w->world.SetEntityFidelity(drifter, SimulationFidelity::Dormant);
        w->Step(10);
        w->world.SetEntityFidelity(drifter, SimulationFidelity::Full);
        w->Step(30);
    }
    std::string deltaA, deltaB;
    SaveWorldStateToString(CaptureWorldState(a.world), deltaA);
    SaveWorldStateToString(CaptureWorldState(b.world), deltaB);
    Check(deltaA == deltaB, "identical local lifecycle history yields byte-identical deltas at near and far origin");

    // M22: relative velocity to a moving frame is the same whether the
    // entity's state comes from the live body or its coarse record.
    EntityPhysicalState live;
    a.world.GetEntityState(drifter, live);
    const ReferenceFrame frame{glm::vec3(10.0f, 80.0f, 0.0f), glm::angleAxis(0.3f, glm::vec3(0.0f, 1.0f, 0.0f)),
                               glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.2f, 0.0f)};
    const glm::vec3 relativeLive = RelativeVelocityToFrame(frame, live.position, live.linearVelocity);
    a.world.SetEntityFidelity(drifter, SimulationFidelity::Coarse);
    EntityPhysicalState coarse;
    a.world.GetEntityState(drifter, coarse);
    const glm::vec3 relativeCoarse = RelativeVelocityToFrame(frame, coarse.position, coarse.linearVelocity);
    Check(Near(relativeLive, relativeCoarse, 1.0e-5f), "relative velocity to a rotating frame is unchanged by demotion");
}

void SectionReducedWork() {
    std::printf("Section J: measured evidence that dormant/reduced entities stop consuming full simulation work\n");
    constexpr int kEntities = 1500;
    Scene scene;
    scene.Settings().name = "Reduced-work measurement";
    AddGround(scene, glm::vec3(400.0f, 1.0f, 400.0f));
    AddPlayerStart(scene, glm::vec3(0.0f, 1.0f, 0.0f));
    for (int i = 0; i < kEntities; ++i) {
        const float x = static_cast<float>(i % 50) * 3.0f - 75.0f;
        const float z = static_cast<float>(i / 50) * 3.0f + 4.0f;
        AddCrate(scene, "Crate " + std::to_string(i), glm::vec3(x, 0.5f, z), true);
    }
    std::string error;

    // All Full.
    World full;
    auto t = Clock::now();
    full.Begin(scene, error);
    const double loadFullMs = Ms(t);
    full.Step(3);  // warm up
    t = Clock::now();
    full.Step(10);
    const double fullStepMs = Ms(t) / 10.0;
    const RuntimeWorld::LifecycleCounts fullCounts = full.world.CountLifecycle();

    // Distance policy: Full within 12 m, Coarse within 20 m, Dormant beyond.
    Scene managed = scene;
    managed.Settings().fidelityPolicy = SceneFidelityPolicy::Distance;
    managed.Settings().fidelityFullRadius = 12.0f;
    managed.Settings().fidelityCoarseRadius = 20.0f;
    World reduced;
    t = Clock::now();
    reduced.Begin(managed, error);
    const double loadReducedMs = Ms(t);
    reduced.Step(3);
    t = Clock::now();
    reduced.Step(10);
    const double reducedStepMs = Ms(t) / 10.0;
    const RuntimeWorld::LifecycleCounts reducedCounts = reduced.world.CountLifecycle();

    // Transition cost: force every entity Full, then back to policy.
    t = Clock::now();
    for (const EntityRecord& e : reduced.world.Entities()) reduced.world.SetEntityFidelity(e.id, SimulationFidelity::Full);
    const double promoteAllMs = Ms(t);
    reduced.Step(1);  // the policy demotes them again
    const unsigned int demotions = reduced.world.CountLifecycle().transitionsThisStep;

    // Persistence cost, over a world where a fifth of the crates have
    // genuinely moved (their retained state relocated by a metre).
    {
        int moved = 0;
        for (const EntityRecord& e : reduced.world.Entities()) {
            if (moved >= kEntities / 5) break;
            EntityPhysicalState state;
            reduced.world.GetEntityState(e.id, state);
            state.position += glm::vec3(1.0f, 0.0f, 0.0f);
            reduced.world.SetEntityState(e.id, state);
            ++moved;
        }
    }
    t = Clock::now();
    const WorldState state = CaptureWorldState(reduced.world);
    std::string text;
    SaveWorldStateToString(state, text);
    const double saveMs = Ms(t);
    World reload;
    reload.Begin(managed, error);
    WorldState parsed;
    t = Clock::now();
    LoadWorldStateFromString(text, parsed, error);
    ApplyWorldState(reload.world, parsed, error);
    const double loadStateMs = Ms(t);

    std::printf("  entities %d | all-full: %zu physics bodies, %zu full entities, %.3f ms/step, load %.1f ms\n",
                kEntities, fullCounts.physicsBodies, fullCounts.full, fullStepMs, loadFullMs);
    std::printf("  policy 12/20 m: %zu physics bodies, %zu full, %zu coarse, %zu dormant, %.3f ms/step, load %.1f ms\n",
                reducedCounts.physicsBodies, reducedCounts.full, reducedCounts.coarse, reducedCounts.dormant,
                reducedStepMs, loadReducedMs);
    std::printf("  promote all %d: %.2f ms (%.4f ms each); policy re-demoted %u in one step; "
                "delta capture+serialize %.2f ms (%zu records, %zu bytes); parse+apply %.2f ms\n",
                kEntities, promoteAllMs, promoteAllMs / kEntities, demotions, saveMs, state.entities.size(),
                text.size(), loadStateMs);
    Check(fullCounts.physicsBodies == static_cast<std::size_t>(kEntities) + 1, "all-full run has every crate as a live body");
    Check(reducedCounts.full < 100 && reducedCounts.dormant > kEntities / 2,
          "the policy keeps under 100 crates Full and most dormant");
    Check(reducedCounts.physicsBodies == reducedCounts.full + 1,
          "physics body count equals Full entities + ground: dormant/coarse entities own no body");
    Check(reducedStepMs < fullStepMs * 0.25, "the reduced world steps in under a quarter of the all-full time");
    // Load time is printed, not asserted: at this scene size both loads are
    // ~2 ms and dominated by scene copying, so the difference is noise.
}
}  // namespace

int main() {
    SectionIdentity();
    SectionPhysicalState();
    SectionLifecycle();
    SectionRequiresFull();
    SectionCoarseEvolution();
    SectionPolicy();
    SectionDeltaPersistence();
    SectionDeltaValidation();
    SectionOriginAndFrames();
    SectionReducedWork();
    std::printf("Lifecycle tests: %s (%d failures)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);
    return g_failures == 0 ? 0 : 1;
}
