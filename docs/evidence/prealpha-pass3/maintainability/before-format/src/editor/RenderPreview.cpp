#include "RenderPreview.h"
#include "EditorPanels.h"
#include "ComponentEditors.h"
#include "RuntimeWorld.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <functional>

struct RuntimeRenderHistory {
    struct Action {std::function<bool(RuntimeWorld&)> undo,redo;ImGuiID gesture=0;int frame=0;};
    RuntimeWorld* world=nullptr;uint64_t document=0;
    std::vector<Action> undo,redo;
    SceneObjectId selected=0;std::string materialKey="*";
};

namespace {
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
} // namespace

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

