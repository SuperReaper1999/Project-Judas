#include "AuthoringDocument.h"
#include "ContentReferences.h"
#include "NamedAuthoring.h"
#include "CollisionAsset.h"
#include "ModelCook.h"
#include "PoseComposition.h"
#include "Project.h"
#include "../SkeletalAnimation.h"
#include "../../third_party/nlohmann/json.hpp"
#include "WorldStreaming.h"
#include "SceneSession.h"
#include "ProfilerView.h"
#include "Environment.h"
#include "ResourceManager.h"
#include "EditorPanels.h"
#include "RetargetPanel.h"
#include "Prefab.h"
#include "RuntimeUI.h"
#include "SceneSerialization.h"
#include <filesystem>
#include <set>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <functional>

#include <algorithm>
#include <cstring>

#include <glm/gtc/quaternion.hpp>

#include "imgui.h"
#include "imgui_internal.h"

#include "ComponentEditors.h"
#include "EditorWidgets.h"
#include "Project.h"
#include "RuntimeWorld.h"

struct RuntimeRenderHistory {
    struct Action {std::function<bool(RuntimeWorld&)> undo,redo;ImGuiID gesture=0;int frame=0;};
    RuntimeWorld* world=nullptr;uint64_t document=0;
    std::vector<Action> undo,redo;
    SceneObjectId selected=0;std::string materialKey="*";
};

namespace {
constexpr ImGuiWindowFlags kWorkspacePanelFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

RuntimeRenderHistory& RenderHistory(EditorDocument& doc,EditorPanelState& state) {
    if(!state.renderPreviewHistory||state.renderPreviewHistory->world!=state.runtime||state.renderPreviewHistory->document!=doc.Generation()) {
        state.renderPreviewHistory=std::make_shared<RuntimeRenderHistory>();state.renderPreviewHistory->world=state.runtime;state.renderPreviewHistory->document=doc.Generation();
    }
    return *state.renderPreviewHistory;
}
void PublishRenderPreview(EditorDocument& doc,EditorPanelState& state,std::function<bool(RuntimeWorld&)> undo,std::function<bool(RuntimeWorld&)> redo) {
    if(!state.runtime||!redo(*state.runtime)){state.status="Render preview rejected: check ranges, asset identity and loaded part target";return;}
    auto& history=RenderHistory(doc,state);const auto gesture=ImGui::GetActiveID();const int frame=ImGui::GetFrameCount();
    // One continuously dragged control makes one preview undo step.
    if(gesture&&!history.undo.empty()&&history.undo.back().gesture==gesture&&history.undo.back().frame>=frame-1){history.undo.back().redo=std::move(redo);history.undo.back().frame=frame;}
    else {history.undo.push_back({std::move(undo),std::move(redo),gesture,frame});if(history.undo.size()>64)history.undo.erase(history.undo.begin());}
    history.redo.clear();state.status="Runtime render preview applied; Stop restores authored values";
}
void RenderPreviewUndo(EditorDocument& doc,EditorPanelState& state) {
    auto& history=RenderHistory(doc,state);
    ImGui::BeginDisabled(history.undo.empty());if(ImGui::Button("Undo preview")){auto action=history.undo.back();if(action.undo(*state.runtime)){history.undo.pop_back();history.redo.push_back(std::move(action));}else state.status="Preview undo target/resource is unavailable";}ImGui::EndDisabled();
    ImGui::SameLine();ImGui::BeginDisabled(history.redo.empty());if(ImGui::Button("Redo preview")){auto action=history.redo.back();if(action.redo(*state.runtime)){history.redo.pop_back();history.undo.push_back(std::move(action));}else state.status="Preview redo target/resource is unavailable";}ImGui::EndDisabled();
}
std::function<bool(RuntimeWorld&)> AppearanceAction(SceneSettings value) {
    value.authoringRecipes.clear();
    return [value=std::move(value)](RuntimeWorld& world){return value.appearanceResetState.empty()?world.ResetAppearance():world.SetAppearance(value);};
}
SceneSettings AppearancePreviewState(const SceneSettings& source) {
    SceneSettings result;
    result.sunEnabled=source.sunEnabled;result.sunIntensity=source.sunIntensity;result.sunDirection=source.sunDirection;result.sunColor=source.sunColor;result.ambientColor=source.ambientColor;
    result.backgroundColor=source.backgroundColor;result.linearRendering=source.linearRendering;result.exposure=source.exposure;result.environmentAsset=source.environmentAsset;
    result.environmentIntensity=source.environmentIntensity;result.environmentRotation=source.environmentRotation;result.environmentBackground=source.environmentBackground;result.appearanceResetState=source.appearanceResetState;
    return result;
}
void DrawRuntimeAppearance(EditorDocument& doc,EditorPanelState& state) {
    if(!state.runtime)return;
    ImGui::TextWrapped("Runtime environment preview (saved by explicit save slots). Stop restores authored scene.");RenderPreviewUndo(doc,state);
    auto before=AppearancePreviewState(state.runtime->Settings()),candidate=before;bool changed=false;
    changed|=ImGui::Checkbox("Sun enabled##runtime",&candidate.sunEnabled);
    changed|=ImGui::DragFloat3("Sun direction toward source (world)##runtime",&candidate.sunDirection.x,.01f);
    changed|=ImGui::DragFloat("Sun intensity##runtime",&candidate.sunIntensity,.05f,0,10000);
    changed|=ImGui::ColorEdit3("Sun colour (linear)##runtime",&candidate.sunColor.x,ImGuiColorEditFlags_HDR);
    changed|=ImGui::ColorEdit3("Ambient colour (linear)##runtime",&candidate.ambientColor.x,ImGuiColorEditFlags_HDR);
    changed|=ImGui::ColorEdit3("Background colour##runtime",&candidate.backgroundColor.x,ImGuiColorEditFlags_HDR);
    changed|=ImGui::Checkbox("Linear HDR rendering##runtime",&candidate.linearRendering);
    changed|=ImGui::DragFloat("Exposure##runtime",&candidate.exposure,.01f,.000001f,10000);
    changed|=ImGui::DragFloat("Environment strength##runtime",&candidate.environmentIntensity,.01f,0,10000);
    changed|=ImGui::Checkbox("Environment background##runtime",&candidate.environmentBackground);
    const auto* selected=state.assets?state.assets->Find(candidate.environmentAsset):nullptr;
    if(ImGui::BeginCombo("Environment asset##runtime",selected?selected->relativePath.c_str():"None")) {
        if(ImGui::Selectable("None",candidate.environmentAsset.empty())){candidate.environmentAsset.clear();changed=true;}
        if(state.assets)for(const auto& [id,record]:state.assets->Records())if(record.type==AssetType::Environment&&!record.missing&&ImGui::Selectable(record.relativePath.c_str(),candidate.environmentAsset==id)){candidate.environmentAsset=id;changed=true;}
        ImGui::EndCombo();
    }
    if(!candidate.environmentAsset.empty()&&state.resources){const auto resourceState=state.resources->StateOf(candidate.environmentAsset);ImGui::TextDisabled("Environment: %s",ResourceStateName(resourceState));if(resourceState==ResourceState::Failed)ImGui::TextWrapped("%s",state.resources->ErrorOf(candidate.environmentAsset).c_str());}
    auto angles=glm::degrees(glm::eulerAngles(candidate.environmentRotation));if(ImGui::DragFloat3("Environment rotation degrees##runtime",&angles.x,.5f)){candidate.environmentRotation=glm::quat(glm::radians(angles));changed=true;}
    if(changed)PublishRenderPreview(doc,state,AppearanceAction(before),[candidate](RuntimeWorld& world){return world.SetAppearance(candidate);});
    if(ImGui::Button("Reset runtime environment"))PublishRenderPreview(doc,state,AppearanceAction(before),[](RuntimeWorld& world){return world.ResetAppearance();});
}

void DrawRuntimeRenderPreview(EditorDocument& doc,SceneObjectId id,EditorPanelState& state) {
    if(!state.runtime)return;
    const auto* definition=state.runtime->RuntimeDefinition(id);
    if(!definition)return;
    if(!ImGui::CollapsingHeader("Runtime render preview",ImGuiTreeNodeFlags_DefaultOpen))return;
    RenderPreviewUndo(doc,state);ImGui::TextWrapped("These controls affect rendering only. Children keep their own settings; physics and scripts continue.");
    bool visible=definition->renderVisible;
    if(ImGui::Checkbox("Entity render visible##runtime",&visible)){const bool previous=definition->renderVisible;PublishRenderPreview(doc,state,[id,previous](RuntimeWorld& world){return world.SetRenderVisible(id,true,previous);},[id,visible](RuntimeWorld& world){return world.SetRenderVisible(id,true,visible);});}
    definition=state.runtime->RuntimeDefinition(id);if(!definition||!definition->render)return;
    visible=definition->render->visible;
    if(ImGui::Checkbox("Render component visible##runtime",&visible)){const bool previous=definition->render->visible;PublishRenderPreview(doc,state,[id,previous](RuntimeWorld& world){return world.SetRenderVisible(id,false,previous);},[id,visible](RuntimeWorld& world){return world.SetRenderVisible(id,false,visible);});}
    definition=state.runtime->RuntimeDefinition(id);if(!definition||!definition->render)return;
    const auto render=*definition->render;auto& history=RenderHistory(doc,state);
    if(history.selected!=id){history.selected=id;history.materialKey="*";}
    const auto* parts=state.resources?state.resources->TryGetModelParts(render.meshAsset):nullptr;
    if(ImGui::BeginCombo("Material target##runtime",history.materialKey.c_str())) {
        if(ImGui::Selectable("Whole renderable (*)",history.materialKey=="*"))history.materialKey="*";
        for(size_t i=0;i<std::max(size_t(1),std::min(size_t(64),std::max(render.materials.size(),parts?parts->size():size_t(0))));++i){auto key="#"+std::to_string(i);if(ImGui::Selectable(("Numeric slot "+key).c_str(),history.materialKey==key))history.materialKey=key;}
        if(parts)for(const auto& part:*parts)if(ImGui::Selectable(part.part.c_str(),history.materialKey==part.part))history.materialKey=part.part;
        ImGui::EndCombo();
    }
    const auto key=history.materialKey;unsigned index=0;std::string part;
    if(key.size()>1&&key[0]=='#')index=unsigned(std::stoul(key.substr(1)));
    else if(key!="*"){part=key;if(parts)for(size_t i=0;i<parts->size();++i)if(parts->at(i).part==key)index=unsigned(i);}
    auto previous=render.runtimeMaterials.find(key);std::optional<MaterialSlot> old;
    if(previous!=render.runtimeMaterials.end())old=previous->second;
    MaterialSlot candidate=old.value_or(MaterialSlot{});bool changed=false;
    const auto* shared=state.assets?state.assets->Find(candidate.asset):nullptr;
    if(ImGui::BeginCombo("Runtime shared material",shared?shared->relativePath.c_str():candidate.useSource?"Imported / default source":"Inherit authored / imported")) {
        if(ImGui::Selectable("Inherit authored / imported",candidate.asset.empty()&&!candidate.useSource)){candidate.asset.clear();candidate.useSource=false;changed=true;}
        if(ImGui::Selectable("Imported / default source",candidate.asset.empty()&&candidate.useSource)){candidate.asset.clear();candidate.useSource=true;changed=true;}
        if(state.assets)for(const auto& [asset,record]:state.assets->Records())if(record.type==AssetType::Material&&!record.missing&&ImGui::Selectable(record.relativePath.c_str(),candidate.asset==asset)){candidate.asset=asset;candidate.useSource=false;changed=true;}
        ImGui::EndCombo();
    }
    auto effective=ResolveRenderMaterial(render,index,part);MaterialDefinition base;base.model=MaterialModel::Legacy;base.baseColor=glm::vec4(render.color,render.alpha);
    if(state.resources){if(auto imported=state.resources->TryGetMeshMaterial(render.meshAsset,index))base=MaterialSettings(*imported);auto asset=candidate.asset.empty()?effective.asset:candidate.asset;if(!asset.empty())if(auto material=state.resources->TryGetMaterialDefinition(asset))base=*material;}
    base=ApplyMaterialOverride(base,effective.overrides);
    changed|=DrawMaterialOverrideControls(candidate.overrides,base,state);
    auto undo=[id,key,old](RuntimeWorld& world){return old?world.SetRuntimeMaterial(id,key,*old):world.ClearRuntimeMaterial(id,key);};
    if(changed)PublishRenderPreview(doc,state,undo,[id,key,candidate](RuntimeWorld& world){return world.SetRuntimeMaterial(id,key,candidate);});
    if(ImGui::Button("Clear runtime material binding"))PublishRenderPreview(doc,state,undo,[id,key](RuntimeWorld& world){return world.ClearRuntimeMaterial(id,key);});
    if(parts)for(const auto& source:*parts) {
        bool shown=std::find(render.hiddenParts.begin(),render.hiddenParts.end(),source.part)==render.hiddenParts.end();const bool oldVisible=shown;ImGui::PushID(source.part.c_str());
        if(ImGui::Checkbox("Part visible##runtime",&shown)){auto target=source.part;PublishRenderPreview(doc,state,[id,target,oldVisible](RuntimeWorld& world){return world.SetModelPartVisible(id,target,oldVisible);},[id,target,shown](RuntimeWorld& world){return world.SetModelPartVisible(id,target,shown);});}
        ImGui::TextWrapped("%s",source.part.c_str());ImGui::PopID();
    }
}

void PlaceWorkspacePanel(const EditorWorkspaceRect& rectangle) {
    ImGui::SetNextWindowPos({rectangle.position.x, rectangle.position.y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({rectangle.size.x, rectangle.size.y}, ImGuiCond_Always);
}

void ToolbarHint(const char* hint) {
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", hint);
}

// Completion is independent of the visible browser tab. Publication still
// uses the existing project check and occurs only in Edit mode.
void PublishFinishedEditorImport(EditorPanelState& state, EditorRequests& requests) {
    if (!state.importTask || !state.importTask->done.load(std::memory_order_acquire)) return;
    auto& task = *state.importTask;
    if (task.job.IsValid() && state.importJobs && state.importJobs->IsFinished(task.job)) {
        state.importJobs->Forget(task.job);
        task.job = {};
    }
    if (!task.success) {
        if (!state.importPublished) state.status = "Import failed; last good model retained: " + task.error;
        state.importPublished = true;
        return;
    }
    if (state.mode != EditorMode::Edit || state.importPublished) return;
    std::string error;
    const auto relative = std::filesystem::path(task.output).lexically_relative(state.project->RootDir());
    if (relative.empty() || relative.is_absolute() || *relative.begin() == "..") {
        task.cancel = true;
        state.status = "Import belongs to previous project; publication cancelled";
    } else if (PublishModelImport(task, error)) {
        state.importAccepted = state.importTask;
        state.status = task.unchanged ? "Unchanged import; reused accepted model" : "Published complete model; sources retained";
        requests.rescanAssets = true;
        if (state.resources) state.resources->Invalidate(task.assetId);
        state.browserSelection = task.assetId;
    } else state.status = error;
    state.importPublished = true;
}

void WorkspaceSplitter(const char* id, const EditorWorkspaceRect& rectangle, bool vertical, float& dimension,
                       float direction, float minimum, float maximum) {
    if (rectangle.size.x <= 0 || rectangle.size.y <= 0) return;
    PlaceWorkspacePanel(rectangle);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0, 0));
    ImGui::Begin(id, nullptr, kWorkspacePanelFlags | ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground |
                 ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);
    ImGui::InvisibleButton("##resize", {rectangle.size.x, rectangle.size.y});
    const bool active = ImGui::IsItemActive();
    if (active || ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(vertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
        const auto color = ImGui::GetColorU32(active ? ImGuiCol_SeparatorActive : ImGuiCol_SeparatorHovered);
        ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), color);
    }
    if (active) {
        const auto delta = ImGui::GetIO().MouseDelta;
        dimension = std::clamp(dimension + direction * (vertical ? delta.x : delta.y), minimum, maximum);
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
}

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

void UpdateEditorWorkspaceLayout(EditorPanelState& state, glm::vec2 displaySize) {
    const bool rails = state.mode == EditorMode::Edit || state.playPaused;
    state.workspace = CalculateEditorWorkspaceLayout(displaySize, state.hierarchyWidth, state.inspectorWidth,
        state.assetBrowserHeight, state.showAssetBrowser && state.mode == EditorMode::Edit, rails);
}

void DrawEditorWorkspaceChrome(EditorDocument& doc, EditorPanelState& state, EditorRequests& requests) {
    const bool editing = state.mode == EditorMode::Edit;
    const auto display = ImGui::GetIO().DisplaySize;
    PlaceWorkspacePanel(state.workspace.toolbar);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 6));
    ImGui::Begin("##workspaceToolbar", nullptr, kWorkspacePanelFlags | ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (ImGui::Button(editing ? "Play" : "Stop", {58, 26})) {
        if (editing) requests.play = true;
        else requests.stop = true;
    }
    ToolbarHint("F5: play this scene / restore authored scene");
    ImGui::SameLine();
    ImGui::BeginDisabled(!editing);
    if (ImGui::Button("Save", {48, 26})) requests.save = true;
    ToolbarHint("Ctrl+S: save scene");
    ImGui::SameLine();
    if (display.x >= 900.0f) {
        const auto tool = [&](const char* label, GizmoMode mode, const char* hint) {
            const bool selected = state.gizmoMode == mode;
            if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(label, {0, 26})) state.gizmoMode = mode;
            if (selected) ImGui::PopStyleColor();
            ToolbarHint(hint);
            ImGui::SameLine();
        };
        tool("Move", GizmoMode::Translate, "W: move selected objects");
        tool("Rotate", GizmoMode::Rotate, "E: rotate selected objects");
        tool("Scale", GizmoMode::Scale, "R: scale selected objects");
        if (ImGui::Button(state.gizmoSpace == GizmoSpace::World ? "World" : "Local", {55, 26}))
            state.gizmoSpace = state.gizmoSpace == GizmoSpace::World ? GizmoSpace::Local : GizmoSpace::World;
        ToolbarHint("X: toggle world/local gizmo axes");
        ImGui::SameLine();
        ImGui::Checkbox("Snap", &state.gizmoSnap);
        ToolbarHint("Ctrl temporarily enables snap");
        ImGui::SameLine();
    } else {
        if (ImGui::Button("Tools", {55, 26})) ImGui::OpenPopup("##compactTools");
        if (ImGui::BeginPopup("##compactTools")) {
            if (ImGui::MenuItem("Move", "W", state.gizmoMode == GizmoMode::Translate)) state.gizmoMode = GizmoMode::Translate;
            if (ImGui::MenuItem("Rotate", "E", state.gizmoMode == GizmoMode::Rotate)) state.gizmoMode = GizmoMode::Rotate;
            if (ImGui::MenuItem("Scale", "R", state.gizmoMode == GizmoMode::Scale)) state.gizmoMode = GizmoMode::Scale;
            if (ImGui::MenuItem("Local axes", "X", state.gizmoSpace == GizmoSpace::Local))
                state.gizmoSpace = state.gizmoSpace == GizmoSpace::Local ? GizmoSpace::World : GizmoSpace::Local;
            ImGui::MenuItem("Snap", "Ctrl", &state.gizmoSnap);
            ImGui::EndPopup();
        }
        ImGui::SameLine();
    }
    ImGui::EndDisabled();
    if (display.x > 1100.0f) {
        ImGui::TextDisabled("%s%s", doc.GetScene().Settings().name.c_str(), doc.IsDirty() ? " *" : "");
        ImGui::SameLine();
    }
    const float controlsWidth = 151.0f;
    if (ImGui::GetCursorPosX() < display.x - controlsWidth) ImGui::SetCursorPosX(display.x - controlsWidth);
    ImGui::BeginDisabled(!editing);
    if (ImGui::Button("Scene", {58, 26})) state.showSceneSettings = !state.showSceneSettings;
    ToolbarHint("Scene settings: name, lighting, world origin and render layers");
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Help", {58, 26})) state.showNavigationHelp = !state.showNavigationHelp;
    ToolbarHint("Scene navigation and editor shortcuts");
    ImGui::End();
    ImGui::PopStyleVar();

    if (state.workspace.showRails) {
        WorkspaceSplitter("##hierarchySplitter", state.workspace.leftSplitter, true, state.hierarchyWidth,
                          1.0f, 150.0f, std::max(150.0f, display.x * 0.40f));
        WorkspaceSplitter("##inspectorSplitter", state.workspace.rightSplitter, true, state.inspectorWidth,
                          -1.0f, 220.0f, std::max(220.0f, display.x * 0.46f));
        WorkspaceSplitter("##assetsSplitter", state.workspace.bottomSplitter, false, state.assetBrowserHeight,
                          -1.0f, 80.0f, std::max(80.0f, display.y * 0.55f));
    }
    if (state.showNavigationHelp) {
        ImGui::SetNextWindowSize({440, 340}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Editor help", &state.showNavigationHelp)) {
            ImGui::TextUnformatted("SCENE NAVIGATION");
            ImGui::Separator();
            ImGui::TextWrapped("Wheel: zoom in / out. Middle drag: pan. Right drag: look; hold WASD / Q / E to fly. Shift flies faster. F frames selection.");
            ImGui::Spacing();
            ImGui::TextWrapped("Start a camera drag inside the scene view. Release, Escape or leaving the app ends capture.");
            ImGui::Spacing();
            ImGui::TextUnformatted("AUTHORING");
            ImGui::Separator();
            ImGui::TextWrapped("W / E / R: move / rotate / scale. X: local / world axes. Ctrl: snap. Left click selects; Ctrl adds; Shift selects a range in the hierarchy.");
            ImGui::Spacing();
            ImGui::TextWrapped("Ctrl+S: save. Ctrl+Z / Ctrl+Y: undo / redo. Ctrl+D: duplicate. F5: Play / Stop. Drag the dividers to resize this workspace.");
            ImGui::Separator();
            if (ImGui::Button("JudasJS reference")) requests.openJudasJS = true;
            ImGui::SameLine();
            if (ImGui::Button("First project guide")) requests.openQuickStart = true;
        }
        ImGui::End();
    }
}

void DrawEditorMainMenu(EditorDocument& doc, EditorPanelState& state, EditorRequests& requests) {
    if(!doc.ValidationError().empty())state.status="Edit rejected; previous document restored: "+doc.ValidationError();
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
        item("Door (legacy gameplay)", "door");
        item("Gravity region", "gravity-region");
        item("Player start (legacy gameplay)", "player-start");
        item("Render camera", "render-camera");
        item("Audio emitter", "audio-emitter");
        item("Audio listener", "audio-listener");
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Animation retargeting", nullptr, &state.showRetarget);
        ImGui::MenuItem("Terrain sculpt / paint", nullptr, &state.showTerrain);
        ImGui::MenuItem("Asset Browser", nullptr, &state.showAssetBrowser);
        ImGui::MenuItem("Scene settings", nullptr, &state.showSceneSettings, editing);
        ImGui::MenuItem("World building / named source", nullptr, &state.showWorldBuilding);
        ImGui::MenuItem("Profiler", nullptr, &state.showProfiler);
        ImGui::MenuItem("Navigation and shortcuts", nullptr, &state.showNavigationHelp);
        if (ImGui::MenuItem("JudasJS reference")) requests.openJudasJS = true;
        if (ImGui::MenuItem("Making your first project")) requests.openQuickStart = true;
        if (ImGui::MenuItem("Reset workspace layout")) {
            state.hierarchyWidth = 250; state.inspectorWidth = 340; state.assetBrowserHeight = 200;
            state.showAssetBrowser = true;
        }
        ImGui::Separator();
        ImGui::TextDisabled("Scene navigation (press in the viewport)");
        ImGui::TextUnformatted("Mouse wheel: zoom toward / away from focus");
        ImGui::TextUnformatted("Middle drag: pan in the view plane");
        ImGui::TextUnformatted("Right drag: look; hold WASD / Q / E to fly");
        ImGui::TextUnformatted("Shift: faster flight; F: focus selection");
        ImGui::TextUnformatted("Release or Escape: finish camera drag");
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
        ImGui::MenuItem("Broadphase bounds", nullptr, &d.broadphase);
        ImGui::MenuItem("Navigation mesh / routes", nullptr, &d.navigation);
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
    if (ImGui::BeginMenu("Play")) {
        if (ImGui::MenuItem(editing ? "Play scene" : "Stop", "F5")) {
            if (editing) requests.play = true; else requests.stop = true;
        }
        if (ImGui::MenuItem("Run project", nullptr, false, hasProject && editing)) requests.runProject = true;
        ImGui::EndMenu();
    }
    const char* projectName = hasProject ? state.project->Settings().name.c_str() : "No project";
    const std::string context = std::string(projectName) + "  /  " +
        (editing ? "EDIT" : state.playPaused ? "PLAY PAUSED" : "PLAY");
    const float contextWidth = ImGui::CalcTextSize(context.c_str()).x;
    if (ImGui::GetContentRegionAvail().x > contextWidth + 20) {
        ImGui::SameLine(ImGui::GetWindowWidth() - contextWidth - 14.0f);
        ImGui::TextDisabled("%s", context.c_str());
    }
    ImGui::EndMainMenuBar();
}

void DrawHierarchyPanel(EditorDocument& doc, EditorPanelState& state, EditorRequests& requests) {
    if (!state.workspace.showRails) return;
    PlaceWorkspacePanel(state.workspace.hierarchy);
    if (!ImGui::Begin("Hierarchy", nullptr, kWorkspacePanelFlags)) { ImGui::End(); return; }
    const bool editing = state.mode == EditorMode::Edit;
    Scene& scene = doc.GetScene();
    char search[256]; CopyToBuffer(state.hierarchySearch, search, sizeof(search));
    ImGui::SetNextItemWidth(std::max(40.0f, ImGui::GetContentRegionAvail().x - 30.0f));
    if (ImGui::InputTextWithHint("##hierarchySearch", "Search names, tags, folders...", search, sizeof(search)))
        state.hierarchySearch = search;
    ImGui::SameLine();
    if (ImGui::SmallButton("X##clearSearch")) state.hierarchySearch.clear();
    ToolbarHint("Clear hierarchy search");
    ImGui::TextDisabled("%zu objects / %zu selected", scene.Objects().size(), doc.Selection().size());
    ImGui::Separator();
    SceneObjectId toDelete = kInvalidSceneObjectId;
    int move = 0;
    SceneObjectId moveId = kInvalidSceneObjectId;
    ImGui::BeginChild("list", ImVec2(0.0f, -34.0f));
    auto visible=doc.Search(state.hierarchySearch);
    if(state.project&&!state.hierarchySearch.empty()){std::set<SceneObjectId> matches(visible.begin(),visible.end());for(auto& o:doc.GetScene().Objects())for(auto& [id,name]:state.project->Settings().classification.tags.names)if((o.tags&CategoryBit(id))&&name.find(state.hierarchySearch)!=std::string::npos){matches.insert(o.id);for(auto p=o.parent;p;){matches.insert(p);auto* parent=doc.GetScene().Find(p);p=parent?parent->parent:0;}}visible.assign(matches.begin(),matches.end());}
    for (const SceneObject& o : scene.Objects()) {
        ImGui::PushID(static_cast<int>(o.id));
        const bool selected = doc.IsSelected(o.id);
        if(std::find(visible.begin(),visible.end(),o.id)==visible.end()){ImGui::PopID();continue;}
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
        unsigned depth = 0;
        for (auto parent = o.parent; parent && depth < 16; ++depth) {
            const auto* ancestor = scene.Find(parent);
            if (!ancestor) break;
            parent = ancestor->parent;
        }
        const float indentation = float(std::min(depth, 8u)) * 12.0f;
        if (indentation > 0) ImGui::Indent(indentation);
        if (broken) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.55f, 0.3f, 1.0f));
        if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick, {0, 22})) {
            if(ImGui::GetIO().KeyShift)doc.SelectRange(o.id,visible,ImGui::GetIO().KeyCtrl);else doc.Select(o.id,ImGui::GetIO().KeyCtrl);
            if (editing && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                state.renamingId = o.id;
                state.renameBuffer = o.name;
            }
        }
        if (broken) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(o.name.empty() ? "(unnamed)" : o.name.c_str());
            ImGui::TextDisabled("ID %llu  /  %s", static_cast<unsigned long long>(o.id),
                               indicators.empty() ? "empty object" : indicators.c_str());
            if (!o.authoringFolder.empty()) ImGui::Text("Folder: %s", o.authoringFolder.c_str());
            if (const auto* parent = scene.Find(o.parent)) ImGui::Text("Parent: %s", parent->name.c_str());
            if (broken) ImGui::TextColored({1, .55f, .3f, 1}, "Mesh asset is missing or unknown");
            ImGui::TextDisabled("Ctrl: add selection   Shift: select range");
            ImGui::EndTooltip();
        }
        if (editing && ImGui::BeginPopupContextItem()) {
            if (!selected) doc.Select(o.id);
            if (ImGui::MenuItem("Focus", "F")) requests.focusSelection = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Rename")) { state.renamingId = o.id; state.renameBuffer = o.name; }
            if (ImGui::MenuItem("Duplicate")) requests.duplicateId = o.id;
            if (ImGui::MenuItem("Move up")) { move = -1; moveId = o.id; }
            if (ImGui::MenuItem("Move down")) { move = 1; moveId = o.id; }
            if (ImGui::MenuItem("Delete")) toDelete = o.id;
            ImGui::EndPopup();
        }
        if (indentation > 0) ImGui::Unindent(indentation);
        ImGui::PopID();
    }
    ImGui::EndChild();
    if (editing) {
        ImGui::BeginDisabled(doc.Selected() == kInvalidSceneObjectId);
        if (ImGui::Button("Focus")) requests.focusSelection = true;
        ImGui::SameLine();
        if (ImGui::Button("Actions")) ImGui::OpenPopup("##selectionActions");
        ToolbarHint("Duplicate, group or delete the selection");
        if (ImGui::BeginPopup("##selectionActions")) {
            if (ImGui::MenuItem("Duplicate selection", "Ctrl+D")) { std::string error; if (!doc.DuplicateSelection(error)) state.status = error; }
            if (ImGui::MenuItem("Group in folder")) { std::string error; if (!doc.GroupSelection(error)) state.status = error; }
            if (ImGui::MenuItem("Delete selection", "Delete")) toDelete = doc.Selected();
            ImGui::EndPopup();
        }
        ImGui::EndDisabled();
    }
    if (toDelete != kInvalidSceneObjectId) {std::string error;if(!doc.DeleteSelection(error))state.status="Deletion rejected: "+error;else state.status="Deleted selected hierarchies";}
    if (move != 0) {
        doc.BeginEdit();
        scene.MoveObject(moveId, move);
        doc.CommitEdit();
    }
    ImGui::End();
}

void DrawInspectorPanel(EditorDocument& doc, EditorPanelState& state) {
    if (!state.workspace.showRails) return;
    PlaceWorkspacePanel(state.workspace.inspector);
    if (!ImGui::Begin("Inspector", nullptr, kWorkspacePanelFlags)) { ImGui::End(); return; }
    doc.PruneSelection();
    if(state.mode==EditorMode::Edit&&doc.Selection().size()>1){
        ImGui::Text("%zu selected",doc.Selection().size());static glm::vec3 delta(0),angles(0),factor(1);static bool worldAxes=true,individual=true;
        ImGui::Checkbox("World axes",&worldAxes);ImGui::SameLine();ImGui::Checkbox("Individual origins",&individual);
        ImGui::DragFloat3("Translation delta",&delta.x,.05f);ImGui::DragFloat3("Rotation delta degrees",&angles.x,1);ImGui::DragFloat3("Scale multiplier",&factor.x,.01f);
        if(ImGui::Button("Apply transform delta")){std::string error;if(!doc.BatchTransform(delta,glm::quat(glm::radians(angles)),factor,error,individual,worldAxes))state.status=error;else{delta=angles=glm::vec3(0);factor=glm::vec3(1);}}
        if(ImGui::BeginCombo("Parent all (keep world pose)","Choose parent")){if(ImGui::Selectable("None")){std::string error;if(!doc.ReparentSelection(0,error))state.status=error;}for(auto& p:doc.GetScene().Objects())if(!doc.IsSelected(p.id)&&ImGui::Selectable((p.name+" ##"+std::to_string(p.id)).c_str())){std::string error;if(!doc.ReparentSelection(p.id,error))state.status=error;}ImGui::EndCombo();}
        auto common=ObjectProperties(*doc.SelectedObject());std::set<std::string> mixed;for(auto id:doc.Selection()){auto properties=ObjectProperties(*doc.GetScene().Find(id));for(auto it=common.begin();it!=common.end();){auto found=properties.find(it->first);if(found==properties.end())it=common.erase(it);else{if(found->second!=it->second)mixed.insert(it->first);++it;}}}
        ImGui::TextWrapped("Common authored fields: mixed values change only after Apply. Values use normal record notation.");
        static std::string field;static char value[2048]{};
        if(ImGui::BeginCombo("Common property",field.c_str())){for(auto& [key,v]:common)if(key!="_name"&&ImGui::Selectable((key+(mixed.count(key)?" (mixed)":"")).c_str(),field==key)){field=key;CopyToBuffer(mixed.count(key)?"":v,value,sizeof(value));}ImGui::EndCombo();}
        ImGui::InputText("New value",value,sizeof(value));if(ImGui::Button("Apply field")&&!field.empty()&&common.count(field)){std::string error;if(!doc.BatchProperties({{field,value}},error))state.status=error;}
    }
    if(state.mode==EditorMode::Edit&&doc.SelectedObject()){
        static int component=0;const char* names[]={"render","body","animation","socket","motor","audio","particle","joint","ragdoll","scripts"};
        ImGui::TextWrapped("Component clipboard");
        ImGui::SetNextItemWidth(-1);
        // Keep the original control identity while its visible label moves
        // above the full-width selector in the narrow inspector rail.
        ImGui::PushOverrideID(ImGui::GetID("Component clipboard"));
        ImGui::Combo("",&component,names,10);
        ImGui::PopID();
        if(ImGui::Button("Copy component"))doc.CopyComponent(names[component]);
        ImGui::SameLine();if(ImGui::Button("Paste to selection")){std::string error;if(!doc.PasteComponent(error))state.status=error;}
    }
    SceneObject* o = doc.SelectedObject();
    if (!o) {
        ImGui::Spacing();
        ImGui::TextUnformatted("Select an object");
        ImGui::TextWrapped("Choose an object in the scene or hierarchy to inspect its transform and components.");
        ImGui::Spacing();
        ImGui::BeginDisabled(state.mode != EditorMode::Edit);
        if (ImGui::Button("Scene settings")) state.showSceneSettings = true;
        ImGui::EndDisabled();
        ImGui::End();
        return;
    }
    if(o->prefabRoot){
        const auto* root=doc.GetScene().Find(o->prefabRoot);
        ImGui::Text("Prefab member %llu / instance %llu",(unsigned long long)o->prefabSource,(unsigned long long)o->prefabRoot);
        if(root)ImGui::TextWrapped("Source asset: %s",root->prefabAsset.c_str());
        ImGui::Text("Property overrides: %zu",o->prefabOverrides.size());
        if(state.mode==EditorMode::Edit&&state.assets){
            std::string revert;
            for(const auto& pair:o->prefabOverrides){
                ImGui::PushID(pair.first.c_str());ImGui::TextWrapped("%s = %s",pair.first.c_str(),pair.second.c_str());
                if(ImGui::SmallButton("Revert property"))revert=pair.first;
                ImGui::PopID();
            }
            if(!revert.empty()){
                doc.BeginEdit();std::string error;
                if(!RevertPrefabProperty(doc.GetScene(),o->id,revert,*state.assets,error))state.status=error;
                doc.CommitEdit(false);o=doc.SelectedObject();root=doc.GetScene().Find(o->prefabRoot);
            }
            if(root&&o->id==o->prefabRoot&&ImGui::Button("Apply this hierarchy to prefab source")){
                const auto id=o->id;doc.BeginEdit();std::string error;
                if(!ApplyPrefabSource(doc.GetScene(),id,*state.assets,error))state.status=error;
                else state.status="Saved prefab source; non-overridden instances refreshed";
                doc.CommitEdit(false);o=doc.SelectedObject();root=doc.GetScene().Find(o->prefabRoot);
            }
            if(ImGui::Button("Reload prefab sources")){
                Scene resolved;std::string error;doc.BeginEdit();
                if(ResolvePrefabs(doc.GetScene(),state.assets,resolved,error)){doc.GetScene()=std::move(resolved);state.status="Reloaded prefab sources";}else state.status=error;
                doc.CommitEdit(false);o=doc.SelectedObject();root=doc.GetScene().Find(o->prefabRoot);
            }
        }
        if(state.mode==EditorMode::Play&&state.runtime&&root&&o->id==o->prefabRoot){
            if(ImGui::Button("Spawn independent prefab")){
                std::string error;auto placement=o->transform;placement.position+=glm::vec3(0,2,3);
                const auto id=state.runtime->SpawnPrefab(root->prefabAsset,placement,error);
                state.lastPrefabSpawn=id;
                state.status=id?"Spawned root "+std::to_string(id):error;
            }
            if(state.lastPrefabSpawn&&ImGui::Button("Destroy last spawned hierarchy")){
                std::string error;
                state.status=state.runtime->DestroyHierarchy(state.lastPrefabSpawn,error)?"Destroyed spawned hierarchy":error;
                state.lastPrefabSpawn=0;
            }
        }
    }
    if(state.mode==EditorMode::Edit&&state.project&&state.assets&&ImGui::Button("Create prefab from hierarchy")){
        Scene source;std::string error;
        if(CreatePrefab(doc.GetScene(),o->id,source,error)){
            auto path=std::filesystem::path(state.project->AssetsDir())/"prefabs"/("prefab-"+MintAssetId()+".judasprefab");
            std::filesystem::create_directories(path.parent_path());
            if(SaveSceneToFile(source,path.string(),error)){
                auto* db=const_cast<AssetDatabase*>(state.assets);AssetRecord record;
                if(db->Track(path.string(),record,error))state.status="Created prefab asset "+record.id;
            }
        }
        if(!error.empty())state.status=error;
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
    if(state.mode==EditorMode::Play)DrawRuntimeRenderPreview(doc,o->id,state);
    if (state.mode == EditorMode::Play) {
        ImGui::TextDisabled("Authored values are read-only while playing.");
        ImGui::BeginDisabled();
    }
    ImGui::Text("id %llu   components: %s", static_cast<unsigned long long>(o->id), ComponentIndicators(*o).c_str());
    TextField(doc, "Name", o->name);
    Checkbox(doc,"Entity render visible",o->renderVisible);
    if(!o->prefabRoot){
        const auto* parent=doc.GetScene().Find(o->parent);
        ImGui::TextWrapped("Parent (local transform)");
        ImGui::SetNextItemWidth(-1);
        ImGui::PushOverrideID(ImGui::GetID("Parent (local transform)"));
        if(ImGui::BeginCombo("",parent?parent->name.c_str():"None")){
            if(ImGui::Selectable("None",!o->parent)){doc.BeginEdit();o->parent=0;doc.CommitEdit();}
            for(const auto& candidate:doc.GetScene().Objects())if(candidate.id!=o->id&&ImGui::Selectable(candidate.name.c_str(),candidate.id==o->parent)){
                const auto old=o->parent;doc.BeginEdit();o->parent=candidate.id;std::string error;
                if(!ValidateHierarchy(doc.GetScene(),error)){o->parent=old;state.status=error;}doc.CommitEdit();
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();
    }
    if(state.project){const auto& c=state.project->Settings().classification;
        DrawCategoryMask(doc,"Tags",o->tags,c.tags,false);
        if(state.runtime)ImGui::Text("Runtime entities matching these tags: %zu",state.runtime->QueryEntities(o->tags).size());
        DrawCategoryLayer(doc,"Render layer",o->renderLayer,c.render);
    }
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
    if (!state.showSceneSettings) return;
    const auto display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize({std::min(430.0f, display.x - 40), std::min(540.0f, display.y - 110)}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Scene settings", &state.showSceneSettings)) { ImGui::End(); return; }
    if(state.mode==EditorMode::Play)DrawRuntimeAppearance(doc,state);
    if (state.mode == EditorMode::Play) ImGui::BeginDisabled();
    SceneSettings& s = doc.GetScene().Settings();
    if(state.project)DrawCategoryMask(doc,"Main/editor camera render mask",s.mainCameraRenderMask,state.project->Settings().classification.render);
    TextField(doc, "Name##scene", s.name);
    double origin[3] = {s.worldOrigin.x, s.worldOrigin.y, s.worldOrigin.z};
    if (ImGui::InputScalarN("World origin (m)", ImGuiDataType_Double, origin, 3, nullptr, nullptr, "%.6g")) {
        s.worldOrigin = glm::dvec3(origin[0], origin[1], origin[2]);
    }
    TrackEdit(doc);
    DragVec3(doc, "Sun direction", s.sunDirection, 0.01f);
    ImGui::TextDisabled("World direction toward the sun; independent of gravity and world up.");
    Checkbox(doc,"Sun enabled",s.sunEnabled);
    DragScalar(doc,"Sun intensity",s.sunIntensity,.05f,0,10000);
    ColorEdit(doc, "Sun color", s.sunColor);
    ColorEdit(doc, "Ambient", s.ambientColor);
    Checkbox(doc,"Linear HDR world rendering",s.linearRendering);DragScalar(doc,"Manual exposure",s.exposure,.01f,.001f,10000);
    std::vector<std::string> names,ids;if(state.assets)for(const auto& [id,a]:state.assets->Records())if(a.type==AssetType::Environment){names.push_back(a.relativePath);ids.push_back(id);}LabelledCombo(doc,"Environment lighting",s.environmentAsset,names,ids,true);
    DragScalar(doc,"Environment intensity",s.environmentIntensity,.01f,0,10000);Checkbox(doc,"Environment background",s.environmentBackground);ColorEdit(doc,"Background colour (linear in HDR)",s.backgroundColor);
    auto angles=glm::degrees(glm::eulerAngles(s.environmentRotation));if(ImGui::DragFloat3("Environment rotation degrees",&angles.x,.5f)){doc.BeginEdit();s.environmentRotation=glm::quat(glm::radians(angles));doc.CommitEdit();}
    ImGui::TextDisabled("Background is independent of lighting. Colour factors are linear; UI ignores world exposure.");
    DragScalar(doc, "Fluid scale", s.fluidScale, 0.1f, 0.01f, 1000.0f);
    DragScalar(doc, "Fluid updates (Hz)", s.fluidUpdateRateHz, 1.0f, 0.1f, 1000.0f);
    DragScalar(doc, "Fluid drag (1/s)", s.fluidHydrostaticDragRate, 0.05f, 0.0f, 1000.0f);
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

namespace {
void DrawUILayoutEditor(const AssetRecord& record,EditorPanelState& state){
    static AuthoringDocument document;static UIDocument source;static std::string error;static int selected=0;static bool loaded=false,pending=false;static std::vector<char> text(4*1024*1024+1);static uint64_t textGeneration=~uint64_t(0);static bool sourceDirty=false;static std::set<std::string> selection;static UIElement clipboard;static bool hasClipboard=false;
    auto reloadTyped=[&]{sourceDirty=false;loaded=ParseUIDocument(document.Source(),source,error);selected=0;pending=false;selection.clear();state.uiPreviewDocument=source;++state.uiPreviewRevision;textGeneration=~uint64_t(0);};
    if(document.Path()!=record.path){if(loaded&&(document.Dirty()||pending||sourceDirty)){ImGui::TextWrapped("Unsaved UI %s. Save or discard before opening another document.",document.Path().c_str());if(ImGui::Button("Discard previous UI changes")){loaded=false;pending=false;}return;}loaded=document.Load("ui",record.path,error);if(loaded)reloadTyped();}
    if(!loaded){ImGui::TextWrapped("UI: %s",error.c_str());return;}
    auto flush=[&]{if(!pending)return true;std::string named;if(!LegacyToNamed(SerializeUIDocument(source),"ui",named,error)||!document.Replace(named,error))return false;pending=false;return true;};
    if(!ImGui::BeginTable("UI authoring columns",2,ImGuiTableFlags_Resizable))return;
    ImGui::TableSetupColumn("Properties / source",ImGuiTableColumnFlags_WidthFixed,440);
    ImGui::TableSetupColumn("Canvas",ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextColumn();ImGui::BeginChild("UI properties and source",{0,0},true);
    ImGui::Text("%s%s",record.relativePath.c_str(),document.Dirty()||pending?" *":"");
    if(document.ExternalChanged())ImGui::TextColored({1,.6f,.2f,1},"External edit detected; saving is blocked until reviewed reload.");
    if(ImGui::Button("Save UI source")){if(sourceDirty)state.status="Apply or discard the named source draft before saving";else if(flush()&&document.Save(error))state.status="Saved named UI source; restart Play to reload";else state.status=error;}
    ImGui::SameLine();if(ImGui::Button("Reload (discard dirty UI)")){if(document.Load("ui",record.path,error))reloadTyped();}
    ImGui::SameLine();if(ImGui::Button("Undo UI")){if(sourceDirty)state.status="Apply or explicitly discard the named source draft before visual undo";else if(flush()&&document.Undo())reloadTyped();}
    ImGui::SameLine();if(ImGui::Button("Redo UI")){if(sourceDirty)state.status="Apply or explicitly discard the named source draft before visual redo";else if(document.Redo())reloadTyped();}
    if(selected<0||selected>=int(source.elements.size()))selected=0;
    if(!state.uiCanvasSelected.empty()){for(size_t i=0;i<source.elements.size();++i)if(source.elements[i].id==state.uiCanvasSelected)selected=int(i);state.uiCanvasSelected.clear();}
    auto before=SerializeUIDocument(source);
    ImGui::Checkbox("Document visible",&source.visible);ImGui::SameLine();ImGui::Checkbox("Document enabled",&source.enabled);ImGui::Checkbox("Modal (pause gameplay)",&source.modal);
    ImGui::BeginChild("UI hierarchy",{0,130},true);for(size_t i=0;i<source.elements.size();++i){ImGui::PushID(int(i));auto& e=source.elements[i];if(ImGui::Selectable((e.parent.empty()?e.id:e.parent+" / "+e.id).c_str(),selected==int(i)||selection.count(e.id))){selected=int(i);if(!ImGui::GetIO().KeyCtrl)selection.clear();if(selection.count(e.id))selection.erase(e.id);else selection.insert(e.id);}ImGui::PopID();}ImGui::EndChild();
    if(ImGui::Button("Add child panel")){UIElement e;e.id="element_"+std::to_string(source.elements.size()+1);while(std::any_of(source.elements.begin(),source.elements.end(),[&](const auto& x){return x.id==e.id;}))e.id+="_";e.parent=source.elements[selected].id;source.elements.push_back(e);selected=int(source.elements.size())-1;}
    ImGui::SameLine();if(selected&&ImGui::Button("Duplicate subtree")){auto root=source.elements[selected].id;std::set<std::string> members{root};std::map<std::string,std::string> ids;bool more=true;while(more){more=false;for(auto& e:source.elements)if(members.count(e.parent))more|=members.insert(e.id).second;}for(auto& key:members){auto copy=key+"_copy";while(std::any_of(source.elements.begin(),source.elements.end(),[&](const auto& e){return e.id==copy;}))copy+="_";ids[key]=copy;}std::vector<UIElement> copies;for(auto e:source.elements)if(members.count(e.id)){auto id=e.id;e.id=ids.at(id);if(ids.count(e.parent))e.parent=ids.at(e.parent);if(id==root)e.offset+=glm::vec2(12);copies.push_back(e);}source.elements.insert(source.elements.end(),copies.begin(),copies.end());}
    auto& e=source.elements[selected];auto str=[](const char* label,std::string& s){char b[16385];std::snprintf(b,sizeof(b),"%s",s.c_str());if(ImGui::InputText(label,b,sizeof(b)))s=b;};
    ImGui::TextDisabled("Stable ID: %s",e.id.c_str());
    if(selected){if(ImGui::BeginCombo("Parent",e.parent.c_str())){for(auto& parent:source.elements)if(parent.id!=e.id&&ImGui::Selectable(parent.id.c_str(),e.parent==parent.id))e.parent=parent.id;ImGui::EndCombo();}}
    int kind=int(e.kind);if(selected&&ImGui::Combo("Element type",&kind,"Canvas\0Panel\0Text\0Image\0Button\0Slider\0Toggle\0"))e.kind=UIKind(kind);
    int flow=int(e.flow);if(ImGui::Combo("Child layout",&flow,"Free\0Horizontal\0Vertical\0"))e.flow=UIFlow(flow);
    for(auto& parent:source.elements)if(parent.id==e.parent&&parent.flow!=UIFlow::Free)ImGui::TextWrapped("%s owns this element's %s placement; offsets and margins still contribute.",parent.id.c_str(),parent.flow==UIFlow::Horizontal?"horizontal":"vertical");
    if(auto it=state.uiPreviewLayout.find(e.id);it!=state.uiPreviewLayout.end())ImGui::TextDisabled("Effective rectangle %.1f %.1f / %.1f %.1f px",it->second.rect.position.x,it->second.rect.position.y,it->second.rect.size.x,it->second.rect.size.y);
    ImGui::Checkbox("Visible",&e.visible);ImGui::SameLine();ImGui::Checkbox("Enabled",&e.enabled);ImGui::Checkbox("Clip children",&e.clip);
    ImGui::InputFloat2("Anchor min",&e.anchorMin.x);ImGui::InputFloat2("Anchor max",&e.anchorMax.x);ImGui::InputFloat2("Offset (reference px)",&e.offset.x);ImGui::InputFloat2("Fixed size",&e.size.x);ImGui::InputFloat2("Relative size",&e.relativeSize.x);ImGui::InputFloat2("Pivot alignment",&e.align.x);ImGui::InputFloat4("Margins L T R B",&e.margin.x);ImGui::InputFloat4("Padding L T R B",&e.padding.x);ImGui::InputFloat("Spacing",&e.spacing);
    ImGui::ColorEdit4("Background",&e.background.x);ImGui::ColorEdit4("Tint",&e.color.x);str("Text",e.text);str("Localization key",e.textKey);int direction=int(e.direction);if(ImGui::Combo("Text direction",&direction,"Auto\0LTR\0RTL\0"))e.direction=TextDirection(direction);int alignment=e.textLogicalAlign+1;if(ImGui::Combo("Text alignment",&alignment,"Legacy\0Left\0Right\0Center\0Start\0End\0"))e.textLogicalAlign=alignment-1;ImGui::Checkbox("Mirror horizontal children for RTL",&e.mirrorRow);auto assetPick=[&](const char* label,AssetType type,std::string& id){if(!state.assets)return;if(ImGui::BeginCombo(label,id.empty()?"(default / none)":id.c_str())){if(ImGui::Selectable("(default / none)",id.empty()))id.clear();for(auto& [key,asset]:state.assets->Records())if(asset.type==type&&!asset.missing&&ImGui::Selectable(asset.relativePath.c_str(),id==key))id=key;ImGui::EndCombo();}};assetPick("Font",AssetType::Font,e.font);assetPick("Image",AssetType::Texture,e.texture);ImGui::InputFloat("Font size",&e.fontSize);ImGui::InputFloat2("Text alignment legacy",&e.textAlign.x);ImGui::Checkbox("Wrap",&e.wrap);ImGui::Checkbox("Fit image",&e.fit);
    if(ImGui::Button("Copy style")){clipboard=e;hasClipboard=true;}ImGui::SameLine();if(hasClipboard&&ImGui::Button("Paste style to selected")){if(selection.empty())selection.insert(e.id);for(auto& target:source.elements)if(selection.count(target.id)){target.background=clipboard.background;target.color=clipboard.color;target.font=clipboard.font;target.fontSize=clipboard.fontSize;target.textLogicalAlign=clipboard.textLogicalAlign;}}
    ImGui::InputFloat("Value",&e.value);ImGui::InputFloat("Minimum",&e.minimum);ImGui::InputFloat("Maximum",&e.maximum);
    if(selected&&ImGui::Button("Delete element subtree")){std::set<std::string> remove{e.id};bool more=true;while(more){more=false;for(const auto& x:source.elements)if(remove.count(x.parent))more|=remove.insert(x.id).second;}source.elements.erase(std::remove_if(source.elements.begin(),source.elements.end(),[&](const auto& x){return remove.count(x.id);}),source.elements.end());selected=0;}
    auto after=SerializeUIDocument(source);if(after!=before)pending=true;
    if(!ImGui::IsAnyItemActive())flush();
    if(!source.Validate(error)){ImGui::TextWrapped("Last-good source retained: %s",error.c_str());if(ImGui::Button("Discard invalid visual draft"))reloadTyped();}else if(state.uiPreviewRevision==0||after!=before){state.uiPreviewDocument=source;++state.uiPreviewRevision;}
    if(ImGui::CollapsingHeader("Named UI source")){if(textGeneration!=document.Generation()&&!sourceDirty){CopyToBuffer(document.Source(),text.data(),text.size());textGeneration=document.Generation();}if(ImGui::InputTextMultiline("##ui-source",text.data(),text.size(),{0,250}))sourceDirty=true;if(ImGui::Button("Discard named draft")){sourceDirty=false;textGeneration=~uint64_t(0);}if(ImGui::Button("Validate / apply UI source")){if(flush()&&document.Replace(text.data(),error))reloadTyped();else state.status=error;}}
    ImGui::EndChild();ImGui::TableNextColumn();
    ImGui::TextUnformatted("Canvas - normal runtime layout / shaping");
    ImGui::Checkbox("Runtime canvas preview",&state.uiCanvasPreview);ImGui::InputInt2("Preview resolution",&state.uiPreviewResolution.x);state.uiPreviewResolution=glm::clamp(state.uiPreviewResolution,glm::ivec2(160,90),glm::ivec2(2560,1440));
    if(state.project){auto& c=state.project->Settings().localization;if(state.previewLocale.empty())state.previewLocale=c.defaultLocale;if(ImGui::BeginCombo("Preview locale",state.previewLocale.c_str())){for(auto& [tag,entry]:c.locales){(void)entry;if(ImGui::Selectable(tag.c_str(),tag==state.previewLocale))state.previewLocale=tag;}ImGui::EndCombo();}}
    if(state.uiCanvasPreview&&state.uiPreviewToken){float width=std::max(160.f,ImGui::GetContentRegionAvail().x),height=width*state.uiPreviewResolution.y/state.uiPreviewResolution.x;auto origin=ImGui::GetCursorScreenPos();ImGui::Image(ImTextureID(state.uiPreviewToken),{width,height},{0,1},{1,0});if(ImGui::IsItemHovered()&&ImGui::IsMouseClicked(0)){auto mouse=ImGui::GetMousePos();state.uiCanvasPick={(mouse.x-origin.x)/width*state.uiPreviewResolution.x,(mouse.y-origin.y)/height*state.uiPreviewResolution.y};}}
    ImGui::EndTable();
}

}
bool BakeEditorCollision(EditorDocument& doc,SceneObjectId target,EditorPanelState& state,
                         const AssetId& sourceId,const CollisionCookSettings& settings,
                         const std::string& output){
    auto fail=[&](const std::string& error){state.status=error;return false;};
    if(!state.assets||state.mode!=EditorMode::Edit)return fail("Collision cooking requires an edit project");
    auto* object=doc.GetScene().Find(target);
    if(!object||!object->body)return fail("Select an object with a Body to assign collision");
    if(!settings.convex&&(object->body->motion==SceneBodyMotion::Dynamic||object->body->sensor))return fail("Triangle surfaces require a static non-sensor body");
    if(object->transform.scale!=glm::vec3(1))return fail("Bake scale into source; rigid instance scale must be one");
    const auto* source=state.assets->Find(sourceId);
    if(!source||source->missing||source->type!=AssetType::Mesh)return fail("Select a registered source model");
    auto relative=std::filesystem::path(output).lexically_normal();
    if(relative.empty()||relative.is_absolute()||relative.extension()!=".judascollision")return fail("Output must be an assets-relative .judascollision path");
    for(auto& part:relative)if(part=="..")return fail("Output cannot escape project assets");
    auto destination=std::filesystem::path(state.assets->AssetsDir())/relative;
    std::error_code ec;std::filesystem::create_directories(destination.parent_path(),ec);
    if(ec)return fail(ec.message());
    CollisionAsset asset;std::string error;
    ModelCollisionCleanupReport cleanupReport;if(!CookImportedCollisionFile(source->path,sourceId,settings,state.collisionCleanup,destination.string(),asset,cleanupReport,state.collisionDiagnostic,error))return fail(error);
    AssetRecord record;auto* existing=state.assets->FindByRelativePath(std::filesystem::relative(destination,state.project->RootDir()).generic_string());
    if(existing)record=*existing;else if(!state.assets->Track(destination.string(),record,error))return fail(error);
    if(state.resources)state.resources->Invalidate(record.id);
    doc.BeginEdit();object->body->shape=settings.convex?SceneShape::ConvexHull:SceneShape::TriangleMesh;
    object->body->collisionAsset=record.id;doc.CommitEdit();
    state.status="Cooked and assigned physical geometry (undo restores authored Body)";
    return true;
}

void DrawAssetBrowserPanel(EditorDocument& doc, EditorPanelState& state, EditorRequests& requests) {
    if (!state.workspace.showAssets) return;
    PlaceWorkspacePanel(state.workspace.assets);
    if (!ImGui::Begin("Asset Browser", &state.showAssetBrowser, kWorkspacePanelFlags)) { ImGui::End(); return; }
    const bool hasProject = state.project && state.project->IsLoaded() && state.assets;
    if (!hasProject) {
        ImGui::TextDisabled("No project open. File > New project / Open project.");
        ImGui::End();
        return;
    }
    const AssetDatabase& db = *state.assets;
    PublishFinishedEditorImport(state, requests);
    if (ImGui::BeginTabBar("##assetBrowserTabs")) {
        if (ImGui::BeginTabItem("Assets")) {
    ImGui::BeginChild("assets", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    char assetSearch[256]; CopyToBuffer(state.assetSearch, assetSearch, sizeof(assetSearch));
    const float browserWidth = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(std::max(60.0f, browserWidth - 290.0f));
    if (ImGui::InputTextWithHint("##assetSearch", "Search assets, IDs or source...", assetSearch, sizeof(assetSearch)))
        state.assetSearch = assetSearch;
    ImGui::SameLine();
    const char* assetTypes[]={"All types","Mesh","Texture","Font","Audio","Prefab","Script","UI","Navigation","Liquid","Material","Environment","Catalog","World","Audio effect","Deformable","Collision","Physical material"};
    int selectedType = state.assetTypeFilter + 1;
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::Combo("##assetType", &selectedType, assetTypes, 18)) state.assetTypeFilter = selectedType - 1;
    ImGui::SameLine();
    ImGui::Checkbox("Missing", &state.assetMissingOnly);
    ImGui::SameLine();
    if (ImGui::SmallButton("Rescan")) requests.rescanAssets = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Refresh project assets\n%zu registered / %zu untracked / %zu problems",
                                               db.Records().size(), db.Untracked().size(), db.Problems().size());
    if (ImGui::BeginTable("assetTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                         ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                         {0, std::max(35.0f, ImGui::GetContentRegionAvail().y)})) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 95.0f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        for (const auto& [id, record] : db.Records()) {
            if(*assetSearch&&record.relativePath.find(assetSearch)==std::string::npos&&id.find(assetSearch)==std::string::npos&&record.source.find(assetSearch)==std::string::npos)continue;
            if(state.assetTypeFilter>=0&&int(record.type)!=state.assetTypeFilter)continue;
            if(state.assetMissingOnly&&!record.missing)continue;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(id.c_str());
            const bool selected = state.browserSelection == id;
            const auto selectAsset = [&] {
                state.browserSelection = id;
                state.moveAssetInput = record.path.size() > db.AssetsDir().size() + 1 &&
                    record.path.compare(0, db.AssetsDir().size(), db.AssetsDir()) == 0
                    ? record.path.substr(db.AssetsDir().size() + 1) : record.relativePath;
            };
            const auto name = std::filesystem::path(record.relativePath).filename().string();
            if (ImGui::Selectable(name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                selectAsset();
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) state.selectAssetDetails = true;
            }
            if (!record.missing && ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload(kAssetDragPayload, id.data(), id.size());
                ImGui::Text("%s (%s)", record.relativePath.c_str(), AssetTypeName(record.type));
                ImGui::TextDisabled(record.type == AssetType::Mesh ? "Drop on the viewport to place, or on a Mesh field."
                                                                    : "Drop on a matching inspector field.");
                ImGui::EndDragDropSource();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\nID %s\nSource %s\nDouble-click to inspect; drag to a matching field.", record.path.c_str(), id.c_str(), record.source.c_str());
            if (ImGui::BeginPopupContextItem("##assetActions")) {
                if (ImGui::MenuItem("Inspect asset")) { selectAsset(); state.selectAssetDetails = true; }
                if (ImGui::MenuItem("Copy file path")) ImGui::SetClipboardText(record.path.c_str());
                if (ImGui::MenuItem("Copy stable ID")) ImGui::SetClipboardText(id.c_str());
                ImGui::EndPopup();
            }
            ImGui::PopID();
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(AssetTypeName(record.type));
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
                        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "ready");
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Resident %.1f KB", static_cast<double>(state.resources->BytesOf(id)) / 1024.0);
                        break;
                    case ResourceState::Failed:
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "FAILED");
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", state.resources->ErrorOf(id).c_str());
                        break;
                    default:
                        ImGui::TextDisabled("registered");
                        break;
                }
            } else {
                ImGui::TextDisabled("ok");
            }
        }
        ImGui::EndTable();
    }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        const bool inspectRequested = state.selectAssetDetails;
        state.selectAssetDetails = false;
        if (ImGui::BeginTabItem("Selected asset", nullptr, inspectRequested ? ImGuiTabItemFlags_SetSelected : 0)) {
            ImGui::BeginChild("assetDetails", {0, 0});
            if (state.browserSelection.empty()) ImGui::TextWrapped("Select an asset to inspect its source, dependencies and editing actions.");
    if (!state.browserSelection.empty()) {
        if (const AssetRecord* record = db.Find(state.browserSelection)) {
            ImGui::Separator();
            ImGui::Text("Selected: %s", record->relativePath.c_str());
            ImGui::BeginDisabled(record->missing);
            if (ImGui::Button("Open file")) requests.openExternalPath = record->path;
            ImGui::EndDisabled();
            ToolbarHint("Open this file with your desktop's associated application");
            ImGui::SameLine();
            if (ImGui::Button("Copy path")) ImGui::SetClipboardText(record->path.c_str());
            ImGui::TextWrapped("Provenance: %s",record->source.empty()?"project-owned":record->source.c_str());
            static uint64_t usersGeneration=~uint64_t(0);static std::string usersAsset;static std::vector<SceneObjectId> users;
            if(usersGeneration!=doc.Generation()||usersAsset!=record->id){users.clear();usersGeneration=doc.Generation();usersAsset=record->id;for(auto& o:doc.GetScene().Objects())for(auto& [key,value]:ObjectProperties(o))if(value.find(record->id)!=std::string::npos){users.push_back(o.id);break;}}
            if(ImGui::TreeNode("Current scene users")){for(auto id:users)if(auto* user=doc.GetScene().Find(id))if(ImGui::Selectable((user->name+" ##asset-user"+std::to_string(id)).c_str()))doc.Select(id);ImGui::TreePop();}
            if(ImGui::TreeNode("Asset dependencies")){
                static std::string dependencyKey,dependencyError;static std::set<AssetId> dependencies;
                auto key=record->id+record->path;
                if(dependencyKey!=key){dependencyKey=key;DirectAssetDependencies(*record,dependencies,dependencyError);}
                if(ImGui::SmallButton("Refresh dependency source"))DirectAssetDependencies(*record,dependencies,dependencyError);
                for(auto& id:dependencies){auto* target=db.Find(id);ImGui::TextWrapped("%s / %s",id.c_str(),target?target->relativePath.c_str():"MISSING");}
                if(!dependencyError.empty())ImGui::TextWrapped("%s",dependencyError.c_str());
                ImGui::TextWrapped("Typed prefab/UI/material/model/collision references. Dynamic script IDs use the registered-asset export policy.");
                ImGui::TreePop();
            }
            if(record->type==AssetType::Mesh&&state.resources){if(auto model=state.resources->TryGetSkeletal(record->id)){if(ImGui::TreeNode("Shared skeleton")){for(auto& name:model->skeleton.names)ImGui::TextUnformatted(name.c_str());ImGui::TreePop();}}}

            if(record->type==AssetType::Collision&&state.mode==EditorMode::Edit){CollisionAsset asset;std::string error;if(LoadCollisionAsset(record->path,asset,error)){ImGui::Text("Physical %s: %zu vertices / %zu faces",asset.convex?"hull":"triangle surface",asset.vertices.size(),asset.faces.size());auto source=db.Find(asset.sourceAsset);bool stale=!source||source->missing||CollisionAssetStale(asset,source->path,error);ImGui::TextWrapped("%s",stale?error.c_str():"Source/settings fingerprint current");if(ImGui::Button("Rebake saved source/settings")&&source){CollisionCookSettings settings;settings.convex=asset.convex;settings.twoSided=asset.twoSided;settings.primitive=asset.selectedPrimitive;settings.transform=asset.sourceTransform;ModelCollisionCleanup cleanup;ModelCollisionCleanupReport cleanupReport;CollisionDiagnostic diagnostic;if(ReadModelCollisionCleanup(record->path,cleanup,error)&&CookImportedCollisionFile(source->path,source->id,settings,cleanup,record->path,asset,cleanupReport,diagnostic,error)){if(state.resources)state.resources->Invalidate(record->id);state.status="Rebaked physical collider";}else state.status=error;}for(auto& warning:asset.warnings)ImGui::TextWrapped("%s",warning.c_str());}else ImGui::TextWrapped("%s",error.c_str());}
            if(record->type==AssetType::Material&&state.mode==EditorMode::Edit){
                static std::string selected,error;static MaterialDefinition draft;if(selected!=record->id){selected=record->id;LoadMaterial(record->path,draft,error);}
                if(ImGui::CollapsingHeader("Edit SHARED material source",ImGuiTreeNodeFlags_DefaultOpen)){int model=int(draft.model),alpha=int(draft.alpha);const char* models[]={"Legacy","PBR metallic / roughness","Unlit"};const char* modes[]={"Opaque","Alpha cutout","Alpha blend"};if(ImGui::Combo("Model",&model,models,3))draft.model=MaterialModel(model);if(ImGui::Combo("Alpha mode",&alpha,modes,3))draft.alpha=MaterialAlpha(alpha);ImGui::ColorEdit4("Base colour factor (linear)",&draft.baseColor.x);ImGui::SliderFloat("Metallic",&draft.metallic,0,1);ImGui::SliderFloat("Roughness",&draft.roughness,0,1);ImGui::ColorEdit3("Emission (linear)",&draft.emissive.x);ImGui::DragFloat("Emission intensity",&draft.emissiveIntensity,.05f,0,100000);ImGui::SliderFloat("Normal strength",&draft.normalStrength,0,8);ImGui::SliderFloat("Occlusion strength",&draft.occlusionStrength,0,1);ImGui::SliderFloat("Alpha cutoff",&draft.alphaCutoff,0,1);ImGui::Checkbox("Double sided",&draft.doubleSided);ImGui::Checkbox("Flip texture V",&draft.flipV);ImGui::DragFloat2("UV tiling",&draft.uvScale.x,.01f);ImGui::DragFloat2("UV offset",&draft.uvOffset.x,.01f);
                    const char* labels[]={"Base colour sRGB","Metallic B / roughness G linear","Normal linear","Occlusion R linear","Emission sRGB"};for(size_t i=0;i<5;++i){auto& map=draft.maps[i];std::string label=map.asset.empty()?"(none)":map.asset;if(ImGui::BeginCombo(labels[i],label.c_str())){if(ImGui::Selectable("(none)",map.asset.empty()))map.asset.clear();for(const auto& [id,a]:db.Records())if(a.type==AssetType::Texture&&ImGui::Selectable(a.relativePath.c_str(),map.asset==id))map.asset=id;ImGui::EndCombo();}ImGui::PushID(int(i));const char* wrap[]={"Repeat","Clamp","Mirror"};int values[]={10497,33071,33648};int ws=map.sampler.wrapS==33071?1:map.sampler.wrapS==33648?2:0,wt=map.sampler.wrapT==33071?1:map.sampler.wrapT==33648?2:0;if(ImGui::Combo("Wrap U",&ws,wrap,3))map.sampler.wrapS=values[ws];if(ImGui::Combo("Wrap V",&wt,wrap,3))map.sampler.wrapT=values[wt];ImGui::PopID();}
                    ImGui::TextWrapped("In-scene preview: assign this asset to a render slot. Saving intentionally updates all non-overridden users.");if(ImGui::Button("Save shared material")){if(SaveMaterial(record->path,draft,error)){if(state.resources)state.resources->Invalidate(record->id);state.status="Saved shared material; instances reload normally";}else state.status=error;}ImGui::SameLine();if(ImGui::Button("Reload source"))LoadMaterial(record->path,draft,error);if(!error.empty())ImGui::TextWrapped("%s",error.c_str());
                }
            }
            if(record->type==AssetType::AudioEffect&&state.mode==EditorMode::Edit&&ImGui::CollapsingHeader("Edit shared reverb settings",ImGuiTreeNodeFlags_DefaultOpen)){
                static std::string selected,error;static AudioEnvironmentSettings draft;if(selected!=record->id){selected=record->id;LoadAudioEnvironment(record->path,draft,error);}
                ImGui::SliderFloat("Room feedback",&draft.roomSize,0,1);ImGui::SliderFloat("High frequency damping",&draft.damping,0,1);ImGui::SliderFloat("Stereo width",&draft.width,0,1);ImGui::SliderFloat("Wet level",&draft.wet,0,1);
                if(ImGui::Button("Save shared reverb")){std::ofstream f(record->path);f<<SerializeAudioEnvironment(draft);if(f){if(state.resources)state.resources->Invalidate(record->id);state.status="Saved reverb settings";error.clear();}else error="Cannot write reverb settings";}ImGui::SameLine();if(ImGui::Button("Reload reverb"))LoadAudioEnvironment(record->path,draft,error);if(!error.empty())ImGui::TextWrapped("%s",error.c_str());
            }
            if(record->type==AssetType::World&&state.mode==EditorMode::Edit&&ImGui::CollapsingHeader("World manifest source",ImGuiTreeNodeFlags_DefaultOpen)){
                ImGui::TextWrapped("Edit this registered manifest through the shared named document editor. Validation, undo and external-change protection use the same service as the CLI.");
                if(ImGui::Button("Open world/source authoring tools"))state.showWorldBuilding=true;
                ImGui::TextWrapped("Path: %s",record->path.c_str());
            }
            if(record->type==AssetType::Catalog&&state.mode==EditorMode::Edit&&ImGui::CollapsingHeader("Catalog UTF-8 source")){
                static std::string catalogPath;static std::vector<char> buffer(2*1024*1024+1);if(catalogPath!=record->path){catalogPath=record->path;std::ifstream f(catalogPath);std::string text(std::istreambuf_iterator<char>(f),{});std::snprintf(buffer.data(),buffer.size(),"%s",text.c_str());}
                ImGui::InputTextMultiline("ICU messages",buffer.data(),buffer.size(),{0,220});
                if(ImGui::Button("Validate and save catalog")){Catalog parsed;std::string error;if(ParseCatalog(buffer.data(),parsed,error)){std::ofstream f(record->path,std::ios::binary);f<<buffer.data();state.status=f?"Saved UTF-8 catalog":"Catalog write failed";state.textReload=true;if(state.resources)state.resources->Invalidate(record->id);}else state.status=error;}
                ImGui::SameLine();if(ImGui::Button("Reload preview catalog"))state.textReload=true;
            }
            if(record->type==AssetType::UI&&state.mode==EditorMode::Edit){
                if(ImGui::Button("Edit UI document"))state.showUIDocument=true;
                if(state.showUIDocument){
                    ImGui::SetNextWindowPos({120,45},ImGuiCond_FirstUseEver);
                    ImGui::SetNextWindowSize({1040,700},ImGuiCond_FirstUseEver);
                    if(ImGui::Begin("UI document",&state.showUIDocument))DrawUILayoutEditor(*record,state);
                    ImGui::End();
                }
            }
            if(record->type==AssetType::Prefab&&state.mode==EditorMode::Edit&&ImGui::Button("Place prefab instance")){
                Scene source;std::string error;SceneObjectId root=0;
                if(LoadPrefab(db,record->id,source,error)){
                    doc.BeginEdit();
                    if(InstantiatePrefab(doc.GetScene(),source,record->id,SceneTransform{},root,error))doc.Select(root);
                    doc.CommitEdit(false);
                }
                state.status=error.empty()?"Placed linked prefab instance":error;
            }
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
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Import & tools")) {
            ImGui::BeginChild("assetTools", {0, 0});
            if(ImGui::Button("Animation retargeting: map / preview / bake"))OpenRetargetPanel(state,{},state.modelRecipe);
    if(ImGui::CollapsingHeader("Model import / reimport",ImGuiTreeNodeFlags_DefaultOpen)){
        auto text=[](const char* label,std::string& value){std::vector<char> buffer(std::max(size_t(4096),value.size()+256),0);std::copy(value.begin(),value.end(),buffer.begin());if(ImGui::InputText(label,buffer.data(),buffer.size()))value=buffer.data();};
        ImGui::TextWrapped("FBX / glTF / GLB / OBJ to an ordinary cooked model. Sources and recipes are author-owned; the game loads only cooked content. Reimport publishes only in Edit mode.");
        text("Original source file",state.modelSource);text("Project-relative runtime output",state.modelOutput);
        if(state.mode==EditorMode::Edit&&ImGui::Button("Copy source and create import recipe")){std::string error;if(CreateModelRecipe(state.project->RootDir(),state.modelSource,state.modelOutput,state.modelRecipe,error))state.status="Recipe created; review settings then Import";else state.status=error;}
        text("Import recipe (.judasimport)",state.modelRecipe);
        if(!state.modelRecipe.empty()&&std::filesystem::is_regular_file(state.modelRecipe))try{
            using Json=nlohmann::json;std::ifstream file(state.modelRecipe);if(std::filesystem::file_size(state.modelRecipe)>4*1024*1024)throw std::runtime_error("Recipe exceeds 4 MiB");auto recipe=Json::parse(file,[](int depth,Json::parse_event_t,const Json&){if(depth>=48)throw std::runtime_error("Recipe nesting bound");return true;});bool changed=false;auto& settings=recipe["settings"];
            double units=settings.value("unitMeters",0.0),rate=settings.value("sampleRate",60.0);bool base=settings.value("allowBaseMesh",false);
            if(ImGui::InputDouble("Source metres per unit (0 = metadata)",&units)){settings["unitMeters"]=units;changed=true;}if(ImGui::InputDouble("Nonlinear bake samples / second",&rate)){settings["sampleRate"]=rate;changed=true;}if(ImGui::Checkbox("Explicit base mesh only (lossy)",&base)){settings["allowBaseMesh"]=base;changed=true;}
            text("Compatible motion source",state.modelMotionSource);text("Source take",state.modelMotionTake);text("Clip name",state.modelMotionName);
            if(state.mode==EditorMode::Edit&&ImGui::Button("Copy / add compatible motion")){auto root=std::filesystem::path(state.project->RootDir());auto directory=(root/recipe.at("source").get<std::string>()).parent_path();auto source=std::filesystem::path(state.modelMotionSource);auto owned=directory/source.filename();if(std::filesystem::weakly_canonical(source)!=std::filesystem::weakly_canonical(owned))std::filesystem::copy_file(source,owned,std::filesystem::copy_options::overwrite_existing);recipe["motions"].push_back({{"source",owned.lexically_relative(root).generic_string()},{"take",state.modelMotionTake},{"name",state.modelMotionName},{"jointRemaps",Json::object()}});changed=true;}
            glm::vec4 basis(0,0,0,1);if(settings.contains("basisRotation")){auto q=settings.at("basisRotation").get<std::vector<float>>();if(q.size()==4)basis={q[0],q[1],q[2],q[3]};}if(ImGui::InputFloat4("Explicit post-metadata basis rotation x/y/z/w",&basis.x)){settings["basisRotation"]={basis.x,basis.y,basis.z,basis.w};changed=true;}
            text("Remap source name/path",state.modelMapSource);text("Project-relative file or stable alias",state.modelMapTarget);
            const char* maps[]={"dependencyRemaps","materialAliases","nodeAliases"};for(auto field:maps){ImGui::PushID(field);if(ImGui::Button(field)&&!state.modelMapSource.empty()){settings[field][state.modelMapSource]=state.modelMapTarget;changed=true;}ImGui::PopID();}
            if(state.importAccepted&&state.importAccepted->preview){auto& preview=*state.importAccepted->preview;
                if(ImGui::TreeNode("Import part selection (empty = all)")){auto parts=settings.value("selectedParts",std::vector<std::string>{});bool all=parts.empty();if(ImGui::Checkbox("All source parts",&all)&&all){settings["selectedParts"]=Json::array();changed=true;}for(auto& part:preview.primitives){bool selected=all||std::find(parts.begin(),parts.end(),part.part)!=parts.end();if(ImGui::Checkbox(part.part.c_str(),&selected)){if(all){for(auto& p:preview.primitives)parts.push_back(p.part);}if(selected){if(std::find(parts.begin(),parts.end(),part.part)==parts.end())parts.push_back(part.part);}else parts.erase(std::remove(parts.begin(),parts.end(),part.part),parts.end());settings["selectedParts"]=parts;changed=true;}}ImGui::TreePop();}
                if(ImGui::TreeNode("Source material slots")){for(size_t i=0;i<preview.materialKeys.size();++i)ImGui::BulletText("%zu: %s",i,preview.materialKeys[i].c_str());ImGui::TextWrapped("Author material overrides in the normal Render inspector; imported defaults are separate.");ImGui::TreePop();}
            }
            if(recipe.contains("clips"))for(auto& clip:recipe["clips"]){ImGui::PushID(clip.at("sourceClip").get<std::string>().c_str());std::string name=clip.value("name",clip.at("sourceClip").get<std::string>());text("Clip output name",name);if(clip.value("name",std::string())!=name){clip["name"]=name;changed=true;}bool loop=clip.value("loop",false);if(ImGui::Checkbox("Loop metadata",&loop)){clip["loop"]=loop;changed=true;}auto& root=clip["rootMotion"];std::string policy=root.value("policy",std::string("preserve")),node=root.value("node",std::string());if(ImGui::BeginCombo("Root policy",policy.c_str())){for(auto value:{"preserve","inPlace","extract"})if(ImGui::Selectable(value,policy==value)){root["policy"]=value;changed=true;}ImGui::EndCombo();}auto rig=state.importTask&&state.importPublished?state.resources->TryGetSkeletal(state.importTask->assetId):nullptr;if(rig&&ImGui::BeginCombo("Motion node",node.c_str())){for(size_t i=0;i<rig->skeleton.names.size();++i){auto key=SkeletonJointKey(rig->skeleton,int(i));if(ImGui::Selectable(key.c_str(),node==key)){root["node"]=key;changed=true;}}ImGui::EndCombo();}auto axes=root.value("translation",std::vector<bool>{true,false,true});if(axes.size()==3)for(int k=0;k<3;++k){bool axis=axes[k];ImGui::PushID(k);if(ImGui::Checkbox(k==0?"Model X":k==1?"Model Y":"Model Z",&axis)){axes[k]=axis;root["translation"]=axes;changed=true;}ImGui::PopID();ImGui::SameLine();}ImGui::NewLine();glm::vec3 rotationAxis(0);if(root.contains("rotationAxis")){auto v=root["rotationAxis"].get<std::vector<float>>();if(v.size()==3)rotationAxis={v[0],v[1],v[2]};}if(ImGui::InputFloat3("Extract twist around model axis (zero = none)",&rotationAxis.x)){if(glm::length(rotationAxis)>1e-6f)root["rotationAxis"]={rotationAxis.x,rotationAxis.y,rotationAxis.z};else root.erase("rotationAxis");changed=true;}glm::vec2 trim(0,1);if(clip.contains("trim"))trim={clip["trim"][0],clip["trim"][1]};if(ImGui::InputFloat2("Trim begin/end seconds",&trim.x)){clip["trim"]={trim.x,trim.y};changed=true;}ImGui::PopID();}
            if(state.importTask&&state.importTask->preview&&state.importTask->preview->skeletal&&ImGui::Button("Populate editable clip selections")){recipe["clips"]=Json::array();for(auto& clip:state.importTask->preview->skeletal->clips)recipe["clips"].push_back({{"sourceClip",clip.name},{"name",clip.name},{"loop",clip.loop},{"rootMotion",{{"policy","preserve"}}}});changed=true;}
            if(changed&&state.mode==EditorMode::Edit){std::ofstream out(state.modelRecipe);out<<recipe.dump(2)<<'\n';state.status="Import recipe saved; accepted runtime content is unchanged until reimport";}
            if(ImGui::TreeNode("Named recipe / selections / remaps")){auto pretty=recipe.dump(2);ImGui::TextUnformatted(pretty.c_str());ImGui::TreePop();}
        }catch(const std::exception& e){ImGui::TextWrapped("Recipe: %s",e.what());}
        bool busy=state.importTask&&!state.importTask->done.load();
        if(state.mode==EditorMode::Edit&&!busy&&!state.modelRecipe.empty()&&ImGui::Button("Import / reimport / retry")){state.importTask=QueueModelImport(*state.importJobs,state.modelRecipe);state.importPublished=false;}
        if(busy){ImGui::ProgressBar(float(state.importTask->progress.load())/100);if(ImGui::Button("Cancel import"))state.importTask->cancel=true;}
        if (state.importTask && state.importTask->done.load(std::memory_order_acquire)) {
            if (!state.importTask->success) ImGui::TextWrapped("Import failed; last good model retained: %s", state.importTask->error.c_str());
            else if (!state.importPublished) ImGui::TextWrapped("Cook ready. Stop Play to publish; live skeleton buffers remain unchanged.");
        }
        if(state.importAccepted){auto& t=*state.importAccepted;{
                ImGui::Text("Source bones: %zu; hierarchy nodes: %zu; palette entries: %zu; parts: %zu",t.report.sourceBones,t.report.hierarchyNodes,t.report.skinJoints,t.report.parts);
                if(ImGui::Button("Place accepted model"))requests.dropMeshAssetId=t.assetId;
                auto rig=state.resources->TryGetSkeletal(t.assetId);
                if(rig){
                    ImGui::Text("Runtime hierarchy: %zu nodes",rig->skeleton.names.size());
                    if(!rig->clips.empty()){int clip=int(std::min(size_t(state.modelPreviewClip),rig->clips.size()-1));if(ImGui::BeginCombo("Preview clip",rig->clips[clip].name.c_str())){for(size_t i=0;i<rig->clips.size();++i)if(ImGui::Selectable(rig->clips[i].name.c_str(),i==size_t(clip))){state.modelPreviewClip=unsigned(i);state.modelPreviewTime=0;}ImGui::EndCombo();}ImGui::Checkbox("Preview playback",&state.modelPreviewPlaying);ImGui::SliderFloat("Scrub seconds",&state.modelPreviewTime,0,rig->clips[state.modelPreviewClip].duration);}
                    ImGui::Checkbox("Skeleton overlay",&state.modelPreviewSkeleton);ImGui::SliderFloat("Orbit view",&state.modelPreviewYaw,-3.14f,3.14f);
                    if(ImGui::TreeNode("Hierarchy / stable joint identities")){for(size_t i=0;i<rig->skeleton.names.size();++i)ImGui::BulletText("%s",SkeletonJointKey(rig->skeleton,int(i)).c_str());ImGui::TreePop();}
                }
                if(auto parts=state.resources->TryGetModelParts(t.assetId))for(auto& p:*parts){bool visible=std::find(state.modelPreviewHidden.begin(),state.modelPreviewHidden.end(),p.part)==state.modelPreviewHidden.end();if(ImGui::Checkbox(p.part.c_str(),&visible)){if(visible)state.modelPreviewHidden.erase(std::remove(state.modelPreviewHidden.begin(),state.modelPreviewHidden.end(),p.part),state.modelPreviewHidden.end());else state.modelPreviewHidden.push_back(p.part);}}
                ImGui::TextWrapped("White ruler = one metre; RGB axes = import model basis; green = hierarchy; orange = extracted root track. These are preview axes, not simulation gravity.");
                if(state.modelPreviewToken)ImGui::Image((ImTextureID)(intptr_t)state.modelPreviewToken,ImVec2(std::min(600.f,ImGui::GetContentRegionAvail().x),360),ImVec2(0,1),ImVec2(1,0));
                for(auto& diagnostic:t.report.diagnostics)ImGui::TextWrapped("%s [%s]: %s. %s",diagnostic.severity.c_str(),diagnostic.code.c_str(),diagnostic.message.c_str(),diagnostic.action.c_str());
            }
        }
    }

    if(state.mode==EditorMode::Edit&&ImGui::Button("Create material")){auto directory=std::filesystem::path(db.AssetsDir())/"materials";std::filesystem::create_directories(directory);auto path=directory/"material.judasmat";unsigned n=2;while(std::filesystem::exists(path))path=directory/("material"+std::to_string(n++)+".judasmat");std::string error;if(SaveMaterial(path.string(),MaterialDefinition{},error))requests.trackAssetPath=path.string();else state.status=error;}
    if(state.mode==EditorMode::Edit&&ImGui::Button("Create reverb settings")){auto directory=std::filesystem::path(db.AssetsDir())/"audio";std::filesystem::create_directories(directory);auto path=directory/"environment.judasreverb";unsigned n=2;while(std::filesystem::exists(path))path=directory/("environment"+std::to_string(n++)+".judasreverb");std::ofstream f(path);f<<SerializeAudioEnvironment({});if(f)requests.trackAssetPath=path.string();else state.status="Cannot write reverb settings";}
    if(state.mode==EditorMode::Edit&&ImGui::CollapsingHeader("Bake HDR environment")){static char source[1024]{};static int width=128,samples=128;ImGui::InputText("Radiance .hdr source",source,sizeof(source));ImGui::InputInt("Bake width (power of two)",&width);ImGui::InputInt("Bake samples",&samples);ImGui::TextWrapped("Offline bake; may briefly block editor. Derived data is exported, never regenerated during play.");if(ImGui::Button("Bake and register environment")){auto directory=std::filesystem::path(db.AssetsDir())/"environments";std::filesystem::create_directories(directory);auto path=directory/(std::filesystem::path(source).stem().string()+".judasenv");unsigned n=2;while(std::filesystem::exists(path))path=directory/(std::filesystem::path(source).stem().string()+std::to_string(n++)+".judasenv");EnvironmentData data;std::string error;if(BakeEnvironment(source,unsigned(width),unsigned(samples),data,error)&&SaveEnvironment(path.string(),data,error))requests.trackAssetPath=path.string();else state.status=error;}}

    if(state.mode==EditorMode::Edit&&ImGui::CollapsingHeader("Cook physical collider")){
        static std::string sourceId;static char output[256]="collision/physical.judascollision";static int primitive=0,kind=0;static bool twoSided=false;static glm::vec3 scale(1),offset(0),angles(0);
        const auto selected=db.Find(sourceId);if(ImGui::BeginCombo("Registered source",selected?selected->relativePath.c_str():"Select OBJ/glTF/GLB")){for(auto& [id,a]:db.Records())if(a.type==AssetType::Mesh&&ImGui::Selectable(a.relativePath.c_str(),id==sourceId))sourceId=id;ImGui::EndCombo();}
        ImGui::InputInt("Object/group or node/primitive ordinal",&primitive);const char* kinds[]={"Static triangle surface","Convex hull (fills recesses)"};ImGui::Combo("Cook kind",&kind,kinds,2);ImGui::Checkbox("Two-sided triangle surface",&twoSided);ImGui::DragFloat3("Baked positive scale",&scale.x,.01f);ImGui::DragFloat3("Baked translation",&offset.x,.01f);ImGui::DragFloat3("Baked rotation degrees",&angles.x,.5f);ImGui::InputText("Output relative to assets",output,sizeof(output));
        ImGui::Checkbox("Explicit removal of true degenerates",&state.collisionCleanup.removeDegenerates);ImGui::Checkbox("Explicit orient connected patches",&state.collisionCleanup.orientPatches);ImGui::InputDouble("Collision-only weld metres (0..0.001)",&state.collisionCleanup.weldTolerance);
        if(!state.collisionDiagnostic.code.empty()){auto& d=state.collisionDiagnostic;ImGui::TextWrapped("%s: %s / source face %u / point %.4f %.4f %.4f",d.code.c_str(),d.node.c_str(),d.sourceFace,d.point.x,d.point.y,d.point.z);if(ImGui::Button("Highlight diagnostic on selected model"))state.modelDiagnosticVisible=true;}
        ImGui::TextWrapped("Explicit single source selection; glTF node transform is included. Failed cooks preserve the previous file. Use a low-detail physical model where appropriate. Convex mass sums child volumes, including overlaps.");
        if(ImGui::Button("Cook / replace and register")){std::string error;CollisionAsset asset;ModelCollisionCleanupReport cleanupReport;state.collisionDiagnosticAsset=sourceId;CollisionCookSettings settings;settings.convex=kind==1;settings.twoSided=twoSided;settings.primitive=unsigned(std::max(primitive,0));settings.transform=glm::translate(glm::dmat4(1),glm::dvec3(offset))*glm::mat4_cast(glm::dquat(glm::radians(glm::dvec3(angles))))*glm::scale(glm::dmat4(1),glm::dvec3(scale));auto relative=std::filesystem::path(output).lexically_normal();bool valid=!relative.empty()&&!relative.is_absolute();for(auto& part:relative)if(part=="..")valid=false;auto destination=std::filesystem::path(db.AssetsDir())/relative;
            if(!selected||selected->missing)error="select an available registered model";else if(!valid||relative.extension()!=".judascollision")error="destination must be an assets-relative .judascollision path";else{std::error_code ec;std::filesystem::create_directories(destination.parent_path(),ec);if(ec)error=ec.message();else if(CookImportedCollisionFile(selected->path,sourceId,settings,state.collisionCleanup,destination.string(),asset,cleanupReport,state.collisionDiagnostic,error)){requests.trackAssetPath=destination.string();for(auto& [id,a]:db.Records())if(a.path==destination.string()&&state.resources)state.resources->Invalidate(id);}}
            state.status=error.empty()?"Cooked collision; assign it in Body / Cooked geometry":error;
        }
        if(ImGui::Button("Cook and assign selected Body")){CollisionCookSettings settings;settings.convex=kind==1;settings.twoSided=twoSided;settings.primitive=unsigned(std::max(primitive,0));settings.transform=glm::translate(glm::dmat4(1),glm::dvec3(offset))*glm::mat4_cast(glm::dquat(glm::radians(glm::dvec3(angles))))*glm::scale(glm::dmat4(1),glm::dvec3(scale));BakeEditorCollision(doc,doc.Selected(),state,sourceId,settings,output);}
    }

    if(state.mode==EditorMode::Edit&&ImGui::Button("Create UI document")){
        UIDocument d;UIElement root;root.id="canvas";root.kind=UIKind::Canvas;d.elements.push_back(root);UIElement panel;panel.id="panel";panel.parent="canvas";panel.background={.1f,.12f,.18f,.95f};d.elements.push_back(panel);
        auto directory=std::filesystem::path(db.AssetsDir())/"ui";std::filesystem::create_directories(directory);auto path=directory/"layout.judasui";int n=2;while(std::filesystem::exists(path))path=directory/("layout"+std::to_string(n++)+".judasui");std::string error;
        if(SaveUIDocument(path.string(),d,error))requests.trackAssetPath=path.string();else state.status=error;
    }
    if (ImGui::CollapsingHeader("Import", ImGuiTreeNodeFlags_DefaultOpen)) {
        char source[512], destination[256];
        CopyToBuffer(state.importSourceInput, source, sizeof(source));
        CopyToBuffer(state.importDestinationInput, destination, sizeof(destination));
        if (ImGui::InputTextWithHint("Source file", "/path/to/model.obj | texture.png | font.ttf | sound.wav", source, sizeof(source))) state.importSourceInput = source;
        if (ImGui::InputTextWithHint("Destination (in assets)", "models/model.obj (empty keeps the file name)", destination, sizeof(destination))) state.importDestinationInput = destination;
        if (ImGui::Button("Import") && !state.importSourceInput.empty()) {
            requests.importSource = state.importSourceInput;
            requests.importDestination = state.importDestinationInput;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(path field; the editor has no OS file dialog)");
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
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
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
    ImGui::Checkbox("Legacy gameplay compatibility (historical scenes)", &s.legacyGameplay);
    ImGui::TextDisabled("%s", project.ProjectFile().c_str());
    char name[256];
    CopyToBuffer(s.name, name, sizeof(name));
    if (ImGui::InputText("Name", name, sizeof(name))) s.name = name;
    if (ImGui::BeginCombo("Game icon", s.iconAsset.empty() ? "(Judas fallback)" : s.iconAsset.c_str())) {
        if (ImGui::Selectable("(Judas fallback)", s.iconAsset.empty())) s.iconAsset.clear();
        if (state.assets) for (const auto& [id, asset] : state.assets->Records())
            if (asset.type == AssetType::Texture && !asset.missing &&
                std::filesystem::path(asset.path).extension() == ".png" &&
                ImGui::Selectable(asset.relativePath.c_str(), s.iconAsset == id)) s.iconAsset = id;
        ImGui::EndCombo();
    }
    if (ImGui::BeginCombo("Startup scene", s.startupScene.empty() ? "(none)" : s.startupScene.c_str())) {
        for (const std::string& scene : state.sceneFiles) {
            if (ImGui::Selectable(scene.c_str(), scene == s.startupScene)) s.startupScene = scene;
        }
        ImGui::EndCombo();
    }
    if(state.assets&&ImGui::BeginCombo("Optional world manifest",s.worldManifest.empty()?"(single scene)":s.worldManifest.c_str())){
        if(ImGui::Selectable("(single scene)",s.worldManifest.empty()))s.worldManifest.clear();
        for(auto& [id,asset]:state.assets->Records())if(asset.type==AssetType::World&&ImGui::Selectable(asset.relativePath.c_str(),s.worldManifest==id))s.worldManifest=id;
        ImGui::EndCombo();
    }
    ImGui::Text("Assets: %s   Scenes: %s   Saves: %s", s.assetsDir.c_str(), s.scenesDir.c_str(), s.savesDir.c_str());
    ImGui::TextWrapped("Runtime scenes: saved .judas files in the project scene directory, plus the startup scene. Scripts load them by project-relative path. Export includes this same set.");
    if(ImGui::CollapsingHeader("Export scene selection")){
        bool closure=s.exportAssetPolicy=="closure";if(ImGui::Checkbox("Asset dependency closure (explicit dynamic roots)",&closure))s.exportAssetPolicy=closure?"closure":"all";
        ImGui::TextWrapped("All registered assets is the safe default. Closure keeps selected scene dependencies, script/UI/localization compatibility roots and the runtime IDs below. Declare dynamically loaded prefabs/audio/materials and save-only assets.");
        for(size_t i=0;i<s.runtimeAssets.size();){ImGui::PushID(int(i));ImGui::TextUnformatted(s.runtimeAssets[i].c_str());ImGui::SameLine();if(ImGui::SmallButton("Remove root"))s.runtimeAssets.erase(s.runtimeAssets.begin()+i);else ++i;ImGui::PopID();}
        if(state.assets)for(const auto& [id,asset]:state.assets->Records())if(std::find(s.runtimeAssets.begin(),s.runtimeAssets.end(),id)==s.runtimeAssets.end()){ImGui::PushID(id.c_str());if(ImGui::Selectable(("Include runtime: "+asset.relativePath).c_str()))s.runtimeAssets.push_back(id);ImGui::PopID();}
        bool explicitSet=!s.exportScenes.empty();if(ImGui::Checkbox("Explicit inclusion list",&explicitSet)){if(explicitSet)s.exportScenes=state.sceneFiles;else s.exportScenes.clear();}
        for(const auto& scene:state.sceneFiles){ImGui::PushID(scene.c_str());bool included=s.exportScenes.empty()||std::find(s.exportScenes.begin(),s.exportScenes.end(),scene)!=s.exportScenes.end();bool excluded=std::find(s.excludeScenes.begin(),s.excludeScenes.end(),scene)!=s.excludeScenes.end();ImGui::TextUnformatted(scene.c_str());if(!s.exportScenes.empty()&&ImGui::Checkbox("Include",&included)){s.exportScenes.erase(std::remove(s.exportScenes.begin(),s.exportScenes.end(),scene),s.exportScenes.end());if(included)s.exportScenes.push_back(scene);}ImGui::SameLine();if(ImGui::Checkbox("Exclude",&excluded)){s.excludeScenes.erase(std::remove(s.excludeScenes.begin(),s.excludeScenes.end(),scene),s.excludeScenes.end());if(excluded)s.excludeScenes.push_back(scene);}ImGui::PopID();}
        ImGui::TextWrapped("Startup and world-manifest regions must be included. Registered assets stay available for dynamic ID loading. Save project before export.");
    }
    if (ImGui::Button("Save project")) requests.saveProject = true;
    ImGui::SameLine();
    if (ImGui::Button("Run project")) requests.runProject = true;
    char destination[4096];
    CopyToBuffer(state.exportDestination, destination, sizeof(destination));
    if (ImGui::InputText("Package directory", destination, sizeof(destination))) state.exportDestination = destination;
    ImGui::TextWrapped("Export uses saved scenes and all registered assets. Save scene edits first. Choose a directory outside the project.");
    if (ImGui::Button("Export project (Release)")) requests.exportProject = true;
    if(ImGui::CollapsingHeader("Audio routing")){
        for(auto it=s.audio.groups.begin();it!=s.audio.groups.end();){ImGui::PushID(it->first.c_str());ImGui::TextUnformatted(it->first.c_str());ImGui::SliderFloat("Gain",&it->second.gain,0,1);ImGui::Checkbox("Mute",&it->second.mute);ImGui::Checkbox("Paused",&it->second.paused);bool remove=ImGui::SmallButton("Remove group");ImGui::PopID();if(remove)it=s.audio.groups.erase(it);else ++it;}
        static char group[65]{};ImGui::InputText("New group",group,sizeof(group));if(ImGui::Button("Add sound group")&&group[0]&&std::string(group)!="master"&&s.audio.groups.size()<16)s.audio.groups.emplace(group,AudioGroupSettings{});
        ImGui::TextWrapped("Empty emitter group uses master. Games choose group names and pause policy in JavaScript. Save project to persist routing.");
    }
    ImGui::Separator();
    if(ImGui::CollapsingHeader("Localization and fallback fonts")){
        auto asset=[&](const char* label,std::string& id,AssetType type){if(ImGui::BeginCombo(label,id.empty()?"(none)":id.c_str())){if(ImGui::Selectable("(none)",id.empty()))id.clear();if(state.assets)for(auto& [key,record]:state.assets->Records())if(record.type==type&&!record.missing){if(ImGui::Selectable(record.relativePath.c_str(),id==key))id=key;}ImGui::EndCombo();}};
        auto fonts=[&](const char* label,std::vector<std::string>& list){ImGui::PushID(label);if(ImGui::TreeNode(label)){for(size_t i=0;i<list.size();++i){ImGui::PushID(int(i));asset("Font",list[i],AssetType::Font);if(i&&ImGui::SmallButton("Move up"))std::swap(list[i],list[i-1]);ImGui::SameLine();if(ImGui::SmallButton("Remove")){list.erase(list.begin()+i);ImGui::PopID();break;}ImGui::PopID();}if(list.size()<16&&ImGui::Button("Add fallback font"))list.push_back("");ImGui::TreePop();}ImGui::PopID();};
        if(ImGui::BeginCombo("Default locale",s.localization.defaultLocale.c_str())){for(auto& [tag,entry]:s.localization.locales){(void)entry;if(ImGui::Selectable(tag.c_str(),tag==s.localization.defaultLocale))s.localization.defaultLocale=tag;}ImGui::EndCombo();}
        fonts("Global ordered fallback fonts",s.localization.fonts);
        for(auto it=s.localization.locales.begin();it!=s.localization.locales.end();++it){ImGui::PushID(it->first.c_str());if(ImGui::TreeNode(it->first.c_str())){asset("Catalog",it->second.catalog,AssetType::Catalog);if(ImGui::BeginCombo("Fallback locale",it->second.fallback.empty()?"(default/parent)":it->second.fallback.c_str())){if(ImGui::Selectable("(default/parent)",it->second.fallback.empty()))it->second.fallback.clear();for(auto& [tag,entry]:s.localization.locales)if(tag!=it->first){(void)entry;if(ImGui::Selectable(tag.c_str(),tag==it->second.fallback))it->second.fallback=tag;}ImGui::EndCombo();}fonts("Locale ordered fallback fonts",it->second.fonts);if(ImGui::Button("Remove locale")){auto tag=it->first;s.localization.locales.erase(it);for(auto& [name,entry]:s.localization.locales){(void)name;if(entry.fallback==tag)entry.fallback.clear();}if(s.localization.defaultLocale==tag)s.localization.defaultLocale=s.localization.locales.empty()?"en":s.localization.locales.begin()->first;ImGui::TreePop();ImGui::PopID();break;}ImGui::TreePop();}ImGui::PopID();}
        static char newTag[65]="";static std::string newCatalog;ImGui::InputText("New canonical BCP 47 locale",newTag,sizeof(newTag));asset("New locale catalog",newCatalog,AssetType::Catalog);if(ImGui::Button("Add locale")){auto candidate=s.localization;candidate.locales[newTag]={newCatalog,"",{}};if(s.localization.locales.empty())candidate.defaultLocale=newTag;std::string error;if(!s.localization.locales.count(newTag)&&state.assets&&ValidateLocalizationAssets(candidate,*state.assets,error)){s.localization=std::move(candidate);newTag[0]=0;newCatalog.clear();}else state.status=error.empty()?"Duplicate locale":error;}
        std::string localizationError;if(!s.localization.Validate(localizationError))ImGui::TextWrapped("Cannot save: %s",localizationError.c_str());
        ImGui::TextDisabled("Save project to persist choices; start fresh Play for configuration edits.");
        static std::string projectPath;static char config[16384];if(projectPath!=project.ProjectFile()){projectPath=project.ProjectFile();CopyToBuffer(s.localization.Encode(),config,sizeof(config));}
        ImGui::TextWrapped("Version, default locale, ordered fallback font count/IDs, locale count, then locale/catalog/fallback/font count/IDs. Catalogs and fonts use registered asset IDs.");
        ImGui::InputTextMultiline("Localization configuration",config,sizeof(config),{0,95});
        if(ImGui::Button("Apply localization settings")){ProjectLocalization c;std::string error;if(ProjectLocalization::Parse(config,c,error)&&state.assets&&ValidateLocalizationAssets(c,*state.assets,error)){s.localization=std::move(c);state.previewLocale=s.localization.defaultLocale;state.status="Applied localization; Save project";state.textReload=true;}else state.status=error;}
        for(auto& [locale,entry]:s.localization.locales)ImGui::BulletText("%s : %s",locale.c_str(),entry.catalog.c_str());
        ImGui::TextDisabled("Import .judasloc / .ttf / .otf in Asset Browser. UI source editor offers keys and real text preview.");
    }
    if(ImGui::CollapsingHeader("Tags and layers")){
        auto registry=[&](const char* label,CategoryRegistry& r,bool preserveDefault){
            ImGui::PushID(label);
            if(ImGui::TreeNode(label)){
                static char addName[128]="";ImGui::InputText("New name",addName,sizeof(addName));
                if(ImGui::Button("Create")){unsigned id;if(!r.Add(addName,id))state.status="Name exists/empty or all 64 lifetime IDs used";else addName[0]=0;}
                for(auto it=r.names.begin();it!=r.names.end();++it){ImGui::PushID(int(it->first));char renamed[256];CopyToBuffer(it->second,renamed,sizeof(renamed));
                    ImGui::Text("Stable ID %u",it->first);if(ImGui::InputText("Name",renamed,sizeof(renamed))&&!r.Rename(it->first,renamed))state.status="Invalid/duplicate category name";
                    if(!(preserveDefault&&it->first==0)&&ImGui::Button("Delete")){r.Remove(it->first,preserveDefault);ImGui::PopID();break;}
                    ImGui::PopID();
                }
                ImGui::TextWrapped("Deleted IDs are never reused. Repair assignments before Play/export. Save project to persist changes.");ImGui::TreePop();
            }
            ImGui::PopID();
        };
        registry("Tags",s.classification.tags,false);registry("Collision layers",s.classification.collision,true);registry("Render layers",s.classification.render,true);
    }
    if(ImGui::CollapsingHeader("Navigation profiles and areas")){
        auto& nav=s.navigation;static char areaName[128]="",profileName[128]="";
        ImGui::InputText("New navigation area",areaName,sizeof(areaName));if(ImGui::Button("Add area")&&nav.areas.nextId<62){unsigned id;if(nav.areas.Add(areaName,id))areaName[0]=0;}
        for(auto it=nav.areas.names.begin();it!=nav.areas.names.end();++it){ImGui::PushID(int(it->first));char name[256];CopyToBuffer(it->second,name,sizeof(name));if(ImGui::InputText("Area name",name,sizeof(name)))nav.areas.Rename(it->first,name);if(it->first&&ImGui::Button("Delete area")){nav.areas.Remove(it->first,true);ImGui::PopID();break;}ImGui::PopID();}
        ImGui::InputText("New agent profile",profileName,sizeof(profileName));if(ImGui::Button("Add profile")&&nav.nextProfile<64&&profileName[0]){NavigationProfile p;p.name=profileName;nav.profiles[nav.nextProfile++]=p;profileName[0]=0;}
        for(auto it=nav.profiles.begin();it!=nav.profiles.end();++it){auto& p=it->second;ImGui::PushID(int(it->first)+1000);char name[256];CopyToBuffer(p.name,name,sizeof(name));if(ImGui::InputText("Profile name",name,sizeof(name)))p.name=name;ImGui::DragFloat("Radius",&p.radius,.01f,.02f,10);ImGui::DragFloat("Height",&p.height,.05f,.1f,20);ImGui::DragFloat("Slope degrees",&p.slope,1,0,89);ImGui::DragFloat("Climb",&p.climb,.01f,0,p.height*.9f);if(it->first&&ImGui::Button("Delete profile")){nav.profiles.erase(it);ImGui::PopID();break;}ImGui::PopID();}
        ImGui::TextWrapped("Stable IDs are never reused. Up to 62 navigation areas and 64 lifetime profile IDs. Changing profiles/source geometry requires rebaking; Save project persists settings.");
    }
    if(ImGui::CollapsingHeader("Input actions, axes and vectors")){
        static char newName[128]="";static int newType=0;
        ImGui::InputText("New input name",newName,sizeof(newName));ImGui::Combo("Input type",&newType,"Action\0Scalar axis\0Paired vector\0");
        if(ImGui::Button("Create input")){if(!(newType==2?s.input.AddVector(newName):s.input.Add(newName,newType==1)))state.status="Input name is empty or already exists";else newName[0]=0;}
        ImGui::TextWrapped("Scalar controls: key:Space, mouse:Left, mouse:dx/dy/wheelX/wheelY, pad:South, stick:LeftX. Paired vectors use stick:Left or stick:Right. Circular processing preserves direction, then applies signed X/Y scales. Save project persists bindings.");
        for(std::size_t i=0;i<s.input.entries.size();++i){
            auto& entry=s.input.entries[i];ImGui::PushID(static_cast<int>(i));
            if(ImGui::TreeNode(entry.name.c_str(),"%s (%s)",entry.name.c_str(),entry.vector?"vector":entry.axis?"axis":"action")){
                char inputName[129];CopyToBuffer(entry.name,inputName,sizeof(inputName));
                if(ImGui::InputText("Rename",inputName,sizeof(inputName)))s.input.Rename(entry.name,inputName);
                if(ImGui::Button("Delete input")){const auto name=entry.name;s.input.Remove(name);ImGui::TreePop();ImGui::PopID();break;}
                for(std::size_t b=0;b<entry.bindings.size();++b){
                    ImGui::PushID(static_cast<int>(b));auto binding=entry.bindings[b];char control[129];CopyToBuffer(binding.control,control,sizeof(control));
                    bool changed=ImGui::InputText("Control",control,sizeof(control));binding.control=control;
                    changed|=ImGui::DragFloat(entry.vector?"X scale / inversion":"Scale",&binding.scale,.05f,-10000,10000);
                    if(entry.vector){changed|=ImGui::DragFloat("Y scale / inversion",&binding.scaleY,.05f,-10000,10000);changed|=ImGui::Checkbox("Circular deadzone",&binding.circular);}
                    if(binding.control.rfind("stick:",0)==0)changed|=ImGui::SliderFloat("Deadzone",&binding.deadzone,0,.99f);
                    if(changed&&!s.input.ReplaceBinding(entry.name,b,binding))state.status="Invalid input binding";
                    if(ImGui::Button("Remove binding")){s.input.RemoveBinding(entry.name,b);ImGui::PopID();break;}
                    ImGui::Separator();ImGui::PopID();
                }
                if(ImGui::Button("Add binding"))s.input.AddBinding(entry.name,entry.vector?InputBinding{"stick:Left",1,.15f,1,true}:InputBinding{"key:Space"});
                ImGui::TreePop();
            }ImGui::PopID();
        }
    }
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
    const auto display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({20, 76}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({std::min(850.0f, display.x - 40), std::min(700.0f, display.y - 110)}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(1.0f);
    if (!ImGui::Begin("Profiler", &state.showProfiler)) { ImGui::End(); return; }
    ImGui::Checkbox("Inspect live Play (F8: release cursor)", &state.profilerInspect);
    DrawIntegratedProfilerView();
    if(ImGui::CollapsingHeader("Specialized diagnostics (last observation, legacy timers)")){
    const ProfilerData& p = state.profiler;
    ImGui::Text("Frame: %.2f ms  (%.0f FPS, rolling)", p.frameMilliseconds, p.framesPerSecond);
    if (!p.playing) ImGui::TextDisabled("Edit mode: no simulation stepping.");
    ImGui::Text("Fixed steps this frame: %d", p.fixedStepsThisFrame);
    ImGui::Text("Last fixed step: %.3f ms (whole StepPlayedWorld)", p.fixedStepMilliseconds);
    ImGui::Separator();
    ImGui::Text("Physics bodies: %zu live (%zu dynamic)", p.physicsBodies, p.dynamicBodies);
    ImGui::Text("Contacts (last step's manifold points): %zu", p.contacts);
    ImGui::Text("Broadphase: %zu candidate pairs of %zu possible (%.3f%%)", p.physics.candidatePairs,
                p.physics.possiblePairs,
                p.physics.possiblePairs > 0 ? 100.0 * p.physics.candidatePairs / p.physics.possiblePairs : 0.0);
    ImGui::Text("  %zu colliding pairs, tree height %d, %zu proxy reinsertions", p.physics.collidingPairs,
                p.physics.treeHeight, p.physics.proxyReinsertions);
    ImGui::Text("  PhysicsWorld::Step %.3f ms (broadphase %.3f, narrowphase %.3f, solver %.3f)",
                p.physics.totalMilliseconds, p.physics.broadphaseMilliseconds, p.physics.narrowphaseMilliseconds,
                p.physics.solverMilliseconds);
    ImGui::Text("Entities: %zu full / %zu coarse / %zu dormant / %zu destroyed", p.entitiesFull, p.entitiesCoarse,
                p.entitiesDormant, p.entitiesDestroyed);
    ImGui::Separator();
    ImGui::Text("Draw calls: %u  (incl. shadow passes)", p.drawCalls);
    ImGui::Text("Triangles submitted: %u", p.triangles);
    ImGui::Text("Visibility (all passes): %u visible / %u culled meshes",p.meshesVisible,p.meshesCulled);
    ImGui::Text("Emitters: %u visible / %u culled; particles submitted: %u",p.emittersVisible,p.emittersCulled,p.particlesSubmitted);
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
    ImGui::TextDisabled("Specialized measurements above retain their original observation cadence; resource bytes are estimates, not driver VRAM.");
    }
    ImGui::End();
}

void DrawStatusBar(EditorDocument& doc, EditorPanelState& state) {
    PlaceWorkspacePanel(state.workspace.status);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 4));
    ImGui::Begin("##status", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
    const std::string message = std::string(doc.IsDirty() ? "Unsaved edits  |  " : "") + state.status;
    ImGui::TextUnformatted(message.c_str());
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\n%s", doc.Path().empty() ? "Unsaved scene" : doc.Path().c_str(), state.status.c_str());
    if (!state.runtimeInfo.empty() && state.workspace.status.size.x - ImGui::CalcTextSize(message.c_str()).x > 480) {
        ImGui::SameLine(); ImGui::TextDisabled("%s", state.runtimeInfo.c_str());
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

SceneObjectId CreateObjectOfKind(EditorDocument& doc, const std::string& kind, const glm::vec3& position,
                                 const std::string& meshAssetId, const SceneAnimationComponent* animation) {
    doc.BeginEdit();
    Scene& scene = doc.GetScene();
    SceneObject& o = scene.CreateObject(kind == "empty" ? "Empty" : kind == "box" ? "Box"
                                        : kind == "sphere" ? "Sphere" : kind == "dynamic-box" ? "Dynamic box"
                                        : kind == "dynamic-sphere" ? "Dynamic sphere" : kind == "mesh" ? "Mesh"
                                        : kind == "point-light" ? "Point light" : kind == "spot-light" ? "Spot light"
                                        : kind == "door" ? "Door" : kind == "gravity-region" ? "Gravity region"
                                        : kind == "player-start" ? "Player start" : kind == "render-camera" ? "Render camera" : kind == "audio-emitter" ? "Audio emitter" : kind == "audio-listener" ? "Audio listener" : "Object");
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
    } else if (kind == "render-camera") {
        o.renderCamera = SceneRenderCameraComponent{};
    } else if (kind == "audio-emitter") {
        o.audioEmitter=SceneAudioEmitterComponent{};
    } else if (kind == "audio-listener") {
        o.audioListener=SceneAudioListenerComponent{};
    } else if (kind == "player-start") {
        o.playerStart = ScenePlayerStartComponent{};
    }
    if(kind=="mesh"&&animation)o.animation=*animation;
    const SceneObjectId id = o.id;
    doc.CommitEdit();
    doc.Select(id);
    return id;
}

SceneObjectId DuplicateObject(EditorDocument& doc, SceneObjectId id) {
    if(doc.Selection().size()>1){std::string error;return doc.DuplicateSelection(error)?doc.Selected():kInvalidSceneObjectId;}
    Scene& scene = doc.GetScene();
    const SceneObject* source = scene.Find(id);
    if (!source) return kInvalidSceneObjectId;
    if(source->prefabRoot){
        if(source->prefabRoot!=id)return kInvalidSceneObjectId;
        const auto asset=source->prefabAsset;const auto placement=source->transform;
        Scene prefab;std::string error;
        if(!CreatePrefab(scene,id,prefab,error))return kInvalidSceneObjectId;
        auto ids=source->prefabIds;std::map<SceneObjectId,SceneObjectId> reverse;
        for(const auto& pair:ids)reverse[pair.second]=pair.first;
        for(auto& o:prefab.Objects()){
            o.id=reverse.at(o.id);if(o.parent)o.parent=reverse.at(o.parent);
            if(o.gravitySelection&&o.gravitySelection->source)o.gravitySelection->source=reverse.at(o.gravitySelection->source);
            if(o.render&&o.render->textureCamera)o.render->textureCamera=reverse.at(o.render->textureCamera);
        }
        prefab.SetNextId(1);doc.BeginEdit();SceneObjectId root=0;
        if(!InstantiatePrefab(scene,prefab,asset,placement,root,error)){doc.CancelEdit();return 0;}
        // The duplicate remains linked and carries only the same explicit overrides.
        for(auto& o:scene.Objects())if(o.prefabRoot==root){auto old=ids.find(o.prefabSource);
            if(old!=ids.end())o.prefabOverrides=scene.Find(old->second)->prefabOverrides;}
        doc.CommitEdit(false);doc.Select(root);return root;
    }
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

void DrawStreamingPanel(EditorDocument& doc,EditorPanelState& state,EditorRequests& requests){
    if(!state.project||state.project->Settings().worldManifest.empty())return;
    if(!ImGui::Begin("World regions")){ImGui::End();return;}
    auto* asset=state.assets?state.assets->Find(state.project->Settings().worldManifest):nullptr;WorldManifest manifest;std::string error;
    if(!asset||!LoadWorldManifest(asset->path,manifest,error)){ImGui::TextWrapped("%s",error.c_str());ImGui::End();return;}
    if(state.mode==EditorMode::Edit){
        ImGui::Checkbox("Read-only additive preview",&state.worldPreview);ImGui::SameLine();if(ImGui::Button("Refresh preview"))++state.worldPreviewRevision;
        ImGui::TextWrapped("Choose an edit target to open its original source. Save writes only that document; other preview regions are read-only.");
        for(auto& [id,r]:manifest.regions){ImGui::PushID(id.c_str());ImGui::Text("%s (%s) / priority %d",id.c_str(),r.policy.c_str(),r.priority);ImGui::SameLine();if(ImGui::Button("Edit source"))requests.openSceneRelative=r.scene;ImGui::PopID();}
    }else if(state.runtime&&state.runtime->SceneControl()){
        auto* stream=state.runtime->SceneControl()->Streaming(*state.runtime,error);
        if(stream){auto stats=stream->Stats();ImGui::Text("%zu active / %zu preparing",stats.active,stats.pending);ImGui::Text("pending %zu / live estimate %zu / retained %zu bytes",stats.pendingBytes,stats.liveBytes,stats.retainedBytes);ImGui::Text("cache %zu / %zu bytes; boundary %.3f ms; largest unit %.3f ms",stats.resourceResidentBytes,stats.resourceCacheBudget,stats.integrationMs,stats.largestUnitMs);
            static std::map<std::string,uint64_t> demand;for(auto& r:stream->Regions()){ImGui::PushID(r.id.c_str());ImGui::Text("%s: %s (%zu/%zu)",r.id.c_str(),r.state.c_str(),r.installed,r.entities);if(ImGui::Button("Request"))demand[r.id]=stream->Request(r.id,false,error);ImGui::SameLine();if(ImGui::Button("Release")){if(demand.count(r.id))stream->Release(demand[r.id]);demand.erase(r.id);stream->Unload(r.id);}for(auto& pin:r.pins)ImGui::BulletText("Pinned: %s",pin.c_str());if(!r.error.empty())ImGui::TextWrapped("%s",r.error.c_str());ImGui::PopID();}}
        if(!error.empty())ImGui::TextWrapped("%s",error.c_str());
    }
    (void)doc;ImGui::End();
}
