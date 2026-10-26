#include "RuntimeInspector.h"
#include "EditorPanels.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "ScriptSystem.h"
#include "SaveService.h"
#include "SaveEvolution.h"
#include "WorldStreaming.h"
#include "imgui.h"
#include <string>
#include <algorithm>

namespace {
const char* PhaseName(ScriptInstancePhase phase) {
    switch (phase) {
    case ScriptInstancePhase::Absent: return "not instantiated";
    case ScriptInstancePhase::AwaitingStart: return "waiting for start";
    case ScriptInstancePhase::AwaitingRestore: return "waiting for restore";
    case ScriptInstancePhase::Running: return "callbacks started";
    case ScriptInstancePhase::Faulted: return "faulted - see script diagnostics";
    }
    return "unknown";
}
void Copyable(const char* label, const std::string& value) {
    ImGui::TextWrapped("%s: %s", label, value.empty() ? "(none)" : value.c_str());
    ImGui::PushID(label);
    if (ImGui::SmallButton("Copy")) ImGui::SetClipboardText(value.c_str());
    ImGui::PopID();
}
}

void DrawRuntimeEntity(RuntimeWorld& world, SceneObjectId id) {
    const auto* definition = world.RuntimeDefinition(id);
    if (!definition) {
        ImGui::TextWrapped("Entity is not currently published. It may be unloaded or destroyed. Select an active entity or inspect World regions.");
        return;
    }
    auto scene = world.SceneControl();
    auto* streaming = scene ? scene->StreamingIfLoaded() : nullptr;
    const auto owner = streaming ? streaming->Owner(id) : "root";
    ImGui::Text("Entity %llu / %s", static_cast<unsigned long long>(id), definition->name.c_str());
    Copyable("Owner", owner);
    if (streaming) {
        Copyable("Save reference", streaming->PersistentKey(id));
        for (const auto& region : streaming->Regions()) {
            if (region.id != owner) continue;
            ImGui::Text("Region: %s; installed %zu / %zu", region.state.c_str(), region.installed, region.entities);
            ImGui::Text("Demands: %zu; visual resources: %s", region.demands, region.visualReady ? "ready" : "pending");
            for (const auto& pin : region.pins) ImGui::BulletText("Retained: %s", pin.c_str());
            if (!region.error.empty()) ImGui::TextWrapped("Region error: %s", region.error.c_str());
        }
    }
    if (owner == "root")
        ImGui::TextWrapped("Root content stays resident until world replacement or explicit destruction; distance alone will not unload it.");
    ImGui::Text("Physics body/proxy: %s; CharacterMotor: %s",
                world.RuntimeBody(id).IsValid() ? "published" : "absent",
                world.RuntimeCharacter(id) ? "published" : "absent");
    auto* scripts = world.Scripts();
    for (const auto& slot : definition->scripts) {
        const char* phase = !slot.enabled ? "disabled" : scripts ? PhaseName(scripts->InspectPhase(id, slot.id)) : "not instantiated";
        ImGui::BulletText("Script slot %llu: %s", static_cast<unsigned long long>(slot.id), phase);
    }
    if (scripts) for (const auto& diagnostic : scripts->Diagnostics()) {
        if (diagnostic.entity == id)
            ImGui::TextWrapped("%s: %s", diagnostic.callback.c_str(), diagnostic.message.c_str());
    }
    if (!definition->scripts.empty())
        ImGui::TextWrapped("Callbacks started is not project readiness. Wait for the specific model, navigation surface or restored registration your script needs.");
    if (definition->parent)
        ImGui::TextWrapped("Hierarchy is not a physical attachment. Ordinary visual children inherit presented poses; physical or socket-driven components can own their pose. See the timing guide before copying transforms.");
    if (definition->body)
        ImGui::TextWrapped("Transform writes teleport this body. Use forces, motors or kinematic commands for physical movement; use presentedTransform for camera follow.");
}

void DrawRuntimeInspector(EditorPanelState& state, EditorRequests& requests) {
    if (!state.showRuntimeInspector) return;
    ImGui::SetNextWindowSize({560, 620}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Runtime Inspector", &state.showRuntimeInspector)) { ImGui::End(); return; }
    if (ImGui::Button("Ownership, readiness & saves guide")) requests.openWorkflowGuide = true;
    ImGui::TextWrapped("Read-only observations. Opening this tool never requests regions, initializes saves, invokes script callbacks or captures script state.");
    auto* world = state.runtime;
    if (!world || state.mode != EditorMode::Play) {
        ImGui::TextWrapped("Start Play to inspect the live world. Use Escape to pause if you need to release the mouse.");
        ImGui::End();
        return;
    }
    ImGui::SeparatorText("Live entities (including streamed content)");
    const auto* current = world->RuntimeDefinition(state.runtimeInspectEntity);
    if (ImGui::BeginCombo("Entity", current ? current->name.c_str() : "Choose an active entity")) {
        // Enumerate lightweight IDs, not full multi-kilobyte SceneObject copies.
        const auto ids = world->DefinitionIds();
        ImGuiListClipper rows;
        rows.Begin(static_cast<int>(ids.size()));
        const auto selected = std::find(ids.begin(), ids.end(), state.runtimeInspectEntity);
        if (selected != ids.end()) rows.IncludeItemByIndex(static_cast<int>(selected - ids.begin()));
        while (rows.Step()) for (int i = rows.DisplayStart; i < rows.DisplayEnd; ++i) {
            const auto* object = world->RuntimeDefinition(ids[i]);
            if (!object) continue;
            const auto label = object->name + " / " + std::to_string(object->id);
            if (ImGui::Selectable(label.c_str(), object->id == state.runtimeInspectEntity))
                state.runtimeInspectEntity = object->id;
            if (object->id == state.runtimeInspectEntity) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (state.runtimeInspectEntity) DrawRuntimeEntity(*world, state.runtimeInspectEntity);
    if (ImGui::CollapsingHeader("Save compatibility (read-only)")) {
        auto scene = world->SceneControl();
        auto* saves = scene ? scene->SavesIfLoaded() : nullptr;
        if (!saves) ImGui::TextWrapped("The project has not initialized save slots. Use its normal save/refresh UI; this inspector does not create a save service.");
        else {
            Copyable("Project save identity", saves->Identity());
            Copyable("Installed content fingerprint", saves->Content());
            ImGui::TextWrapped("Editor Play uses isolated EditorSlots. Standalone uses Slots. An identity match alone does not certify every saved participant.");
            for (const auto& slot : saves->List()) {
                ImGui::PushID(slot.id.c_str());
                if (ImGui::TreeNode(slot.id.c_str(), "%s / %s", slot.id.c_str(), slot.status.c_str())) {
                    Copyable("Saved content", slot.content);
                    Copyable("Saved compatibility", slot.compatibility);
                    ImGui::Text("Saved content version: %u", slot.contentVersion);
                    if (!slot.error.empty()) ImGui::TextWrapped("%s", slot.error.c_str());
                    ImGui::TextWrapped("Keep the original slot and matching game build. Migrations require an explicit project policy and migration request; this panel never loads, migrates or deletes a save.");
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }
    }
    ImGui::End();
}
