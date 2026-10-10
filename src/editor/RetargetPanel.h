#pragma once

#include <string>

class Renderer;
struct EditorPanelState;
struct EditorRequests;
struct RetargetPanelState;

// M74 authoring workspace. Its CPU jobs own immutable inputs; it never creates
// a RuntimeWorld, runs project scripts, or modifies the open scene for preview.
void OpenRetargetPanel(EditorPanelState&, const std::string& source = {},
                       const std::string& targetRecipe = {}, const std::string& profile = {});
void DrawRetargetPanel(EditorPanelState&, EditorRequests&);
void UpdateRetargetPreview(EditorPanelState&, Renderer&, float deltaSeconds);
void ShutdownRetargetPreview(EditorPanelState&, Renderer&);
// Call on successful project replacement even when reopening the same path.
void RetireRetargetProject(EditorPanelState&);

// Existing editor-autotest harness only. It drives the same authoring services
// and preview without saving a profile/recipe or publishing runtime content.
bool BeginRetargetAutotest(EditorPanelState&, const std::string& profile, std::string& error);
// 0 loading/evaluating, 1 real preview rendered, -1 failed/cancelled.
int RetargetAutotestStatus(const EditorPanelState&, std::string& error);
