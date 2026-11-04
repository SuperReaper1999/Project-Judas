#include "SceneSerialization.h"
#include "ComponentEditors.h"
#include "EditorPanelWidgets.h"
#include "EditorPanels.h"
#include "EditorWidgets.h"
#include "EditorWorkspace.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "imgui.h"
#include "imgui_internal.h"
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
#include "EditorAIWorkflow.h"
#include "EditorScriptWorkflow.h"
#include "ModelCollision.h"
#include "ModelCook.h"
#include "Prefab.h"
#include "RenderPreview.h"
#include "RuntimeInspector.h"

namespace {
bool ComponentHeader(EditorDocument &doc, const ComponentEditor &editor, SceneObject &o,
                     EditorPanelState &state) {
    ImGui::PushID(editor.name);
    if (state.revealComponent == editor.name) {
        ImGui::SetNextItemOpen(true);
        state.revealComponent.clear();
    }
    const bool common = std::string(editor.name) == "Scripts";
    const char *title =
        std::string(editor.name) == "Render" && o.render && o.render->shape == SceneShape::Sprite
            ? "Sprite"
            : editor.name;
    bool open =
        ImGui::CollapsingHeader(title, common ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None);
    ImGui::SetItemTooltip("Expand to edit. Right-click for component actions.");
    if (ImGui::BeginPopupContextItem("Component actions")) {
        if (ImGui::MenuItem("Remove component")) {
            doc.BeginEdit();
            editor.remove(o);
            doc.CommitEdit();
            open = false;
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return open && editor.has(o);
}

void DrawAddComponentMenu(EditorDocument &doc, SceneObject &o, EditorPanelState &state) {
    ImGui::SetNextItemWidth(-1);
    ImGui::SetNextWindowSizeConstraints({260, 0}, {600, 420});
    if (!ImGui::BeginCombo("##add", "Add component..."))
        return;
    static ImGuiTextFilter filter;
    if (ImGui::IsWindowAppearing()) {
        filter.Clear();
        ImGui::SetKeyboardFocusHere();
    }
    filter.Draw("Find component", -1);
    std::vector<const ComponentEditor *> editors;
    for (const auto &editor : ComponentEditorRegistry())
        editors.push_back(&editor);
    std::sort(editors.begin(), editors.end(),
              [](const auto *a, const auto *b) { return std::strcmp(a->name, b->name) < 0; });
    for (const auto *entry : editors) {
        const auto &editor = *entry;
        if (editor.has(o))
            continue;
        if (!filter.PassFilter(editor.name))
            continue;
        if (ImGui::Selectable(editor.name)) {
            doc.BeginEdit();
            editor.add(o);
            doc.CommitEdit();
            state.revealComponent = editor.name;
        }
    }
    ImGui::EndCombo();
}
} // namespace

void DrawInspectorPanel(EditorDocument &doc, EditorPanelState &state) {
    if (state.dockingEnabled ? !state.showInspector : !state.workspace.showRails)
        return;
    if (!BeginEditorPanel("Inspector", state.dockingEnabled ? &state.showInspector : nullptr, state,
                          &state.workspace.inspector)) {
        ImGui::End();
        return;
    }
    doc.PruneSelection();
    if (state.mode == EditorMode::Edit && doc.Selection().size() > 1) {
        ImGui::Text("%zu selected", doc.Selection().size());
        static glm::vec3 delta(0), angles(0), factor(1);
        static bool worldAxes = true, individual = true;
        ImGui::Checkbox("World axes", &worldAxes);
        ImGui::SameLine();
        ImGui::Checkbox("Individual origins", &individual);
        ImGui::DragFloat3("Translation delta", &delta.x, .05f);
        ImGui::DragFloat3("Rotation delta degrees", &angles.x, 1);
        ImGui::DragFloat3("Scale multiplier", &factor.x, .01f);
        if (ImGui::Button("Apply transform delta")) {
            std::string error;
            if (!doc.BatchTransform(delta, glm::quat(glm::radians(angles)), factor, error, individual,
                                    worldAxes))
                state.status = error;
            else {
                delta = angles = glm::vec3(0);
                factor = glm::vec3(1);
            }
        }
        if (ImGui::BeginCombo("Parent all (keep world pose)", "Choose parent")) {
            if (ImGui::Selectable("None")) {
                std::string error;
                if (!doc.ReparentSelection(0, error))
                    state.status = error;
            }
            for (auto &p : doc.GetScene().Objects())
                if (!doc.IsSelected(p.id) &&
                    ImGui::Selectable((p.name + " ##" + std::to_string(p.id)).c_str())) {
                    std::string error;
                    if (!doc.ReparentSelection(p.id, error))
                        state.status = error;
                }
            ImGui::EndCombo();
        }
        if (ImGui::CollapsingHeader("Advanced batch properties")) {
            auto common = ObjectProperties(*doc.SelectedObject());
            std::set<std::string> mixed;
            for (auto id : doc.Selection()) {
                auto properties = ObjectProperties(*doc.GetScene().Find(id));
                for (auto it = common.begin(); it != common.end();) {
                    auto found = properties.find(it->first);
                    if (found == properties.end())
                        it = common.erase(it);
                    else {
                        if (found->second != it->second)
                            mixed.insert(it->first);
                        ++it;
                    }
                }
            }
            ImGui::TextWrapped("Common authored fields: mixed values change only after Apply. Values "
                               "use normal record notation.");
            static std::string field;
            static char value[2048]{};
            if (ImGui::BeginCombo("Common property", field.c_str())) {
                for (auto &[key, v] : common)
                    if (key != "_name" &&
                        ImGui::Selectable((key + (mixed.count(key) ? " (mixed)" : "")).c_str(),
                                          field == key)) {
                        field = key;
                        CopyToBuffer(mixed.count(key) ? "" : v, value, sizeof(value));
                    }
                ImGui::EndCombo();
            }
            ImGui::InputText("New value", value, sizeof(value));
            if (ImGui::Button("Apply field") && !field.empty() && common.count(field)) {
                std::string error;
                if (!doc.BatchProperties({{field, value}}, error))
                    state.status = error;
            }
        }
    }
    SceneObject *o = doc.SelectedObject();
    if (!o) {
        ImGui::Spacing();
        ImGui::TextUnformatted("Select an object");
        ImGui::TextWrapped("Right-click the hierarchy to create an object, then select it here. Use Add "
                           "component for capabilities or Create script & attach for gameplay.");
        ImGui::Spacing();
        ImGui::BeginDisabled(state.mode != EditorMode::Edit);
        if (ImGui::Button("Scene settings"))
            state.showSceneSettings = true;
        ImGui::EndDisabled();
        ImGui::End();
        return;
    }
    if (state.mode == EditorMode::Play)
        DrawRuntimeRenderPreview(doc, o->id, state);
    if (state.mode == EditorMode::Play) {
        ImGui::TextDisabled("Authored values are read-only while playing.");
        ImGui::BeginDisabled();
    }
    TextField(doc, "Name", o->name);
    DrawTransformEditor(doc, *o);
    // Name and Transform remain visible while component details scroll.
    ImGui::BeginChild("Inspector component properties", {0, 0});
    if (state.mode == EditorMode::Play && state.runtime) {
        ImGui::EndDisabled();
        DrawSelectedAIShortcuts(*o, state);
        if (ImGui::CollapsingHeader("Ownership & readiness")) {
            DrawRuntimeEntity(*state.runtime, o->id, &state);
            if (ImGui::SmallButton("Open Runtime Inspector")) {
                state.runtimeInspectEntity = o->id;
                state.showRuntimeInspector = true;
            }
        }
        ImGui::BeginDisabled();
    }
    if (state.mode == EditorMode::Edit)
        DrawSelectedAIShortcuts(*o, state);
    ImGui::SeparatorText("Components");
    DrawAddComponentMenu(doc, *o, state);
    for (const ComponentEditor &editor : ComponentEditorRegistry()) {
        if (!editor.has(*o))
            continue;
        if (ComponentHeader(doc, editor, *o, state))
            editor.draw(doc, *o, state);
    }
    if (ImGui::CollapsingHeader("Add gameplay script", o->scripts.empty() ? ImGuiTreeNodeFlags_DefaultOpen
                                                                          : ImGuiTreeNodeFlags_None))
        DrawEditorScriptWorkflow(doc, *o, state);
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Object settings")) {
        ImGui::TextDisabled("Object ID: %llu", static_cast<unsigned long long>(o->id));
        Checkbox(doc, "Entity render visible", o->renderVisible);
        if (!o->prefabRoot) {
            const auto *parent = doc.GetScene().Find(o->parent);
            ImGui::TextWrapped("Parent (local transform)");
            ImGui::SetNextItemWidth(-1);
            ImGui::PushOverrideID(ImGui::GetID("Parent (local transform)"));
            if (ImGui::BeginCombo("", parent ? parent->name.c_str() : "None")) {
                if (ImGui::Selectable("None", !o->parent)) {
                    doc.BeginEdit();
                    o->parent = 0;
                    doc.CommitEdit();
                }
                for (const auto &candidate : doc.GetScene().Objects())
                    if (candidate.id != o->id &&
                        ImGui::Selectable(candidate.name.c_str(), candidate.id == o->parent)) {
                        const auto old = o->parent;
                        doc.BeginEdit();
                        o->parent = candidate.id;
                        std::string error;
                        if (!ValidateHierarchy(doc.GetScene(), error)) {
                            o->parent = old;
                            state.status = error;
                        }
                        doc.CommitEdit();
                    }
                ImGui::EndCombo();
            }
            ImGui::PopID();
        }
        if (state.project) {
            const auto &c = state.project->Settings().classification;
            DrawCategoryMask(doc, "Tags", o->tags, c.tags, false);
            if (state.runtime)
                ImGui::Text("Runtime entities matching these tags: %zu",
                            state.runtime->QueryEntities(o->tags).size());
            DrawCategoryLayer(doc, "Render layer", o->renderLayer, c.render);
        }
    }
    if (state.mode == EditorMode::Edit && state.project && state.assets && ImGui::TreeNode("Prefab tools")) {
        if (ImGui::Button("Create prefab from hierarchy")) {
            Scene source;
            std::string error;
            if (CreatePrefab(doc.GetScene(), o->id, source, error)) {
                auto path = std::filesystem::path(state.project->AssetsDir()) / "prefabs" /
                            ("prefab-" + MintAssetId() + ".judasprefab");
                std::filesystem::create_directories(path.parent_path());
                if (SaveSceneToFile(source, path.string(), error)) {
                    auto *db = const_cast<AssetDatabase *>(state.assets);
                    AssetRecord record;
                    if (db->Track(path.string(), record, error))
                        state.status = "Created prefab asset " + record.id;
                }
            }
            if (!error.empty())
                state.status = error;
        }
        ImGui::TreePop();
    }
    if (state.mode == EditorMode::Edit && doc.SelectedObject() &&
        ImGui::CollapsingHeader("Component clipboard")) {
        static int component = 0;
        const char *names[] = {"render", "body",     "animation", "socket",  "motor",
                               "audio",  "particle", "joint",     "ragdoll", "scripts"};
        ImGui::TextWrapped("Component clipboard");
        ImGui::SetNextItemWidth(-1);
        // Keep the original control identity while its visible label moves
        // above the full-width selector in the narrow inspector rail.
        ImGui::PushOverrideID(ImGui::GetID("Component clipboard"));
        ImGui::Combo("", &component, names, 10);
        ImGui::PopID();
        if (ImGui::Button("Copy component"))
            doc.CopyComponent(names[component]);
        ImGui::SameLine();
        if (ImGui::Button("Paste to selection")) {
            std::string error;
            if (!doc.PasteComponent(error))
                state.status = error;
        }
    }
    ImGui::Separator();
    if (state.mode == EditorMode::Play)
        ImGui::EndDisabled();
    if (o->prefabRoot && ImGui::CollapsingHeader("Prefab instance")) {
        const auto *root = doc.GetScene().Find(o->prefabRoot);
        ImGui::Text("Prefab member %llu / instance %llu", (unsigned long long)o->prefabSource,
                    (unsigned long long)o->prefabRoot);
        if (root)
            ImGui::TextWrapped("Source asset: %s", root->prefabAsset.c_str());
        ImGui::Text("Property overrides: %zu", o->prefabOverrides.size());
        if (state.mode == EditorMode::Edit && state.assets) {
            std::string revert;
            for (const auto &pair : o->prefabOverrides) {
                ImGui::PushID(pair.first.c_str());
                ImGui::TextWrapped("%s = %s", pair.first.c_str(), pair.second.c_str());
                if (ImGui::SmallButton("Revert property"))
                    revert = pair.first;
                ImGui::PopID();
            }
            if (!revert.empty()) {
                doc.BeginEdit();
                std::string error;
                if (!RevertPrefabProperty(doc.GetScene(), o->id, revert, *state.assets, error))
                    state.status = error;
                doc.CommitEdit(false);
                o = doc.SelectedObject();
                root = doc.GetScene().Find(o->prefabRoot);
            }
            if (root && o->id == o->prefabRoot && ImGui::Button("Apply this hierarchy to prefab source")) {
                const auto id = o->id;
                doc.BeginEdit();
                std::string error;
                if (!ApplyPrefabSource(doc.GetScene(), id, *state.assets, error))
                    state.status = error;
                else
                    state.status = "Saved prefab source; non-overridden instances refreshed";
                doc.CommitEdit(false);
                o = doc.SelectedObject();
                root = doc.GetScene().Find(o->prefabRoot);
            }
            if (ImGui::Button("Reload prefab sources")) {
                Scene resolved;
                std::string error;
                doc.BeginEdit();
                if (ResolvePrefabs(doc.GetScene(), state.assets, resolved, error)) {
                    doc.GetScene() = std::move(resolved);
                    state.status = "Reloaded prefab sources";
                } else
                    state.status = error;
                doc.CommitEdit(false);
                o = doc.SelectedObject();
                root = doc.GetScene().Find(o->prefabRoot);
            }
        }
        if (state.mode == EditorMode::Play && state.runtime && root && o->id == o->prefabRoot) {
            if (ImGui::Button("Spawn independent prefab")) {
                std::string error;
                auto placement = o->transform;
                placement.position += glm::vec3(0, 2, 3);
                const auto id = state.runtime->SpawnPrefab(root->prefabAsset, placement, error);
                state.lastPrefabSpawn = id;
                state.status = id ? "Spawned root " + std::to_string(id) : error;
            }
            if (state.lastPrefabSpawn && ImGui::Button("Destroy last spawned hierarchy")) {
                std::string error;
                state.status = state.runtime->DestroyHierarchy(state.lastPrefabSpawn, error)
                                   ? "Destroyed spawned hierarchy"
                                   : error;
                state.lastPrefabSpawn = 0;
            }
        }
    }
    if (state.mode == EditorMode::Play && state.runtime) {
        // Milestone 29: the runtime entity behind the selected object.
        if (ImGui::CollapsingHeader("Runtime diagnostics")) {
            const EntityRecord *e = state.runtime->FindEntity(o->id);
            if (!e) {
                ImGui::TextDisabled("Not a persistent entity (static content or no body).");
            } else {
                ImGui::Text("Persistent id: %llu (%s)", static_cast<unsigned long long>(e->id),
                            e->authored ? "authored" : "runtime-created");
                ImGui::Text("Lifecycle: %s", LifecycleName(e->lifecycle));
                ImGui::Text("Fidelity: %s%s", FidelityName(e->fidelity),
                            e->forcedFidelity ? " (forced)" : "");
                ImGui::Text("Managed by policy: %s | reduced form: %s", e->managed ? "yes" : "no",
                            e->requiresFull ? "none (Full-only)" : "coarse/dormant");
                if (e->fidelity == SimulationFidelity::Coarse) {
                    ImGui::Text("Coarse motion: %s | coarse steps: %llu",
                                e->coarseMotion == CoarseMotion::Settled ? "settled" : "inertial",
                                static_cast<unsigned long long>(e->coarseStepsSimulated));
                }
                ImGui::Text("Reconstructions: %u", e->reconstructions);
                EntityPhysicalState live;
                if (state.runtime->GetEntityState(e->id, live)) {
                    ImGui::Text("Position: %.3f %.3f %.3f", live.position.x, live.position.y,
                                live.position.z);
                    ImGui::Text("Velocity: %.3f %.3f %.3f", live.linearVelocity.x, live.linearVelocity.y,
                                live.linearVelocity.z);
                }
                if (e->lifecycle != EntityLifecycle::Destroyed) {
                    ImGui::TextDisabled("Debug override:");
                    std::string error;
                    if (ImGui::SmallButton("Force Full"))
                        state.runtime->ForceEntityFidelity(e->id, SimulationFidelity::Full, &error);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Force Coarse"))
                        state.runtime->ForceEntityFidelity(e->id, SimulationFidelity::Coarse, &error);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Force Dormant"))
                        state.runtime->ForceEntityFidelity(e->id, SimulationFidelity::Dormant, &error);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Clear"))
                        state.runtime->ForceEntityFidelity(e->id, std::nullopt, &error);
                    if (!error.empty())
                        state.status = error;
                }
            }
        }
        ImGui::Separator();
    }
    ImGui::EndChild();
    ImGui::End();
}
