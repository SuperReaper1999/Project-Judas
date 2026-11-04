#pragma once

#include "EditorWorkspaceLayout.h"
#include "imgui.h"
#include <string>

struct EditorPanelState;
class EditorPreferences;

// Every panel calls End even when Begin returns false, matching Dear ImGui.
// The compatibility rectangle is only used by non-docking test/legacy hosts.
bool BeginEditorPanel(const char *name, bool *open, EditorPanelState &,
                      const EditorWorkspaceRect *legacy = nullptr, ImGuiWindowFlags flags = 0);

// One native dock host. Scene/Game content windows publish their own actual
// content bounds to the presenter; dock geometry never edits authored content.
class EditorWorkspace {
  public:
    void BeginFrame(EditorPanelState &, EditorPreferences &);
    void RecoverFloatingWindows();
    ImGuiID DockspaceId() const { return dockspace; }

  private:
    void BuildDefault(const std::string &workspace, ImVec2 size);
    ImGuiID dockspace = 0;
    bool firstFrame = true;
};

void DrawEditorWorkspaceSelector(EditorPreferences &, EditorPanelState &);
