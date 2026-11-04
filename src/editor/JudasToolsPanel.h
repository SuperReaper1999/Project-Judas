#pragma once
#include "imgui.h"
#include <memory>
#include <string>

struct EditorContext;
struct ScriptTools;

// A launcher for existing editor workflows. Owns only presentation selection,
// never another script runtime, project document or build session.
class JudasToolsPanel {
  public:
    void Draw(EditorContext &, std::unique_ptr<ScriptTools> &);

  private:
    void Scripts(EditorContext &, std::unique_ptr<ScriptTools> &);
    void Build(EditorContext &);
    void Utilities(EditorContext &);
    ImGuiTextFilter filter;
    std::string selectedScript;
};
