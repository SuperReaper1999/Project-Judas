#pragma once
#include "DebugDraw.h"
#include "EditorCamera.h"
#include "EditorPicking.h"
#include "GizmoMath.h"
#include "PlanetTiles.h"
#include "TerrainPlanetPreview.h"
#include "ResourceManager.h"
#include <map>
#include <memory>
#include <optional>
struct EditorContext;
struct EditorPlaySession;
struct EditorAssetPreview;
class TerrainV2Editor;
struct TilemapEditor;

// Scene-view tools own camera/gesture/target state, independently of the shell.
struct EditorSceneViewport {
    EditorSceneViewport();
    ~EditorSceneViewport();
    void ViewportRay(EditorContext &, int, int, glm::vec3 &, glm::vec3 &) const;
    void PickAtPixel(EditorContext &, int, int);
    void UpdateGizmo(EditorContext &, bool);
    void DrawEditOverlay(EditorContext &, Renderer &, const Scene &);
    void CancelCameraNavigation(EditorContext &);
    void UpdateCameraNavigation(EditorContext &, const EditorCameraGestureInput &, int, int, float, bool);
    void FrameEditMode(EditorContext &, float, TerrainV2Editor *, TilemapEditor *);
    void Destroy(Renderer &);
    void RenderLive(EditorContext &, const EditorPlaySession &, float);
    void BeginObservation(glm::dvec3 authoredOrigin);
    void EndObservation(EditorContext &);
    EditorCamera camera;
    EditorScenePicker picker;
    RenderTargetHandle target;
    EditorCameraGestureState gesture;
    bool focused = true;
    int mouseDeltaX = 0, mouseDeltaY = 0;
    GizmoDrag drag;
    GizmoAxis hoverAxis = GizmoAxis::None;
    float gizmoHandleLength = 1;
    std::string pendingDropAsset;
    bool pendingDropDelivered = false;
    glm::vec2 pendingDropPosition{0};
    DebugLineList debugLines, gizmoLines;
    TerrainPlanetPreview planetPreview;
    std::vector<Scene> regionPreview;
    std::string regionPreviewKey;

  private:
    void SynchronizeObservationOrigin(EditorContext &, glm::dvec3);
    std::optional<glm::dvec3> observedOrigin;
};
