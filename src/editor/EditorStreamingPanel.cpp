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
#include "RegionRequests.h"
#include "SceneSession.h"
#include "WorldStreaming.h"

void DrawStreamingPanel(EditorDocument &doc, EditorPanelState &state, EditorRequests &requests) {
    if (!state.project || state.project->Settings().worldManifest.empty())
        return;
    if (!ImGui::Begin("World regions")) {
        ImGui::End();
        return;
    }
    auto *asset = state.assets ? state.assets->Find(state.project->Settings().worldManifest) : nullptr;
    WorldManifest manifest;
    std::string error;
    if (!asset || !LoadWorldManifest(asset->path, manifest, error)) {
        ImGui::TextWrapped("%s", error.c_str());
        ImGui::End();
        return;
    }
    if (state.mode == EditorMode::Edit) {
        ImGui::Checkbox("Read-only additive preview", &state.worldPreview);
        ImGui::SameLine();
        if (ImGui::Button("Refresh preview"))
            ++state.worldPreviewRevision;
        ImGui::TextWrapped("Choose an edit target to open its original source. Save writes only that "
                           "document; other preview regions are read-only.");
        for (auto &[id, r] : manifest.regions) {
            ImGui::PushID(id.c_str());
            ImGui::Text("%s (%s) / priority %d", id.c_str(), r.policy.c_str(), r.priority);
            ImGui::SameLine();
            if (ImGui::Button("Edit source"))
                requests.openSceneRelative = r.scene;
            ImGui::PopID();
        }
    } else if (state.runtime && state.runtime->SceneControl()) {
        auto *stream = state.runtime->SceneControl()->Streaming(*state.runtime, error);
        if (stream) {
            auto stats = stream->Stats();
            ImGui::Text("%zu active / %zu preparing", stats.active, stats.pending);
            ImGui::Text("pending %zu / live estimate %zu / retained %zu bytes", stats.pendingBytes,
                        stats.liveBytes, stats.retainedBytes);
            ImGui::Text("cache %zu / %zu bytes; boundary %.3f ms; largest unit %.3f ms",
                        stats.resourceResidentBytes, stats.resourceCacheBudget, stats.integrationMs,
                        stats.largestUnitMs);
            static EditorRegionRequests demand;
            for (auto &r : stream->Regions()) {
                ImGui::PushID(r.id.c_str());
                ImGui::Text("%s: %s (%zu/%zu)", r.id.c_str(), r.state.c_str(), r.installed, r.entities);
                ImGui::BeginDisabled(demand.Owns(*stream, r.id));
                if (ImGui::Button("Request"))
                    demand.Request(*stream, r.id, error);
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Release")) {
                    demand.Release(*stream, r.id);
                    stream->Unload(r.id);
                }
                for (auto &pin : r.pins)
                    ImGui::BulletText("Pinned: %s", pin.c_str());
                if (!r.error.empty())
                    ImGui::TextWrapped("%s", r.error.c_str());
                ImGui::PopID();
            }
        }
        if (!error.empty())
            ImGui::TextWrapped("%s", error.c_str());
    }
    (void)doc;
    ImGui::End();
}
