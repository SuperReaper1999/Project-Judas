#include "TerrainV2Inspector.h"
#include "EditorDesign.h"
#include "EditorPanels.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "imgui.h"
#include "terrain/TerrainAsset.h"
#include "terrain/TerrainComponent.h"
#include "terrain/TerrainPlanetBinding.h"
#include <algorithm>

namespace {
void Draw(EditorDocument &document, SceneObject &object, EditorPanelState &state) {
    const auto &id = object.landscape->asset;
    const auto *asset = state.assets ? state.assets->Find(id) : nullptr;
    ImGui::TextWrapped("%s", asset ? asset->relativePath.c_str() : "Missing terrain source");
    ImGui::TextWrapped("This source owns the landscape, painted surfaces, caves and generated "
                       "collision. Use Terrain tools to edit it.");
    if (object.landscape->planet) {
        const auto *planet = document.GetScene().Find(object.landscape->planet);
        ImGui::TextWrapped("Planet: %s — fixed geographic placement",
                           planet ? planet->name.c_str() : "Missing planetary owner");
        ImGui::TextWrapped("Use the brushes to change this patch. Moving or scaling its object "
                           "would detach it from the planet and is rejected at Play.");
    }
    if (EditorDesign::ActionButton("Edit landscape source", EditorDesign::Tone::Primary)) {
        state.terrainEditAsset = id;
        state.showTerrain = true;
    }
    bool navigation = object.navigationSurface.has_value();
    if (ImGui::Checkbox("Live terrain navigation", &navigation)) {
        NavigationSurfaceSettings settings;
        std::string error;
        TerrainV2::AssetPages pages;
        TerrainV2::Source source;
        if (!navigation ||
            (asset && pages.Open(asset->path, error) && pages.ReadAll(source, error))) {
            if (navigation) {
                settings.sources = CategoryBit(object.body ? object.body->collisionLayer : 0);
                settings.halfExtents = {(source.samplesX - 1) * source.spacing * .5f + 1, 2,
                                        (source.samplesZ - 1) * source.spacing * .5f + 1};
                for (float height : source.heights)
                    settings.halfExtents.y = std::max(settings.halfExtents.y, std::abs(height) + 2);
                if (source.chart) {
                    // Navigation is in the patch's local frame, not the
                    // absolute radial elevation stored in its height samples.
                    glm::vec3 extent(0);
                    for (unsigned z = 0; z < source.samplesZ; ++z)
                        for (unsigned x = 0; x < source.samplesX; ++x) {
                            const auto p = source.chart->LocalPoint(
                                {(double(x) - (source.samplesX - 1) * .5) * source.spacing,
                                 source.heights[size_t(z) * source.samplesX + x],
                                 (double(z) - (source.samplesZ - 1) * .5) * source.spacing});
                            extent = glm::max(extent, glm::abs(glm::vec3(p)));
                        }
                    settings.halfExtents = extent + glm::vec3(2);
                }
                settings.cellSize = std::max(.2f, source.spacing * .5f);
                settings.minRegion = 0;
            }
            document.BeginEdit();
            object.navigationSurface = navigation ? std::optional(settings) : std::nullopt;
            document.CommitEdit();
        } else
            state.status = "Cannot configure terrain navigation: " + error;
    }
    ImGui::TextWrapped("Live navigation follows this source's edits at collision publication. "
                       "Configure its profile and coverage in the Navigation surface card.");
    std::string error;
    if (!TerrainV2::ValidateOwnerPlacement(object, error))
        ImGui::TextWrapped("Landscape needs attention: %s", error.c_str());
    if (ImGui::Button("Validate terrain source"))
        state.status =
            TerrainV2::ValidateOwner(object, state.assets, error) &&
                    TerrainV2::ValidatePlanetTerrains(document.GetScene(), state.assets, error)
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
void DrawTerrainV2Navigation(EditorDocument &document, SceneObject &object,
                             EditorPanelState &state) {
    auto candidate = *object.navigationSurface;
    bool changed = ImGui::Checkbox("Enabled", &candidate.enabled);
    if (state.project) {
        const auto &profiles = state.project->Settings().navigation.profiles;
        const auto selected = profiles.find(candidate.profile);
        if (ImGui::BeginCombo("Agent profile", selected == profiles.end()
                                                   ? "Missing profile"
                                                   : selected->second.name.c_str())) {
            for (const auto &[id, profile] : profiles)
                if (ImGui::Selectable(profile.name.c_str(), id == candidate.profile)) {
                    candidate.profile = id;
                    changed = true;
                }
            ImGui::EndCombo();
        }
    }
    auto coverage = candidate.halfExtents * 2.f;
    if (ImGui::DragFloat3("Coverage size (metres)", &coverage.x, .1f, .2f, 8192)) {
        candidate.halfExtents = coverage * .5f;
        changed = true;
    }
    changed |= ImGui::DragFloat("Cell size (metres)", &candidate.cellSize, .01f, .02f, 2);
    changed |= ImGui::DragFloat("Height resolution (metres)", &candidate.cellHeight, .01f, .01f, 1);
    if (ImGui::TreeNode("Advanced navigation")) {
        changed |= ImGui::SliderInt("Tile size (cells)", &candidate.tileSize, 16, 128);
        changed |= ImGui::SliderInt("Minimum region (cells)", &candidate.minRegion, 0, 100);
        changed |= ImGui::DragFloat("Simplification", &candidate.simplification, .1f, .1f, 10);
        ImGui::TreePop();
    }
    if (changed) {
        document.BeginEdit();
        object.navigationSurface = std::move(candidate);
        document.CommitEdit();
    }
    ImGui::TextWrapped("Source-owned: only this landscape's actual collision is included. "
                       "Other objects use navigation obstacles or separate surfaces. "
                       "Local +Y is traversal up. No baked asset is required.");
    if (state.mode == EditorMode::Edit && ImGui::Button("Preview live terrain navigation"))
        BakeEditorNavigation(document, object.id, state);
    if (state.runtime)
        if (const auto *status = state.runtime->LiveTerrain().Status(object.id))
            ImGui::TextWrapped(
                "%s", status->error.empty()
                          ? (status->navigation ? "Live navigation ready" : "Navigation disabled")
                          : status->error.c_str());
}
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
