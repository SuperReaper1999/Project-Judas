#pragma once

#include <vector>
#include <unordered_map>
#include <memory_resource>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Contacts.h"

struct RigidBody;

// Judas-owned contact resolution (Milestone 32 rewrite).
//
// Sequential impulses with *accumulated* impulses: every manifold point of a
// fixed step becomes one constraint that remembers the total normal impulse
// and the total (vector) friction impulse it has applied so far this step.
// Each velocity iteration computes an incremental correction, adds it to the
// accumulated total, clamps the TOTAL — the normal total to be non-negative,
// the friction total to the Coulomb disc |F_t| <= mu * (accumulated normal)
// — and applies only the difference. This is what lets four corner points
// share a resting box's weight: an early point may over-push in one
// iteration and give some back in the next, and each point's friction limit
// is its converged share of the normal support rather than whatever normal
// impulse that single point happened to receive in one pass. The M29 creep
// (four-point manifold, one point doing all the normal work, the other three
// allowed zero friction, net torque from single-point friction) came from
// exactly the per-iteration, per-point clamp this replaces.
//
// Warm starting (the one piece of state kept ACROSS fixed steps): each
// constraint may start from the accumulated impulses its matching contact
// converged to in the previous step (PhysicsWorld matches contacts by body
// identity, primitive and local anchor). This is not decoration: measured
// without it, a two-box stack keeps a residual ~0.008 rad/s wobble after 10
// iterations (the sequential solve has not converged when the step ends)
// and a five-box stack walks apart; it needs ~100 iterations per step to
// settle. Starting from last step's answer converges in a few.
//
// The step order is the standard one for such a solver (PhysicsWorld::Step):
// integrate velocities from forces; detect contacts at the current poses;
// Prepare(); SolveVelocities(); integrate positions from the solved
// velocities; SolvePositions() removes remaining penetration by a direct,
// mass-weighted translation along the contact normal (no velocity is created
// by that correction, so it cannot add energy). The friction direction is the
// current tangential slip itself — no tangent basis is constructed, so
// nothing here depends on how the world is rotated.
struct ContactConstraint {
    std::size_t frameA = 0, frameB = 0;
    RigidBody* bodyA = nullptr;
    RigidBody* bodyB = nullptr;
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f};      // separates A from B, toward A
    float penetration = 0.0f;    // at detection (negative: a speculative contact's gap)
    float friction = 0.0f;
    float restitutionBias = 0.0f;  // target separating normal speed (negative: gap/dt a
                                   // speculative contact may still close this step)
    float normalMass = 0.0f;       // 1 / effective mass along the normal
    float normalImpulse = 0.0f;    // accumulated this step
    glm::vec3 tangentImpulse{0.0f};  // accumulated this step (lies in the tangent plane)
    // Physical anchors come from pair-local geometry, never the debug point.
    glm::dvec3 localAnchorA{0.0};
    glm::dvec3 localAnchorB{0.0};
    glm::dvec3 preciseNormal{0.0};
    glm::dvec3 initialAnchorDifference{0.0};
    glm::dvec3 positionOffsetDifference{0.0}; // fixed within SolvePositions
    double signedSeparation = 0.0;  // positive gap, negative penetration
    bool hasLocalAnchors = false;
    bool anchorAInWorldFrame = false, anchorBInWorldFrame = false;
    glm::vec3 velocityOffsetA{0.0f}; // fixed throughout the velocity solve
    glm::vec3 velocityOffsetB{0.0f};
    glm::mat3 inverseInertiaA{0.0f};  // world-space, fixed during the velocity phase
    glm::mat3 inverseInertiaB{0.0f};
};

class ContactSolver {
public:
    // Solver constants. Velocity iterations are cheap (cached constraint
    // data); position iterations re-evaluate penetration from anchors.
    static constexpr int kVelocityIterations = 10;
    static constexpr int kPositionIterations = 4;

    void Clear();
    // One manifold point between two bodies (either may be static). The
    // bodies must outlive the solve; `contact.hit` must be true.
    void AddContact(RigidBody& bodyA, RigidBody& bodyB, const Contact& contact, float friction,
                    float restitution, float warmNormalImpulse = 0.0f,
                    const glm::vec3& warmTangentImpulse = glm::vec3(0.0f),
                    const glm::dmat3* preparedRotationA = nullptr,
                    const glm::dmat3* preparedRotationB = nullptr);
    // Effective masses, anchors and restitution targets from the bodies'
    // current (post-force-integration, pre-position-integration) state,
    // then the warm-start impulses are applied.
    // `fixedDeltaTime` lets a speculative contact (negative penetration)
    // allow exactly its gap to close within the step; 0 treats it as touching.
    void Prepare(float fixedDeltaTime = 0.0f);
    void SolveVelocities(int iterations = kVelocityIterations);
    void SolvePositions(int iterations = kPositionIterations);
    // PhysicsWorld supplies its freshly keyed post-integration rotation.
    // Copies the value; no pointer to a reallocatable geometry cache is kept.
    void UpdatePreparedRotation(RigidBody& body, const glm::dmat3& rotation);

    const std::vector<ContactConstraint>& Constraints() const { return m_constraints; }

    std::size_t FrameBuilds() const { return m_frameBuilds; }
    std::size_t FrameAllocations() const {
        return m_frameAllocations + m_frameUpstream.allocations - m_poolAllocationStart;
    }
    std::size_t FrameAllocatedBytes() const {
        return m_frameAllocatedBytes + m_frameUpstream.allocatedBytes - m_poolBytesStart;
    }
    std::size_t FrameNodeRequests() const { return m_frames.size(); }
    // Retained vector/index payload plus actual outstanding pool upstream bytes.
    // Allocator-internal metadata and body-struct padding are not included.
    std::size_t FrameStorageBytes() const;

private:
    struct BodyFrame {
        RigidBody* body = nullptr;
        glm::quat orientation{1,0,0,0};
        glm::dmat3 rotation{1.0};
        glm::mat3 inverseInertia{0.0f};
        bool rotationValid = false;
    };
    // Declaration order ensures map destruction precedes pool, then upstream.
    // Only storage survives Clear: every key/frame is reconstructed each solve.
    struct FrameUpstream final : std::pmr::memory_resource {
        std::size_t allocations=0, allocatedBytes=0, retainedBytes=0;
        void* do_allocate(std::size_t bytes, std::size_t alignment) override;
        void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override;
        bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override { return this==&other; }
    } m_frameUpstream;
    std::pmr::unsynchronized_pool_resource m_framePool{std::pmr::pool_options{256,64}, &m_frameUpstream};
    std::vector<BodyFrame> m_frames;
    std::pmr::unordered_map<RigidBody*, std::size_t> m_frameIndex{&m_framePool};
    std::size_t m_frameBuilds = 0, m_frameAllocations = 0, m_frameAllocatedBytes = 0;
    std::size_t m_poolAllocationStart = 0, m_poolBytesStart = 0;
    std::size_t RegisterFrame(RigidBody&, const glm::dmat3*);
    struct Pending {
        float restitution = 0.0f;
        float warmNormal = 0.0f;
        glm::vec3 warmTangent{0.0f};
    };
    std::vector<ContactConstraint> m_constraints;
    std::vector<Pending> m_pending;
};
