#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

#include "CollisionShapes.h"

// Opaque handle to a body inside PhysicsWorld. Deliberately not tied to
// any concrete physics-engine body-ID representation — no file outside
// PhysicsWorld.cpp needs to know what implements this class.
struct BodyHandle {
    static constexpr unsigned int kInvalidId = 0xFFFFFFFFu;
    unsigned int id = kInvalidId;

    bool IsValid() const { return id != kInvalidId; }
};

struct BodyTransform {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};  // identity
};

// One box of a rigid body's collision geometry after transforming it into
// simulation/world space. Ordinary boxes return one entry; compound bodies
// return their children in stable order; spheres return none. Consumers such
// as the fluid solver can collide with the same geometry as rigid bodies.
struct BodyBox {
    glm::vec3 center{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 halfExtents{0.0f};
};

// The result of sweeping a shape through the world: the only question
// Judas's player controller ever asks the physics engine ("how far can
// this shape move, and what does it touch?"). What the answer MEANS —
// sliding, support, grounded state — is entirely PlayerController's
// decision; this struct carries no interpretation of its own.
struct ShapeSweepHit {
    bool hit = false;
    float distance = 0.0f;      // world-space distance traveled before the hit
    glm::vec3 normal{0.0f};     // meaningful only when `hit` is true; contact normal,
                                 // pointing back toward the caster — no default direction
                                 // is implied when there was no hit
    BodyHandle hitBody;         // meaningful only when `hit` is true; identifies what was
                                 // hit (added in Milestone 7-A so a caller can distinguish
                                 // static world geometry from a pushable dynamic body —
                                 // still says nothing about WHAT that means, same as
                                 // `normal`; interpretation stays the caller's job)
};

// Wraps Judas's own physics engine (see docs/ARCHITECTURE.md, "Physics
// ownership" — Jolt Physics served this role through the first Milestone
// 7-Final attempt and is no longer used at all). Owns collision detection,
// contact resolution, and rigid-body integration.
//
// Ownership boundary (see docs/ARCHITECTURE.md for the full rationale):
// Judas owns gravity, reference frames, and world coordinates. This class
// and the engine behind it own collision/contact/rigid-body solving ONLY.
// Nothing in this class ever computes or applies gravity of its own —
// gravity always arrives from the outside via ApplyLinearAcceleration,
// sourced from Judas's own GravityField. Nothing in this header or its
// implementation assumes gravity points in any particular direction.
//
// No concrete physics-engine type appears in this header, so no other
// engine file needs to include one just to hold a body, ask for its
// transform, or sweep the player's collision shape.
class PhysicsWorld {
public:
    PhysicsWorld() = default;
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    bool Init();
    void Shutdown();

    BodyHandle CreateStaticBox(const glm::vec3& position, const glm::vec3& halfExtents,
                                float friction, float restitution);

    // Same as above, with an explicit orientation. Added in Milestone 10
    // so demo geometry (steps, ramps) can be authored with its own "up"
    // aligned to wherever local gravity actually points at that position
    // — e.g. a step on a curved planet's surface, whose own local up is
    // radial, not world +Y — rather than being restricted to axis-aligned
    // boxes. The original 4-argument overload is unchanged and still
    // exactly equivalent to passing an identity rotation here.
    BodyHandle CreateStaticBox(const glm::vec3& position, const glm::quat& rotation,
                                const glm::vec3& halfExtents, float friction, float restitution);
    BodyHandle CreateStaticSphere(const glm::vec3& position, float radius, float friction,
                                   float restitution);
    BodyHandle CreateDynamicBox(const glm::vec3& position, const glm::vec3& halfExtents,
                                 float mass, float friction, float restitution);
    BodyHandle CreateDynamicSphere(const glm::vec3& position, float radius, float mass,
                                    float friction, float restitution);
    // A set of local boxes sharing one dynamic body's centre of mass, pose,
    // momentum, and inertia. `position` is the centre of mass; child centres
    // are relative to it. No semantic knowledge of the assembled object is
    // needed by physics or by callers querying its box geometry.
    BodyHandle CreateDynamicCompoundBoxes(const glm::vec3& position,
                                           const std::vector<CompoundBox>& boxes,
                                           float mass, float friction, float restitution);
    void DestroyBody(BodyHandle handle);

    // True only for a body created via CreateDynamic*. Added in Milestone
    // 7-A so a caller holding a ShapeSweepHit::hitBody can tell "pushable
    // object" apart from "static world geometry" without needing to
    // remember which handles it created dynamic vs. static itself.
    bool IsDynamicBody(BodyHandle handle) const;
    float GetMass(BodyHandle handle) const;
    void ApplyLinearImpulse(BodyHandle handle, const glm::vec3& impulse);
    void ApplyImpulseAtPoint(BodyHandle handle, const glm::vec3& impulse,
                             const glm::vec3& worldPoint);

    // Generic rigid-body velocity access — the same category as
    // GetTransform/ResetBody below, just for velocity. Added in Milestone
    // 7-A for the player's minimal push-on-contact behavior (see
    // PlayerController::FixedUpdate); nothing about gravity or gameplay
    // lives here, only the query/command itself.
    glm::vec3 GetLinearVelocity(BodyHandle handle) const;
    void SetLinearVelocity(BodyHandle handle, const glm::vec3& velocity);

    // Same shape, for angular velocity. Added in Milestone 8 for the flying
    // primitive's rotation control (see src/FlyingPrimitiveControl.h) and
    // for PlayerController's moving-support velocity carry (a rotating
    // support's own angular velocity contributes to the point-velocity a
    // standing player must inherit) — not vehicle-specific, ordinary
    // rigid-body angular velocity access of the same kind
    // GetLinearVelocity/SetLinearVelocity already provide.
    glm::vec3 GetAngularVelocity(BodyHandle handle) const;
    void SetAngularVelocity(BodyHandle handle, const glm::vec3& angularVelocity);
    // World-space inertia tensor of a dynamic body. Consumers that implement
    // torque controllers can convert a desired angular acceleration into a
    // physical torque without duplicating the body's shape/orientation math.
    glm::mat3 GetInertiaWorld(BodyHandle handle) const;

    // Shape support distances used for separation along an arbitrary world
    // direction. Directions are normalized internally. The body result uses
    // its current orientation; the player result is the capsule's maximum
    // support distance so it remains safe if the player's frame is
    // reoriented immediately after release.
    float GetBodySupportDistance(BodyHandle handle, const glm::vec3& worldDirection) const;
    float GetPlayerShapeMaxSupportDistance() const;

    // Integrates `acceleration` into the body's linear velocity over
    // `fixedDeltaTime` (velocity += acceleration * dt). This is how Judas
    // hands a sampled GravityField value to a physics body — the
    // middleware never computes gravity itself.
    void ApplyLinearAcceleration(BodyHandle handle, const glm::vec3& acceleration,
                                  float fixedDeltaTime);

    // Milestone 12: adds `force`/`torque` (world-space) to the body's
    // RigidBody force/torque accumulator (RigidBody::ApplyForce/
    // ApplyTorque) — genuine F=ma physics, turned into an actual velocity/
    // angular-velocity change by the NEXT Step() call (via
    // IntegrateRigidBody, which also clears both accumulators
    // afterward — see Step()'s own comment). Unlike
    // ApplyLinearAcceleration (which mutates velocity immediately and
    // unconditionally, mass-independent, exactly right for gravity) or
    // SetLinearVelocity/SetAngularVelocity (which overwrite velocity
    // outright), these two respect the body's actual mass/inertia and
    // compose additively with whatever else affected the accumulator or
    // velocity that same step — nothing here overwrites anything. Must be
    // called fresh every fixed step a force/torque should act; nothing
    // persists it across steps (a no-op call this step means no
    // contribution this step — see law #22/#32 in Project_Persistent_Memory.md).
    // A no-op on a static or unknown handle.
    void ApplyForce(BodyHandle handle, const glm::vec3& force);
    void ApplyTorque(BodyHandle handle, const glm::vec3& torque);

    // Advances the simulation by exactly one fixed step. The caller owns
    // the accumulator that decides how many times to call this per frame.
    // Milestone 12: each dynamic body's own accumulated force/torque
    // (ApplyForce/ApplyTorque above) is integrated into its velocity/
    // angular velocity here (via RigidBody.h's IntegrateRigidBody — the
    // same free function the standalone physics/collision test suites
    // already exercise directly against a bare RigidBody, now genuinely
    // used by the live simulation for the first time), then cleared,
    // before position/orientation integrate from the resulting velocity —
    // so gravity (already folded into velocity directly via
    // ApplyLinearAcceleration, called before Step()) and any force/torque
    // applied this step compose into the same integration pass, in the
    // order they were applied, with no double-counting.
    void Step(float fixedDeltaTime);

    BodyTransform GetTransform(BodyHandle handle) const;
    // Pose at the start of the most recent fixed step for dynamic bodies;
    // static bodies return their current pose.
    BodyTransform GetPreviousTransform(BodyHandle handle) const;
    std::vector<BodyBox> GetBodyBoxes(BodyHandle handle) const;
    std::vector<BodyBox> GetPreviousBodyBoxes(BodyHandle handle) const;

    // Restores a body to a pose with zero linear and angular velocity.
    void ResetBody(BodyHandle handle, const glm::vec3& position, const glm::quat& rotation);

    // --- Player collision shape & queries ---
    //
    // As of Milestone 5, the player is NOT a physics-engine body or
    // character controller of any kind — see docs/ARCHITECTURE.md,
    // "Player/controller ownership." Judas (PlayerController) owns the
    // player's position, velocity, orientation, and support interpretation
    // entirely as plain data. The only thing this class provides is a
    // capsule shape used purely for on-demand geometry queries; it is
    // never added to the world as a body, so it never appears in the
    // broadphase/contact-resolution pass and never needs a layer,
    // activation state, or mass of its own.
    bool CreatePlayerShape(float radius, float halfHeight);
    void DestroyPlayerShape();

    // Sweeps the player's capsule shape (at `fromCenter`/`rotation`) along
    // `displacement` (direction and length together) and reports the
    // closest thing it would hit, if any. This is Judas's ONLY question to
    // the physics engine about player movement or support — everything the answer is
    // used for (sliding along a surface, deciding "grounded," permitting a
    // jump) is PlayerController's decision, not this class's.
    // Queries can interpolate dynamic bodies between the previous and
    // current PhysicsWorld::Step transforms. Airborne movement uses this to
    // compare both trajectories over one fixed-step interval; support probes
    // pin both endpoints to the previous pose so they query the player's
    // start-of-step state. `bodyMotionStart`/`bodyMotionEnd` preserve timing
    // when a move-and-slide sweep continues after contact.
    ShapeSweepHit SweepPlayerShape(const glm::vec3& fromCenter, const glm::quat& rotation,
                                    const glm::vec3& displacement,
                                    bool interpolateDynamicBodyMotion = false,
                                    float bodyMotionStart = 0.0f,
                                    float bodyMotionEnd = 1.0f) const;

private:
    struct Impl;
    Impl* m_impl = nullptr;
};
