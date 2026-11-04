#pragma once
#include "DebugDraw.h"
#include "EditorCamera.h"
#include "EditorPicking.h"
#include "GizmoMath.h"
#include "PlanetTiles.h"
#include "ResourceManager.h"
#include <map>
#include <memory>
struct EditorContext;
struct EditorPlaySession;
struct EditorAssetPreview;
struct TerrainEditor;
struct VolumeTerrainEditor;
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
    void FrameEditMode(EditorContext &, float, TerrainEditor *, VolumeTerrainEditor *, TilemapEditor *);
    void Destroy(Renderer &);
    void RenderLive(EditorContext &, const EditorPlaySession &, float);
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
    std::map<SceneObjectId, std::pair<std::string, std::unique_ptr<PlanetTiles>>> planetPreview;
    std::vector<Scene> regionPreview;
    std::string regionPreviewKey;
};
