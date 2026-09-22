#include "Contacts.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace {
constexpr float kEpsilon = 1.0e-6f;
}

glm::vec3 ClosestPointOnOBB(const glm::vec3& point, const glm::vec3& boxCenter,
                             const glm::quat& boxOrientation, const glm::vec3& halfExtents) {
    const glm::quat inverseOrientation = glm::conjugate(boxOrientation);
    glm::vec3 localPoint = inverseOrientation * (point - boxCenter);
    localPoint = glm::clamp(localPoint, -halfExtents, halfExtents);
    return boxCenter + boxOrientation * localPoint;
}

glm::vec3 ClosestPointOnSegment(const glm::vec3& point, const glm::vec3& a, const glm::vec3& b) {
    const glm::vec3 ab = b - a;
    const float lengthSquared = glm::dot(ab, ab);
    if (lengthSquared < kEpsilon) return a;
    const float t = glm::clamp(glm::dot(point - a, ab) / lengthSquared, 0.0f, 1.0f);
    return a + ab * t;
}

void ClosestPointsSegmentToOBB(const glm::vec3& segA, const glm::vec3& segB,
                                const glm::vec3& boxCenter, const glm::quat& boxOrientation,
                                const glm::vec3& halfExtents, glm::vec3& outSegmentPoint,
                                glm::vec3& outBoxPoint) {
    // Alternating projection: start at the segment's midpoint, repeatedly
    // find the closest box point to the current segment point and the
    // closest segment point to that box point. Converges quickly for two
    // convex shapes (each step can only reduce or hold the distance
    // between the two points) -- a handful of iterations is more than
    // enough at this engine's scale.
    glm::vec3 segmentPoint = (segA + segB) * 0.5f;
    glm::vec3 boxPoint = boxCenter;
    for (int i = 0; i < 8; ++i) {
        boxPoint = ClosestPointOnOBB(segmentPoint, boxCenter, boxOrientation, halfExtents);
        segmentPoint = ClosestPointOnSegment(boxPoint, segA, segB);
    }
    outSegmentPoint = segmentPoint;
    outBoxPoint = boxPoint;
}

Contact SphereVsSphere(const glm::vec3& centerA, float radiusA, const glm::vec3& centerB,
                        float radiusB) {
    Contact contact;
    const glm::vec3 delta = centerA - centerB;
    const float distance = glm::length(delta);
    const float combinedRadius = radiusA + radiusB;
    if (distance >= combinedRadius) return contact;

    contact.hit = true;
    contact.normal = distance > kEpsilon ? delta / distance : glm::vec3(0.0f, 1.0f, 0.0f);
    contact.penetration = combinedRadius - distance;
    contact.point = centerB + contact.normal * radiusB;
    return contact;
}

Contact SphereVsBox(const glm::vec3& sphereCenter, float sphereRadius, const glm::vec3& boxCenter,
                     const glm::quat& boxOrientation, const glm::vec3& boxHalfExtents) {
    Contact contact;
    const glm::vec3 closest =
        ClosestPointOnOBB(sphereCenter, boxCenter, boxOrientation, boxHalfExtents);
    const glm::vec3 delta = sphereCenter - closest;
    const float distance = glm::length(delta);
    if (distance >= sphereRadius) return contact;

    contact.hit = true;
    contact.point = closest;
    if (distance > kEpsilon) {
        contact.normal = delta / distance;
        contact.penetration = sphereRadius - distance;
        return contact;
    }

    // Sphere center is exactly on or inside the box's surface (deep
    // penetration, e.g. spawned overlapping) -- fall back to pushing out
    // along whichever local box axis has the least remaining penetration,
    // the standard "deepest axis" resolution for this degenerate case.
    const glm::vec3 localCenter = glm::conjugate(boxOrientation) * (sphereCenter - boxCenter);
    const glm::vec3 axisPenetration = boxHalfExtents - glm::abs(localCenter);
    int minAxis = 0;
    if (axisPenetration.y < axisPenetration[minAxis]) minAxis = 1;
    if (axisPenetration.z < axisPenetration[minAxis]) minAxis = 2;
    glm::vec3 localNormal(0.0f);
    localNormal[minAxis] = localCenter[minAxis] >= 0.0f ? 1.0f : -1.0f;
    contact.normal = boxOrientation * localNormal;
    contact.penetration = sphereRadius + axisPenetration[minAxis];
    return contact;
}

namespace {

// One candidate separating axis for box-box SAT: returns false (shapes
// provably separated on this axis) or true with `outOverlap` set to the
// (positive) overlap amount along it. A near-zero-length axis (parallel
// edges in the cross-product set) is skipped by reporting an effectively
// infinite overlap, so it can never become the minimum and is harmless.
bool TestSATAxis(const glm::vec3& axis, const glm::vec3& centerA, const glm::mat3& rotationA,
                  const glm::vec3& halfExtentsA, const glm::vec3& centerB,
                  const glm::mat3& rotationB, const glm::vec3& halfExtentsB, float& outOverlap) {
    const float axisLength = glm::length(axis);
    if (axisLength < kEpsilon) {
        outOverlap = std::numeric_limits<float>::max();
        return true;
    }
    const glm::vec3 a = axis / axisLength;
    const float centerDistance = std::abs(glm::dot(centerB - centerA, a));
    const float projectionA = std::abs(glm::dot(rotationA[0], a)) * halfExtentsA.x +
                               std::abs(glm::dot(rotationA[1], a)) * halfExtentsA.y +
                               std::abs(glm::dot(rotationA[2], a)) * halfExtentsA.z;
    const float projectionB = std::abs(glm::dot(rotationB[0], a)) * halfExtentsB.x +
                               std::abs(glm::dot(rotationB[1], a)) * halfExtentsB.y +
                               std::abs(glm::dot(rotationB[2], a)) * halfExtentsB.z;
    outOverlap = projectionA + projectionB - centerDistance;
    return outOverlap > 0.0f;
}

// Shared separating-axis computation for both BoxVsBox and
// BoxVsBoxManifold below. Returns false if a separating axis exists (no
// contact at all); otherwise fills `outNormal` (pointing from B toward A)
// and `outPenetration` (the minimum-overlap depth along it).
bool ComputeBoxBoxSAT(const glm::vec3& centerA, const glm::mat3& rotationA,
                       const glm::vec3& halfExtentsA, const glm::vec3& centerB,
                       const glm::mat3& rotationB, const glm::vec3& halfExtentsB,
                       glm::vec3& outNormal, float& outPenetration) {
    // 15 candidate axes: each box's own 3 face normals, plus all 9
    // pairwise cross products of their edge directions -- the complete
    // separating-axis set for two OBBs.
    std::array<glm::vec3, 15> axes;
    int axisCount = 0;
    for (int i = 0; i < 3; ++i) axes[axisCount++] = rotationA[i];
    for (int i = 0; i < 3; ++i) axes[axisCount++] = rotationB[i];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            axes[axisCount++] = glm::cross(rotationA[i], rotationB[j]);
        }
    }

    float minOverlap = std::numeric_limits<float>::max();
    glm::vec3 minAxis(0.0f);
    for (int i = 0; i < axisCount; ++i) {
        float overlap = 0.0f;
        if (!TestSATAxis(axes[i], centerA, rotationA, halfExtentsA, centerB, rotationB,
                          halfExtentsB, overlap)) {
            return false;  // a separating axis exists -- no contact
        }
        if (overlap < minOverlap) {
            minOverlap = overlap;
            const float axisLength = glm::length(axes[i]);
            minAxis = axisLength > kEpsilon ? axes[i] / axisLength : glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }

    if (glm::dot(minAxis, centerA - centerB) < 0.0f) {
        minAxis = -minAxis;
    }
    outNormal = minAxis;
    outPenetration = minOverlap;
    return true;
}

}  // namespace

Contact BoxVsBox(const glm::vec3& centerA, const glm::quat& orientA, const glm::vec3& halfExtentsA,
                  const glm::vec3& centerB, const glm::quat& orientB,
                  const glm::vec3& halfExtentsB) {
    Contact contact;
    const glm::mat3 rotationA = glm::mat3_cast(orientA);
    const glm::mat3 rotationB = glm::mat3_cast(orientB);

    glm::vec3 normal(0.0f);
    float penetration = 0.0f;
    if (!ComputeBoxBoxSAT(centerA, rotationA, halfExtentsA, centerB, rotationB, halfExtentsB,
                           normal, penetration)) {
        return contact;
    }
    contact.hit = true;
    contact.normal = normal;
    contact.penetration = penetration;

    // Contact point: a single-point approximation (see the comment on
    // Contact itself for why that's an acceptable, explicit simplification
    // for callers that only need one point). Each box's own "deepest
    // vertex along the resolving normal" is computed, then CLAMPED onto
    // the OTHER box's own extent (ClosestPointOnOBB) before averaging the
    // two. The clamp is what makes this robust when the boxes are very
    // different sizes (e.g. a small dynamic box on a large static ground
    // plane): a large box's own "deepest vertex" is one of its far
    // corners, which can be nowhere near the actual overlap region —
    // clamping it onto the small box's tight extent pulls it back to
    // somewhere physically meaningful. Averaging the two clamped points
    // lands close to the true overlap region regardless of which box is
    // "the big one." PhysicsWorld's own solver uses BoxVsBoxManifold
    // instead, specifically because a resting/stacked box needs more than
    // one such point (see that function).
    auto deepestVertexAlong = [](const glm::vec3& center, const glm::mat3& rotation,
                                  const glm::vec3& halfExtents, const glm::vec3& direction) {
        glm::vec3 vertex = center;
        for (int i = 0; i < 3; ++i) {
            const float sign = glm::dot(rotation[i], direction) >= 0.0f ? -1.0f : 1.0f;
            vertex += rotation[i] * (halfExtents[i] * sign);
        }
        return vertex;
    };
    const glm::vec3 deepestOfA = deepestVertexAlong(centerA, rotationA, halfExtentsA, contact.normal);
    const glm::vec3 deepestOfB =
        deepestVertexAlong(centerB, rotationB, halfExtentsB, -contact.normal);
    const glm::vec3 clampedA = ClosestPointOnOBB(deepestOfA, centerB, orientB, halfExtentsB);
    const glm::vec3 clampedB = ClosestPointOnOBB(deepestOfB, centerA, orientA, halfExtentsA);
    contact.point = (clampedA + clampedB) * 0.5f;
    return contact;
}

ContactManifold BoxVsBoxManifold(const glm::vec3& centerA, const glm::quat& orientA,
                                  const glm::vec3& halfExtentsA, const glm::vec3& centerB,
                                  const glm::quat& orientB, const glm::vec3& halfExtentsB) {
    ContactManifold manifold;
    const glm::mat3 rotationA = glm::mat3_cast(orientA);
    const glm::mat3 rotationB = glm::mat3_cast(orientB);

    glm::vec3 normal(0.0f);
    float penetration = 0.0f;
    if (!ComputeBoxBoxSAT(centerA, rotationA, halfExtentsA, centerB, rotationB, halfExtentsB,
                           normal, penetration)) {
        return manifold;
    }

    // Vertex-based manifold: any vertex of A that lies INSIDE B, or any
    // vertex of B that lies inside A, is a genuine penetrating contact
    // point. For the common "box resting flat on a larger surface" case,
    // this correctly finds all 4 of the resting box's bottom corners
    // simultaneously, so the solver's impulses/positional correction act
    // symmetrically and the box settles flat instead of rocking on
    // whichever single corner an arbitrary tie-break happened to pick.
    auto addVertexContactsInside = [&](const glm::vec3& ownerCenter, const glm::mat3& ownerRotation,
                                        const glm::vec3& ownerHalfExtents,
                                        const glm::vec3& otherCenter, const glm::quat& otherOrientation,
                                        const glm::vec3& otherHalfExtents) {
        for (int signX = -1; signX <= 1; signX += 2) {
            for (int signY = -1; signY <= 1; signY += 2) {
                for (int signZ = -1; signZ <= 1; signZ += 2) {
                    const glm::vec3 localVertex(signX * ownerHalfExtents.x, signY * ownerHalfExtents.y,
                                                 signZ * ownerHalfExtents.z);
                    const glm::vec3 worldVertex = ownerCenter + ownerRotation * localVertex;
                    const glm::vec3 localInOther =
                        glm::conjugate(otherOrientation) * (worldVertex - otherCenter);
                    if (std::abs(localInOther.x) <= otherHalfExtents.x &&
                        std::abs(localInOther.y) <= otherHalfExtents.y &&
                        std::abs(localInOther.z) <= otherHalfExtents.z) {
                        Contact contact;
                        contact.hit = true;
                        contact.normal = normal;
                        contact.penetration = penetration;
                        contact.point = worldVertex;
                        manifold.Add(contact);
                    }
                }
            }
        }
    };

    addVertexContactsInside(centerA, rotationA, halfExtentsA, centerB, orientB, halfExtentsB);
    if (manifold.count < 4) {
        addVertexContactsInside(centerB, rotationB, halfExtentsB, centerA, orientA, halfExtentsA);
    }

    if (manifold.count == 0) {
        // No vertex of either box is strictly inside the other -- an
        // edge-edge contact, or two faces that overlap without either
        // box's corner poking through (can happen with very different
        // sizes). Fall back to the single clamped-average point, which
        // handles this case correctly even without a vertex to anchor on.
        manifold.Add(BoxVsBox(centerA, orientA, halfExtentsA, centerB, orientB, halfExtentsB));
    }
    return manifold;
}

CapsuleDistance CapsuleDistanceToSphere(const glm::vec3& segA, const glm::vec3& segB,
                                         float capsuleRadius, const glm::vec3& sphereCenter,
                                         float sphereRadius) {
    CapsuleDistance result;
    const glm::vec3 closestOnSegment = ClosestPointOnSegment(sphereCenter, segA, segB);
    const glm::vec3 delta = closestOnSegment - sphereCenter;
    const float centerDistance = glm::length(delta);
    result.distance = centerDistance - capsuleRadius - sphereRadius;
    if (centerDistance > kEpsilon) {
        result.normal = delta / centerDistance;
    } else {
        result.normal = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    result.otherPoint = sphereCenter + result.normal * sphereRadius;
    return result;
}

CapsuleDistance CapsuleDistanceToBox(const glm::vec3& segA, const glm::vec3& segB,
                                      float capsuleRadius, const glm::vec3& boxCenter,
                                      const glm::quat& boxOrientation,
                                      const glm::vec3& boxHalfExtents) {
    CapsuleDistance result;
    glm::vec3 segmentPoint, boxPoint;
    ClosestPointsSegmentToOBB(segA, segB, boxCenter, boxOrientation, boxHalfExtents, segmentPoint,
                               boxPoint);
    const glm::vec3 delta = segmentPoint - boxPoint;
    float distance = glm::length(delta);

    if (distance > kEpsilon) {
        result.normal = delta / distance;
        result.distance = distance - capsuleRadius;
        result.otherPoint = boxPoint;
        return result;
    }

    // The segment's closest point lies exactly on (or the segment
    // penetrates) the box's surface -- fall back to the deepest-local-axis
    // resolution, same technique as SphereVsBox's degenerate case.
    const glm::vec3 localPoint = glm::conjugate(boxOrientation) * (segmentPoint - boxCenter);
    const glm::vec3 axisPenetration = boxHalfExtents - glm::abs(localPoint);
    int minAxis = 0;
    if (axisPenetration.y < axisPenetration[minAxis]) minAxis = 1;
    if (axisPenetration.z < axisPenetration[minAxis]) minAxis = 2;
    glm::vec3 localNormal(0.0f);
    localNormal[minAxis] = localPoint[minAxis] >= 0.0f ? 1.0f : -1.0f;
    result.normal = boxOrientation * localNormal;
    result.distance = -(axisPenetration[minAxis] + capsuleRadius);
    result.otherPoint = boxPoint;
    return result;
}
