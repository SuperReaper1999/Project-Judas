#include "Narrowphase.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "RadialTerrain.h"

int PrimitiveCount(const Shape& shape) {
    return shape.type == ShapeType::CompoundBoxes ? static_cast<int>(shape.boxes.size()) : 1;
}

PrimitivePose PrimitiveAt(const Shape& shape, const RigidBody& parent, int index) {
    if (shape.type != ShapeType::CompoundBoxes) return {shape, parent};
    const CompoundBox& child = shape.boxes[static_cast<std::size_t>(index)];
    RigidBody childPose = parent;
    childPose.position += parent.orientation * child.localCenter;
    return {Shape::Box(child.halfExtents), childPose};
}

TerrainSample SampleTerrainAtWorld(const RadialTerrain& terrain, const RigidBody& body,
                                   const glm::vec3& worldPoint) {
    const glm::quat inverseRotation = glm::conjugate(glm::normalize(body.orientation));
    TerrainSample sample = terrain.Sample(inverseRotation * (worldPoint - body.position));
    sample.surfacePoint = body.position + body.orientation * sample.surfacePoint;
    sample.outwardNormal = glm::normalize(body.orientation * sample.outwardNormal);
    return sample;
}

namespace {

// Normals follow Contact's shape-A convention. For terrain A the normal
// points *into* the terrain, so resolving dynamic B pushes it outward.
// Each box contributes its corners and face centres: a hill can contact the
// middle of a broad face even while all four corners clear the surface.
ContactManifold TerrainVsPrimitive(const Shape& terrainShape, const RigidBody& terrainBody,
                                   const Shape& otherShape, const RigidBody& otherBody, float margin) {
    ContactManifold manifold;
    if (!terrainShape.terrain) return manifold;
    const RadialTerrain& terrain = *terrainShape.terrain;
    float boundingRadius = 0.0f;
    if (otherShape.type == ShapeType::Sphere) boundingRadius = otherShape.radius;
    else if (otherShape.type == ShapeType::Box) boundingRadius = glm::length(otherShape.halfExtents);
    else return manifold;
    if (glm::length(otherBody.position - terrainBody.position) >
        terrain.BoundRadius() + boundingRadius + margin) return manifold;

    if (otherShape.type == ShapeType::Sphere) {
        const TerrainSample sample = SampleTerrainAtWorld(terrain, terrainBody, otherBody.position);
        if (sample.signedDistance < otherShape.radius + margin) {
            Contact contact;
            contact.hit = true;
            contact.point = sample.surfacePoint;
            contact.normal = -sample.outwardNormal;
            contact.penetration = otherShape.radius - sample.signedDistance;
            manifold.Add(contact);
        }
        return manifold;
    }

    std::array<Contact, 14> candidates{};
    int count = 0;
    const glm::vec3 h = otherShape.halfExtents;
    const auto tryPoint = [&](const glm::vec3& localPoint) {
        const glm::vec3 worldPoint = otherBody.position + otherBody.orientation * localPoint;
        const TerrainSample sample = SampleTerrainAtWorld(terrain, terrainBody, worldPoint);
        if (sample.signedDistance >= margin) return;
        Contact contact;
        contact.hit = true;
        contact.point = sample.surfacePoint;
        contact.normal = -sample.outwardNormal;
        contact.penetration = -sample.signedDistance;
        candidates[static_cast<std::size_t>(count++)] = contact;
    };
    for (int x : {-1, 1})
        for (int y : {-1, 1})
            for (int z : {-1, 1})
                tryPoint(glm::vec3(x * h.x, y * h.y, z * h.z));
    for (int axis = 0; axis < 3; ++axis) {
        for (int sign : {-1, 1}) {
            glm::vec3 point(0.0f);
            point[axis] = sign * h[axis];
            tryPoint(point);
        }
    }
    std::sort(candidates.begin(), candidates.begin() + count,
              [](const Contact& a, const Contact& b) { return a.penetration > b.penetration; });
    for (int i = 0; i < std::min(count, 4); ++i) manifold.Add(candidates[static_cast<std::size_t>(i)]);
    return manifold;
}

}  // namespace

// Uniform manifold dispatcher: sphere-involving pairs always produce at
// most one contact point (wrapped in a 1-point manifold); box-vs-box uses
// the real multi-point manifold (see Contacts.h for why that one
// specifically needs more than one point).
ContactManifold ComputeContacts(const Shape& shapeA, const RigidBody& bodyA, const Shape& shapeB,
                                const RigidBody& bodyB, float margin) {
    ContactManifold manifold;
    if (shapeA.type == ShapeType::Terrain) {
        return TerrainVsPrimitive(shapeA, bodyA, shapeB, bodyB, margin);
    }
    if (shapeB.type == ShapeType::Terrain) {
        manifold = TerrainVsPrimitive(shapeB, bodyB, shapeA, bodyA, margin);
        for (int i = 0; i < manifold.count; ++i) manifold.points[i].normal = -manifold.points[i].normal;
        return manifold;
    }
    if (shapeA.type == ShapeType::Sphere && shapeB.type == ShapeType::Sphere) {
        manifold.Add(SphereVsSphere(bodyA.position, shapeA.radius, bodyB.position, shapeB.radius, margin));
        return manifold;
    }
    if (shapeA.type == ShapeType::Sphere && shapeB.type == ShapeType::Box) {
        manifold.Add(SphereVsBox(bodyA.position, shapeA.radius, bodyB.position, bodyB.orientation,
                                  shapeB.halfExtents, margin));
        return manifold;
    }
    if (shapeA.type == ShapeType::Box && shapeB.type == ShapeType::Sphere) {
        Contact contact = SphereVsBox(bodyB.position, shapeB.radius, bodyA.position,
                                       bodyA.orientation, shapeA.halfExtents, margin);
        if (contact.hit) contact.normal = -contact.normal;  // keep the "points toward A" convention
        manifold.Add(contact);
        return manifold;
    }
    if (shapeA.type == ShapeType::Box && shapeB.type == ShapeType::Box) {
        return BoxVsBoxManifold(bodyA.position, bodyA.orientation, shapeA.halfExtents, bodyB.position,
                                 bodyB.orientation, shapeB.halfExtents, margin);
    }
    return manifold;  // capsules never appear as world bodies -- only as the player's query shape
}

namespace {
Aabb BoxAabb(const glm::vec3& center, const glm::quat& orientation, const glm::vec3& halfExtents) {
    const glm::mat3 r = glm::mat3_cast(orientation);
    // Row-wise |R| * h: the extent of an oriented box along each world axis.
    const glm::vec3 extent(
        std::abs(r[0][0]) * halfExtents.x + std::abs(r[1][0]) * halfExtents.y + std::abs(r[2][0]) * halfExtents.z,
        std::abs(r[0][1]) * halfExtents.x + std::abs(r[1][1]) * halfExtents.y + std::abs(r[2][1]) * halfExtents.z,
        std::abs(r[0][2]) * halfExtents.x + std::abs(r[1][2]) * halfExtents.y + std::abs(r[2][2]) * halfExtents.z);
    return Aabb{center - extent, center + extent};
}
}  // namespace

Aabb ShapeAabb(const Shape& shape, const glm::vec3& position, const glm::quat& orientation) {
    switch (shape.type) {
        case ShapeType::Sphere:
            return Aabb{position - glm::vec3(shape.radius), position + glm::vec3(shape.radius)};
        case ShapeType::Box:
            return BoxAabb(position, orientation, shape.halfExtents);
        case ShapeType::CompoundBoxes: {
            Aabb bound{position, position};
            for (std::size_t i = 0; i < shape.boxes.size(); ++i) {
                const CompoundBox& child = shape.boxes[i];
                const Aabb childBound = BoxAabb(position + orientation * child.localCenter,
                                                orientation, child.halfExtents);
                bound = i == 0 ? childBound : bound.Union(childBound);
            }
            return bound;
        }
        case ShapeType::Terrain: {
            const float r = shape.terrain ? shape.terrain->BoundRadius() : 0.0f;
            return Aabb{position - glm::vec3(r), position + glm::vec3(r)};
        }
        case ShapeType::Capsule: {
            const float r = shape.radius + shape.halfHeight;
            return Aabb{position - glm::vec3(r), position + glm::vec3(r)};
        }
    }
    return Aabb{position, position};
}

float ShapeBoundingRadius(const Shape& shape) {
    switch (shape.type) {
        case ShapeType::Sphere: return shape.radius;
        case ShapeType::Box: return glm::length(shape.halfExtents);
        case ShapeType::CompoundBoxes: {
            float r = 0.0f;
            for (const CompoundBox& child : shape.boxes) {
                r = std::max(r, glm::length(child.localCenter) + glm::length(child.halfExtents));
            }
            return r;
        }
        case ShapeType::Terrain: return shape.terrain ? shape.terrain->BoundRadius() : 0.0f;
        case ShapeType::Capsule: return shape.radius + shape.halfHeight;
    }
    return 0.0f;
}
