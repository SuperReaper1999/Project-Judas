#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "AssetDatabase.h"
#include "EditorDocument.h"
#include "GizmoMath.h"
#include "ResourceManager.h"
#include "WorldDebugView.h"

// Milestone 28/30: the editor's Dear ImGui panels. Every panel operates on
// the EditorDocument's authored Scene through BeginEdit/CommitEdit; none
// of them touch a RuntimeWorld, PhysicsWorld or Renderer. They know
// generic engine components (through the ComponentEditors registry) and
// the project/asset layer, and nothing about any particular game.

enum class EditorMode { Edit, Play };

// The Asset Browser's drag payload type: the dragged asset's id bytes.
constexpr const char* kAssetDragPayload = "JUDAS_ASSET_ID";

// What the panels ask the application to do this frame.
struct EditorRequests {
    bool newScene = false;
    bool open = false;
    bool save = false;
    bool saveAs = false;
    bool play = false;
    bool stop = false;
    bool undo = false;
    bool redo = false;
    bool focusSelection = false;
    bool quit = false;
    bool saveWorldState = false;
    bool deleteWorldState = false;
    // Milestone 30: project and asset operations.
    bool newProject = false;
    bool openProject = false;
    bool saveProject = false;
    bool runProject = false;
    bool rescanAssets = false;
    std::string openSceneRelative;  // open this project-relative scene
    std::string importSource;       // Asset Browser: import this file
    std::string importDestination;  //   into <assets>/<this> ("" keeps the name)
    std::string trackAssetPath;     // adopt an untracked file in place
    std::string moveAssetId;        // rename/move this asset ...
    std::string moveAssetTo;        //   ... to this assets-relative path
    std::string removeAssetId;
    SceneObjectId duplicateId = kInvalidSceneObjectId;
    // Object creation: kind + where (the application fills the position).
    std::string createKind;
    glm::vec3 createPosition{0.0f};
    // Asset Browser drag released over the viewport: create a mesh object.
    std::string dropMeshAssetId;
};

class Project;
class RuntimeWorld;

// Milestone 30: what the profiler panel shows. Every number is measured
// by the code that does the work and labelled with what it counts.
struct ProfilerData {
    float frameMilliseconds = 0.0f;      // wall time between frames (rolling average)
    float framesPerSecond = 0.0f;
    int fixedStepsThisFrame = 0;
    float fixedStepMilliseconds = 0.0f;  // wall time of the last StepPlayedWorld
    float physicsMilliseconds = 0.0f;    // PhysicsWorld::Step inside it (0 when not measured)
    std::size_t physicsBodies = 0;       // live PhysicsWorld bodies
    std::size_t dynamicBodies = 0;
    std::size_t contacts = 0;            // last step's resolved contact points
    std::size_t entitiesFull = 0, entitiesCoarse = 0, entitiesDormant = 0, entitiesDestroyed = 0;
    unsigned int drawCalls = 0, triangles = 0, shadowPasses = 0, dynamicLights = 0, debugLines = 0;
    std::size_t fluidParticles = 0;
    float fluidMilliseconds = 0.0f;      // fluid solve inside the last step (0 when not measured)
    float surfaceMilliseconds = 0.0f;    // fluid surface rebuild (presentation)
    float sceneMilliseconds = 0.0f;      // RenderWorldFrame submission
    ResourceStats resources;
    bool playing = false;
};

struct EditorPanelState {
    EditorMode mode = EditorMode::Edit;
    // Milestone 29: the live world while playing (null in edit mode) so the
    // inspector can show persistent identity, lifecycle and fidelity and
    // offer the debug override; and the world-state path for the World menu.
    RuntimeWorld* runtime = nullptr;
    std::string worldStatePath;
    bool playPaused = false;
    std::string status;
    std::string pathInput;  // Open / Save As text field
    std::vector<std::string> terrainSurfaces;
    glm::vec3 cameraFocus{0.0f};  // where "create" places new objects
    std::string runtimeInfo;      // one-line runtime summary while playing

    // Milestone 30: project, assets, gizmo, debug view, profiler.
    Project* project = nullptr;              // the open project (may be !IsLoaded())
    const AssetDatabase* assets = nullptr;   // its asset database
    std::vector<std::string> sceneFiles;     // project-relative .judas files
    GizmoMode gizmoMode = GizmoMode::Translate;
    GizmoSpace gizmoSpace = GizmoSpace::World;
    bool gizmoSnap = false;
    DebugViewOptions debug;
    bool showProfiler = false;
    bool showProjectSettings = false;
    bool showAssetBrowser = true;
    ProfilerData profiler;
    std::string runProjectInfo;  // last Run Project outcome
    // Asset Browser text fields.
    std::string importSourceInput;
    std::string importDestinationInput;
    std::string moveAssetInput;
    AssetId browserSelection;
    // Hierarchy inline rename.
    SceneObjectId renamingId = kInvalidSceneObjectId;
    std::string renameBuffer;
};

void DrawEditorMainMenu(EditorDocument& doc, EditorPanelState& state, EditorRequests& requests);
void DrawHierarchyPanel(EditorDocument& doc, EditorPanelState& state, EditorRequests& requests);
void DrawInspectorPanel(EditorDocument& doc, EditorPanelState& state);
void DrawSceneSettingsPanel(EditorDocument& doc, EditorPanelState& state);
void DrawAssetBrowserPanel(EditorDocument& doc, EditorPanelState& state, EditorRequests& requests);
void DrawProjectSettingsPanel(EditorDocument& doc, EditorPanelState& state, EditorRequests& requests);
void DrawProfilerPanel(EditorPanelState& state);
void DrawStatusBar(EditorDocument& doc, EditorPanelState& state);

// Creates an object of `kind` ("empty", "box", "sphere", "dynamic-box",
// "dynamic-sphere", "point-light", "spot-light", "player-start", "mesh",
// "door", "gravity-region") at `position`, recording one undo step.
// Returns its id. `meshAssetId` is used by "mesh".
SceneObjectId CreateObjectOfKind(EditorDocument& doc, const std::string& kind, const glm::vec3& position,
                                 const std::string& meshAssetId);

// Duplicates an object under a new id (name suffixed " copy"), inserted
// right after the original, recording one undo step. Returns the new id.
SceneObjectId DuplicateObject(EditorDocument& doc, SceneObjectId id);
