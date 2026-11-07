#include "TerrainV2Inspector.h"
#include "EditorDesign.h"
#include "EditorPanels.h"
#include "imgui.h"
#include "terrain/TerrainComponent.h"

namespace {
void Draw(EditorDocument &, SceneObject &object, EditorPanelState &state) {
    const auto &id = object.landscape->asset;
    const auto *asset = state.assets ? state.assets->Find(id) : nullptr;
    ImGui::TextWrapped("%s", asset ? asset->relativePath.c_str() : "Missing terrain source");
    ImGui::TextWrapped("This source owns the landscape, painted surfaces, caves and generated "
                       "collision. Use Terrain tools to edit it.");
    if (EditorDesign::ActionButton("Edit landscape source", EditorDesign::Tone::Primary)) {
        state.terrainEditAsset = id;
        state.showTerrain = true;
    }
    std::string error;
    if (!TerrainV2::ValidateOwnerPlacement(object, error))
        ImGui::TextWrapped("Landscape needs attention: %s", error.c_str());
    if (ImGui::Button("Validate terrain source"))
        state.status = TerrainV2::ValidateOwner(object, state.assets, error)
                           ? "Terrain setup is valid; Play also validates the stored heights."
                           : error;
    if (ImGui::TreeNode("Source identity")) {
        ImGui::TextWrapped("%s", id.c_str());
        if (ImGui::SmallButton("Copy terrain ID"))
            ImGui::SetClipboardText(id.c_str());
        ImGui::TreePop();
    }
}
} // namespace
ComponentEditor TerrainV2ComponentEditor() {
    return {"Landscape",
            'T',
            [](const SceneObject &o) { return bool(o.landscape); },
            nullptr,
            [](SceneObject &o) {
                o.landscape.reset();
                o.body.reset(); // Never expose the disabled identity Box as an uncut collider.
            },
            Draw,
            [](EditorPanelState &state) {
                state.terrainCreateRequested = true;
                state.showTerrain = true;
            }};
}
