#pragma once
#include "EditorPanels.h"
#include "FileBrowser.h"
#include <functional>
struct EditorContext;
class EditorPreferences;
class EditorMilestoneNotice;
struct EditorSceneViewport;
struct EditorPlaySession;
struct TerrainEditor;
struct VolumeTerrainEditor;
struct UtilityAIEditor;
struct SpriteSheetEditor;

// A call-scoped service view: cross-workflow dependencies are explicit and
// testable. No panel or workflow retains an application pointer.
struct EditorProjectServices {
    EditorSceneViewport &viewport;
    EditorPlaySession &session;
    EditorPreferences &preferences;
    EditorMilestoneNotice &updates;
    std::unique_ptr<TerrainEditor> &terrain;
    std::unique_ptr<VolumeTerrainEditor> &volumeTerrain;
    std::unique_ptr<UtilityAIEditor> &utilityAI;
    std::unique_ptr<SpriteSheetEditor> &spriteSheets;
    bool &quit;
    std::function<bool(std::string &)> startPlay;
    std::function<void()> stopPlay;
};

// Project/document commands and their pending confirmation/file-dialog state.
// Cross-workflow actions are supplied as typed services; this owner never owns the Play world.
class EditorProjectWorkflow {
  public:
    void RefreshProjectLists(EditorContext &);
    bool OpenProject(EditorContext &, EditorProjectServices &, const std::string &, std::string &);
    bool CreateProject(EditorContext &, EditorProjectServices &, const std::string &, const std::string &,
                       std::string &, bool = true, int = 0);
    std::string ResolveScenePath(EditorContext &, const std::string &) const;
    std::string WorldStatePathFor(EditorContext &, const std::string &) const;
    bool OpenScene(EditorContext &, EditorProjectServices &, const std::string &, std::string &);
    bool RunProject(EditorContext &, std::string &);
    void HandleRequests(EditorContext &, EditorProjectServices &, EditorRequests &);
    EditorRequests pendingDocumentAction;
    bool documentActionPending = false, documentActionApproved = false, documentActionSaveAs = false;
    bool actionNeedsScene = false, actionNeedsProject = false;
    std::string confirmationTitle;
    EditorFileBrowser fileBrowser;
    std::string newProjectDirectory, newProjectName, fileBrowserParent;
    int newProjectTemplate = 0;
};
