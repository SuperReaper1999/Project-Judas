#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Broadphase.h"
#include "CollisionShapes.h"
#include "Contacts.h"
#include "ContactPreparedGeometry.h"
#include "RigidBody.h"

class RadialTerrain;
struct TerrainSample;

// Milestone 32: the narrowphase, factored out of PhysicsWorld.cpp unchanged
// so that PhysicsWorld (through its broadphase) and the brute-force oracle
// in the broadphase tests/benchmark run literally the same contact code.
// The narrowphase remains the sole authority on whether two shapes touch.

// Compound bodies are a set of boxes sharing one rigid body; each primitive
// is tested as the ordinary shape it is.
int PrimitiveCount(const Shape& shape);
struct PrimitivePose {
    Shape shape;
    RigidBody body; // legacy approximate world pose for terrain/player queries
    glm::vec3 parentPosition{0.0f};
    glm::quat parentOrientation{1.0f,0.0f,0.0f,0.0f};
    glm::vec3 parentLocalCenter{0.0f};
    glm::quat childRotation{1,0,0,0};
    uint32_t childKey=0;
};
PrimitivePose PrimitiveAt(const Shape& shape, const RigidBody& parent, int index,
                          const ContactPreparedOrientation* prepared = nullptr);

// Precise primitive dispatcher preserves child offsets before world rounding.
ContactManifold ComputeContacts(const PrimitivePose& a, const PrimitivePose& b,
                                float margin = 0.0f,
                                const ContactPreparedOrientation* preparedA = nullptr,
                                const ContactPreparedOrientation* preparedB = nullptr);

// A terrain sample expressed in simulation space for a terrain body's pose.
TerrainSample SampleTerrainAtWorld(const RadialTerrain& terrain, const RigidBody& body,
                                   const glm::vec3& worldPoint);

// Contacts between two primitive shapes at the given poses (normal follows
// Contact's "separates A from B, toward A" convention). `margin` > 0 also
// reports speculative contacts separated by at most that distance
// (negative penetration; see Contacts.h). 0 = overlapping pairs only.
ContactManifold ComputeContacts(const Shape& shapeA, const RigidBody& bodyA,
                                const Shape& shapeB, const RigidBody& bodyB, float margin = 0.0f);

// Outward-rounded conservative shape bound (terrain: its conservative
// radial bound). Used only for broadphase candidate generation.
Aabb ShapeAabb(const Shape& shape, const glm::vec3& position, const glm::quat& orientation);
// Callers rebuild after an exact shape/orientation change. The original
// uncached overload remains the independent arithmetic/cache comparison path.
PreparedShapeBounds PrepareShapeBounds(const Shape& shape,
                                      const ContactPreparedOrientation& orientation);
// Rebuild invalid contents while retaining child storage; caller owns lifetime.
void PrepareShapeBounds(const Shape& shape, const ContactPreparedOrientation& orientation,
                        PreparedShapeBounds& prepared);
Aabb ShapeAabb(const PreparedShapeBounds& prepared, const glm::vec3& position);
// Largest distance from the body origin to any point of the shape — a bound
// valid for every orientation (used to bound rotation during a step).
float ShapeBoundingRadius(const Shape& shape);
