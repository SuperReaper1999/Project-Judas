#include "WorldInternal.h"

using namespace JudasPhysicsDetail;

auto PhysicsWorld::Impl::JointInputs(const std::vector<unsigned char> *included)
    -> std::vector<JointInput> {
    std::vector<JointInput> result;
    for (auto &joint : joints) {
        joint.state.active = JointActive(joint);
        if (!joint.state.active) {
            joint.warm.fill(0);
            continue;
        }
        auto *a = Get(joint.state.settings.bodyA);
        auto *b = Get(joint.state.settings.bodyB);
        if ((a->sleeping || a->rigidBody.IsStatic()) &&
            (!b || b->sleeping || b->rigidBody.IsStatic()))
            continue;
        if (included && !(*included)[joint.state.settings.bodyA.id & kSlotMask] &&
            (!b || !(*included)[joint.state.settings.bodyB.id & kSlotMask]))
            continue;
        result.push_back({&a->rigidBody, b ? &b->rigidBody : &worldAnchor, &joint.state,
                          &joint.warm, a->shape.pivotOffset,
                          b ? b->shape.pivotOffset : glm::vec3(0)});
    }
    return result;
}

auto PhysicsWorld::Impl::PrepareJoints(JointSolver &target, const std::vector<JointInput> &inputs,
                                       float dt, bool warm, bool impact, bool append,
                                       float positionalRate, bool warmEqualityOnly,
                                       bool projection, float speculativeRate) -> void {
    PhysicsWorkTimer timer(work.jointPreparation, work.active);
    target.Prepare(inputs, dt, warm, impact, append, positionalRate, warmEqualityOnly, projection,
                   speculativeRate);
}

JointHandle PhysicsWorld::CreateJoint(const JointSettings &settings) {
    if (!m_impl || !ValidJointSettings(settings))
        return {};
    auto *a = m_impl->Get(settings.bodyA);
    auto *b = m_impl->Get(settings.bodyB);
    if (!a || (settings.bodyB.IsValid() && !b) || (b && a == b) ||
        (a->rigidBody.IsStatic() && (!b || b->rigidBody.IsStatic())))
        return {};
    static std::atomic<std::uint64_t> next{1};
    JointHandle handle{next.fetch_add(1)};
    Wake(settings.bodyA);
    Wake(settings.bodyB);
    JointState state;
    state.settings = settings;
    m_impl->joints.push_back({handle, state, {}});
    return handle;
}

bool PhysicsWorld::DestroyJoint(JointHandle handle) {
    JointState old;
    if (GetJoint(handle, old)) {
        Wake(old.settings.bodyA);
        Wake(old.settings.bodyB);
    }
    if (!m_impl)
        return false;
    auto &joints = m_impl->joints;
    auto it = std::find_if(joints.begin(), joints.end(),
                           [&](const auto &j) { return j.handle.id == handle.id; });
    if (it == joints.end())
        return false;
    joints.erase(it);
    return true;
}

bool PhysicsWorld::GetJoint(JointHandle handle, JointState &state) const {
    if (!m_impl)
        return false;
    for (auto &joint : m_impl->joints)
        if (joint.handle.id == handle.id) {
            if (!m_impl->Get(joint.state.settings.bodyA) ||
                (joint.state.settings.bodyB.IsValid() && !m_impl->Get(joint.state.settings.bodyB)))
                return false;
            state = joint.state;
            state.active = m_impl->JointActive(joint);
            return true;
        }
    return false;
}

bool PhysicsWorld::SetJointAngularDrive(JointHandle handle, const JointAngularDrive &request) {
    if (!m_impl)
        return false;
    if (!std::isfinite(
            glm::dot(request.relativeAngularVelocity, request.relativeAngularVelocity)) ||
        glm::length(request.relativeAngularVelocity) > 1000 ||
        !std::isfinite(glm::dot(request.target, request.target)) ||
        glm::dot(request.target, request.target) < 1e-12f || !std::isfinite(request.stiffness) ||
        request.stiffness < 0 || request.stiffness > 10000 || !std::isfinite(request.damping) ||
        request.damping < 0 || request.damping > 1000 || !std::isfinite(request.maxTorque) ||
        request.maxTorque < 0 || request.maxTorque > 10000)
        return false;
    for (auto &joint : m_impl->joints)
        if (joint.handle.id == handle.id) {
            const auto &settings = joint.state.settings;
            if (request.enabled &&
                (!m_impl->Get(settings.bodyB) ||
                 (settings.type != JointType::Ball && settings.type != JointType::Hinge) ||
                 settings.motor || settings.spring))
                return false;
            auto target = glm::normalize(request.target);
            auto &previous = joint.state.angularDrive;
            bool changed = previous.enabled != request.enabled ||
                           std::abs(glm::dot(previous.target, target)) < 1 - 1e-7f ||
                           previous.relativeAngularVelocity != request.relativeAngularVelocity ||
                           previous.stiffness != request.stiffness ||
                           previous.damping != request.damping ||
                           previous.maxTorque != request.maxTorque;
            if (!changed)
                return true;
            auto candidate = request;
            candidate.target = target;
            candidate.impulses = previous.impulses;
            if (previous.enabled != request.enabled)
                candidate.impulses = {0, 0, 0};
            previous = candidate;
            m_impl->Wake(settings.bodyA.id & Impl::kSlotMask);
            if (m_impl->Get(settings.bodyB))
                m_impl->Wake(settings.bodyB.id & Impl::kSlotMask);
            return true;
        }
    return false;
}

bool PhysicsWorld::SetJoint(JointHandle handle, const JointSettings &settings) {
    JointState previous;
    if (!GetJoint(handle, previous) || !ValidJointSettings(settings) ||
        settings.bodyA.id != previous.settings.bodyA.id ||
        settings.bodyB.id != previous.settings.bodyB.id)
        return false;
    for (auto &joint : m_impl->joints)
        if (joint.handle.id == handle.id) {
            if (JointSettingsEqual(joint.state.settings, settings))
                return true;
            m_impl->Wake(settings.bodyA.id & Impl::kSlotMask);
            if (m_impl->Get(settings.bodyB))
                m_impl->Wake(settings.bodyB.id & Impl::kSlotMask);
            joint.state.settings = settings;
            joint.warm.fill(0);
            if (joint.restoredContinuationDt > 0) {
                joint.state.angularDrive.impulses = {0, 0, 0};
                joint.restoredContinuationDt = 0;
            }
            return true;
        }
    return false;
}

bool PhysicsWorld::SetPairCollisionEnabled(BodyHandle a, BodyHandle b, bool enabled) {
    if (!m_impl || a.id == b.id || !m_impl->Get(a) || !m_impl->Get(b))
        return false;
    auto pair = std::minmax(a.id, b.id);
    if ((m_impl->suppressedPairs.count(pair) == 0) == enabled)
        return true;
    Wake(a);
    Wake(b);
    m_impl->InvalidateRestoredContinuation(a.id & Impl::kSlotMask);
    m_impl->InvalidateRestoredContinuation(b.id & Impl::kSlotMask);
    ++m_impl->Get(a)->contactObservationRevision;
    ++m_impl->Get(b)->contactObservationRevision;
    if (enabled)
        m_impl->suppressedPairs.erase(pair);
    else
        m_impl->suppressedPairs.insert(pair);
    return true;
}

bool PhysicsWorld::ConfigureJointMaterial(JointHandle h, const JointMaterial &material) {
    if (!m_impl || !ValidJointMaterial(material))
        return false;
    for (auto &j : m_impl->joints)
        if (j.handle.id == h.id) {
            if (j.state.settings.type != JointType::Fixed || j.state.angularDrive.enabled)
                return false;
            j.state.constitutive = {};
            j.state.constitutive.material = material;
            j.warm.fill(0);
            Wake(j.state.settings.bodyA);
            if (j.state.settings.bodyB.IsValid())
                Wake(j.state.settings.bodyB);
            return true;
        }
    return false;
}
