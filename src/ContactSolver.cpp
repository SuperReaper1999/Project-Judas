#include "ContactSolver.h"
#include "ContactVelocity.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

#include "RigidBody.h"

namespace {
constexpr float kEpsilon = 1.0e-6f;

// How much of the remaining penetration to correct per position iteration,
// and the small allowed overlap ("slop") left uncorrected so resting
// contacts do not fight the solver every step trying to reach exactly zero
// — unchanged from the pre-M32 solver.
constexpr float kPositionalCorrectionPercent = 0.2f;
constexpr float kPenetrationSlop = ContactSolver::kPenetrationSlop;

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
    m_blocks.clear();
    m_blockAt.clear();
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

void ContactSolver::PreservePointNormals(RigidBody& body) {
    const auto found = m_frameIndex.find(&body);
    if (found != m_frameIndex.end())
        m_frames[found->second].pointNormals = true;
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
           m_constraints.capacity()*2*sizeof(std::size_t) +
           m_blocks.capacity()*sizeof(Block) + m_blockAt.capacity()*sizeof(std::size_t);
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

void ContactSolver::Prepare(float fixedDeltaTime) { PrepareAppended(fixedDeltaTime, 0, 0); }

void ContactSolver::PrepareAppended(float fixedDeltaTime, std::size_t firstRow,
                                    std::size_t firstFrame) {
    if (firstRow > m_constraints.size() || firstFrame > m_frames.size() ||
        m_pending.size() != m_constraints.size() - firstRow)
        throw std::invalid_argument("invalid contact preparation range");
    // Frames live only within this solve. The body pointers have the same
    // lifetime contract as constraints; Clear drops every pointer/key.
    for (std::size_t index = firstFrame; index < m_frames.size(); ++index) {
        auto& frame = m_frames[index];
        const RigidBody& body = *frame.body;
        if (!frame.rotationValid || !SameQuaternion(frame.orientation,body.orientation)) {
            frame.rotation = ContactRotation(body.orientation);
            frame.orientation = body.orientation;
            frame.rotationValid = true;
        }
        frame.inverseInertia = body.IsStatic() ? glm::mat3(0.0f) : body.InverseInertiaWorld();
        ++m_frameBuilds;
    }
    for (std::size_t i = firstRow; i < m_constraints.size(); ++i) {
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
        const float restitution = m_pending[i - firstRow].restitution;
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
    for (std::size_t i = firstRow; i < m_constraints.size(); ++i) {
        ContactConstraint& c = m_constraints[i];
        if (c.normalMass <= 0.0f) continue;
        c.normalImpulse = m_pending[i - firstRow].warmNormal;
        glm::vec3 tangent = m_pending[i - firstRow].warmTangent -
                            c.normal * glm::dot(m_pending[i - firstRow].warmTangent, c.normal);
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
    if(m_blockNormals)PrepareBlocks(firstRow);
}

void ContactSolver::SetInelasticVelocityTarget(std::size_t row, float target) {
    if (row >= m_constraints.size() || !std::isfinite(target) || target > 0 ||
        m_constraints[row].restitutionBias != 0)
        throw std::invalid_argument("invalid inelastic velocity target");
    m_constraints[row].restitutionBias = target;
}

namespace {
void SolveFrictionConstraint(ContactConstraint& c) {
    const glm::vec3 rA=c.velocityOffsetA, rB=c.velocityOffsetB;
    // --- Friction: Coulomb disc against the ACCUMULATED normal
    // impulse of this point.
    const glm::vec3 relative = PointVelocity(*c.bodyA, rA) - PointVelocity(*c.bodyB, rB);
    const glm::vec3 slip = relative - c.normal * glm::dot(relative, c.normal);
    const float slipSpeed = glm::length(slip);
    const float limit = c.friction * c.normalImpulse;
    glm::vec3 newTangent = c.tangentImpulse;
    if (slipSpeed > kEpsilon) {
        const glm::vec3 direction = slip / slipSpeed;
        const float k = contact_velocity::FrictionInverseEffectiveMass(
            c.bodyA->inverseMass + c.bodyB->inverseMass, rA, rB,
            c.inverseInertiaA, c.inverseInertiaB, direction);
        if (k > kEpsilon)
            newTangent -= direction * (slipSpeed / k);
    }
    const float magnitude = glm::length(newTangent);
    if (magnitude > limit) {
        newTangent = magnitude > kEpsilon ? newTangent * (limit / magnitude) : glm::vec3(0.0f);
    }
    const glm::vec3 appliedTangent = newTangent - c.tangentImpulse;
    c.tangentImpulse = newTangent;
    if (glm::dot(appliedTangent, appliedTangent) > 0.0f)
        ApplyImpulse(c, rA, rB, appliedTangent);
 }

bool SolveVelocityConstraint(ContactConstraint& c) {
    if (c.normalMass <= 0.0f)
        return false;
    const glm::vec3 rA = c.velocityOffsetA;
    const glm::vec3 rB = c.velocityOffsetB;

    // --- Normal: accumulate, clamp the total at zero, apply delta.
    const float vn = glm::dot(PointVelocity(*c.bodyA, rA) - PointVelocity(*c.bodyB, rB), c.normal);
    const float lambda = c.normalMass * (c.restitutionBias - vn);
    const glm::vec3 oldTangent = c.tangentImpulse;
    const float oldNormal = c.normalImpulse;
    const float newNormal = std::max(c.normalImpulse + lambda, 0.0f);
    const float appliedNormal = newNormal - c.normalImpulse;
    c.normalImpulse = newNormal;
    if (appliedNormal != 0.0f)
        ApplyImpulse(c, rA, rB, c.normal * appliedNormal);

    SolveFrictionConstraint(c);
    return c.normalImpulse != oldNormal || glm::any(glm::notEqual(c.tangentImpulse, oldTangent));
}
} // namespace

void ContactSolver::PrepareBlocks(std::size_t firstRow) {
    constexpr auto absent=std::numeric_limits<std::size_t>::max();
    constexpr std::size_t budget=4*1024*1024;
    if(m_constraints.size()>budget/sizeof(std::size_t)){
        std::vector<Block>{}.swap(m_blocks);std::vector<std::size_t>{}.swap(m_blockAt);return;
    }
    if(m_blockAt.capacity()<m_constraints.size())m_blockAt.reserve(m_constraints.size());
    if (m_blocks.capacity() * sizeof(Block) + m_blockAt.capacity() * sizeof(std::size_t) > budget) {
        std::vector<Block>{}.swap(m_blocks);
        std::fill(m_blockAt.begin(), m_blockAt.end(), absent);
        firstRow = 0; // Rebuild every row mapping after releasing old blocks.
    }
    m_blockAt.resize(m_constraints.size(),absent);
    const auto eligible = [&](const ContactConstraint& c) {
        return c.normalMass > 0 && c.hasLocalAnchors && c.signedSeparation <= 0 &&
               !m_frames[c.frameA].pointNormals && !m_frames[c.frameB].pointNormals;
    };
    for(std::size_t first=firstRow;first<m_constraints.size();){
        auto& a=m_constraints[first];unsigned count=1;
        if(eligible(a))while(count<4&&first+count<m_constraints.size()){
            const auto& b=m_constraints[first+count];
            if(!eligible(b)||a.bodyA!=b.bodyA||a.bodyB!=b.bodyB)break;
            ++count;
        }
        if(count>1){
            if(m_blocks.size()==m_blocks.capacity()){
                const auto limit=(budget-m_blockAt.capacity()*sizeof(std::size_t))/sizeof(Block);
                const auto next=std::min(limit,std::max(std::size_t(4),m_blocks.size()*2));
                if(next<=m_blocks.size()){first+=count;continue;}
                m_blocks.reserve(next);
            }
            const auto id=m_blocks.size();m_blocks.emplace_back();auto& block=m_blocks.back();
            block.first=first;block.count=count;m_blockAt[first]=id;
            for(unsigned i=0;i<count;++i)for(unsigned j=i;j<count;++j){
                const auto& x=m_constraints[first+i];const auto& y=m_constraints[first+j];
                const glm::dvec3 ni(x.normal),nj(y.normal);
                const auto ai=glm::cross(glm::dvec3(x.velocityOffsetA),ni),aj=glm::cross(glm::dvec3(y.velocityOffsetA),nj);
                const auto bi=glm::cross(glm::dvec3(x.velocityOffsetB),ni),bj=glm::cross(glm::dvec3(y.velocityOffsetB),nj);
                const double value=double(x.bodyA->inverseMass+x.bodyB->inverseMass)*glm::dot(ni,nj)+
                    glm::dot(ai,glm::dmat3(x.inverseInertiaA)*aj)+glm::dot(bi,glm::dmat3(x.inverseInertiaB)*bj);
                block.response[i][j]=block.response[j][i]=value;
            }
            block.normal.Prepare(block.response,count);
        }
        first+=count;
    }
}

bool ContactSolver::SolveBlock(Block& block, std::vector<unsigned char>* seen,
                               std::vector<RigidBody*>* activated) {
    return SolveBlockState(block, m_constraints.data(), seen, activated);
}

bool ContactSolver::SolveBlockState(Block& block, ContactConstraint* rows, std::vector<unsigned char>* seen,
                               std::vector<RigidBody*>* activated) {
    contact_block::Vector previous{},q{},next{};
    for(unsigned i=0;i<block.count;++i)previous[i]=rows[block.first+i].normalImpulse;
    for(unsigned i=0;i<block.count;++i){
        const auto& c=rows[block.first+i];
        // The stored target uses the engine's represented normal velocity.
        // Re-evaluating the dot in double would create a false residual at
        // an unchanged continuing contact and spuriously expand its island.
        q[i] = double(glm::dot(PointVelocity(*c.bodyA, c.velocityOffsetA) -
                                  PointVelocity(*c.bodyB, c.velocityOffsetB), c.normal)) -
               double(c.restitutionBias);
        for(unsigned j=0;j<block.count;++j)q[i]-=block.response[i][j]*previous[j];
    }
    if(!block.normal.Solve(q,previous,next)){
        ++contact_block::diagnostics.fallbacks;return false;
    }
    std::array<float,4> represented{};
    for(unsigned i=0;i<block.count;++i){represented[i]=float(next[i]);if(!std::isfinite(represented[i])){++contact_block::diagnostics.fallbacks;return false;}}
    const auto observe=[&](ContactConstraint& c,const glm::vec3& va,const glm::vec3& wa,
                           const glm::vec3& vb,const glm::vec3& wb){
        if(!seen)return;
        if((va!=c.bodyA->linearVelocity||wa!=c.bodyA->angularVelocity)&&!c.bodyA->IsStatic()&&!(*seen)[c.frameA]){(*seen)[c.frameA]=1;if (activated) activated->push_back(c.bodyA);}
        if((vb!=c.bodyB->linearVelocity||wb!=c.bodyB->angularVelocity)&&!c.bodyB->IsStatic()&&!(*seen)[c.frameB]){(*seen)[c.frameB]=1;if (activated) activated->push_back(c.bodyB);}
    };
    // A block is one constraint operation: no other body/constraint can
    // observe its internal normal/friction updates. Publish activation from
    // its completed represented velocity delta, rather than algebraic states
    // that cancel before the operation returns. Keep every impulse and its
    // arithmetic order; point constraints retain their per-row publication.
    auto& first = rows[block.first];
    const auto beforeVA = first.bodyA->linearVelocity, beforeWA = first.bodyA->angularVelocity;
    const auto beforeVB = first.bodyB->linearVelocity, beforeWB = first.bodyB->angularVelocity;
    for (unsigned i = 0; i < block.count; ++i) {
        auto& c = rows[block.first+i];
        const float delta = represented[i] - c.normalImpulse;
        c.normalImpulse = represented[i];
        if (delta != 0) ApplyImpulse(c, c.velocityOffsetA, c.velocityOffsetB, c.normal * delta);
    }
    for (unsigned i = 0; i < block.count; ++i)
        SolveFrictionConstraint(rows[block.first+i]);
    observe(first, beforeVA, beforeWA, beforeVB, beforeWB);
    ++contact_block::diagnostics.solves;
    return true;
}

void ContactSolver::SolveVelocities(int iterations) {
    for (int iteration = 0; iteration < iterations; ++iteration)
        for(std::size_t i=0;i<m_constraints.size();){
            if(m_blockNormals&&i<m_blockAt.size()&&m_blockAt[i]!=std::numeric_limits<std::size_t>::max()){
                auto& block=m_blocks[m_blockAt[i]];
                if(SolveBlock(block,nullptr,nullptr)){i+=block.count;continue;}
            }
            SolveVelocityConstraint(m_constraints[i++]);
        }
}

void ContactSolver::SolveTrackedVelocities(int iterations, std::vector<RigidBody*>& activated) {
    SolveTrackedVelocityRange(0, iterations, activated);
}

void ContactSolver::SolveTrackedVelocityRange(std::size_t firstRow, int iterations,
                                              std::vector<RigidBody*>& activated) {
    std::vector<unsigned char> seen(m_frames.size(), 0);
    for (auto* body : activated) {
        const auto found = m_frameIndex.find(body);
        if (found != m_frameIndex.end())
            seen[found->second] = 1;
    }
    for (int iteration = 0; iteration < iterations; ++iteration)
        for (std::size_t index = firstRow; index < m_constraints.size(); ++index) {
            if(m_blockNormals&&index<m_blockAt.size()&&m_blockAt[index]!=std::numeric_limits<std::size_t>::max()){
                auto& block=m_blocks[m_blockAt[index]];
                if(SolveBlock(block,&seen,&activated)){index+=block.count-1;continue;}
            }
            auto& c = m_constraints[index];
            const auto linearA = c.bodyA->linearVelocity, angularA = c.bodyA->angularVelocity;
            const auto linearB = c.bodyB->linearVelocity, angularB = c.bodyB->angularVelocity;
            if (!SolveVelocityConstraint(c))
                continue;
            // Downstream constraints depend on represented body velocities, not
            // merely on a row's accumulator changing. Check after EACH row, so
            // a transient response cancelled by a later row is still discovered.
            // Exact equality, no magnitude cutoff. A rounded-away impulse cannot
            // change an unqueried neighbour's constraint velocity.
            const bool changedA =
                linearA != c.bodyA->linearVelocity || angularA != c.bodyA->angularVelocity;
            const bool changedB =
                linearB != c.bodyB->linearVelocity || angularB != c.bodyB->angularVelocity;
            if (changedA && !c.bodyA->IsStatic() && !seen[c.frameA]) {
                seen[c.frameA] = 1;
                activated.push_back(c.bodyA);
            }
            if (changedB && !c.bodyB->IsStatic() && !seen[c.frameB]) {
                seen[c.frameB] = 1;
                activated.push_back(c.bodyB);
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
