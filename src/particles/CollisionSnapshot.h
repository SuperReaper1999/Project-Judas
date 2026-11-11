#pragma once

#include "Broadphase.h"
#include "PhysicsWorld.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct CollisionAsset;

namespace JudasParticles {

struct CollisionBody {
    BodyHandle handle;
    std::uint64_t entity = 0;
    BodyTransform pose; // latest COM pose, never the authored pivot
    glm::vec3 linearVelocity{0}, angularVelocity{0};
    std::vector<PhysicsWorld::BodyMotionSegment> motion;
    float boundingRadius = 0;
    bool stationary = true; // derived once from the immutable captured trajectory
};

struct CollisionCollider {
    std::size_t bodyIndex = 0;
    std::uint32_t childKey = 0;
    std::uint64_t geometryRevision = 0; // runtime snapshot-owner identity, not serialized
    ShapeType kind = ShapeType::Sphere;
    float radius = 0, halfHeight = 0;
    glm::vec3 halfExtents{0}, localCenter{0}; // relative to body's COM
    glm::quat localRotation{1, 0, 0, 0};
    // Conservative static candidate bounds; narrowphase remains authoritative.
    glm::dvec3 queryMinimum{0}, queryMaximum{0};
    std::shared_ptr<const CollisionAsset> asset; // immutable asset-local BVH/vertices
    std::shared_ptr<const RadialTerrain> terrain; // CPU callback; not silently evaluated on GPU
};

struct CollisionCaptureLimits {
    std::size_t maxBodies = 512, maxColliders = 2048;
    std::size_t maxTriangles = 262144, maxMotionSegments = 8192;
};

struct CollisionCaptureStats {
    std::size_t examinedBodies = 0, retainedBodies = 0, colliders = 0;
    std::size_t uniqueTriangles = 0, motionSegments = 0, unsupportedBodies = 0;
};

enum class CollisionQueryStatus { Miss, Hit, Unsupported, Exhausted, OutsideDomain };

struct ParticleCollisionHit {
    CollisionQueryStatus status = CollisionQueryStatus::Miss;
    bool initialOverlap = false, repairedEndpoint = false;
    double fraction = 1, penetration = 0;
    glm::dvec3 center{0}, point{0}, normal{0}, surfaceVelocity{0};
    glm::dvec3 localPoint{0}, localNormal{0}; // exact hit-time COM frame, including endpoint repairs
    BodyHandle body;
    std::uint64_t entity = 0, geometryRevision = 0;
    std::uint32_t childKey = 0, feature = UINT32_MAX;
    std::size_t colliderIndex = 0;
};

struct CollisionAnchor {
    BodyHandle body;
    std::uint64_t geometryRevision = 0;
    std::uint32_t childKey = 0;
    glm::dvec3 localPoint{0}, localNormal{0}; // body-COM frame
};

// Capture only on the owning physics thread at a completed fixed boundary.
// The published arrays retain their own shapes/motion: queries never read the
// mutable world. Capture is transactional; failure preserves the prior sample.
// Reuse this owner between captures to retain geometry revisions. Copies remain
// immutable snapshots of their captured sample. No particle creates a body or
// calls a world query. Initial capture examines filtered live bodies once so a
// mover crossing the domain cannot disappear merely by ending outside it.
class CollisionSnapshot {
public:
    using EntityMapper = std::function<std::uint64_t(BodyHandle)>;
    bool Capture(const PhysicsWorld&, const Aabb& domain, double dt, std::string& error,
                 const PhysicsQueryFilter& filter = {}, const EntityMapper& entity = {},
                 const CollisionCaptureLimits& limits = {});
    ParticleCollisionHit SweepSphere(glm::dvec3 from, glm::dvec3 to, double radius,
                                     double beginFraction = 0, double endFraction = 1,
                                     unsigned maxIterations = 128,
                                     double tolerance = 1e-4) const;
    ParticleCollisionHit OverlapSphere(glm::dvec3 center, double radius,
                                       double fraction = 1, double tolerance = 1e-4) const;
    CollisionAnchor MakeAnchor(const ParticleCollisionHit&) const;
    bool ResolveAnchor(const CollisionAnchor&, double fraction, glm::dvec3& point,
                       glm::dvec3& normal, glm::dvec3* velocity = nullptr) const;
    const std::vector<CollisionBody>& Bodies() const { return m_bodies; }
    const std::vector<CollisionCollider>& Colliders() const { return m_colliders; }
    const CollisionCaptureStats& Stats() const { return m_stats; }
    const Aabb& Domain() const { return m_domain; }
    double Duration() const { return m_dt; }
    // Copies of one captured sample retain this token. Successful recapture
    // publishes a new token; consumers can prove sharing without pointer reuse
    // or treating a geometry fingerprint as an identity proof.
    const std::shared_ptr<const void>& CaptureToken() const { return m_captureToken; }
    // Exact anchored MK2 pose evaluation. Endpoint corrections are sampled at
    // the endpoint, never interpreted as a finite-duration swept trajectory.
    BodyTransform PoseAt(std::size_t body, double fraction) const;
    bool IsStationary(std::size_t body) const;

private:
    std::shared_ptr<const void> m_captureToken=std::make_shared<const unsigned char>(0);
    Aabb m_domain;
    double m_dt = 0;
    std::uint64_t m_nextGeometryRevision = 1;
    std::vector<CollisionBody> m_bodies;
    std::vector<CollisionCollider> m_colliders;
    CollisionCaptureStats m_stats;
};

// One-way visual response in the boundary's moving frame. No rigid-world
// impulse, damping, gravity or position integration is hidden in this helper.
glm::dvec3 CollisionResponse(glm::dvec3 velocity, const ParticleCollisionHit&,
                             double restitution, double friction, bool slide = false);

} // namespace JudasParticles
