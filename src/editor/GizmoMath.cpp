#include "GizmoMath.h"

#include <cmath>

namespace {
glm::vec3 LocalAxis(GizmoAxis axis) {
    switch (axis) {
        case GizmoAxis::X: return glm::vec3(1.0f, 0.0f, 0.0f);
        case GizmoAxis::Y: return glm::vec3(0.0f, 1.0f, 0.0f);
        case GizmoAxis::Z: return glm::vec3(0.0f, 0.0f, 1.0f);
        case GizmoAxis::None: break;
    }
    return glm::vec3(0.0f);
}

glm::vec3 AxisColor(GizmoAxis axis, bool highlight) {
    glm::vec3 color(1.0f);
    switch (axis) {
        case GizmoAxis::X: color = glm::vec3(0.9f, 0.2f, 0.2f); break;
        case GizmoAxis::Y: color = glm::vec3(0.2f, 0.9f, 0.2f); break;
        case GizmoAxis::Z: color = glm::vec3(0.25f, 0.45f, 1.0f); break;
        case GizmoAxis::None: break;
    }
    return highlight ? glm::mix(color, glm::vec3(1.0f, 1.0f, 0.4f), 0.6f) : color;
}

// Distance between a ray and a segment [a, b], and the ray parameter of
// the closest approach.
float RaySegmentDistance(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const glm::vec3& a,
                         const glm::vec3& b, float& outRayT) {
    const glm::vec3 segment = b - a;
    const float segmentLength2 = glm::dot(segment, segment);
    const glm::vec3 w = rayOrigin - a;
    const float aa = glm::dot(rayDirection, rayDirection);
    const float bb = glm::dot(rayDirection, segment);
    const float cc = segmentLength2;
    const float dd = glm::dot(rayDirection, w);
    const float ee = glm::dot(segment, w);
    const float denominator = aa * cc - bb * bb;
    float s = 0.0f, t = 0.0f;
    if (denominator > 1.0e-8f) {
        s = (bb * ee - cc * dd) / denominator;
        t = (aa * ee - bb * dd) / denominator;
    } else {
        t = ee / std::max(cc, 1.0e-8f);
    }
    t = glm::clamp(t, 0.0f, 1.0f);
    s = glm::dot((a + segment * t) - rayOrigin, rayDirection) / aa;
    s = std::max(s, 0.0f);
    outRayT = s;
    return glm::length((rayOrigin + rayDirection * s) - (a + segment * t));
}
}  // namespace

float SnapValue(float value, float step) {
    if (step <= 0.0f) return value;
    return std::round(value / step) * step;
}

glm::vec3 GizmoAxisDirection(GizmoAxis axis, const glm::quat& objectRotation, GizmoSpace space, GizmoMode mode) {
    const glm::vec3 local = LocalAxis(axis);
    if (space == GizmoSpace::Local || mode == GizmoMode::Scale) return glm::normalize(glm::normalize(objectRotation) * local);
    return local;
}

bool RayAxisClosestParameter(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const glm::vec3& axisOrigin,
                             const glm::vec3& axisDirection, float& outT) {
    // Closest points between two lines: solve for the axis parameter.
    const glm::vec3 w = rayOrigin - axisOrigin;
    const float a = glm::dot(rayDirection, rayDirection);
    const float b = glm::dot(rayDirection, axisDirection);
    const float c = glm::dot(axisDirection, axisDirection);
    const float d = glm::dot(rayDirection, w);
    const float e = glm::dot(axisDirection, w);
    const float denominator = a * c - b * b;
    if (std::abs(denominator) < 1.0e-6f) return false;
    outT = (a * e - b * d) / denominator;
    return true;
}

bool RayPlaneIntersection(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const glm::vec3& planePoint,
                          const glm::vec3& planeNormal, glm::vec3& outHit) {
    const float denominator = glm::dot(rayDirection, planeNormal);
    if (std::abs(denominator) < 1.0e-6f) return false;
    const float t = glm::dot(planePoint - rayOrigin, planeNormal) / denominator;
    if (t < 0.0f) return false;
    outHit = rayOrigin + rayDirection * t;
    return true;
}

GizmoAxis PickGizmoAxis(GizmoMode mode, const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
                        const glm::vec3& gizmoOrigin, const glm::quat& objectRotation, GizmoSpace space,
                        float handleLength, float pickRadius) {
    GizmoAxis best = GizmoAxis::None;
    float bestDistance = pickRadius;
    float bestRayT = 1.0e30f;
    for (GizmoAxis axis : {GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z}) {
        const glm::vec3 direction = GizmoAxisDirection(axis, objectRotation, space, mode);
        float distance = 1.0e30f;
        float rayT = 0.0f;
        if (mode == GizmoMode::Rotate) {
            // Ring in the plane perpendicular to the axis: intersect the
            // plane and measure the radial distance from the ring.
            glm::vec3 hit;
            if (!RayPlaneIntersection(rayOrigin, rayDirection, gizmoOrigin, direction, hit)) continue;
            distance = std::abs(glm::length(hit - gizmoOrigin) - handleLength);
            rayT = glm::length(hit - rayOrigin);
        } else {
            distance = RaySegmentDistance(rayOrigin, rayDirection, gizmoOrigin, gizmoOrigin + direction * handleLength, rayT);
        }
        // Within the pick radius the nearest handle wins; on a tie (two
        // rings crossing under the cursor) the one nearer the camera does.
        if (distance >= pickRadius) continue;
        const bool closer = best == GizmoAxis::None || distance < bestDistance - 1.0e-4f ||
                            (std::abs(distance - bestDistance) <= 1.0e-4f && rayT < bestRayT);
        if (closer) {
            best = axis;
            bestDistance = distance;
            bestRayT = rayT;
        }
    }
    return best;
}

bool BeginGizmoDrag(GizmoMode mode, GizmoAxis axis, const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
                    const SceneTransform& start, GizmoSpace space, float handleLength, GizmoDrag& outDrag) {
    GizmoDrag drag;
    drag.mode = mode;
    drag.axis = axis;
    drag.origin = start.position;
    drag.startTransform = start;
    drag.handleLength = handleLength;
    drag.axisDirection = GizmoAxisDirection(axis, start.rotation, space, mode);
    if (axis == GizmoAxis::None) return false;
    if (mode == GizmoMode::Rotate) {
        if (!RayPlaneIntersection(rayOrigin, rayDirection, drag.origin, drag.axisDirection, drag.startPlaneHit)) return false;
        if (glm::length(drag.startPlaneHit - drag.origin) < 1.0e-4f) return false;
    } else {
        if (!RayAxisClosestParameter(rayOrigin, rayDirection, drag.origin, drag.axisDirection, drag.startParameter)) return false;
    }
    outDrag = drag;
    return true;
}

SceneTransform UpdateGizmoDrag(const GizmoDrag& drag, const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
                               bool snap) {
    SceneTransform result = drag.startTransform;
    if (!drag.Active()) return result;
    switch (drag.mode) {
        case GizmoMode::Translate: {
            float t = 0.0f;
            if (!RayAxisClosestParameter(rayOrigin, rayDirection, drag.origin, drag.axisDirection, t)) return result;
            float delta = t - drag.startParameter;
            if (snap) delta = SnapValue(delta, kTranslateSnap);
            result.position = drag.startTransform.position + drag.axisDirection * delta;
            return result;
        }
        case GizmoMode::Rotate: {
            glm::vec3 hit;
            if (!RayPlaneIntersection(rayOrigin, rayDirection, drag.origin, drag.axisDirection, hit)) return result;
            const glm::vec3 from = hit - drag.origin;
            const glm::vec3 to = drag.startPlaneHit - drag.origin;
            if (glm::length(from) < 1.0e-4f) return result;
            const glm::vec3 a = glm::normalize(to);
            const glm::vec3 b = glm::normalize(from);
            float angle = std::atan2(glm::dot(glm::cross(a, b), drag.axisDirection), glm::dot(a, b));
            if (snap) angle = glm::radians(SnapValue(glm::degrees(angle), kRotateSnapDegrees));
            const glm::quat rotation = glm::angleAxis(angle, drag.axisDirection);
            result.rotation = glm::normalize(rotation * drag.startTransform.rotation);
            return result;
        }
        case GizmoMode::Scale: {
            float t = 0.0f;
            if (!RayAxisClosestParameter(rayOrigin, rayDirection, drag.origin, drag.axisDirection, t)) return result;
            float delta = (t - drag.startParameter) / std::max(drag.handleLength, 1.0e-4f);
            if (snap) delta = SnapValue(delta, kScaleSnap);
            const glm::vec3 local = LocalAxis(drag.axis);
            glm::vec3 scale = drag.startTransform.scale + local * delta;
            scale = glm::max(scale, glm::vec3(0.01f));
            result.scale = scale;
            return result;
        }
    }
    return result;
}

void BuildGizmoLines(GizmoMode mode, const glm::vec3& origin, const glm::quat& objectRotation, GizmoSpace space,
                     float handleLength, GizmoAxis highlight, DebugLineList& out) {
    for (GizmoAxis axis : {GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z}) {
        const glm::vec3 direction = GizmoAxisDirection(axis, objectRotation, space, mode);
        const glm::vec3 color = AxisColor(axis, axis == highlight);
        if (mode == GizmoMode::Rotate) {
            out.Circle(origin, direction, handleLength, color, 48);
            continue;
        }
        const glm::vec3 tip = origin + direction * handleLength;
        if (mode == GizmoMode::Translate) {
            out.Arrow(origin, tip, color, handleLength * 0.18f);
        } else {
            out.Line(origin, tip, color);
            out.Box(tip, glm::normalize(objectRotation), glm::vec3(handleLength * 0.06f), color);
        }
    }
    out.Cross(origin, handleLength * 0.08f, glm::vec3(1.0f));
}
