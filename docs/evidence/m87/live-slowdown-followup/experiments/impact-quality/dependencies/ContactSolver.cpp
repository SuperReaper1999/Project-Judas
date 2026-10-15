#include "ContactSolver.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "RigidBody.h"

namespace {
constexpr float kEpsilon = 1.0e-6f;

// How much of the remaining penetration to correct per position iteration,
// and the small allowed overlap ("slop") left uncorrected so resting
// contacts do not fight the solver every step trying to reach exactly zero
// — unchanged from the pre-M32 solver.
constexpr float kPositionalCorrectionPercent = 0.2f;
constexpr float kPenetrationSlop = 0.005f;

// Restitution only above a meaningful closing speed — otherwise a resting
// contact's own per-step gravity nudge would "bounce" forever at
// ever-smaller amplitude. Unchanged from the pre-M32 solver.
constexpr float kRestitutionVelocityThreshold = 0.5f;

glm::vec3 PointVelocity(const RigidBody& body, const glm::vec3& r) {
    return body.linearVelocity + glm::cross(body.angularVelocity, r);
}

// Linear inverse masses plus the angular contribution each body's inertia
// produces from an impulse along `direction` at offsets rA/rB. No axis
// assumption: `direction` is the contact normal or the current slip.
float InverseEffectiveMass(const ContactConstraint& c, const glm::vec3& rA, const glm::vec3& rB,
                           const glm::vec3& direction) {
    const glm::vec3 angularA = glm::cross(c.inverseInertiaA * glm::cross(rA, direction), rA);
    const glm::vec3 angularB = glm::cross(c.inverseInertiaB * glm::cross(rB, direction), rB);
    return c.bodyA->inverseMass + c.bodyB->inverseMass + glm::dot(direction, angularA + angularB);
}

void ApplyImpulse(ContactConstraint& c, const glm::vec3& rA, const glm::vec3& rB,
                  const glm::vec3& impulse) {
    RigidBody& a = *c.bodyA;
    RigidBody& b = *c.bodyB;
    if (!a.IsStatic()) {
        a.linearVelocity += impulse * a.inverseMass;
        a.angularVelocity += c.inverseInertiaA * glm::cross(rA, impulse);
    }
    if (!b.IsStatic()) {
        b.linearVelocity -= impulse * b.inverseMass;
        b.angularVelocity -= c.inverseInertiaB * glm::cross(rB, impulse);
    }
}
}  // namespace

namespace {
bool SameQuaternion(const glm::quat& a, const glm::quat& b) {
    // Component representations, including signed zero; no tolerance key.
    return std::memcmp(&a.w,&b.w,sizeof(float))==0 &&
           std::memcmp(&a.x,&b.x,sizeof(float))==0 &&
           std::memcmp(&a.y,&b.y,sizeof(float))==0 &&
           std::memcmp(&a.z,&b.z,sizeof(float))==0;
}
}

void* ContactSolver::FrameUpstream::do_allocate(std::size_t bytes, std::size_t alignment) {
    void* p=std::pmr::new_delete_resource()->allocate(bytes,alignment);
    ++allocations;
    allocatedBytes+=bytes;
    retainedBytes+=bytes;
    return p;
}
void ContactSolver::FrameUpstream::do_deallocate(void* p,std::size_t bytes,std::size_t alignment) {
    retainedBytes-=bytes;
    std::pmr::new_delete_resource()->deallocate(p,bytes,alignment);
}

void ContactSolver::Clear() {
    m_constraints.clear();
    m_pending.clear();
    m_frames.clear();
    m_frameIndex.clear(); // keys die; freed node storage remains in the pool
    m_frameBuilds=0;
    m_frameAllocations=0;
    m_frameAllocatedBytes=0;
    m_poolAllocationStart=m_frameUpstream.allocations;
    m_poolBytesStart=m_frameUpstream.allocatedBytes;
}

std::size_t ContactSolver::RegisterFrame(RigidBody& body, const glm::dmat3* prepared) {
    const auto found = m_frameIndex.find(&body);
    if (found != m_frameIndex.end()) return found->second;
    const std::size_t index = m_frames.size();
    BodyFrame frame;
    frame.body = &body;
    frame.orientation = body.orientation;
    if (prepared) { frame.rotation = *prepared; frame.rotationValid = true; }
    const auto capacity=m_frames.capacity();
    m_frames.push_back(frame);
    if (m_frames.capacity()!=capacity) {
        ++m_frameAllocations;
        m_frameAllocatedBytes+=m_frames.capacity()*sizeof(BodyFrame);
    }
    m_frameIndex.emplace(&body,index);
    return index;
}

void ContactSolver::UpdatePreparedRotation(RigidBody& body, const glm::dmat3& rotation) {
    const auto found = m_frameIndex.find(&body);
    if (found == m_frameIndex.end()) return;
    BodyFrame& frame = m_frames[found->second];
    frame.orientation = body.orientation;
    frame.rotation = rotation;
    frame.rotationValid = true;
}

std::size_t ContactSolver::FrameStorageBytes() const {
    return m_frames.capacity()*sizeof(BodyFrame) + m_frameUpstream.retainedBytes +
           m_constraints.capacity()*2*sizeof(std::size_t);
}

void ContactSolver::AddContact(RigidBody& bodyA, RigidBody& bodyB, const Contact& contact,
                               float friction, float restitution, float warmNormalImpulse,
                               const glm::vec3& warmTangentImpulse,
                               const glm::dmat3* preparedRotationA,
                               const glm::dmat3* preparedRotationB) {
    if (!contact.hit) return;
    if (bodyA.IsStatic() && bodyB.IsStatic()) return;
    ContactConstraint c;
    c.bodyA = &bodyA;
    c.bodyB = &bodyB;
    c.frameA = RegisterFrame(bodyA, preparedRotationA);
    c.frameB = RegisterFrame(bodyB, preparedRotationB);
    c.point = contact.point;
    c.normal = contact.normal;
    c.penetration = contact.penetration;
    c.signedSeparation = contact.hasLocalAnchors ? contact.signedSeparation : -double(contact.penetration);
    c.preciseNormal = contact.hasLocalAnchors ? contact.preciseNormal : glm::dvec3(contact.normal);
    c.hasLocalAnchors = contact.hasLocalAnchors;
    c.anchorAInWorldFrame = contact.anchorAInWorldFrame;
    c.anchorBInWorldFrame = contact.anchorBInWorldFrame;
    c.localAnchorA = contact.localAnchorA;
    c.localAnchorB = contact.localAnchorB;
    c.friction = friction;
    m_constraints.push_back(c);
    m_pending.push_back(Pending{restitution, std::max(warmNormalImpulse, 0.0f), warmTangentImpulse});
}

void ContactSolver::Prepare(float fixedDeltaTime) {
    // Frames live only within this solve. The body pointers have the same
    // lifetime contract as constraints; Clear drops every pointer/key.
    for (BodyFrame& frame : m_frames) {
        const RigidBody& body = *frame.body;
        if (!frame.rotationValid || !SameQuaternion(frame.orientation,body.orientation)) {
            frame.rotation = ContactRotation(body.orientation);
            frame.orientation = body.orientation;
            frame.rotationValid = true;
        }
        frame.inverseInertia = body.IsStatic() ? glm::mat3(0.0f) : body.InverseInertiaWorld();
        ++m_frameBuilds;
    }
    for (std::size_t i = 0; i < m_constraints.size(); ++i) {
        ContactConstraint& c = m_constraints[i];
        const RigidBody& a = *c.bodyA;
        const RigidBody& b = *c.bodyB;
        c.inverseInertiaA = m_frames[c.frameA].inverseInertia;
        c.inverseInertiaB = m_frames[c.frameB].inverseInertia;
        const glm::dmat3& rotationA = m_frames[c.frameA].rotation;
        const glm::dmat3& rotationB = m_frames[c.frameB].rotation;
        if (!c.hasLocalAnchors) {
            // Compatibility for explicit caller contacts and the specialized
            // approximate terrain path. Primitive contacts supply precise
            // parent-local anchors and never take this world-point path.
            c.localAnchorA = glm::transpose(rotationA) * (glm::dvec3(c.point) - glm::dvec3(a.position));
            c.localAnchorB = glm::transpose(rotationB) * (glm::dvec3(c.point) - glm::dvec3(b.position));
        }
        // Sphere convenience signatures have no sphere orientation. They
        // explicitly provide precise world offsets, not assumed parent axes.
        if (c.anchorAInWorldFrame) {
            c.localAnchorA = glm::transpose(rotationA) * c.localAnchorA;
            c.anchorAInWorldFrame = false;
        }
        if (c.anchorBInWorldFrame) {
            c.localAnchorB = glm::transpose(rotationB) * c.localAnchorB;
            c.anchorBInWorldFrame = false;
        }
        const glm::dvec3 offsetA = rotationA * c.localAnchorA;
        const glm::dvec3 offsetB = rotationB * c.localAnchorB;
        c.initialAnchorDifference = (glm::dvec3(a.position) - glm::dvec3(b.position)) + offsetA - offsetB;
        c.velocityOffsetA = glm::vec3(offsetA);
        c.velocityOffsetB = glm::vec3(offsetB);
        const glm::vec3 rA = c.velocityOffsetA;
        const glm::vec3 rB = c.velocityOffsetB;
        const float k = InverseEffectiveMass(c, rA, rB, c.normal);
        c.normalMass = k > kEpsilon ? 1.0f / k : 0.0f;
        c.normalImpulse = 0.0f;
        c.tangentImpulse = glm::vec3(0.0f);
        const float closingSpeed = glm::dot(PointVelocity(a, rA) - PointVelocity(b, rB), c.normal);
        const float restitution = m_pending[i].restitution;
        const bool bounces = -closingSpeed > kRestitutionVelocityThreshold;
        // FTFT4A preserves the measured signed gap. The existing separated
        // impact target/step order remains unchanged and is OPEN FTFT4B.
        const double gap = std::max(c.signedSeparation, 0.0);
        if (gap <= 0.0f) {
            c.restitutionBias = bounces ? -restitution * closingSpeed : 0.0f;
        } else {
            // Speculative contact (Milestone 32): the pair is `gap` apart.
            // It may close exactly that gap this step (v_n >= -gap/dt) but
            // not penetrate. Restitution applies only if this step's
            // approach actually reaches the surface.
            const bool reaches = fixedDeltaTime > 0.0f && -closingSpeed * fixedDeltaTime > gap;
            if (reaches && bounces) {
                c.restitutionBias = -restitution * closingSpeed;
            } else {
                c.restitutionBias = fixedDeltaTime > 0.0f ? -gap / fixedDeltaTime : 0.0f;
            }
        }
    }
    // Warm start after every restitution target has been read from the
    // untouched velocities. The friction part is re-projected onto this
    // step's tangent plane and kept inside this step's Coulomb disc.
    for (std::size_t i = 0; i < m_constraints.size(); ++i) {
        ContactConstraint& c = m_constraints[i];
        if (c.normalMass <= 0.0f) continue;
        c.normalImpulse = m_pending[i].warmNormal;
        glm::vec3 tangent = m_pending[i].warmTangent - c.normal * glm::dot(m_pending[i].warmTangent, c.normal);
        const float limit = c.friction * c.normalImpulse;
        const float magnitude = glm::length(tangent);
        if (magnitude > limit) tangent = magnitude > kEpsilon ? tangent * (limit / magnitude) : glm::vec3(0.0f);
        c.tangentImpulse = tangent;
        const glm::vec3 impulse = c.normal * c.normalImpulse + c.tangentImpulse;
        if (glm::dot(impulse, impulse) > 0.0f) {
            ApplyImpulse(c, c.velocityOffsetA, c.velocityOffsetB, impulse);
        }
    }
    m_pending.clear();
}

void ContactSolver::SolveVelocities(int iterations) {
    for (int iteration = 0; iteration < iterations; ++iteration) {
        for (ContactConstraint& c : m_constraints) {
            if (c.normalMass <= 0.0f) continue;
            const glm::vec3 rA = c.velocityOffsetA;
            const glm::vec3 rB = c.velocityOffsetB;

            // --- Normal: accumulate, clamp the total at zero, apply delta.
            const float vn = glm::dot(PointVelocity(*c.bodyA, rA) - PointVelocity(*c.bodyB, rB), c.normal);
            const float lambda = c.normalMass * (c.restitutionBias - vn);
            const float newNormal = std::max(c.normalImpulse + lambda, 0.0f);
            const float appliedNormal = newNormal - c.normalImpulse;
            c.normalImpulse = newNormal;
            if (appliedNormal != 0.0f) ApplyImpulse(c, rA, rB, c.normal * appliedNormal);

            // --- Friction: Coulomb disc against the ACCUMULATED normal
            // impulse of this point.
            const glm::vec3 relative = PointVelocity(*c.bodyA, rA) - PointVelocity(*c.bodyB, rB);
            const glm::vec3 slip = relative - c.normal * glm::dot(relative, c.normal);
            const float slipSpeed = glm::length(slip);
            const float limit = c.friction * c.normalImpulse;
            glm::vec3 newTangent = c.tangentImpulse;
            if (slipSpeed > kEpsilon) {
                const glm::vec3 direction = slip / slipSpeed;
                const float k = InverseEffectiveMass(c, rA, rB, direction);
                if (k > kEpsilon) newTangent -= direction * (slipSpeed / k);
            }
            const float magnitude = glm::length(newTangent);
            if (magnitude > limit) {
                newTangent = magnitude > kEpsilon ? newTangent * (limit / magnitude) : glm::vec3(0.0f);
            }
            const glm::vec3 appliedTangent = newTangent - c.tangentImpulse;
            c.tangentImpulse = newTangent;
            if (glm::dot(appliedTangent, appliedTangent) > 0.0f) ApplyImpulse(c, rA, rB, appliedTangent);
        }
    }
}

void ContactSolver::SolvePositions(int iterations) {
    // This pass changes translations only. Rotate precise anchors once after
    // pose integration; reuse the offsets through all position iterations.
    for (BodyFrame& frame : m_frames) {
        // Pose integration may have changed orientation. Do not use the
        // pre-integration frame without this exact represented-value check.
        if (!SameQuaternion(frame.orientation,frame.body->orientation)) {
            frame.rotation = ContactRotation(frame.body->orientation);
            frame.orientation = frame.body->orientation;
        }
        ++m_frameBuilds;
    }
    for (ContactConstraint& c : m_constraints) {
        c.positionOffsetDifference =
            m_frames[c.frameA].rotation * c.localAnchorA -
            m_frames[c.frameB].rotation * c.localAnchorB;
    }
    for (int iteration = 0; iteration < iterations; ++iteration) {
        for (ContactConstraint& c : m_constraints) {
            RigidBody& a = *c.bodyA;
            RigidBody& b = *c.bodyB;
            const float inverseMassSum = a.inverseMass + b.inverseMass;
            if (inverseMassSum <= kEpsilon) continue;
            // Both anchors coincided with the contact point at detection;
            // their current separation along the normal is how much the
            // bodies have moved apart (positive) or together since.
            const glm::dvec3 relativeAnchors =
                (glm::dvec3(a.position) - glm::dvec3(b.position)) +
                c.positionOffsetDifference;
            const double penetration = -c.signedSeparation -
                glm::dot(relativeAnchors - c.initialAnchorDifference, c.preciseNormal);
            const float correctionMagnitude = static_cast<float>(
                std::max(penetration - double(kPenetrationSlop), 0.0) * double(kPositionalCorrectionPercent));
            if (correctionMagnitude <= 0.0f) continue;
            const glm::vec3 correction = c.normal * (correctionMagnitude / inverseMassSum);
            if (!a.IsStatic()) a.position += correction * a.inverseMass;
            if (!b.IsStatic()) b.position -= correction * b.inverseMass;
        }
    }
}
