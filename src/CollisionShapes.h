#pragma once

#include <glm/glm.hpp>

// The small, closed set of collision primitives Judas's own physics needs
// for Project Judas's purpose (large spheres, flat connectors, simple
// dynamic test bodies, a capsule-shaped player query surface) — not a
// general shape system. A shape carries no position/orientation of its
// own; those live on the RigidBody (or, for the player, the caller-
// supplied pose passed to a sweep query) that references it.
enum class ShapeType { Sphere, Box, Capsule };

struct Shape {
    ShapeType type = ShapeType::Sphere;
    float radius = 0.0f;          // Sphere, Capsule (radius of the rounded part)
    float halfHeight = 0.0f;      // Capsule only: half-height of the cylindrical
                                   // segment along LOCAL +Y (total capsule height
                                   // is 2*(halfHeight+radius)) -- matches the
                                   // existing player-capsule convention.
    glm::vec3 halfExtents{0.0f};  // Box only

    static Shape Sphere(float r) {
        Shape s;
        s.type = ShapeType::Sphere;
        s.radius = r;
        return s;
    }
    static Shape Box(const glm::vec3& halfExtents) {
        Shape s;
        s.type = ShapeType::Box;
        s.halfExtents = halfExtents;
        return s;
    }
    static Shape Capsule(float radius, float halfHeight) {
        Shape s;
        s.type = ShapeType::Capsule;
        s.radius = radius;
        s.halfHeight = halfHeight;
        return s;
    }
};
