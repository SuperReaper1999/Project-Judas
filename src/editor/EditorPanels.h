#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "EditorDocument.h"

// Milestone 28: the editor's Dear ImGui panels. Every panel operates on
// the EditorDocument's authored Scene through BeginEdit/CommitEdit; none
// of them touch a RuntimeWorld, PhysicsWorld or Renderer. They know
// generic engine components (body, render, gravity, light, ...) and
// nothing about any particular demonstration.

enum class EditorMode { Edit, Play };

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
    // Object creation: kind + where (the application fills the position).
    std::string createKind;
    glm::vec3 createPosition{0.0f};
};

class RuntimeWorld;

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
    std::vector<std::string> modelAssets;
    std::vector<std::string> textureAssets;
    std::vector<std::string> terrainSurfaces;
    glm::vec3 cameraFocus{0.0f};  // where "create" places new objects
    std::string runtimeInfo;      // one-line runtime summary while playing
};

void DrawEditorMainMenu(EditorDocument& doc, EditorPanelState& state, EditorRequests& requests);
void DrawHierarchyPanel(EditorDocument& doc, EditorPanelState& state, EditorRequests& requests);
void DrawInspectorPanel(EditorDocument& doc, EditorPanelState& state);
void DrawSceneSettingsPanel(EditorDocument& doc, EditorPanelState& state);
void DrawAssetPanel(EditorDocument& doc, EditorPanelState& state);
void DrawStatusBar(EditorDocument& doc, EditorPanelState& state);

// Creates an object of `kind` ("empty", "box", "sphere", "dynamic-box",
// "dynamic-sphere", "point-light", "spot-light", "player-start", "mesh")
// at `position`, recording one undo step. Returns its id.
SceneObjectId CreateObjectOfKind(EditorDocument& doc, const std::string& kind, const glm::vec3& position,
                                 const std::string& defaultMeshPath);
