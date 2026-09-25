#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "DebugDraw.h"
#include "Scene.h"

// Milestone 30: the viewport gizmo's mathematics, with no ImGui, no
// Renderer and no window in it so it is testable headlessly
// (tests/ProjectTests.cpp, "gizmo math"). EditorApplication feeds it the
// camera ray and the mouse state; it answers which handle is under the
// cursor and how a drag moves/rotates/scales an authored transform.
//
// Conventions:
//   - A gizmo sits at the selected object's position. Its three handles
//     are the world axes (GizmoSpace::World) or the object's rotated axes
//     (GizmoSpace::Local); scale always uses the object's local axes since
//     SceneTransform::scale is a per-local-axis value.
//   - Translate: drag along the picked axis line by the difference of the
//     ray's closest-approach parameters. Rotate: drag around the axis; the
//     angle is measured between the drag-start and current intersections
//     of the mouse ray with the plane perpendicular to the axis. Scale:
//     the axis-line displacement divided by the handle length is added to
//     the start scale on that axis.
//   - Snapping (Ctrl): translation to kTranslateSnap, rotation to
//     kRotateSnapDegrees, scale to kScaleSnap. Applied to the DELTA, so an
//     object at 0.37 dragged with snap moves in 0.5 m steps from 0.37.
enum class GizmoMode { Translate, Rotate, Scale };
enum class GizmoAxis { None, X, Y, Z };
enum class GizmoSpace { World, Local };

constexpr float kTranslateSnap = 0.5f;
constexpr float kRotateSnapDegrees = 15.0f;
constexpr float kScaleSnap = 0.25f;

// The world-space unit direction of one handle.
glm::vec3 GizmoAxisDirection(GizmoAxis axis, const glm::quat& objectRotation, GizmoSpace space, GizmoMode mode);

// Closest-approach parameter along the axis line (origin + t * dir) to the
// ray; false when the ray is parallel to the axis.
bool RayAxisClosestParameter(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const glm::vec3& axisOrigin,
                             const glm::vec3& axisDirection, float& outT);
bool RayPlaneIntersection(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const glm::vec3& planePoint,
                          const glm::vec3& planeNormal, glm::vec3& outHit);

// Which handle a ray hits, if any: the closest of the three axis segments
// (translate/scale) or axis rings (rotate) within `pickRadius`.
GizmoAxis PickGizmoAxis(GizmoMode mode, const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
                        const glm::vec3& gizmoOrigin, const glm::quat& objectRotation, GizmoSpace space,
                        float handleLength, float pickRadius);

// One drag in progress: everything captured at the press.
struct GizmoDrag {
    GizmoMode mode = GizmoMode::Translate;
    GizmoAxis axis = GizmoAxis::None;
    glm::vec3 axisDirection{1.0f, 0.0f, 0.0f};  // world space, unit
    glm::vec3 origin{0.0f};                     // gizmo origin at press
    SceneTransform startTransform;
    float startParameter = 0.0f;   // translate/scale: axis parameter at press
    glm::vec3 startPlaneHit{0.0f};  // rotate: plane intersection at press
    float handleLength = 1.0f;
    bool Active() const { return axis != GizmoAxis::None; }
};

// Starts a drag from the press ray; false if the ray gives no usable start.
bool BeginGizmoDrag(GizmoMode mode, GizmoAxis axis, const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
                    const SceneTransform& start, GizmoSpace space, float handleLength, GizmoDrag& outDrag);

// The transform the current ray implies for the drag. `snap` applies the
// mode's snap step to the delta.
SceneTransform UpdateGizmoDrag(const GizmoDrag& drag, const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
                               bool snap);

float SnapValue(float value, float step);

// The handles as lines: axes coloured red/green/blue, the active axis
// brightened, rotate rings as circles, scale tips as small boxes.
void BuildGizmoLines(GizmoMode mode, const glm::vec3& origin, const glm::quat& objectRotation, GizmoSpace space,
                     float handleLength, GizmoAxis highlight, DebugLineList& out);
