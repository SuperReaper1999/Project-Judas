#pragma once

#include <vector>

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
    glm::vec3 localAnchorA{0.0f};  // contact point in each body's frame at detection
    glm::vec3 localAnchorB{0.0f};
    glm::mat3 inverseInertiaA{0.0f};  // world-space, fixed during the velocity phase
    glm::mat3 inverseInertiaB{0.0f};
};

class ContactSolver {
public:
    // Solver constants. Velocity iterations are cheap (cached constraint
    // data); position iterations re-evaluate penetration from anchors.
    static constexpr int kVelocityIterations = 10;
    static constexpr int kPositionIterations = 4;

    void Clear() { m_constraints.clear(); }
    // One manifold point between two bodies (either may be static). The
    // bodies must outlive the solve; `contact.hit` must be true.
    void AddContact(RigidBody& bodyA, RigidBody& bodyB, const Contact& contact, float friction,
                    float restitution, float warmNormalImpulse = 0.0f,
                    const glm::vec3& warmTangentImpulse = glm::vec3(0.0f));
    // Effective masses, anchors and restitution targets from the bodies'
    // current (post-force-integration, pre-position-integration) state,
    // then the warm-start impulses are applied.
    // `fixedDeltaTime` lets a speculative contact (negative penetration)
    // allow exactly its gap to close within the step; 0 treats it as touching.
    void Prepare(float fixedDeltaTime = 0.0f);
    void SolveVelocities(int iterations = kVelocityIterations);
    void SolvePositions(int iterations = kPositionIterations);

    const std::vector<ContactConstraint>& Constraints() const { return m_constraints; }

private:
    struct Pending {
        float restitution = 0.0f;
        float warmNormal = 0.0f;
        glm::vec3 warmTangent{0.0f};
    };
    std::vector<ContactConstraint> m_constraints;
    std::vector<Pending> m_pending;
};
