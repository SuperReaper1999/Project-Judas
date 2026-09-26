#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "CollisionShapes.h"

// A single-point approximation of a contact between two convex shapes.
// Chosen deliberately over a full clipped manifold (multiple points per
// contact) — Project Judas's own bodies (a handful of slow dynamic
// boxes/spheres resting on curved or flat surfaces) don't need
// multi-point stability the way a tall stacked tower would; a single point
// plus a few solver iterations and positional correction (see
// ContactSolver.h) is the smallest mechanism that holds bodies still at
// rest and resolves pushes/collisions correctly for this demo. Documented
// here as a real, known simplification — not hidden — so a future
// milestone with a genuine multi-point-stability requirement knows exactly
// what to revisit.
struct Contact {
    bool hit = false;
    glm::vec3 point{0.0f};    // world-space, roughly the overlap region's center
    glm::vec3 normal{0.0f};   // world-space unit vector; SEPARATES shape A from shape B
                               // (push A along +normal, B along -normal, to resolve)
    float penetration = 0.0f;  // positive = currently overlapping by this much
};

// Closest point on an axis-aligned-in-its-own-frame box (an OBB, given by
// center/orientation/halfExtents) to an arbitrary world-space point.
glm::vec3 ClosestPointOnOBB(const glm::vec3& point, const glm::vec3& boxCenter,
                             const glm::quat& boxOrientation, const glm::vec3& halfExtents);

// Closest point on a line segment [a,b] to an arbitrary world-space point.
glm::vec3 ClosestPointOnSegment(const glm::vec3& point, const glm::vec3& a, const glm::vec3& b);

// Closest pair of points between a line segment (a capsule's own core) and
// an OBB, found by a small fixed number of alternating projections — exact
// closed-form segment-vs-box closest-point formulas exist but are
// considerably more code for the same answer on two convex shapes; this
// converges to the same result in a handful of iterations and is far
// easier to verify by inspection.
void ClosestPointsSegmentToOBB(const glm::vec3& segA, const glm::vec3& segB,
                                const glm::vec3& boxCenter, const glm::quat& boxOrientation,
                                const glm::vec3& halfExtents, glm::vec3& outSegmentPoint,
                                glm::vec3& outBoxPoint);

// Discrete narrowphase pair tests. Each returns a Contact with `hit=false`
// if the shapes are not currently overlapping.
//
// Milestone 32: `margin` (default 0 = overlap only, the pre-M32 answer)
// also reports a pair whose separation is at most `margin` — a speculative
// contact, with `penetration` = -separation (negative). PhysicsWorld passes
// the distance the pair can close within the current fixed step, so a body
// placed exactly touching (or about to touch) is constrained before it
// penetrates instead of one step after.
Contact SphereVsSphere(const glm::vec3& centerA, float radiusA, const glm::vec3& centerB,
                        float radiusB, float margin = 0.0f);
Contact SphereVsBox(const glm::vec3& sphereCenter, float sphereRadius, const glm::vec3& boxCenter,
                     const glm::quat& boxOrientation, const glm::vec3& boxHalfExtents, float margin = 0.0f);

// Single-point box-vs-box (kept for simple callers/tests that only need a
// yes/no + one representative point) — see BoxVsBoxManifold below for the
// version PhysicsWorld's own solver actually uses.
Contact BoxVsBox(const glm::vec3& centerA, const glm::quat& orientA, const glm::vec3& halfExtentsA,
                  const glm::vec3& centerB, const glm::quat& orientB,
                  const glm::vec3& halfExtentsB, float margin = 0.0f);

// Up to 4 simultaneous contact points between two boxes. Most shape pairs
// in this engine need only one point (a sphere touches a box at exactly
// one place) — but two boxes resting flush against each other (a crate on
// a flat plank, say) need every corner that's actually penetrating, or an
// impulse applied at just one arbitrarily-chosen corner injects spurious
// torque and the box slowly rocks/tips instead of settling flat. Fixed
// capacity, no heap allocation: 4 is a hard geometric ceiling for two
// boxes' worth of vertices.
struct ContactManifold {
    Contact points[4];
    int count = 0;
    void Add(const Contact& contact) {
        if (count < 4) points[count++] = contact;
    }
};
ContactManifold BoxVsBoxManifold(const glm::vec3& centerA, const glm::quat& orientA,
                                  const glm::vec3& halfExtentsA, const glm::vec3& centerB,
                                  const glm::quat& orientB, const glm::vec3& halfExtentsB,
                                  float margin = 0.0f);

// Distance from a capsule (defined by its own core segment endpoints and
// radius) to a sphere or a box, along with the closest point ON THE OTHER
// SHAPE and the separating normal (pointing away from the other shape,
// toward the capsule) -- used by the player's sweep query (see
// PhysicsWorld.cpp), which samples the capsule at several positions along
// a displacement and needs "how close, and which way is out" at each one,
// not a discrete overlap/penetration answer.
struct CapsuleDistance {
    float distance = 0.0f;     // capsule-surface-to-other-shape-surface; negative if overlapping
    glm::vec3 normal{0.0f};    // world-space unit vector, points from the other shape toward the capsule
    glm::vec3 otherPoint{0.0f};// closest point on the OTHER shape's surface
};
CapsuleDistance CapsuleDistanceToSphere(const glm::vec3& segA, const glm::vec3& segB,
                                         float capsuleRadius, const glm::vec3& sphereCenter,
                                         float sphereRadius);
CapsuleDistance CapsuleDistanceToBox(const glm::vec3& segA, const glm::vec3& segB,
                                      float capsuleRadius, const glm::vec3& boxCenter,
                                      const glm::quat& boxOrientation,
                                      const glm::vec3& boxHalfExtents);
