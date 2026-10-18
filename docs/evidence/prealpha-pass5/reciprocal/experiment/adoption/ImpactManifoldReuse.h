#pragma once
#include "Contacts.h"
#include <utility>
namespace JudasPhysicsDetail {
// Both native argument orders call SphereBoxPair with the same sphere-first
// Context. BoxBox and SphereSphere are deliberately outside this proof.
inline bool CanReverseSphereBoxManifold(const Shape &first, int firstPart, const Shape &second,
                                        int secondPart) {
    // Noncompound asset pointers redirect ComputeContacts to cooked geometry,
    // even for an unusual Box/Sphere descriptor. Compound simple children are
    // rebuilt as native Shape::Box/Sphere by PrimitiveAt and discard that pointer.
    if ((first.type != ShapeType::CompoundBoxes && first.asset) ||
        (second.type != ShapeType::CompoundBoxes && second.asset))
        return false;
    const auto typeAt = [](const Shape &shape, int part) {
        return shape.type == ShapeType::CompoundBoxes ? shape.boxes[std::size_t(part)].type
                                                      : shape.type;
    };
    const auto a = typeAt(first, firstPart), b = typeAt(second, secondPart);
    return (a == ShapeType::Sphere && b == ShapeType::Box) ||
           (a == ShapeType::Box && b == ShapeType::Sphere);
}
inline void ReverseSphereBoxManifold(ContactManifold &manifold) {
    for (int i = 0; i < manifold.count; ++i) {
        auto &contact = manifold.points[i];
        contact.normal = -contact.normal;
        contact.preciseNormal = -contact.preciseNormal;
        std::swap(contact.localAnchorA, contact.localAnchorB);
        std::swap(contact.localWitnessA, contact.localWitnessB);
    }
}
}
