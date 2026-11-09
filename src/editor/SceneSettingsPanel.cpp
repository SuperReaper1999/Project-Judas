#include "ComponentEditors.h"
#include "EditorPanelWidgets.h"
#include "EditorPanels.h"
#include "EditorWidgets.h"
#include "EditorWorkspace.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <glm/gtc/quaternion.hpp>
#include <set>
#include <sstream>

using namespace EditorPanelWidgets;
#include "RenderPreview.h"

void DrawSceneSettingsPanel(EditorDocument &doc, EditorPanelState &state) {
    if (!state.showSceneSettings)
        return;
    const auto display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize({std::min(430.0f, display.x - 40), std::min(540.0f, display.y - 110)},
                             ImGuiCond_FirstUseEver);
    if (!BeginEditorPanel("Scene settings", &state.showSceneSettings, state)) {
        ImGui::End();
        return;
    }
    if (state.mode == EditorMode::Play)
        DrawRuntimeAppearance(doc, state);
    if (state.mode == EditorMode::Play)
        ImGui::BeginDisabled();
    SceneSettings &s = doc.GetScene().Settings();
    if (state.project)
        DrawCategoryMask(doc, "Main/editor camera render mask", s.mainCameraRenderMask,
                         state.project->Settings().classification.render);
    TextField(doc, "Name##scene", s.name);
    double origin[3] = {s.worldOrigin.x, s.worldOrigin.y, s.worldOrigin.z};
    if (ImGui::InputScalarN("World origin (m)", ImGuiDataType_Double, origin, 3, nullptr, nullptr, "%.6g")) {
        s.worldOrigin = glm::dvec3(origin[0], origin[1], origin[2]);
    }
    TrackEdit(doc);
    DragVec3(doc, "Sun direction", s.sunDirection, 0.01f);
    ImGui::TextDisabled("World direction toward the sun; independent of gravity and world up.");
    Checkbox(doc, "Sun enabled", s.sunEnabled);
    DragScalar(doc, "Sun intensity", s.sunIntensity, .05f, 0, 10000);
    ColorEdit(doc, "Sun color", s.sunColor);
    ColorEdit(doc, "Ambient", s.ambientColor);
    Checkbox(doc, "Linear HDR world rendering", s.linearRendering);
    DragScalar(doc, "Manual exposure", s.exposure, .01f, .001f, 10000);
    std::vector<std::string> names, ids;
    if (state.assets)
        for (const auto &[id, a] : state.assets->Records())
            if (a.type == AssetType::Environment) {
                names.push_back(a.relativePath);
                ids.push_back(id);
            }
    LabelledCombo(doc, "Environment lighting", s.environmentAsset, names, ids, true);
    DragScalar(doc, "Environment intensity", s.environmentIntensity, .01f, 0, 10000);
    Checkbox(doc, "Environment background", s.environmentBackground);
    ColorEdit(doc, "Background colour (linear in HDR)", s.backgroundColor);
    auto angles = glm::degrees(glm::eulerAngles(s.environmentRotation));
    if (ImGui::DragFloat3("Environment rotation degrees", &angles.x, .5f)) {
        doc.BeginEdit();
        s.environmentRotation = glm::quat(glm::radians(angles));
        doc.CommitEdit();
    }
    ImGui::TextDisabled(
        "Background is independent of lighting. Colour factors are linear; UI ignores world exposure.");
    if (!state.project || state.project->Settings().advancedFluidDynamics) {
    } else
        ImGui::TextDisabled("Advanced fluid controls disabled in Project Settings > Experimental.");
    static const char *const kPolicyNames[] = {"none (everything Full)", "distance"};
    Combo(doc, "Fidelity policy", s.fidelityPolicy, kPolicyNames, 2);
    if (s.fidelityPolicy == SceneFidelityPolicy::Distance) {
        DragScalar(doc, "Full radius (m)", s.fidelityFullRadius, 0.5f, 0.0f, 1.0e7f);
        DragScalar(doc, "Coarse radius (m)", s.fidelityCoarseRadius, 0.5f, 0.0f, 1.0e7f);
        ImGui::TextDisabled("Applies to bodies marked 'Managed'.");
    }
    if (state.mode == EditorMode::Play)
        ImGui::EndDisabled();
    ImGui::End();
}
