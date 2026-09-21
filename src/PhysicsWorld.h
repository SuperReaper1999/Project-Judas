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

// What the player is currently physically supported by, as reported by the
// character controller's own collision/contact detection — never derived
// from comparing a world-space height to a known floor coordinate. See
// docs/ARCHITECTURE.md, "Ground/support semantics": `normal` is the actual
// contact-surface normal, a distinct concept from "the direction opposite
// gravity," even though the two happen to coincide on today's flat floor.
struct PlayerGroundContact {
    bool isGrounded = false;
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec3 velocity{0.0f, 0.0f, 0.0f};  // world-space velocity of whatever the player is standing on
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
// include a Jolt header just to hold a body or ask for its transform.
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
    // Used by the Milestone 3 debug reset control.
    void ResetBody(BodyHandle handle, const glm::vec3& position, const glm::quat& rotation);

    // --- Player (character controller) ---
    //
    // Backed by a Jolt CharacterVirtual — a kinematic, collision-aware
    // controller rather than a full dynamic rigid body. See
    // docs/ARCHITECTURE.md, "Player/controller representation," for why.
    // There is exactly one player; see PlayerController for the
    // input/locomotion logic that drives these calls. As with everything
    // else in this class, no Jolt type appears in this signature list.
    //
    // `feetPosition` is the position at the bottom of the player's capsule
    // (its "feet"), not the capsule's center. `up` seeds the character
    // controller's own internal reference axis (used only for classifying
    // ground vs. too-steep-to-climb slopes) — callers derive it from the
    // active GravityField at spawn time rather than hard-coding it, though
    // it is not re-derived every frame in this milestone (FaithfulGravity
    // is constant). This is a controller implementation detail, not the
    // same concept as a contact normal — see PlayerGroundContact above.
    bool CreatePlayer(const glm::vec3& feetPosition, const glm::vec3& up, float capsuleRadius,
                       float capsuleHalfHeight, float mass);
    void DestroyPlayer();

    void SetPlayerVelocity(const glm::vec3& velocity);
    glm::vec3 GetPlayerVelocity() const;

    // Advances the player's own collision-aware movement by exactly one
    // fixed step. `gravity` here is passed straight through to Jolt's
    // CharacterVirtual::Update, which by its own documented contract uses
    // it ONLY for the edge case of standing on a moving/rotating object —
    // it does not integrate gravity into the player's velocity itself
    // (that remains PlayerController's job, exactly like
    // ApplyLinearAcceleration does for ordinary bodies).
    void UpdatePlayer(float fixedDeltaTime, const glm::vec3& gravity);

    glm::vec3 GetPlayerPosition() const;  // feet position

    // Restores the player to a feet position with zero velocity.
    void ResetPlayer(const glm::vec3& feetPosition);

    PlayerGroundContact GetPlayerGroundContact() const;

private:
    struct Impl;
    Impl* m_impl = nullptr;
};
