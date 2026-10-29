#pragma once

#include "PhysicsConstraintBatch.h"
#include "RigidBody.h"
#include <vector>

// Engine-private bindings. The participant receives copies without these
// pointers; bindings live only through one physical predictor boundary.
namespace JudasPhysicsDetail {
struct ConstraintBodyBinding {
    BodyHandle handle;
    RigidBody* body = nullptr;
    float inverseMass = 0;
    glm::mat3 inverseInertia{0};
};
struct ConstraintContactBinding {
    unsigned a = 0, b = 0;
    glm::vec3 normal{0}, armA{0}, armB{0};
    float separation = 0, friction = 0, minimumNormalSpeed = 0;
    bool arrival = false;
    float* normalImpulse = nullptr;
    float* tangent1 = nullptr;
    float* tangent2 = nullptr;
};
struct ConstraintPublication {
    std::vector<unsigned char> contacts;
    std::vector<JointHandle> joints;
    bool Includes(JointHandle handle) const {
        for (const auto joint : joints)
            if (joint.id == handle.id) return true;
        return false;
    }
};
}
