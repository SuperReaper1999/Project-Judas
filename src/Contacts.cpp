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
                        float radiusB, float margin) {
    Contact contact;
    const glm::vec3 delta = centerA - centerB;
    const float distance = glm::length(delta);
    const float combinedRadius = radiusA + radiusB;
    if (distance >= combinedRadius + margin) return contact;

    contact.hit = true;
    contact.normal = distance > kEpsilon ? delta / distance : glm::vec3(0.0f, 1.0f, 0.0f);
    contact.penetration = combinedRadius - distance;
    contact.point = centerB + contact.normal * radiusB;
    return contact;
}

Contact SphereVsBox(const glm::vec3& sphereCenter, float sphereRadius, const glm::vec3& boxCenter,
                     const glm::quat& boxOrientation, const glm::vec3& boxHalfExtents, float margin) {
    Contact contact;
    const glm::vec3 closest =
        ClosestPointOnOBB(sphereCenter, boxCenter, boxOrientation, boxHalfExtents);
    const glm::vec3 delta = sphereCenter - closest;
    const float distance = glm::length(delta);
    if (distance >= sphereRadius + margin) return contact;

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
                  const glm::mat3& rotationB, const glm::vec3& halfExtentsB, float& outOverlap,
                  float margin) {
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
    return outOverlap > -margin;
}

// Shared separating-axis computation for both BoxVsBox and
// BoxVsBoxManifold below. Returns false if a separating axis exists (no
// contact at all); otherwise fills `outNormal` (pointing from B toward A)
// and `outPenetration` (the minimum-overlap depth along it).
bool ComputeBoxBoxSAT(const glm::vec3& centerA, const glm::mat3& rotationA,
                       const glm::vec3& halfExtentsA, const glm::vec3& centerB,
                       const glm::mat3& rotationB, const glm::vec3& halfExtentsB,
                       glm::vec3& outNormal, float& outPenetration, float margin) {
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
                          halfExtentsB, overlap, margin)) {
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
                  const glm::vec3& halfExtentsB, float margin) {
    Contact contact;
    const glm::mat3 rotationA = glm::mat3_cast(orientA);
    const glm::mat3 rotationB = glm::mat3_cast(orientB);

    glm::vec3 normal(0.0f);
    float penetration = 0.0f;
    if (!ComputeBoxBoxSAT(centerA, rotationA, halfExtentsA, centerB, rotationB, halfExtentsB,
                           normal, penetration, margin)) {
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

namespace {

// Milestone 32: separating-axis selection that keeps face and edge axes
// apart. Face axes are preferred unless an edge-edge axis is clearly
// shallower (the usual relative/absolute tolerance), so a box resting flat
// is never reported as an edge contact because of float noise.
struct BoxSatResult {
    bool overlapping = false;
    bool faceOfA = false;
    bool faceOfB = false;
    int faceIndex = -1;
    glm::vec3 normal{0.0f};  // from B toward A
    float penetration = 0.0f;
};

BoxSatResult ClassifyBoxBox(const glm::vec3& centerA, const glm::mat3& rotationA,
                            const glm::vec3& halfExtentsA, const glm::vec3& centerB,
                            const glm::mat3& rotationB, const glm::vec3& halfExtentsB, float margin) {
    BoxSatResult result;
    float bestFace = std::numeric_limits<float>::max();
    glm::vec3 faceAxis(0.0f);
    int faceOwner = -1;
    int faceIndex = -1;
    for (int owner = 0; owner < 2; ++owner) {
        const glm::mat3& rotation = owner == 0 ? rotationA : rotationB;
        for (int i = 0; i < 3; ++i) {
            float overlap = 0.0f;
            if (!TestSATAxis(rotation[i], centerA, rotationA, halfExtentsA, centerB, rotationB,
                             halfExtentsB, overlap, margin)) {
                return result;
            }
            if (overlap < bestFace) {
                bestFace = overlap;
                faceAxis = rotation[i];
                faceOwner = owner;
                faceIndex = i;
            }
        }
    }
    float bestEdge = std::numeric_limits<float>::max();
    glm::vec3 edgeAxis(0.0f);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            const glm::vec3 axis = glm::cross(rotationA[i], rotationB[j]);
            float overlap = 0.0f;
            if (!TestSATAxis(axis, centerA, rotationA, halfExtentsA, centerB, rotationB, halfExtentsB,
                             overlap, margin)) {
                return result;
            }
            const float length = glm::length(axis);
            if (length > 1.0e-3f && overlap < bestEdge) {
                bestEdge = overlap;
                edgeAxis = axis / length;
            }
        }
    }
    result.overlapping = true;
    glm::vec3 axis;
    if (bestEdge < 0.95f * bestFace - 1.0e-3f) {
        axis = edgeAxis;
        result.penetration = bestEdge;
    } else {
        axis = faceAxis;
        result.penetration = bestFace;
        result.faceOfA = faceOwner == 0;
        result.faceOfB = faceOwner == 1;
        result.faceIndex = faceIndex;
    }
    if (glm::dot(axis, centerA - centerB) < 0.0f) axis = -axis;
    result.normal = axis;
    return result;
}

}  // namespace

ContactManifold BoxVsBoxManifold(const glm::vec3& centerA, const glm::quat& orientA,
                                  const glm::vec3& halfExtentsA, const glm::vec3& centerB,
                                  const glm::quat& orientB, const glm::vec3& halfExtentsB,
                                  float margin) {
    ContactManifold manifold;
    const glm::mat3 rotationA = glm::mat3_cast(orientA);
    const glm::mat3 rotationB = glm::mat3_cast(orientB);
    const BoxSatResult sat =
        ClassifyBoxBox(centerA, rotationA, halfExtentsA, centerB, rotationB, halfExtentsB, margin);
    if (!sat.overlapping) return manifold;

    if (!sat.faceOfA && !sat.faceOfB) {
        // Edge-edge: one point is the correct contact for two crossing edges.
        Contact contact = BoxVsBox(centerA, orientA, halfExtentsA, centerB, orientB, halfExtentsB, margin);
        if (contact.hit) {
            contact.normal = sat.normal;
            contact.penetration = sat.penetration;
            manifold.Add(contact);
        }
        return manifold;
    }

    // Face contact (Milestone 32): clip the incident box's most
    // anti-parallel face against the side planes of the reference face.
    // Unlike the earlier "which corners lie inside the other box" manifold,
    // this finds the corners of the actual overlap polygon — including the
    // ones that are edge crossings rather than vertices — so a box resting
    // offset on an identical box is supported at four points, not two
    // diagonal ones it could rock about.
    const bool referenceIsA = sat.faceOfA;
    const glm::vec3& refCenter = referenceIsA ? centerA : centerB;
    const glm::mat3& refRotation = referenceIsA ? rotationA : rotationB;
    const glm::vec3& refHalf = referenceIsA ? halfExtentsA : halfExtentsB;
    const glm::vec3& incCenter = referenceIsA ? centerB : centerA;
    const glm::mat3& incRotation = referenceIsA ? rotationB : rotationA;
    const glm::vec3& incHalf = referenceIsA ? halfExtentsB : halfExtentsA;
    // Reference face normal points from the reference box toward the other.
    const glm::vec3 referenceNormal = referenceIsA ? -sat.normal : sat.normal;
    const int r = sat.faceIndex;
    const int u = (r + 1) % 3;
    const int v = (r + 2) % 3;
    const glm::vec3 refFaceCenter = refCenter + referenceNormal * refHalf[r];
    const glm::vec3 uAxis = refRotation[u];
    const glm::vec3 vAxis = refRotation[v];

    int incidentAxis = 0;
    float mostAligned = -1.0f;
    for (int i = 0; i < 3; ++i) {
        const float alignment = std::abs(glm::dot(incRotation[i], referenceNormal));
        if (alignment > mostAligned) {
            mostAligned = alignment;
            incidentAxis = i;
        }
    }
    const float incidentSign = glm::dot(incRotation[incidentAxis], referenceNormal) > 0.0f ? -1.0f : 1.0f;
    const glm::vec3 incFaceCenter = incCenter + incRotation[incidentAxis] * (incidentSign * incHalf[incidentAxis]);
    const int iu = (incidentAxis + 1) % 3;
    const int iv = (incidentAxis + 2) % 3;
    const glm::vec3 eu = incRotation[iu] * incHalf[iu];
    const glm::vec3 ev = incRotation[iv] * incHalf[iv];

    std::array<glm::vec3, 16> polygon{};
    std::array<glm::vec3, 16> clipped{};
    int count = 4;
    polygon[0] = incFaceCenter + eu + ev;
    polygon[1] = incFaceCenter - eu + ev;
    polygon[2] = incFaceCenter - eu - ev;
    polygon[3] = incFaceCenter + eu - ev;
    // Sutherland–Hodgman against the four side planes dot(p - c, axis) <= h.
    const auto clip = [&](const glm::vec3& axis, float limit) {
        int out = 0;
        for (int i = 0; i < count; ++i) {
            const glm::vec3& a = polygon[static_cast<std::size_t>(i)];
            const glm::vec3& b = polygon[static_cast<std::size_t>((i + 1) % count)];
            const float da = glm::dot(a - refFaceCenter, axis) - limit;
            const float db = glm::dot(b - refFaceCenter, axis) - limit;
            if (da <= 0.0f && out < 16) clipped[static_cast<std::size_t>(out++)] = a;
            if ((da < 0.0f && db > 0.0f) || (da > 0.0f && db < 0.0f)) {
                if (out < 16) clipped[static_cast<std::size_t>(out++)] = a + (b - a) * (da / (da - db));
            }
        }
        count = out;
        polygon = clipped;
    };
    clip(uAxis, refHalf[u]);
    clip(-uAxis, refHalf[u]);
    clip(vAxis, refHalf[v]);
    clip(-vAxis, refHalf[v]);

    std::array<Contact, 16> candidates{};
    int candidateCount = 0;
    for (int i = 0; i < count; ++i) {
        const glm::vec3& p = polygon[static_cast<std::size_t>(i)];
        const float depth = glm::dot(refFaceCenter - p, referenceNormal);
        if (depth < -margin) continue;
        Contact contact;
        contact.hit = true;
        contact.normal = sat.normal;
        contact.penetration = depth;
        contact.point = p + referenceNormal * (0.5f * depth);
        candidates[static_cast<std::size_t>(candidateCount++)] = contact;
    }
    if (candidateCount == 0) {
        Contact contact = BoxVsBox(centerA, orientA, halfExtentsA, centerB, orientB, halfExtentsB, margin);
        if (contact.hit) manifold.Add(contact);
        return manifold;
    }
    if (candidateCount <= 4) {
        for (int i = 0; i < candidateCount; ++i) manifold.Add(candidates[static_cast<std::size_t>(i)]);
        return manifold;
    }
    // More than four: keep the deepest, the farthest from it, and the two
    // spanning the largest area on either side of that diagonal.
    int first = 0;
    for (int i = 1; i < candidateCount; ++i) {
        if (candidates[static_cast<std::size_t>(i)].penetration > candidates[static_cast<std::size_t>(first)].penetration) first = i;
    }
    const glm::vec3 p0 = candidates[static_cast<std::size_t>(first)].point;
    int second = first == 0 ? 1 : 0;
    for (int i = 0; i < candidateCount; ++i) {
        if (i == first) continue;
        if (glm::distance(candidates[static_cast<std::size_t>(i)].point, p0) >
            glm::distance(candidates[static_cast<std::size_t>(second)].point, p0)) second = i;
    }
    const glm::vec3 diagonal = candidates[static_cast<std::size_t>(second)].point - p0;
    int third = -1;
    int fourth = -1;
    float bestPositive = 0.0f;
    float bestNegative = 0.0f;
    for (int i = 0; i < candidateCount; ++i) {
        if (i == first || i == second) continue;
        const float area = glm::dot(glm::cross(diagonal, candidates[static_cast<std::size_t>(i)].point - p0),
                                    referenceNormal);
        if (area > bestPositive) { bestPositive = area; third = i; }
        if (area < bestNegative) { bestNegative = area; fourth = i; }
    }
    manifold.Add(candidates[static_cast<std::size_t>(first)]);
    manifold.Add(candidates[static_cast<std::size_t>(second)]);
    if (third >= 0) manifold.Add(candidates[static_cast<std::size_t>(third)]);
    if (fourth >= 0) manifold.Add(candidates[static_cast<std::size_t>(fourth)]);
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
