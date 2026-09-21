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

private:
    struct Impl;
    Impl* m_impl = nullptr;
};
