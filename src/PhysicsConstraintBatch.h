#pragma once

#include "PhysicsHandles.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <functional>
#include <vector>

// Optional native material coupling. These are value-owned, per-boundary
// snapshots, not persistent solver rows or independently integrated bodies.
struct PhysicsConstraintBatch {
    struct Context {
        float stepDeltaTime = 0, deltaTime = 0;
        unsigned index = 0, count = 0;
    } context;
    struct Body {
        BodyHandle handle;
        bool worldAnchor = false;
        glm::vec3 center{0}, linearVelocity{0}, angularVelocity{0};
        glm::quat orientation{1, 0, 0, 0};
        float inverseMass = 0;
        glm::mat3 inverseInertia{0};
    };
    struct Contact {
        unsigned a = 0, b = 0;
        glm::vec3 normal{0}, armA{0}, armB{0};
        float separation = 0, minimumNormalSpeed = 0;
    };
    struct JointRow {
        JointHandle joint;
        unsigned a = 0, b = 0;
        glm::vec3 linearA{0}, linearB{0}, angularA{0}, angularB{0};
        float bias = 0;
    };
    std::vector<Body> bodies;
    std::vector<Contact> contacts;
    std::vector<JointRow> joints;
};

struct PhysicsConstraintProposal {
    // Foreign/material impulse only. Native computes physical velocity from
    // this plus the matching native reactions; callers cannot assign a pose
    // or an unrelated velocity. All arrays correspond exactly to the snapshot.
    std::vector<glm::dvec3> linearImpulses, angularImpulses;
    std::vector<float> contactImpulses, jointImpulses;
};

struct PhysicsConstraintParticipant {
    std::vector<BodyHandle> bodies;
    unsigned maximumBodies = 64, maximumContacts = 256, maximumJointRows = 256;
    // Stage foreign material state here. No world mutation, recursive stepping
    // or retained references into the snapshot. False deliberately requests
    // ordinary native response and publishes no material state. A failed live
    // material solve must throw/stop the world, never silently decline. This
    // aborts the current Step; it does not roll back earlier substeps/impacts.
    std::function<bool(const PhysicsConstraintBatch&, PhysicsConstraintProposal&)> propose;
    // Called only after native validation/publication; use a nonthrowing swap
    // of already prepared material state. No allocation or world mutation.
    std::function<void(const PhysicsConstraintBatch::Context&)> published;
};

// Initial supported native subset: frictionless non-impact contacts and rigid
// bilateral joints. The native speculative gap allowance is explicit. Unsupported friction,
// arriving contacts, joint limits,
// drives, springs and materials fail explicitly, before candidate publication.
// Legacy begin/solve/finish callbacks cannot be combined with this participant;
// afterSubstep observation remains available. This is a substep transaction,
// NOT whole-Step/world rollback or CCD coupling.
