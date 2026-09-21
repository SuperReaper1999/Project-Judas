#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Opaque handle to a body inside PhysicsWorld. Deliberately not the
// middleware's own body-ID type — no file outside PhysicsWorld.cpp needs
// to know that Jolt exists.
struct BodyHandle {
    static constexpr unsigned int kInvalidId = 0xFFFFFFFFu;
    unsigned int id = kInvalidId;

    bool IsValid() const { return id != kInvalidId; }
};

struct BodyTransform {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};  // identity
};

// The result of sweeping a shape through the world: the only question
// Judas's player controller ever asks the physics middleware ("how far can
// this shape move, and what does it touch?"). What the answer MEANS —
// sliding, support, grounded state — is entirely PlayerController's
// decision; this struct carries no interpretation of its own.
struct ShapeSweepHit {
    bool hit = false;
    float distance = 0.0f;      // world-space distance traveled before the hit
    glm::vec3 normal{0.0f};     // meaningful only when `hit` is true; contact normal,
                                 // pointing back toward the caster — no default direction
                                 // is implied when there was no hit
};

// Wraps the physics middleware (currently Jolt Physics — see
// docs/ARCHITECTURE.md, "Physics middleware"). Owns collision detection,
// contact resolution, and rigid-body integration.
//
// Ownership boundary (see docs/ARCHITECTURE.md for the full rationale):
// Judas owns gravity, reference frames, and world coordinates. This class
// and the middleware behind it own collision/contact/rigid-body solving
// ONLY. The middleware's own built-in global gravity is explicitly disabled
// in Init() — gravity always arrives from the outside via
// ApplyLinearAcceleration, sourced from Judas's own GravityField. Nothing
// in this header or its implementation assumes gravity points in any
// particular direction.
//
// No Jolt type appears in this header, so no other engine file needs to
// include a Jolt header just to hold a body, ask for its transform, or
// sweep the player's collision shape.
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
    BodyHandle CreateStaticSphere(const glm::vec3& position, float radius, float friction,
                                   float restitution);
    BodyHandle CreateDynamicBox(const glm::vec3& position, const glm::vec3& halfExtents,
                                 float mass, float friction, float restitution);
    void DestroyBody(BodyHandle handle);

    // Integrates `acceleration` into the body's linear velocity over
    // `fixedDeltaTime` (velocity += acceleration * dt). This is how Judas
    // hands a sampled GravityField value to a physics body — the
    // middleware never computes gravity itself.
    void ApplyLinearAcceleration(BodyHandle handle, const glm::vec3& acceleration,
                                  float fixedDeltaTime);

    // Advances the simulation by exactly one fixed step. The caller owns
    // the accumulator that decides how many times to call this per frame.
    void Step(float fixedDeltaTime);

    BodyTransform GetTransform(BodyHandle handle) const;

    // Restores a body to a pose with zero linear and angular velocity.
    void ResetBody(BodyHandle handle, const glm::vec3& position, const glm::quat& rotation);

    // --- Player collision shape & queries ---
    //
    // As of Milestone 5, the player is NOT a Jolt body or character
    // controller of any kind — see docs/ARCHITECTURE.md, "Player/controller
    // ownership." Judas (PlayerController) owns the player's position,
    // velocity, orientation, and support interpretation entirely as plain
    // data. The only thing this class provides is a capsule Shape used
    // purely for on-demand geometry queries; it is never added to the
    // PhysicsSystem as a body, so it never appears in the broadphase and
    // never needs a layer, activation state, or mass of its own.
    bool CreatePlayerShape(float radius, float halfHeight);
    void DestroyPlayerShape();

    // Sweeps the player's capsule shape (at `fromCenter`/`rotation`) along
    // `displacement` (direction and length together) and reports the
    // closest thing it would hit, if any. This is Judas's ONLY question to
    // Jolt about player movement or support — everything the answer is
    // used for (sliding along a surface, deciding "grounded," permitting a
    // jump) is PlayerController's decision, not this class's.
    ShapeSweepHit SweepPlayerShape(const glm::vec3& fromCenter, const glm::quat& rotation,
                                    const glm::vec3& displacement) const;

private:
    struct Impl;
    Impl* m_impl = nullptr;
};
