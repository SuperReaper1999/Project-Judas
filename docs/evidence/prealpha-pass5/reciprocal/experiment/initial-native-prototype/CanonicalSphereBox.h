#pragma once
#include "Contacts.h"
#include <utility>
namespace JudasPhysicsDetail {
// Both native argument orders call SphereBoxPair with the same sphere-first
// Context. BoxBox and SphereSphere are deliberately outside this proof.
inline bool CanReverseSphereBoxManifold(ShapeType a, ShapeType b) {
    return (a == ShapeType::Sphere && b == ShapeType::Box) ||
           (a == ShapeType::Box && b == ShapeType::Sphere);
}
inline void ReverseSphereBoxManifold(ContactManifold& manifold) {
    for (int i = 0; i < manifold.count; ++i) {
        auto& contact = manifold.points[i];
        contact.normal = -contact.normal;
        contact.preciseNormal = -contact.preciseNormal;
        std::swap(contact.localAnchorA, contact.localAnchorB);
        std::swap(contact.localWitnessA, contact.localWitnessB);
    }
}
}
