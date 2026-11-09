#pragma once
#include "Narrowphase.h"
#include "PhysicsWorld.h"

// Read-only translational cast against one resolved primitive. Orientations stay
// fixed; this is a snapshot query, not a body trajectory/CCD solver.
struct PrimitiveCastHit {
    bool hit=false, initialOverlap=false;
    double distance=0;
    glm::vec3 point{0}, normal{0};
    uint32_t feature=UINT32_MAX;
};
PrimitiveCastHit CastAgainstPrimitive(const Shape& cast, const BodyTransform& pose,
    const glm::vec3& unitDirection, double maxDistance, const PrimitivePose& target, bool initialOnly=false,
    const glm::vec3& normalAxis=glm::vec3(0), float minimumNormalDot=0);
