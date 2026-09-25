#pragma once

#include <memory>
#include <string>

#include "EditorCamera.h"
#include "EditorDocument.h"
#include "EditorPanels.h"

class EngineHost;
class InteractivePlay;
class RuntimeWorld;

// Milestone 28: the `judas_editor` executable's orchestration. Brings up
// the same EngineHost the runtime uses, adds a Dear ImGui layer over the
// window, and runs one of two modes over an EditorDocument:
//
//   Edit  — the authored Scene is drawn directly (DrawAuthoredScene) from a
//           free editor camera; panels edit it; nothing is simulated.
//   Play  — a RuntimeWorld is instantiated from the Scene and driven by
//           the identical InteractivePlay frame the runtime runs; the
//           Scene itself is never written. Stop discards the runtime
//           world, so the authored scene is exactly what it was.
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
    void RefreshAssetLists();
    void FrameEditMode(float deltaSeconds);

    EngineHost* m_host = nullptr;
    EditorDocument m_document;
    EditorPanelState m_panels;
    EditorCamera m_camera;
    std::unique_ptr<RuntimeWorld> m_world;
    std::unique_ptr<InteractivePlay> m_play;
    bool m_lookActive = false;
    bool m_quit = false;
};
