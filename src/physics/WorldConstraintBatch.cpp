#include "WorldInternal.h"

#include <stdexcept>
#include <unordered_map>

using namespace JudasPhysicsDetail;

namespace {
bool Finite(glm::dvec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
[[noreturn]] void Unsupported(const char* detail) {
    throw std::runtime_error(std::string("physical constraint batch: ") + detail);
}
}

ConstraintPublication PhysicsWorld::Impl::ProjectConstraintBatch(
    const PhysicsConstraintParticipant& participant,
    const PhysicsConstraintBatch::Context& context,
    const std::vector<ConstraintBodyBinding>& preparedBodies,
    const std::vector<ConstraintContactBinding>& preparedContacts) {
    JUDAS_PROFILE_SCOPE("Physics material constraint batch");
    ConstraintPublication publication;
    publication.contacts.assign(preparedContacts.size(), 0);
    auto bindings = preparedBodies;
    std::unordered_map<unsigned, unsigned> byHandle;
    for (unsigned i = 0; i < bindings.size(); ++i)
        byHandle.emplace(bindings[i].handle.id, i);

    // Expand through dynamic bodies only. A common static floor does not join
    // otherwise independent assemblies into one material solve.
    std::set<unsigned> selected;
    std::vector<unsigned> pending;
    const auto include = [&](BodyHandle handle) {
        auto found = byHandle.find(handle.id);
        if (found == byHandle.end()) {
            auto* body = Get(handle);
            if (!body || !body->rigidBody.IsStatic())
                Unsupported("participant missing from the awake physical predictor");
            const unsigned index = unsigned(bindings.size());
            bindings.push_back({handle, &body->rigidBody, 0, glm::mat3(0)});
            found = byHandle.emplace(handle.id, index).first;
        }
        if (selected.insert(found->second).second && bindings[found->second].inverseMass > 0)
            pending.push_back(found->second);
        if (selected.size() > participant.maximumBodies)
            Unsupported("connected body count exceeds the declared bound");
    };
    for (const auto handle : participant.bodies) include(handle);
    std::vector<unsigned char> selectedJoints(joints.size(), 0);
    for (std::size_t cursor = 0; cursor < pending.size(); ++cursor) {
        const unsigned current = pending[cursor];
        for (unsigned i = 0; i < preparedContacts.size(); ++i) {
            const auto& contact = preparedContacts[i];
            if (contact.a != current && contact.b != current) continue;
            publication.contacts[i] = 1;
            include(bindings[contact.a].handle);
            include(bindings[contact.b].handle);
        }
        const auto handle = bindings[current].handle;
        for (unsigned i = 0; i < joints.size(); ++i) {
            const auto& joint = joints[i];
            const auto& settings = joint.state.settings;
            if (!JointActive(joint) ||
                (settings.bodyA.id != handle.id && settings.bodyB.id != handle.id)) continue;
            selectedJoints[i] = 1;
            include(settings.bodyA);
            if (settings.bodyB.IsValid()) include(settings.bodyB);
        }
    }

    PhysicsConstraintBatch batch;
    batch.context = context;
    std::vector<unsigned> local(bindings.size(), unsigned(-1));
    std::vector<RigidBody*> bodyPointers;
    for (const auto index : selected) {
        const auto& binding = bindings[index];
        const auto& body = *binding.body;
        local[index] = unsigned(batch.bodies.size());
        batch.bodies.push_back({binding.handle, false, body.position,
            body.linearVelocity, body.angularVelocity, body.orientation,
            binding.inverseMass, binding.inverseInertia});
        bodyPointers.push_back(binding.body);
    }
    std::vector<unsigned> contactIndices;
    for (unsigned i = 0; i < preparedContacts.size(); ++i) {
        if (!publication.contacts[i]) continue;
        const auto& contact = preparedContacts[i];
        if (contact.friction != 0) Unsupported("frictional contacts are not yet supported");
        if (contact.arrival)
            Unsupported("arriving contacts require chronological material coupling");
        if (batch.contacts.size() == participant.maximumContacts)
            Unsupported("contact count exceeds the declared bound");
        batch.contacts.push_back({local[contact.a], local[contact.b], contact.normal,
            contact.armA, contact.armB, contact.separation, contact.minimumNormalSpeed});
        contactIndices.push_back(i);
    }

    // Snapshot preparation owns all side effects. Declining or rejecting a
    // proposal must not change native joint history, material state or caches.
    std::vector<JointState> states;
    std::vector<std::array<float, 12>> warm;
    std::vector<unsigned> jointIndices;
    std::vector<JointHandle> jointIdentities;
    std::vector<std::array<float, 12>> previousWarm;
    for (unsigned i = 0; i < joints.size(); ++i) {
        if (!selectedJoints[i]) continue;
        const auto& joint = joints[i];
        const auto& settings = joint.state.settings;
        if (settings.limits || settings.spring || settings.motor ||
            settings.rotationalResistance != 0 || joint.state.angularDrive.enabled ||
            joint.state.constitutive.material.enabled)
            Unsupported("joint limits, drives, springs or materials are not yet supported");
        states.push_back(joint.state);
        warm.push_back({});
        jointIndices.push_back(i);
        jointIdentities.push_back(joint.handle);
        previousWarm.push_back(joint.warm);
    }
    std::vector<JointInput> inputs;
    for (unsigned i = 0; i < jointIndices.size(); ++i) {
        const auto& settings = states[i].settings;
        auto* a = Get(settings.bodyA);
        auto* b = Get(settings.bodyB);
        inputs.push_back({&a->rigidBody, b ? &b->rigidBody : &worldAnchor,
            &states[i], &warm[i], a->shape.pivotOffset,
            b ? b->shape.pivotOffset : glm::vec3(0)});
    }
    JointSolver snapshot;
    PrepareJoints(snapshot, inputs, context.deltaTime, false, false, false, 0, false, false);
    const auto jointRows = snapshot.ReadVelocityRows();
    const auto frame = [&](RigidBody* pointer) {
        for (unsigned i = 0; i < bodyPointers.size(); ++i)
            if (bodyPointers[i] == pointer) return i;
        if (pointer != &worldAnchor) Unsupported("joint row escaped its connected body snapshot");
        if (batch.bodies.size() == participant.maximumBodies)
            Unsupported("world anchor exceeds the declared body bound");
        const unsigned index = unsigned(batch.bodies.size());
        batch.bodies.push_back({{}, true, pointer->position,
            pointer->linearVelocity, pointer->angularVelocity, pointer->orientation, 0, glm::mat3(0)});
        bodyPointers.push_back(pointer);
        return index;
    };
    for (const auto& row : jointRows) {
        if (row.gamma != 0 || !std::isinf(row.low) || !std::isinf(row.high))
            Unsupported("prepared joint row is not rigid bilateral");
        if (batch.joints.size() == participant.maximumJointRows)
            Unsupported("joint row count exceeds the declared bound");
        // Rows preserve preparation order. The identity is the owning joint,
        // never a native pointer or index intended to survive this callback.
        JointHandle identity;
        for (unsigned i = 0; i < inputs.size(); ++i)
            if (inputs[i].state == row.owner) {
                identity = joints[jointIndices[i]].handle;
                break;
            }
        batch.joints.push_back({identity, frame(row.a), frame(row.b),
            row.linearA, row.linearB, row.angularA, row.angularB, row.bias});
    }

    std::vector<glm::mat3> previousLocalInertia;
    std::vector<float> previousInverseMass;
    for (const auto* body : bodyPointers) {
        previousLocalInertia.push_back(body->inverseInertiaLocal);
        previousInverseMass.push_back(body->inverseMass);
    }
    const auto* sourceBodies = bodies.data();
    const auto sourceBodyCount = bodies.size(), sourceAliveCount = aliveSlots.size();
    const auto* sourceJoints = joints.data();
    const auto sourceJointCount = joints.size();
    std::vector<uint64_t> sourceRevisions;
    std::vector<std::pair<float, float>> sourceMaterials;
    for (const auto& body : batch.bodies) {
        const auto* live = body.worldAnchor ? nullptr : Get(body.handle);
        sourceRevisions.push_back(live ? live->contactObservationRevision : 0);
        sourceMaterials.emplace_back(live ? live->friction : 0, live ? live->restitution : 0);
    }
    PhysicsConstraintProposal proposal;
    const bool handled = participant.propose(batch, proposal);
    if (bodies.data() != sourceBodies || bodies.size() != sourceBodyCount || aliveSlots.size() != sourceAliveCount ||
        joints.data() != sourceJoints || joints.size() != sourceJointCount)
        Unsupported("world topology/storage changed during proposal");
    // Declining is not permission to mutate the world either: native rows will
    // still be used by the ordinary solve immediately after this callback.
    for (unsigned i = 0; i < batch.bodies.size(); ++i) {
        const auto& body = batch.bodies[i];
        if (body.worldAnchor) continue;
        const auto* live = Get(body.handle);
        if (!live || &live->rigidBody != bodyPointers[i])
            Unsupported("body lifetime/storage changed during proposal");
        const auto& current = live->rigidBody;
        if (current.position != body.center || current.orientation != body.orientation ||
            current.linearVelocity != body.linearVelocity || current.angularVelocity != body.angularVelocity ||
            current.inverseMass != previousInverseMass[i] || current.inverseInertiaLocal != previousLocalInertia[i] ||
            live->contactObservationRevision != sourceRevisions[i] || !live->enabled || live->simulationSuspended ||
            live->friction != sourceMaterials[i].first || live->restitution != sourceMaterials[i].second)
            Unsupported("world mutation during a constraint proposal");
    }
    for (unsigned i = 0; i < jointIndices.size(); ++i) {
        const auto index = jointIndices[i];
        if (index >= joints.size() || joints[index].handle.id != jointIdentities[i].id ||
            !JointSettingsEqual(joints[index].state.settings, states[i].settings) ||
            joints[index].warm != previousWarm[i] || joints[index].state.angularDrive.enabled ||
            joints[index].state.constitutive.material.enabled)
            Unsupported("joint lifetime/configuration changed during proposal");
    }
    if (!handled) {
        publication.contacts.assign(preparedContacts.size(), 0);
        return publication;
    }
    if (proposal.linearImpulses.size() != batch.bodies.size() ||
        proposal.angularImpulses.size() != batch.bodies.size() ||
        proposal.contactImpulses.size() != batch.contacts.size() ||
        proposal.jointImpulses.size() != batch.joints.size())
        Unsupported("proposal array sizes do not match the boundary snapshot");
    auto linear = proposal.linearImpulses, angular = proposal.angularImpulses;
    for (unsigned i = 0; i < linear.size(); ++i)
        if (!Finite(linear[i]) || !Finite(angular[i])) Unsupported("nonfinite material impulse");
    for (unsigned i = 0; i < batch.contacts.size(); ++i) {
        const double impulse = proposal.contactImpulses[i];
        if (!std::isfinite(impulse) || impulse < 0) Unsupported("invalid normal contact impulse");
        const auto& row = batch.contacts[i];
        const glm::dvec3 force = glm::dvec3(row.normal) * impulse;
        linear[row.a] += force; linear[row.b] -= force;
        angular[row.a] += glm::cross(glm::dvec3(row.armA), force);
        angular[row.b] -= glm::cross(glm::dvec3(row.armB), force);
    }
    for (unsigned i = 0; i < batch.joints.size(); ++i) {
        const double impulse = proposal.jointImpulses[i];
        if (!std::isfinite(impulse)) Unsupported("nonfinite joint impulse");
        const auto& row = batch.joints[i];
        linear[row.a] += glm::dvec3(row.linearA) * impulse;
        linear[row.b] += glm::dvec3(row.linearB) * impulse;
        angular[row.a] += glm::dvec3(row.angularA) * impulse;
        angular[row.b] += glm::dvec3(row.angularB) * impulse;
    }
    std::vector<glm::vec3> nextLinear, nextAngular;
    double velocityScale = 1;
    for (unsigned i = 0; i < batch.bodies.size(); ++i) {
        const auto& body = batch.bodies[i];
        const auto v = glm::dvec3(body.linearVelocity) + double(body.inverseMass) * linear[i];
        const auto w = glm::dvec3(body.angularVelocity) + glm::dmat3(body.inverseInertia) * angular[i];
        if (!Finite(v) || !Finite(w) || !Finite(glm::dvec3(glm::vec3(v))) || !Finite(glm::dvec3(glm::vec3(w))))
            Unsupported("nonfinite physical response");
        nextLinear.emplace_back(v); nextAngular.emplace_back(w);
        velocityScale = std::max(velocityScale, glm::length(v) + glm::length(w));
    }
    const double tolerance = 64 * std::numeric_limits<float>::epsilon() * velocityScale;
    const auto rate = [&](unsigned a, unsigned b, glm::vec3 la, glm::vec3 lb,
                          glm::vec3 aa, glm::vec3 ab) {
        return glm::dot(glm::dvec3(la), glm::dvec3(nextLinear[a])) +
            glm::dot(glm::dvec3(lb), glm::dvec3(nextLinear[b])) +
            glm::dot(glm::dvec3(aa), glm::dvec3(nextAngular[a])) +
            glm::dot(glm::dvec3(ab), glm::dvec3(nextAngular[b]));
    };
    for (unsigned i = 0; i < batch.contacts.size(); ++i) {
        const auto& row = batch.contacts[i];
        const double speed = rate(row.a, row.b, row.normal, -row.normal,
            glm::cross(row.armA, row.normal), -glm::cross(row.armB, row.normal)) - row.minimumNormalSpeed;
        if (speed < -tolerance || (proposal.contactImpulses[i] > 0 && speed > tolerance))
            Unsupported("proposal violates native normal complementarity");
    }
    for (const auto& row : batch.joints)
        if (std::abs(rate(row.a, row.b, row.linearA, row.linearB, row.angularA, row.angularB) + row.bias) > tolerance)
            Unsupported("proposal violates a native joint constraint");

    // Every candidate check precedes every native write. Foreign state commits
    // through published only after the same impulse sum owns native velocities.
    snapshot.RecordProjectedImpulses(proposal.jointImpulses);
    for (unsigned i = 0; i < jointIndices.size(); ++i) {
        auto& joint = joints[jointIndices[i]];
        joint.warm = warm[i];
        joint.state.coordinate = states[i].coordinate;
        publication.joints.push_back(joint.handle);
    }
    for (unsigned i = 0; i < bodyPointers.size(); ++i) {
        if (!(batch.bodies[i].inverseMass > 0)) continue;
        bodyPointers[i]->linearVelocity = nextLinear[i];
        bodyPointers[i]->angularVelocity = nextAngular[i];
    }
    for (unsigned i = 0; i < contactIndices.size(); ++i) {
        const auto& row = preparedContacts[contactIndices[i]];
        *row.normalImpulse = proposal.contactImpulses[i];
        *row.tangent1 = *row.tangent2 = 0;
    }
    if (participant.published) participant.published(context);
    if (bodies.data() != sourceBodies || bodies.size() != sourceBodyCount || aliveSlots.size() != sourceAliveCount ||
        joints.data() != sourceJoints || joints.size() != sourceJointCount)
        Unsupported("world topology/storage changed during material publication");
    return publication;
}
