#pragma once

#include <string>
#include <vector>
#include <memory>

#include <glm/glm.hpp>

#include "AssetDatabase.h"
#include "EditorDocument.h"
#include "EditorWorkspaceLayout.h"
#include "GizmoMath.h"
#include "JobSystem.h"
#include "PhysicsWorld.h"
#include "ModelCollision.h"
#include "ResourceManager.h"
#include "RuntimeUI.h"
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
    bool exportProject = false;
    bool runProject = false;
    bool rescanAssets = false;
    bool openJudasJS = false;
    bool openQuickStart = false;
    std::string openExternalPath;
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
    unsigned int meshesVisible=0,meshesCulled=0,emittersVisible=0,emittersCulled=0,particlesSubmitted=0;
    std::size_t fluidParticles = 0;
    float fluidMilliseconds = 0.0f;      // fluid solve inside the last step (0 when not measured)
    float surfaceMilliseconds = 0.0f;    // fluid surface rebuild (presentation)
    float sceneMilliseconds = 0.0f;      // RenderWorldFrame submission
    PhysicsWorld::StepStats physics;     // Milestone 32: broadphase/solver of the last step
    ResourceStats resources;
    JobStats jobs;  // Milestone 31
    bool playing = false;
};

struct ModelCookTask;
struct RuntimeRenderHistory;
struct EditorPanelState {
    EditorWorkspaceLayout workspace;
    float hierarchyWidth = 250.0f;
    float inspectorWidth = 340.0f;
    float assetBrowserHeight = 200.0f;
    bool showSceneSettings = false;
    bool showNavigationHelp = false;
    std::string assetSearch;
    int assetTypeFilter = -1;
    bool assetMissingOnly = false;
    bool selectAssetDetails = false;
    JobSystem* importJobs=nullptr;
    std::shared_ptr<ModelCookTask> importTask,importAccepted;
    bool importPublished=false;
    unsigned modelPreviewClip=0;float modelPreviewTime=0,modelPreviewYaw=.45f;bool modelPreviewPlaying=false,modelPreviewSkeleton=true;
    std::vector<std::string> skeletonPicked;
    std::optional<RagdollDefinition> skeletonFitPreview;
    SceneObjectId skeletonFitOwner=0;
    uint64_t skeletonFitGeneration=0;
    ModelCollisionCleanup collisionCleanup;CollisionDiagnostic collisionDiagnostic;std::string collisionDiagnosticAsset;bool modelDiagnosticVisible=false;
    std::uintptr_t modelPreviewToken=0;std::vector<std::string> modelPreviewHidden;
    std::string modelMapSource,modelMapTarget;
    CollisionDiagnostic modelCollisionDiagnostic;
    ModelCollisionCleanup modelCollisionCleanup;
    std::string modelSource,modelOutput="Assets/models/model.judasmodel",modelRecipe,modelMotionSource,modelMotionTake,modelMotionName="Motion";
    EditorMode mode = EditorMode::Edit;
    // Milestone 29: the live world while playing (null in edit mode) so the
    // inspector can show persistent identity, lifecycle and fidelity and
    // offer the debug override; and the world-state path for the World menu.
    bool worldPreview=false;unsigned worldPreviewRevision=0;
    RuntimeWorld* runtime = nullptr;
    std::shared_ptr<RuntimeRenderHistory> renderPreviewHistory; // discarded on Play/Stop or world replacement
    std::string worldStatePath;
    bool playPaused = false;
    bool showWorldBuilding=false;
    DebugLineList recipePreview;
    DebugLineList navigationPreview;
    DebugLineList liquidPreview;
    DebugLineList deformablePreview;
    DebugLineList collisionPreview;
    std::string deformableDestination="deformable.judasdeform",deformableSource,deformableGroup="selection",fractureInterface;
    glm::vec3 deformableSelectionMin{-1,-1,-1},deformableSelectionMax{1,1,1};
    int deformableColumns=12,deformableRows=16,deformableSubdivision=2;
    glm::vec3 deformableSize{2,3,1};
    bool textPreview=false,textReload=false;
    bool showUIDocument=false;bool uiCanvasPreview=false;UIDocument uiPreviewDocument;uint64_t uiPreviewRevision=0;
    std::uintptr_t uiPreviewToken=0;glm::ivec2 uiPreviewResolution{1280,720};
    std::map<std::string,UILayout> uiPreviewLayout;std::string uiCanvasSelected;glm::vec2 uiCanvasPick{-1};
    UIElement textElement;
    std::string previewLocale;
    glm::vec2 textPreviewPosition{0},textPreviewSize{0};
    std::string hierarchySearch;
    std::string status;
    std::string pathInput;  // Open / Save As text field
    std::vector<std::string> terrainSurfaces;
    glm::vec3 cameraFocus{0.0f};  // where "create" places new objects
    std::string runtimeInfo;      // one-line runtime summary while playing

    // Milestone 30: project, assets, gizmo, debug view, profiler.
    Project* project = nullptr;              // the open project (may be !IsLoaded())
    AssetDatabase* assets = nullptr;   // its asset database
    ResourceManager* resources = nullptr;    // Milestone 31: live resource states for the browser
    std::vector<std::string> sceneFiles;     // project-relative .judas files
    GizmoMode gizmoMode = GizmoMode::Translate;
    GizmoSpace gizmoSpace = GizmoSpace::World;
    bool gizmoSnap = false;
    bool gizmoIndividualOrigins = false;
    DebugViewOptions debug;
    bool showProfiler = false;
    bool profilerInspect = false;
    bool showProjectSettings = false;
    bool showAssetBrowser = true;
    ProfilerData profiler;
    std::string exportDestination;
    std::string runProjectInfo;  // last Run Project outcome
    // Asset Browser text fields.
    std::string importSourceInput;
    std::string importDestinationInput;
    std::string moveAssetInput;
    AssetId browserSelection;
    SceneObjectId lastPrefabSpawn = 0;
    // Hierarchy inline rename.
    SceneObjectId renamingId = kInvalidSceneObjectId;
    std::string renameBuffer;
};

void DrawEditorMainMenu(EditorDocument& doc, EditorPanelState& state, EditorRequests& requests);
void UpdateEditorWorkspaceLayout(EditorPanelState& state, glm::vec2 displaySize);
void DrawEditorWorkspaceChrome(EditorDocument& doc, EditorPanelState& state, EditorRequests& requests);
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
                                 const std::string& meshAssetId, const SceneAnimationComponent* animation = nullptr);

// Duplicates an object under a new id (name suffixed " copy"), inserted
// right after the original, recording one undo step. Returns the new id.
SceneObjectId DuplicateObject(EditorDocument& doc, SceneObjectId id);

void DrawStreamingPanel(EditorDocument&,EditorPanelState&,EditorRequests&);

struct CollisionCookSettings;
bool BakeEditorCollision(EditorDocument&,SceneObjectId,EditorPanelState&,const AssetId&,const CollisionCookSettings&,const std::string&);

void DrawWorldBuildingPanel(EditorDocument&,EditorPanelState&);
