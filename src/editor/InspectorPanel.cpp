#include "SceneSerialization.h"
#include "ComponentEditors.h"
#include "EditorPanelWidgets.h"
#include "EditorPanels.h"
#include "EditorWidgets.h"
#include "EditorWorkspace.h"
#include "EditorTheme.h"
#include "EditorVisuals.h"
#include "EditorDesign.h"
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
#include <optional>
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
#include "ResourceManager.h"
#include "RuntimeInspector.h"

namespace {
// Shared surfaces do not introduce child windows or ID scopes. The inspector
// owns edit transactions; the design primitives own only interaction/paint.
EditorVisuals::Icon ObjectIcon(const SceneObject &object) {
    using Icon = EditorVisuals::Icon;
    if (object.characterMotor || object.playerStart)
        return Icon::Person;
    if (object.renderCamera)
        return Icon::Camera;
    if (object.light)
        return Icon::Light;
    if (object.planet || object.landscape || object.gravity || object.celestial)
        return Icon::Globe;
    if (object.ui)
        return Icon::UI;
    if (object.audioEmitter || object.audioListener)
        return Icon::Audio;
    if (!object.scripts.empty() && !object.render)
        return Icon::Script;
    return Icon::Cube;
}

const char *ObjectType(const SceneObject &object) {
    if (object.characterMotor)
        return "Character";
    if (object.renderCamera)
        return "Camera";
    if (object.light)
        return "Light";
    if (object.planet)
        return "Planet terrain";
    if (object.landscape)
        return "Landscape";
    if (object.body)
        return "Physical object";
    if (object.render && object.render->shape == SceneShape::Sprite)
        return "Sprite";
    if (!object.scripts.empty())
        return "Scripted object";
    return "Scene object";
}

EditorVisuals::Icon ComponentIcon(const char *name) {
    using Icon = EditorVisuals::Icon;
    const std::string title(name);
    if (title == "Scripts")
        return Icon::Script;
    if (title == "Character motor" || title == "Player start")
        return Icon::Person;
    if (title.find("Audio") == 0)
        return Icon::Audio;
    if (title.find("Animation") == 0 || title == "Ragdoll")
        return Icon::Animation;
    if (title == "Render camera")
        return Icon::Camera;
    if (title == "Light")
        return Icon::Light;
    if (title == "Runtime UI")
        return Icon::UI;
    if (title == "Planet terrain" || title == "Landscape" || title.find("Gravity") == 0)
        return Icon::Globe;
    return Icon::Cube;
}

const char *ComponentTitle(const ComponentEditor &editor, const SceneObject &object) {
    const std::string name(editor.name);
    if (name == "Render")
        return object.render && object.render->shape == SceneShape::Sprite ? "Sprite" : "Appearance";
    if (name == "Body")
        return "Physics body";
    if (name == "Character motor")
        return "Character movement";
    if (name == "Render camera")
        return "Camera";
    if (name == "Runtime UI")
        return "User interface";
    if (name == "Visual socket")
        return "Attachment";
    if (name == "Landscape" && !object.landscape)
        return "New landscape (Terrain tools)";
    return editor.name;
}

const char *ComponentPurpose(const char *name) {
    const std::string type(name);
    if (type == "Scripts")
        return "Game behaviour written in JavaScript.";
    if (type == "Render")
        return "How this object looks. Collision is configured separately.";
    if (type == "Body")
        return "The shape that collides, and how physics moves it.";
    if (type == "Character motor")
        return "Collision and ground support for a character. Its script controls walking and jumping.";
    if (type == "Animation")
        return "Play and blend animation on this object's model.";
    if (type == "Render camera")
        return "A view of your game, including secondary-camera output.";
    if (type == "Light")
        return "Colour, brightness and reach of this light.";
    if (type == "Audio emitter")
        return "The sound this object plays.";
    if (type == "Runtime UI")
        return "An interface document displayed by this object.";
    if (type == "Basic water")
        return "An inexpensive water surface and swimming area; no advanced fluid simulation.";
    if (type == "Landscape")
        return "An editable landscape with source-owned collision. Opens its terrain authoring workflow.";
    if (type == "Navigation agent")
        return "Route following for this character or moving object.";
    return "Expand to edit this component. Its advanced options stay with it.";
}

template <class Publish>
void CompactAxes(EditorDocument &doc, const char *id, const char *label, glm::vec3 values, float speed,
                 const char *format, Publish publish) {
    const auto originalId = ImGui::GetID(id);
    const float scale = ImGui::GetIO().FontGlobalScale;
    const bool narrow = ImGui::GetContentRegionAvail().x < 235.f * scale;
    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", label);
    if (!narrow)
        ImGui::SameLine(65.f * scale);
    const float gap = 5.f * scale;
    const float axisWidth = 12.f * scale;
    const float fieldWidth = std::max(1.f, (ImGui::GetContentRegionAvail().x - 2 * gap) / 3);
    const ImVec4 colors[] = {{.89f, .31f, .37f, 1}, {.22f, .69f, .45f, 1}, {.31f, .56f, .96f, 1}};
    const char *axes[] = {"X", "Y", "Z"};
    // Match the ID stack of the original StackedField + DragFloat3 controls.
    ImGui::PushOverrideID(originalId);
    ImGui::PushID("");
    for (int axis = 0; axis < 3; ++axis) {
        if (axis)
            ImGui::SameLine(0, gap);
        ImGui::PushID(axis);
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(colors[axis], "%s", axes[axis]);
        ImGui::SameLine(0, 2.f * scale);
        ImGui::SetNextItemWidth(std::max(1.f, fieldWidth - axisWidth - 2.f * scale));
        EditorDesign::FieldFrame field;
        const bool changed = ImGui::DragFloat("", &values[axis], speed, 0, 0, format);
        if (ImGui::IsItemActivated())
            doc.BeginEdit(); // Snapshot the authored quaternion before Euler conversion.
        if (changed)
            publish(values);
        TrackEdit(doc);
        ImGui::PopID();
    }
    ImGui::PopID();
    ImGui::PopID();
    ImGui::EndGroup();
}

void PinnedTransform(EditorDocument &doc, SceneObject &object) {
    EditorDesign::Surface card(EditorDesign::Tone::Secondary);
    ImGui::PushFont(EditorVisuals::HeadingFont());
    EditorVisuals::IconText(EditorVisuals::Icon::Move, "Transform");
    ImGui::PopFont();
    ImGui::Spacing();
    CompactAxes(doc, "Position", "Position", object.transform.position, .05f, "%.4g",
                [&](glm::vec3 value) { object.transform.position = value; });
    CompactAxes(
        doc, "Rotation (deg)", "Rotation",
        glm::degrees(glm::eulerAngles(glm::normalize(object.transform.rotation))), .5f, "%.3g",
        [&](glm::vec3 value) { object.transform.rotation = glm::normalize(glm::quat(glm::radians(value))); });
    CompactAxes(doc, "Scale", "Scale", object.transform.scale, .01f, "%.4g",
                [&](glm::vec3 value) { object.transform.scale = value; });
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Scale changes appearance. Collider dimensions are in Physics body.");
}

bool ComponentHeader(EditorDocument &doc, const ComponentEditor &editor, SceneObject &o,
                     EditorPanelState &state) {
    ImGui::PushID(editor.name);
    if (state.revealComponent == editor.name) {
        ImGui::SetNextItemOpen(true);
        state.revealComponent.clear();
    }
    const bool common = std::string(editor.name) == "Scripts" || std::string(editor.name) == "Landscape";
    const char *title = ComponentTitle(editor, o);
    // Display friendlier titles without changing existing collapse/widget IDs.
    const char *originalTitle =
        std::string(editor.name) == "Render" && o.render && o.render->shape == SceneShape::Sprite
            ? "Sprite"
            : editor.name;
    const auto headerId = ImGui::GetID(originalTitle);
    bool open = InspectorDisclosure(title, common, headerId, ImGui::GetFontSize() + 9);
    const auto minimum = ImGui::GetItemRectMin(), maximum = ImGui::GetItemRectMax();
    const float size = ImGui::GetFontSize();
    const float top = minimum.y + (maximum.y - minimum.y - size) * .5f;
    const auto color = common ? EditorSecondaryAccent() : EditorVisuals::AccentColor();
    EditorVisuals::DrawIcon(ImGui::GetWindowDrawList(), ComponentIcon(editor.name),
                            {minimum.x + size + 8, top}, size, ImGui::GetColorU32(color));
    ImGui::SetItemTooltip("%s\nComponent: %s\nRight-click or use ... for component actions.",
                          ComponentPurpose(editor.name), editor.name);
    ImGui::OpenPopupOnItemClick("Component actions");
    const auto afterHeader = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos({maximum.x - 28, minimum.y + 3});
    if (EditorDesign::ActionButton("...##componentActions", EditorDesign::Tone::Quiet, {25, 22}))
        ImGui::OpenPopup("Component actions");
    ImGui::SetCursorScreenPos(afterHeader);
    if (ImGui::BeginPopup("Component actions")) {
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
    const auto accent = EditorVisuals::AccentColor();
    ImGui::PushStyleColor(ImGuiCol_FrameBg, {accent.x, accent.y, accent.z, .22f});
    ImGui::PushStyleColor(ImGuiCol_Border, accent);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {10, 8});
    ImGui::SetNextItemWidth(-1);
    ImGui::SetNextWindowSizeConstraints({260, 0}, {600, 420});
    bool open;
    {
        EditorDesign::FieldFrame field;
        open = ImGui::BeginCombo("##add", "+  Add component...");
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    if (!open)
        return;
    static ImGuiTextFilter filter;
    if (ImGui::IsWindowAppearing()) {
        filter.Clear();
        ImGui::SetKeyboardFocusHere();
    }
    if (EditorDesign::Search("Find component", "Find a component...", filter.InputBuf,
                             sizeof(filter.InputBuf)))
        filter.Build();
    std::vector<const ComponentEditor *> editors;
    for (const auto &editor : ComponentEditorRegistry())
        editors.push_back(&editor);
    std::sort(editors.begin(), editors.end(),
              [](const auto *a, const auto *b) { return std::strcmp(a->name, b->name) < 0; });
    for (const auto *entry : editors) {
        const auto &editor = *entry;
        if (editor.has(o) || (!editor.add && !editor.requestAdd))
            continue;
        if (!filter.PassFilter(editor.name) && !filter.PassFilter(ComponentTitle(editor, o)))
            continue;
        const std::string label = std::string(ComponentTitle(editor, o)) + "###" + editor.name;
        if (EditorDesign::Row(label.c_str(), false)) {
            if (editor.requestAdd) editor.requestAdd(state);
            else {
                doc.BeginEdit();
                editor.add(o);
                doc.CommitEdit();
                state.revealComponent = editor.name;
            }
        }
        ImGui::SetItemTooltip("%s", ComponentPurpose(editor.name));
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
    std::optional<InspectorReadOnlyScope> authoredLock;
    if (state.mode == EditorMode::Play) {
        ImGui::TextDisabled("Authored values are read-only while playing.");
        authoredLock.emplace(true);
    }
    {
        EditorDesign::Surface identity(EditorDesign::Tone::Quiet);
        const float scale = ImGui::GetIO().FontGlobalScale;
        const auto origin = ImGui::GetCursorScreenPos();
        EditorVisuals::DrawIcon(ImGui::GetWindowDrawList(), ObjectIcon(*o),
                                {origin.x + 1, origin.y + 5.f * scale}, 29.f * scale,
                                ImGui::GetColorU32(EditorSecondaryAccent()));
        ImGui::Indent(40.f * scale);
        // Keep TextField("Name")'s original ID, buffer bound and transaction
        // boundary while making the editable actual name the primary line.
        char name[512];
        CopyToBuffer(o->name, name, sizeof(name));
        ImGui::PushOverrideID(ImGui::GetID("Name"));
        ImGui::PushFont(EditorVisuals::HeadingFont());
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {6, 3});
        ImGui::SetNextItemWidth(-1);
        {
            EditorDesign::FieldFrame nameField;
            const bool renamed = ImGui::InputText("", name, sizeof(name));
            if (ImGui::IsItemActivated())
                doc.BeginEdit();
            if (renamed)
                o->name = name;
            TrackEdit(doc);
        }
        ImGui::PopStyleVar();
        ImGui::PopFont();
        ImGui::PopID();
        ImGui::TextDisabled("%s", ObjectType(*o));
        if (o->render && o->render->shape == SceneShape::Mesh && !o->render->meshAsset.empty()) {
            // The asset authoring panel is available after Stop, not during Play.
            const auto skeleton =
                state.resources ? state.resources->TryGetSkeletal(o->render->meshAsset) : nullptr;
            if (skeleton && !skeleton->skeleton.skinNodes.empty())
                ImGui::TextDisabled("Skinned model / %zu nodes", skeleton->skeleton.names.size());
            if (EditorDesign::ActionButton("Inspect model", EditorDesign::Tone::Secondary)) {
                state.browserSelection = o->render->meshAsset;
                state.selectAssetDetails = true;
                state.showAssetBrowser = true;
                state.revealModelStructure = true;
            }
            ImGui::SetItemTooltip(
                state.mode == EditorMode::Play
                    ? "Stop Play to inspect the authored model in Assets."
                    : "View the actual imported model and its skeleton in Assets. Model nodes are not scene children.");
        }
        ImGui::Unindent(40.f * scale);
    }
    PinnedTransform(doc, *o);
    // Name and Transform remain visible while component details scroll.
    const float addHeight = ImGui::GetFontSize() + 16.f + ImGui::GetStyle().ItemSpacing.y;
    ImGui::BeginChild("Inspector component properties", {0, -addHeight});
    if (state.mode == EditorMode::Play && state.runtime) {
        authoredLock.reset();
        DrawRuntimeRenderPreview(doc, o->id, state);
        DrawSelectedAIShortcuts(*o, state);
        if (ImGui::CollapsingHeader("Ownership & readiness")) {
            DrawRuntimeEntity(*state.runtime, o->id, &state);
            if (ImGui::SmallButton("Open Runtime Inspector")) {
                state.runtimeInspectEntity = o->id;
                state.showRuntimeInspector = true;
            }
        }
        authoredLock.emplace(true);
    }
    if (state.mode == EditorMode::Edit)
        DrawSelectedAIShortcuts(*o, state);
    // Keep ordinary authoring ahead of specialist systems regardless of the
    // engine component registry's historical registration order.
    std::vector<const ComponentEditor *> attached;
    for (const auto &editor : ComponentEditorRegistry())
        if (editor.has(*o))
            attached.push_back(&editor);
    const auto priority = [](const ComponentEditor *editor) {
        const std::string name(editor->name);
        return name == "Landscape" ? -1
               : name == "Scripts" ? 0
               : name == "Render"          ? 1
               : name == "Body"            ? 2
               : name == "Character motor" ? 3
               : name == "Animation"       ? 4
                                           : 5;
    };
    std::stable_sort(attached.begin(), attached.end(),
                     [&](const auto *a, const auto *b) { return priority(a) < priority(b); });
    const auto drawScriptStarter = [&] {
        EditorDesign::Surface scriptCard(EditorDesign::Tone::Secondary);
        ImGui::PushFont(EditorVisuals::HeadingFont());
        EditorVisuals::IconText(EditorVisuals::Icon::Script, "Scripts");
        ImGui::PopFont();
        ImGui::TextWrapped("Add behaviour to this object with JavaScript.");
        const auto owner = o->id;
        DrawEditorScriptWorkflow(doc, *o, state);
        o = doc.GetScene().Find(owner); // An AI starter may append scene objects.
    };
    // A landscape's primary workflow is editing its source, not attaching a
    // script. Keep the optional script starter available below its components.
    const bool landscape = bool(o->landscape);
    if (o->scripts.empty() && !landscape)
        drawScriptStarter();
    for (const auto *entry : attached) {
        const auto &editor = *entry;
        EditorDesign::Surface card(EditorDesign::Tone::Secondary);
        if (ComponentHeader(doc, editor, *o, state)) {
            if (std::string(editor.name) != "Scripts")
                ImGui::TextWrapped("%s", ComponentPurpose(editor.name));
            editor.draw(doc, *o, state);
            if (std::string(editor.name) == "Scripts" && editor.has(*o) &&
                InspectorDisclosure("Add another script")) {
                const auto owner = o->id;
                DrawEditorScriptWorkflow(doc, *o, state);
                o = doc.GetScene().Find(owner);
            }
        }
    }
    if (o->scripts.empty() && landscape)
        drawScriptStarter();
    authoredLock.reset();
    ImGui::Spacing();
    if (EditorDesign::Disclosure("Advanced object tools")) {
        {
            InspectorReadOnlyScope advancedLock(state.mode != EditorMode::Edit);
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
            if (state.mode == EditorMode::Edit && state.project && state.assets &&
                ImGui::TreeNode("Prefab tools")) {
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
        }
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
                if (root && o->id == o->prefabRoot &&
                    ImGui::Button("Apply this hierarchy to prefab source")) {
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
    } // Advanced object tools
    ImGui::EndChild();
    {
        InspectorReadOnlyScope addLock(state.mode != EditorMode::Edit);
        if (auto *selected = doc.SelectedObject())
            DrawAddComponentMenu(doc, *selected, state);
    }
    ImGui::End();
}
