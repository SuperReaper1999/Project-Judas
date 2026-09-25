#pragma once

#include <memory>
#include <string>
#include <vector>

#include "DebugDraw.h"
#include "EditorCamera.h"
#include "EditorDocument.h"
#include "EditorPanels.h"
#include "GizmoMath.h"
#include "Project.h"

class EngineHost;
class InteractivePlay;
class RuntimeWorld;

// Milestone 28/30: the `judas_editor` executable's orchestration. Brings up
// the same EngineHost the runtime uses, adds a Dear ImGui layer over the
// window, opens a PROJECT (M30) and runs one of two modes over an
// EditorDocument:
//
//   Edit  — the authored Scene is drawn directly (DrawAuthoredScene) from a
//           free editor camera; panels and the viewport gizmo edit it;
//           nothing is simulated.
//   Play  — a RuntimeWorld is instantiated from the Scene and driven by
//           the identical InteractivePlay frame the runtime runs; the
//           Scene itself is never written. Stop discards the runtime
//           world, so the authored scene is exactly what it was.
//
// "Run project" is a third thing: it launches the ordinary `judas` runtime
// as a separate process on the project's startup scene, exactly as a
// player would start the game. The editor never simulates that run.
//
// The engine never includes anything from src/editor or third_party/imgui.
class EditorApplication {
public:
    EditorApplication();
    ~EditorApplication();
    int Run(int argc, char** argv);

private:
    bool StartPlay(std::string& outError);
    void StopPlay();
    void HandleRequests(EditorRequests& requests);
    void PickAtPixel(int x, int y);
    void RefreshProjectLists();
    bool OpenProject(const std::string& projectFile, std::string& outError);
    bool CreateProject(const std::string& directory, const std::string& name, std::string& outError);
    bool OpenScene(const std::string& path, std::string& outError);
    bool RunProject(std::string& outMessage);
    std::string ResolveScenePath(const std::string& input) const;
    std::string WorldStatePathFor(const std::string& scenePath) const;
    void FrameEditMode(float deltaSeconds);
    void UpdateGizmo(bool allowInteraction);
    void DrawEditOverlay(class Renderer& renderer, const Scene& scene);
    void CollectProfilerData(float frameDeltaSeconds);

    EngineHost* m_host = nullptr;
    Project m_project;
    EditorDocument m_document;
    EditorPanelState m_panels;
    EditorCamera m_camera;
    std::unique_ptr<RuntimeWorld> m_world;
    std::unique_ptr<InteractivePlay> m_play;
    bool m_lookActive = false;
    bool m_quit = false;

    // Milestone 30: viewport gizmo interaction state.
    GizmoDrag m_drag;
    GizmoAxis m_hoverAxis = GizmoAxis::None;
    float m_gizmoHandleLength = 1.0f;
    std::string m_pendingDropAsset;  // Asset Browser drag currently over the viewport
    DebugLineList m_debugLines;
    DebugLineList m_gizmoLines;
    float m_frameAverageMilliseconds = 16.0f;
};
