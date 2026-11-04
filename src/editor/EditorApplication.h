#pragma once

#include "EditorAssetPreview.h"
#include "EditorContext.h"
#include "EditorShell.h"
#include "EditorProfilerSnapshot.h"
#include "EditorPlaySession.h"
#include "EditorPreferences.h"
#include "EditorProjectWorkflow.h"
#include "EditorSceneViewport.h"
#include <memory>
#include <string>

#include "EditorDocument.h"
#include "EditorMilestoneNotice.h"
#include "EditorPanels.h"
#include "Project.h"

class EngineHost;
struct AnimationTimeline;
struct UtilityAIEditor;
struct SpriteSheetEditor;
struct TilemapEditor;

// Coordinates one editor-process lifetime around the shared project/document
// authority and EngineHost. Cohesive owners implement project requests, Play,
// scene navigation, previews, tools and docked presentation; the frame loop
// orders those services without owning their unrelated implementation details.
//
// Edit changes the authored document. Play owns a separate RuntimeWorld and
// advances it once per frame through InteractivePlay; Scene/Game panels only
// present their views. Stop discards that world and returns to authoring.
// "Run project" launches the standalone runtime as a separate process.
//
// Run is one-shot. The owned host outlives tools, GPU previews and callbacks,
// including initialization failures. Exported games never link editor modules.
class EditorApplication {
  public:
    EditorApplication();
    ~EditorApplication();
    int Run(int argc, char **argv);

  private:
    friend class EditorAutomation;
    EditorContext Context(EditorRequests &);
    EditorProjectServices ProjectServices();
    std::unique_ptr<EngineHost> m_ownedHost;
    EngineHost *m_host = nullptr;
    Project m_project;
    EditorDocument m_document;
    EditorPanelState m_panels;
    EditorPreferences m_preferences;
    bool m_guiContextOwned=false,m_guiPlatformReady=false,m_guiRendererReady=false;
    void Shutdown(bool savePreferences=false);
    std::unique_ptr<struct ScriptTools> m_scriptTools; // outlives the played world/debugger callback
    EditorShell m_shell;
    EditorProjectWorkflow m_projectWorkflow;
    EditorSceneViewport m_sceneViewport;
    EditorPlaySession m_playSession;
    EditorAssetPreview m_assetPreview;
    EditorMilestoneNotice m_milestoneNotice;
    std::unique_ptr<struct TerrainEditor> m_terrain;
    std::unique_ptr<struct VolumeTerrainEditor> m_volumeTerrain;
    std::unique_ptr<struct ModTools> m_modTools;

    bool StartPlay(std::string &outError);
    void StopPlay();
    void HandleRequests(EditorRequests &requests);
    void PickAtPixel(int x, int y);
    void RefreshProjectLists();
    bool OpenProject(const std::string &projectFile, std::string &outError);
    bool CreateProject(const std::string &directory, const std::string &name, std::string &outError,
                       bool visibleStarter = true, int twoDTemplate = 0);
    bool OpenScene(const std::string &path, std::string &outError);
    bool RunProject(std::string &outMessage);
    std::string ResolveScenePath(const std::string &input);
    std::string WorldStatePathFor(const std::string &scenePath);
    void FrameEditMode(float deltaSeconds);
    void UpdateCameraNavigation(const EditorCameraGestureInput &input, int mouseDeltaX, int mouseDeltaY,
                                float deltaSeconds, bool fast);
    void CancelCameraNavigation();
    void LoadEditorPreferences();
    void SaveEditorPreferences();
    void ViewportRay(int x, int y, glm::vec3 &origin, glm::vec3 &direction);
    void DrawModelImportPreview(float deltaSeconds);
    void DrawUIAuthoringPreview();
    void UpdateGizmo(bool allowInteraction);
    void DrawEditOverlay(class Renderer &renderer, const Scene &scene);
    void CollectProfilerData(float frameDeltaSeconds);
    // Milestone 31: the open scene's assets are referenced (never evicted,
    // never cancelled) while it is open; the set follows every edit.
    void RefreshAssetDemand();

    struct StabilizationAutomation;
    void AdvanceStabilizationAutomation();
    std::unique_ptr<StabilizationAutomation> m_stabilization;

    std::unique_ptr<AnimationTimeline> m_animationTimeline;
    std::unique_ptr<UtilityAIEditor> m_utilityAIEditor;
    std::unique_ptr<SpriteSheetEditor> m_spriteSheetEditor;
    std::unique_ptr<TilemapEditor> m_tilemapEditor;
    bool m_quit = false;

    // Milestone 30: viewport gizmo interaction state.
    EditorProfilerSnapshot m_profilerSnapshot;
};
