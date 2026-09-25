#include "EditorPanels.h"

#include <algorithm>
#include <cstring>

#include <glm/gtc/quaternion.hpp>

#include "imgui.h"

#include "ComponentEditors.h"
#include "EditorWidgets.h"
#include "Project.h"
#include "RuntimeWorld.h"

namespace {
void CopyToBuffer(const std::string& s, char* buffer, std::size_t size) {
    std::strncpy(buffer, s.c_str(), size - 1);
    buffer[size - 1] = '\0';
}

bool ComponentHeader(EditorDocument& doc, const ComponentEditor& editor, SceneObject& o) {
    ImGui::PushID(editor.name);
    bool open = ImGui::CollapsingHeader(editor.name, ImGuiTreeNodeFlags_DefaultOpen);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
    if (ImGui::SmallButton("Remove")) {
        doc.BeginEdit();
        editor.remove(o);
        doc.CommitEdit();
        open = false;
    }
    ImGui::PopID();
    return open && editor.has(o);
}

void DrawAddComponentMenu(EditorDocument& doc, SceneObject& o) {
    if (!ImGui::BeginCombo("##add", "Add component...")) return;
    for (const ComponentEditor& editor : ComponentEditorRegistry()) {
        if (editor.has(o)) continue;
        if (ImGui::Selectable(editor.name)) {
            doc.BeginEdit();
            editor.add(o);
            doc.CommitEdit();
        }
    }
    ImGui::EndCombo();
}
}  // namespace

void DrawEditorMainMenu(EditorDocument& doc, EditorPanelState& state, EditorRequests& requests) {
    if (!ImGui::BeginMainMenuBar()) return;
    const bool editing = state.mode == EditorMode::Edit;
    const bool hasProject = state.project && state.project->IsLoaded();
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New project...", nullptr, false, editing)) requests.newProject = true;
        if (ImGui::MenuItem("Open project...", nullptr, false, editing)) requests.openProject = true;
        if (ImGui::MenuItem("Project settings", nullptr, state.showProjectSettings, hasProject)) {
            state.showProjectSettings = !state.showProjectSettings;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("New scene", nullptr, false, editing)) requests.newScene = true;
        if (ImGui::MenuItem("Open scene...", nullptr, false, editing)) requests.open = true;
        if (ImGui::MenuItem("Save scene", "Ctrl+S", false, editing)) requests.save = true;
        if (ImGui::MenuItem("Save scene as...", nullptr, false, editing)) requests.saveAs = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Quit")) requests.quit = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, editing && doc.CanUndo())) requests.undo = true;
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, editing && doc.CanRedo())) requests.redo = true;
        ImGui::Separator();
        const bool selected = doc.Selected() != kInvalidSceneObjectId;
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, editing && selected)) requests.duplicateId = doc.Selected();
        if (ImGui::MenuItem("Focus selection", "F", false, selected)) requests.focusSelection = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Translate gizmo", "W", state.gizmoMode == GizmoMode::Translate)) state.gizmoMode = GizmoMode::Translate;
        if (ImGui::MenuItem("Rotate gizmo", "E", state.gizmoMode == GizmoMode::Rotate)) state.gizmoMode = GizmoMode::Rotate;
        if (ImGui::MenuItem("Scale gizmo", "R", state.gizmoMode == GizmoMode::Scale)) state.gizmoMode = GizmoMode::Scale;
        if (ImGui::MenuItem("Local space", "X", state.gizmoSpace == GizmoSpace::Local)) {
            state.gizmoSpace = state.gizmoSpace == GizmoSpace::Local ? GizmoSpace::World : GizmoSpace::Local;
        }
        ImGui::MenuItem("Snap (hold Ctrl)", nullptr, &state.gizmoSnap);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Create", editing)) {
        const auto item = [&](const char* label, const char* kind) {
            if (ImGui::MenuItem(label)) { requests.createKind = kind; requests.createPosition = state.cameraFocus; }
        };
        item("Empty object", "empty");
        item("Static box", "box");
        item("Static sphere", "sphere");
        item("Dynamic box", "dynamic-box");
        item("Dynamic sphere", "dynamic-sphere");
        item("Mesh", "mesh");
        item("Point light", "point-light");
        item("Spot light", "spot-light");
        item("Door", "door");
        item("Gravity region", "gravity-region");
        item("Player start", "player-start");
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Asset Browser", nullptr, &state.showAssetBrowser);
        ImGui::MenuItem("Profiler", nullptr, &state.showProfiler);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Debug")) {
        DebugViewOptions& d = state.debug;
        ImGui::MenuItem("Collision shapes", nullptr, &d.collisionShapes);
        ImGui::MenuItem("Player capsule + support", nullptr, &d.playerCapsule);
        ImGui::MenuItem("Contacts", nullptr, &d.contacts);
        ImGui::MenuItem("Gravity vectors + regions", nullptr, &d.gravity);
        ImGui::MenuItem("Frame axes", nullptr, &d.frameAxes);
        ImGui::MenuItem("Lights", nullptr, &d.lights);
        ImGui::MenuItem("Interaction ranges", nullptr, &d.interactionRanges);
        ImGui::MenuItem("Lifecycle / fidelity", nullptr, &d.lifecycle);
        ImGui::MenuItem("Terrain normals (sampled)", nullptr, &d.terrainNormals);
        ImGui::MenuItem("Fluid particles", nullptr, &d.fluidParticles);
        ImGui::MenuItem("Atmosphere radii", nullptr, &d.atmosphere);
        ImGui::Separator();
        if (ImGui::MenuItem("All off")) d = DebugViewOptions{};
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("World", !editing)) {
        ImGui::TextDisabled("%s", state.worldStatePath.empty() ? "(no world-state path: save the scene first)"
                                                                : state.worldStatePath.c_str());
        if (ImGui::MenuItem("Save world state", "F6", false, !state.worldStatePath.empty())) requests.saveWorldState = true;
        if (ImGui::MenuItem("Delete world state (pristine baseline next Play)", "F7", false, !state.worldStatePath.empty())) requests.deleteWorldState = true;
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if (editing) {
        if (ImGui::MenuItem("Play scene", "F5")) requests.play = true;
        if (ImGui::MenuItem("Run project", nullptr, false, hasProject)) requests.runProject = true;
    } else {
        if (ImGui::MenuItem("Stop", "F5")) requests.stop = true;
        ImGui::TextDisabled(state.playPaused ? "PLAYING (paused - Escape resumes)" : "PLAYING - Escape pauses and frees the mouse");
    }
    ImGui::EndMainMenuBar();
}

void DrawHierarchyPanel(EditorDocument& doc, EditorPanelState& state, EditorRequests& requests) {
    ImGui::SetNextWindowPos(ImVec2(0.0f, 24.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300.0f, 356.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Hierarchy")) { ImGui::End(); return; }
    const bool editing = state.mode == EditorMode::Edit;
    Scene& scene = doc.GetScene();
    ImGui::TextDisabled("%zu objects   (double-click renames)", scene.Objects().size());
    ImGui::Separator();
    SceneObjectId toDelete = kInvalidSceneObjectId;
    int move = 0;
    SceneObjectId moveId = kInvalidSceneObjectId;
    ImGui::BeginChild("list", ImVec2(0.0f, -32.0f));
    for (const SceneObject& o : scene.Objects()) {
        ImGui::PushID(static_cast<int>(o.id));
        const bool selected = o.id == doc.Selected();
        if (editing && state.renamingId == o.id) {
            char buffer[256];
            CopyToBuffer(state.renameBuffer, buffer, sizeof(buffer));
            ImGui::SetKeyboardFocusHere();
            if (ImGui::InputText("##rename", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                if (SceneObject* target = scene.Find(o.id)) {
                    doc.BeginEdit();
                    target->name = buffer;
                    doc.CommitEdit();
                }
                state.renamingId = kInvalidSceneObjectId;
            } else {
                state.renameBuffer = buffer;
                if (ImGui::IsItemDeactivated() || ImGui::IsKeyPressed(ImGuiKey_Escape)) state.renamingId = kInvalidSceneObjectId;
            }
            ImGui::PopID();
            continue;
        }
        std::string label = o.name.empty() ? "(unnamed)" : o.name;
        const std::string indicators = ComponentIndicators(o);
        if (!indicators.empty()) label += "  [" + indicators + "]";
        if (state.runtime) {
            if (const EntityRecord* e = state.runtime->FindEntity(o.id)) {
                label += e->lifecycle == EntityLifecycle::Destroyed ? "  [destroyed]"
                                                                     : std::string("  [") + FidelityName(e->fidelity) + "]";
            }
        }
        // Problem indicator: a mesh render whose asset does not resolve.
        bool broken = false;
        if (o.render && o.render->shape == SceneShape::Mesh) {
            const AssetRecord* record = state.assets && !o.render->meshAsset.empty() ? state.assets->Find(o.render->meshAsset) : nullptr;
            broken = !record || record->missing;
        }
        if (broken) label += "  (!)";
        label += "##" + std::to_string(o.id);
        if (broken) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.55f, 0.3f, 1.0f));
        if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
            doc.Select(o.id);
            if (editing && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                state.renamingId = o.id;
                state.renameBuffer = o.name;
            }
        }
        if (broken) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("id %llu%s%s", static_cast<unsigned long long>(o.id),
                              indicators.empty() ? "" : ("  components: " + indicators).c_str(),
                              broken ? "\nmesh asset missing or unknown" : "");
        }
        if (editing && ImGui::BeginPopupContextItem()) {
            doc.Select(o.id);
            if (ImGui::MenuItem("Rename")) { state.renamingId = o.id; state.renameBuffer = o.name; }
            if (ImGui::MenuItem("Duplicate")) requests.duplicateId = o.id;
            if (ImGui::MenuItem("Move up")) { move = -1; moveId = o.id; }
            if (ImGui::MenuItem("Move down")) { move = 1; moveId = o.id; }
            if (ImGui::MenuItem("Delete")) toDelete = o.id;
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    if (editing) {
        if (ImGui::Button("Delete") && doc.Selected() != kInvalidSceneObjectId) toDelete = doc.Selected();
        ImGui::SameLine();
        if (ImGui::Button("Duplicate") && doc.Selected() != kInvalidSceneObjectId) requests.duplicateId = doc.Selected();
        ImGui::SameLine();
        if (ImGui::Button("Focus")) requests.focusSelection = true;
    }
    if (toDelete != kInvalidSceneObjectId) {
        doc.BeginEdit();
        scene.DestroyObject(toDelete);
        doc.CommitEdit();
        if (doc.Selected() == toDelete) doc.Select(kInvalidSceneObjectId);
        state.status = "Deleted object";
    }
    if (move != 0) {
        doc.BeginEdit();
        scene.MoveObject(moveId, move);
        doc.CommitEdit();
    }
    ImGui::End();
}

void DrawInspectorPanel(EditorDocument& doc, EditorPanelState& state) {
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 400.0f, 24.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400.0f, ImGui::GetIO().DisplaySize.y - 24.0f - 28.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Inspector")) { ImGui::End(); return; }
    SceneObject* o = doc.SelectedObject();
    if (!o) {
        ImGui::TextDisabled("Nothing selected. Click an object in the viewport or the hierarchy.");
        ImGui::End();
        return;
    }
    if (state.mode == EditorMode::Play && state.runtime) {
        // Milestone 29: the runtime entity behind the selected object.
        if (ImGui::CollapsingHeader("Runtime entity (M29)", ImGuiTreeNodeFlags_DefaultOpen)) {
            const EntityRecord* e = state.runtime->FindEntity(o->id);
            if (!e) {
                ImGui::TextDisabled("Not a persistent entity (static content or no body).");
            } else {
                ImGui::Text("Persistent id: %llu (%s)", static_cast<unsigned long long>(e->id), e->authored ? "authored" : "runtime-created");
                ImGui::Text("Lifecycle: %s", LifecycleName(e->lifecycle));
                ImGui::Text("Fidelity: %s%s", FidelityName(e->fidelity), e->forcedFidelity ? " (forced)" : "");
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
                    ImGui::Text("Position: %.3f %.3f %.3f", live.position.x, live.position.y, live.position.z);
                    ImGui::Text("Velocity: %.3f %.3f %.3f", live.linearVelocity.x, live.linearVelocity.y, live.linearVelocity.z);
                }
                if (e->lifecycle != EntityLifecycle::Destroyed) {
                    ImGui::TextDisabled("Debug override:");
                    std::string error;
                    if (ImGui::SmallButton("Force Full")) state.runtime->ForceEntityFidelity(e->id, SimulationFidelity::Full, &error);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Force Coarse")) state.runtime->ForceEntityFidelity(e->id, SimulationFidelity::Coarse, &error);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Force Dormant")) state.runtime->ForceEntityFidelity(e->id, SimulationFidelity::Dormant, &error);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Clear")) state.runtime->ForceEntityFidelity(e->id, std::nullopt, &error);
                    if (!error.empty()) state.status = error;
                }
            }
        }
        ImGui::Separator();
    }
    if (state.mode == EditorMode::Play) {
        ImGui::TextDisabled("Authored values are read-only while playing.");
        ImGui::BeginDisabled();
    }
    ImGui::Text("id %llu   components: %s", static_cast<unsigned long long>(o->id), ComponentIndicators(*o).c_str());
    TextField(doc, "Name", o->name);
    DrawTransformEditor(doc, *o);
    for (const ComponentEditor& editor : ComponentEditorRegistry()) {
        if (!editor.has(*o)) continue;
        if (ComponentHeader(doc, editor, *o)) editor.draw(doc, *o, state);
    }
    ImGui::Separator();
    DrawAddComponentMenu(doc, *o);
    if (state.mode == EditorMode::Play) ImGui::EndDisabled();
    ImGui::End();
}

void DrawSceneSettingsPanel(EditorDocument& doc, EditorPanelState& state) {
    ImGui::SetNextWindowPos(ImVec2(0.0f, 380.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300.0f, 180.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Scene")) { ImGui::End(); return; }
    if (state.mode == EditorMode::Play) ImGui::BeginDisabled();
    SceneSettings& s = doc.GetScene().Settings();
    TextField(doc, "Name##scene", s.name);
    double origin[3] = {s.worldOrigin.x, s.worldOrigin.y, s.worldOrigin.z};
    if (ImGui::InputScalarN("World origin (m)", ImGuiDataType_Double, origin, 3, nullptr, nullptr, "%.6g")) {
        s.worldOrigin = glm::dvec3(origin[0], origin[1], origin[2]);
    }
    TrackEdit(doc);
    DragVec3(doc, "Sun direction", s.sunDirection, 0.01f);
    ColorEdit(doc, "Sun color", s.sunColor);
    ColorEdit(doc, "Ambient", s.ambientColor);
    DragScalar(doc, "Fluid scale", s.fluidScale, 0.1f, 0.01f, 1000.0f);
    static const char* const kPolicyNames[] = {"none (everything Full)", "distance"};
    Combo(doc, "Fidelity policy", s.fidelityPolicy, kPolicyNames, 2);
    if (s.fidelityPolicy == SceneFidelityPolicy::Distance) {
        DragScalar(doc, "Full radius (m)", s.fidelityFullRadius, 0.5f, 0.0f, 1.0e7f);
        DragScalar(doc, "Coarse radius (m)", s.fidelityCoarseRadius, 0.5f, 0.0f, 1.0e7f);
        ImGui::TextDisabled("Applies to bodies marked 'Managed'.");
    }
    if (state.mode == EditorMode::Play) ImGui::EndDisabled();
    ImGui::End();
}

void DrawAssetBrowserPanel(EditorDocument& doc, EditorPanelState& state, EditorRequests& requests) {
    (void)doc;
    if (!state.showAssetBrowser) return;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 560.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(560.0f, 212.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Asset Browser", &state.showAssetBrowser)) { ImGui::End(); return; }
    const bool hasProject = state.project && state.project->IsLoaded() && state.assets;
    if (!hasProject) {
        ImGui::TextDisabled("No project open. File > New project / Open project.");
        ImGui::End();
        return;
    }
    const AssetDatabase& db = *state.assets;
    ImGui::TextDisabled("%s  (%zu assets, %zu untracked, %zu problems)", db.AssetsDir().c_str(), db.Records().size(),
                        db.Untracked().size(), db.Problems().size());
    ImGui::SameLine();
    if (ImGui::SmallButton("Rescan")) requests.rescanAssets = true;

    if (ImGui::CollapsingHeader("Import", ImGuiTreeNodeFlags_DefaultOpen)) {
        char source[512], destination[256];
        CopyToBuffer(state.importSourceInput, source, sizeof(source));
        CopyToBuffer(state.importDestinationInput, destination, sizeof(destination));
        if (ImGui::InputTextWithHint("Source file", "/path/to/model.obj | texture.png | font.ttf", source, sizeof(source))) state.importSourceInput = source;
        if (ImGui::InputTextWithHint("Destination (in assets)", "models/model.obj (empty keeps the file name)", destination, sizeof(destination))) state.importDestinationInput = destination;
        if (ImGui::Button("Import") && !state.importSourceInput.empty()) {
            requests.importSource = state.importSourceInput;
            requests.importDestination = state.importDestinationInput;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(path field; the editor has no OS file dialog)");
    }

    ImGui::BeginChild("assets", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    if (ImGui::BeginTable("assetTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Path");
        ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Id", ImGuiTableColumnFlags_WidthFixed, 130.0f);
        ImGui::TableHeadersRow();
        for (const auto& [id, record] : db.Records()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(AssetTypeName(record.type));
            ImGui::TableSetColumnIndex(1);
            ImGui::PushID(id.c_str());
            const bool selected = state.browserSelection == id;
            if (ImGui::Selectable(record.relativePath.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                state.browserSelection = id;
                state.moveAssetInput = record.relativePath;
                const std::string assetsRelative = record.path.size() > db.AssetsDir().size() + 1 &&
                                                           record.path.compare(0, db.AssetsDir().size(), db.AssetsDir()) == 0
                                                       ? record.path.substr(db.AssetsDir().size() + 1)
                                                       : record.relativePath;
                state.moveAssetInput = assetsRelative;
            }
            if (!record.missing && ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload(kAssetDragPayload, id.data(), id.size());
                ImGui::Text("%s (%s)", record.relativePath.c_str(), AssetTypeName(record.type));
                ImGui::TextDisabled(record.type == AssetType::Mesh ? "Drop on the viewport to place, or on a Mesh field."
                                                                    : "Drop on a matching inspector field.");
                ImGui::EndDragDropSource();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\nid %s\nsource %s", record.path.c_str(), id.c_str(), record.source.c_str());
            ImGui::PopID();
            ImGui::TableSetColumnIndex(2);
            if (record.missing) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "MISSING");
            } else if (state.resources) {
                // Milestone 31: the live resource state behind the asset.
                const ResourceState rs = state.resources->StateOf(id);
                switch (rs) {
                    case ResourceState::Queued:
                    case ResourceState::Loading:
                    case ResourceState::CpuReady:
                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "loading");
                        break;
                    case ResourceState::Ready:
                        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "ready %.1f KB", static_cast<double>(state.resources->BytesOf(id)) / 1024.0);
                        break;
                    case ResourceState::Failed:
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "FAILED");
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", state.resources->ErrorOf(id).c_str());
                        break;
                    default:
                        ImGui::TextDisabled("unloaded");
                        break;
                }
            } else {
                ImGui::TextDisabled("ok");
            }
            ImGui::TableSetColumnIndex(3);
            ImGui::TextDisabled("%.12s...", id.c_str());
        }
        ImGui::EndTable();
    }
    if (!state.browserSelection.empty()) {
        if (const AssetRecord* record = db.Find(state.browserSelection)) {
            ImGui::Separator();
            ImGui::Text("Selected: %s", record->relativePath.c_str());
            char moveTo[256];
            CopyToBuffer(state.moveAssetInput, moveTo, sizeof(moveTo));
            if (ImGui::InputText("Rename / move to (in assets)", moveTo, sizeof(moveTo))) state.moveAssetInput = moveTo;
            if (ImGui::Button("Apply move") && !state.moveAssetInput.empty()) {
                requests.moveAssetId = state.browserSelection;
                requests.moveAssetTo = state.moveAssetInput;
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove asset")) requests.removeAssetId = state.browserSelection;
            ImGui::SameLine();
            ImGui::TextDisabled("Scene references use the id, so a move keeps them valid.");
        }
    }
    if (!db.Untracked().empty()) {
        ImGui::Separator();
        ImGui::TextDisabled("Untracked files (no .judasmeta; not referenceable until tracked):");
        for (const std::string& path : db.Untracked()) {
            ImGui::PushID(path.c_str());
            ImGui::BulletText("%s", path.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Track")) requests.trackAssetPath = path;
            ImGui::PopID();
        }
    }
    if (!db.Problems().empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "Problems:");
        for (const AssetProblem& problem : db.Problems()) ImGui::BulletText("%s: %s", problem.path.c_str(), problem.message.c_str());
    }
    ImGui::EndChild();
    ImGui::End();
}

void DrawProjectSettingsPanel(EditorDocument& doc, EditorPanelState& state, EditorRequests& requests) {
    (void)doc;
    if (!state.showProjectSettings || !state.project) return;
    ImGui::SetNextWindowSize(ImVec2(460.0f, 260.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Project settings", &state.showProjectSettings)) { ImGui::End(); return; }
    Project& project = *state.project;
    if (!project.IsLoaded()) {
        ImGui::TextDisabled("No project open.");
        ImGui::End();
        return;
    }
    ProjectSettings& s = project.Settings();
    ImGui::TextDisabled("%s", project.ProjectFile().c_str());
    char name[256];
    CopyToBuffer(s.name, name, sizeof(name));
    if (ImGui::InputText("Name", name, sizeof(name))) s.name = name;
    if (ImGui::BeginCombo("Startup scene", s.startupScene.empty() ? "(none)" : s.startupScene.c_str())) {
        for (const std::string& scene : state.sceneFiles) {
            if (ImGui::Selectable(scene.c_str(), scene == s.startupScene)) s.startupScene = scene;
        }
        ImGui::EndCombo();
    }
    ImGui::Text("Assets: %s   Scenes: %s   Saves: %s", s.assetsDir.c_str(), s.scenesDir.c_str(), s.savesDir.c_str());
    if (ImGui::Button("Save project")) requests.saveProject = true;
    ImGui::SameLine();
    if (ImGui::Button("Run project")) requests.runProject = true;
    ImGui::Separator();
    ImGui::TextDisabled("Scenes in the project (double-click opens):");
    for (const std::string& scene : state.sceneFiles) {
        if (ImGui::Selectable(scene.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            requests.openSceneRelative = scene;
        }
    }
    if (!state.runProjectInfo.empty()) { ImGui::Separator(); ImGui::TextWrapped("%s", state.runProjectInfo.c_str()); }
    ImGui::End();
}

void DrawProfilerPanel(EditorPanelState& state) {
    if (!state.showProfiler) return;
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 400.0f - 330.0f, 24.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(330.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Profiler", &state.showProfiler)) { ImGui::End(); return; }
    const ProfilerData& p = state.profiler;
    ImGui::Text("Frame: %.2f ms  (%.0f FPS, rolling)", p.frameMilliseconds, p.framesPerSecond);
    if (!p.playing) ImGui::TextDisabled("Edit mode: no simulation stepping.");
    ImGui::Text("Fixed steps this frame: %d", p.fixedStepsThisFrame);
    ImGui::Text("Last fixed step: %.3f ms (whole StepPlayedWorld)", p.fixedStepMilliseconds);
    ImGui::Separator();
    ImGui::Text("Physics bodies: %zu live (%zu dynamic)", p.physicsBodies, p.dynamicBodies);
    ImGui::Text("Contacts (last step, final iteration): %zu", p.contacts);
    ImGui::Text("Entities: %zu full / %zu coarse / %zu dormant / %zu destroyed", p.entitiesFull, p.entitiesCoarse,
                p.entitiesDormant, p.entitiesDestroyed);
    ImGui::Separator();
    ImGui::Text("Draw calls: %u  (incl. shadow passes)", p.drawCalls);
    ImGui::Text("Triangles submitted: %u", p.triangles);
    ImGui::Text("Shadow passes: %u   Dynamic lights: %u", p.shadowPasses, p.dynamicLights);
    ImGui::Text("Debug lines: %u", p.debugLines);
    ImGui::Text("Scene submission: %.2f ms", p.sceneMilliseconds);
    ImGui::Separator();
    ImGui::Text("Fluid particles: %zu", p.fluidParticles);
    ImGui::Text("Fluid solve (in step): %s", p.fluidMilliseconds > 0.0f ? "" : "not measured");
    if (p.fluidMilliseconds > 0.0f) { ImGui::SameLine(); ImGui::Text("%.3f ms", p.fluidMilliseconds); }
    ImGui::Text("Fluid surface rebuild: %.2f ms", p.surfaceMilliseconds);
    ImGui::Separator();
    ImGui::Text("Resources: %zu ready (%zu meshes, %zu textures), %zu loading, %zu failed, %zu terrain meshes",
                p.resources.ready, p.resources.loadedMeshes, p.resources.loadedTextures, p.resources.loading,
                p.resources.failed, p.resources.loadedTerrainMeshes);
    ImGui::Text("  resident %.1f MB of %.0f MB budget (peak %.1f MB)", p.resources.bytesResident / 1048576.0,
                p.resources.budgetBytes / 1048576.0, p.resources.peakBytesResident / 1048576.0);
    ImGui::Text("  hits %llu / loads %llu / uploads %llu / evictions %llu / cancelled %llu / stale %llu",
                p.resources.hits, p.resources.misses, p.resources.uploads, p.resources.evictions, p.resources.cancelled,
                p.resources.staleDiscarded);
    ImGui::Separator();
    const double utilization = p.jobs.workers > 0 && p.jobs.elapsedSeconds > 0.0
                                   ? 100.0 * p.jobs.workerBusySeconds / (p.jobs.workers * p.jobs.elapsedSeconds)
                                   : 0.0;
    ImGui::Text("Jobs: %u workers, %zu queued, %zu running", p.jobs.workers, p.jobs.queued, p.jobs.running);
    ImGui::Text("  %llu completed / %llu failed / %llu cancelled of %llu submitted", p.jobs.completed, p.jobs.failed,
                p.jobs.cancelled, p.jobs.submitted);
    ImGui::Text("  worker utilization since start: %.1f%%", utilization);
    ImGui::TextDisabled("All times are wall-clock on the editor thread.");
    ImGui::End();
}

void DrawStatusBar(EditorDocument& doc, EditorPanelState& state) {
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0.0f, io.DisplaySize.y - 26.0f));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 26.0f));
    ImGui::Begin("##status", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
    const char* gizmo = state.gizmoMode == GizmoMode::Translate ? "Move" : state.gizmoMode == GizmoMode::Rotate ? "Rotate" : "Scale";
    ImGui::Text("%s | %s%s | %s | %s/%s%s | %s",
                state.project && state.project->IsLoaded() ? state.project->Settings().name.c_str() : "(no project)",
                doc.Path().empty() ? "(unsaved scene)" : doc.Path().c_str(), doc.IsDirty() ? " *" : "",
                state.mode == EditorMode::Edit ? "Edit" : "Play", gizmo,
                state.gizmoSpace == GizmoSpace::World ? "world" : "local", state.gizmoSnap ? "/snap" : "",
                state.status.c_str());
    if (!state.runtimeInfo.empty()) { ImGui::SameLine(); ImGui::TextDisabled("%s", state.runtimeInfo.c_str()); }
    ImGui::End();
}

SceneObjectId CreateObjectOfKind(EditorDocument& doc, const std::string& kind, const glm::vec3& position,
                                 const std::string& meshAssetId) {
    doc.BeginEdit();
    Scene& scene = doc.GetScene();
    SceneObject& o = scene.CreateObject(kind == "empty" ? "Empty" : kind == "box" ? "Box"
                                        : kind == "sphere" ? "Sphere" : kind == "dynamic-box" ? "Dynamic box"
                                        : kind == "dynamic-sphere" ? "Dynamic sphere" : kind == "mesh" ? "Mesh"
                                        : kind == "point-light" ? "Point light" : kind == "spot-light" ? "Spot light"
                                        : kind == "door" ? "Door" : kind == "gravity-region" ? "Gravity region"
                                        : kind == "player-start" ? "Player start" : "Object");
    o.transform.position = position;
    if (kind == "box" || kind == "dynamic-box") {
        o.render = SceneRenderComponent{};
        o.render->shape = SceneShape::Box;
        o.body = SceneBodyComponent{};
        o.body->shape = SceneShape::Box;
        o.body->motion = kind == "box" ? SceneBodyMotion::Static : SceneBodyMotion::Dynamic;
        o.body->mass = 5.0f;
        o.body->pickable = kind == "dynamic-box";
    } else if (kind == "sphere" || kind == "dynamic-sphere") {
        o.render = SceneRenderComponent{};
        o.render->shape = SceneShape::Sphere;
        o.body = SceneBodyComponent{};
        o.body->shape = SceneShape::Sphere;
        o.body->motion = kind == "sphere" ? SceneBodyMotion::Static : SceneBodyMotion::Dynamic;
        o.body->mass = 4.0f;
        o.body->pickable = kind == "dynamic-sphere";
    } else if (kind == "mesh") {
        o.render = SceneRenderComponent{};
        o.render->shape = SceneShape::Mesh;
        o.render->meshAsset = meshAssetId;
        o.render->color = glm::vec3(1.0f);
    } else if (kind == "point-light" || kind == "spot-light") {
        o.light = SceneLightComponent{};
        o.light->kind = kind == "point-light" ? SceneLightKind::Point : SceneLightKind::Spot;
        o.light->color = glm::vec3(3.0f, 2.8f, 2.4f);
    } else if (kind == "door") {
        o.render = SceneRenderComponent{};
        o.render->shape = SceneShape::Box;
        o.render->halfExtents = glm::vec3(1.0f, 1.0f, 0.1f);
        o.render->color = glm::vec3(0.55f, 0.38f, 0.22f);
        o.door = SceneDoorComponent{};
    } else if (kind == "gravity-region") {
        o.gravity = SceneGravityComponent{};
        o.gravity->kind = SceneGravityKind::Uniform;
        o.gravity->regionShape = SceneRegionShape::Box;
        o.gravity->regionHalfExtents = glm::vec3(20.0f);
    } else if (kind == "player-start") {
        o.playerStart = ScenePlayerStartComponent{};
    }
    const SceneObjectId id = o.id;
    doc.CommitEdit();
    doc.Select(id);
    return id;
}

SceneObjectId DuplicateObject(EditorDocument& doc, SceneObjectId id) {
    Scene& scene = doc.GetScene();
    const SceneObject* source = scene.Find(id);
    if (!source) return kInvalidSceneObjectId;
    doc.BeginEdit();
    SceneObject copy = *source;  // by value: CreateObject may reallocate
    SceneObject& created = scene.CreateObject(copy.name + " copy");
    const SceneObjectId newId = created.id;
    const std::string newName = created.name;
    created = copy;
    created.id = newId;
    created.name = newName;
    // A scene has at most one player start; the copy must not carry it.
    created.playerStart.reset();
    // Place the copy right after the original (order is authored data).
    std::vector<SceneObject>& objects = scene.Objects();
    std::size_t sourceIndex = 0, copyIndex = objects.size() - 1;
    for (std::size_t i = 0; i < objects.size(); ++i) if (objects[i].id == id) sourceIndex = i;
    while (copyIndex > sourceIndex + 1) {
        std::swap(objects[copyIndex], objects[copyIndex - 1]);
        --copyIndex;
    }
    doc.CommitEdit();
    doc.Select(newId);
    return newId;
}
