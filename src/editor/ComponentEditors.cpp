#include "../PowertrainAuthoring.h"
#include "SkeletonAuthoring.h"
#include <set>
#include "CollisionAsset.h"
#include "NavigationAsset.h"
#include "Prefab.h"
#include "RuntimeWorld.h"
#include "ComponentEditors.h"
#include "TerrainV2Inspector.h"
#include "ParticleEmitterEditor.h"
#include "DeformableEditor.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include "ModelLoader.h"
#include "SceneFingerprint.h"
#include "ScriptSystem.h"
#include "ResourceManager.h"

#include <glm/gtc/quaternion.hpp>

#include "imgui.h"
#include "imgui_internal.h"

#include "AssetDatabase.h"
#include "Project.h"
#include "EditorPanels.h"
#include "CollisionAsset.h"
#include "EditorWidgets.h"
#include "EditorPanelWidgets.h"
#include "EditorDesign.h"

bool DrawMaterialOverrideControls(MaterialOverride& candidate,const MaterialDefinition& base,EditorPanelState& state) {
    auto resolved=ApplyMaterialOverride(base,candidate);bool changed=false;
    auto colour=resolved.baseColor;if(ImGui::ColorEdit4("Base colour / opacity (linear)",&colour.x)){candidate.baseColor=colour;changed=true;}
    int alpha=int(resolved.alpha);if(ImGui::Combo("Alpha mode",&alpha,"Opaque\0Cutout\0Alpha blend\0")){candidate.alpha=MaterialAlpha(alpha);changed=true;}
    float cutoff=resolved.alphaCutoff;if(ImGui::SliderFloat("Cutout threshold",&cutoff,0,1)){candidate.alphaCutoff=cutoff;changed=true;}
    float rough=resolved.roughness;if(ImGui::SliderFloat("Roughness",&rough,0,1)){candidate.roughness=rough;changed=true;}
    float metal=resolved.metallic;if(ImGui::SliderFloat("Metallic",&metal,0,1)){candidate.metallic=metal;changed=true;}
    auto emission=resolved.emissive;if(ImGui::ColorEdit3("Emission (linear)",&emission.x,ImGuiColorEditFlags_HDR)){candidate.emissive=emission;changed=true;}
    float intensity=resolved.emissiveIntensity;if(ImGui::DragFloat("Emission intensity",&intensity,.05f,0,100000)){candidate.emissiveIntensity=intensity;changed=true;}
    float normal=resolved.normalStrength;if(ImGui::SliderFloat("Normal strength",&normal,0,8)){candidate.normalStrength=normal;changed=true;}
    float occlusion=resolved.occlusionStrength;if(ImGui::SliderFloat("Occlusion strength",&occlusion,0,1)){candidate.occlusionStrength=occlusion;changed=true;}
    bool sided=resolved.doubleSided;if(ImGui::Checkbox("Double sided",&sided)){candidate.doubleSided=sided;changed=true;}
    auto uvScale=resolved.uvScale;if(ImGui::DragFloat2("UV scale",&uvScale.x,.05f)){candidate.uvScale=uvScale;changed=true;}
    auto uvOffset=resolved.uvOffset;if(ImGui::DragFloat2("UV offset",&uvOffset.x,.01f)){candidate.uvOffset=uvOffset;changed=true;}
    const char* labels[]={"Base colour texture","Metallic / roughness texture","Normal texture","Occlusion texture","Emission texture"};
    for(size_t i=0;i<5;++i) {
        ImGui::PushID(int(i));auto& override=candidate.textures[i];const auto* asset=override&&state.assets?state.assets->Find(*override):nullptr;
        std::string label=!override?"Inherit source":override->empty()?"Remove texture":asset?asset->relativePath:*override;
        if(ImGui::BeginCombo(labels[i],label.c_str())) {
            if(ImGui::Selectable("Inherit source",!override)){override.reset();changed=true;}
            if(ImGui::Selectable("Remove texture",override&&override->empty())){override=std::string{};changed=true;}
            if(state.assets)for(const auto& [id,record]:state.assets->Records())if(record.type==AssetType::Texture&&!record.missing&&ImGui::Selectable(record.relativePath.c_str(),override&&*override==id)){override=id;changed=true;}
            ImGui::EndCombo();
        }
        if(override&&!override->empty()&&state.resources) {
            auto resourceState=state.resources->StateOf(*override);ImGui::TextDisabled("%s",ResourceStateName(resourceState));
            if(resourceState==ResourceState::Failed)ImGui::TextWrapped("%s",state.resources->ErrorOf(*override).c_str());
        }
        ImGui::PopID();
    }
    if(!MaterialOverrideEmpty(candidate))ImGui::TextColored(ImVec4(1,.8f,.3f,1),"INSTANCE OVERRIDE (source asset unchanged)");
    if(ImGui::Button("Clear parameter and texture overrides")){candidate={};changed=true;}
    ImGui::TextWrapped("Opaque ignores opacity; cutout tests colour × texture alpha; alpha blend uses opacity. The camera screen uses cheaper rendering.");
    return changed;
}


// The inspector's asset fields: a combo over the project's assets of one
// type (name shown, id stored), a drop target for the Asset Browser's
// drag payload, full identity on hover, and explicit missing-file status.
void AssetField(EditorDocument& doc, const char* label, std::string& assetId, AssetType type, bool allowNone,
                EditorPanelState& state) {
    std::vector<std::string> labels, values;
    if (state.assets) {
        for (const auto& [id, record] : state.assets->Records()) {
            if (record.type != type) continue;
            labels.push_back(record.relativePath + (record.missing ? "  [missing]" : ""));
            values.push_back(id);
        }
    }
    // A path needs the whole inspector row; keep its label above the selector.
    // Override the hidden control's seed so it retains the original field ID.
    // Popup state and existing automation still resolve the same asset field.
    ImGui::TextWrapped("%s",label);
    ImGui::SetNextItemWidth(-1);
    ImGui::PushOverrideID(ImGui::GetID(label));
    LabelledCombo(doc, "", assetId, labels, values, allowNone);
    ImGui::PopID();
    if(ImGui::BeginItemTooltip()){
        ImGui::PushTextWrapPos(ImGui::GetFontSize()*35);
        const AssetRecord* selected=state.assets?state.assets->Find(assetId):nullptr;
        if(selected)ImGui::TextUnformatted(selected->relativePath.c_str());
        if(!assetId.empty())ImGui::Text("Stored asset ID: %s",assetId.c_str());
        ImGui::Text("Select a %s asset or drop one from Assets.",AssetTypeName(type));
        ImGui::PopTextWrapPos();ImGui::EndTooltip();
    }
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kAssetDragPayload)) {
            const std::string droppedId(static_cast<const char*>(payload->Data), payload->DataSize);
            const AssetRecord* record = state.assets ? state.assets->Find(droppedId) : nullptr;
            if (record && record->type == type) {
                doc.BeginEdit();
                assetId = droppedId;
                doc.CommitEdit();
            } else {
                state.status = std::string("Dropped asset is not a ") + AssetTypeName(type);
            }
        }
        ImGui::EndDragDropTarget();
    }
    if (assetId.empty()) return;
    const AssetRecord* record = state.assets ? state.assets->Find(assetId) : nullptr;
    ImGui::PushTextWrapPos(0);
    if (!record) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "  unknown asset id %s", assetId.c_str());
    } else if (record->missing) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "  file missing: %s", record->relativePath.c_str());
    }
    ImGui::PopTextWrapPos();
}

namespace {
const char* const kShapeNames[] = {"box", "sphere", "compound", "mesh", "terrain", "convex hull", "static triangle mesh"};
const char* const kBodyShapeNames[] = {"box", "sphere", "compound", "(mesh: not a body shape)", "terrain", "convex hull", "static triangle mesh"};
const char* const kMotionNames[] = {"static", "dynamic", "kinematic"};
const char* const kGravityKindNames[] = {"radial", "uniform"};
const char* const kRegionNames[] = {"sphere", "box"};
const char* const kLightKindNames[] = {"point", "spot"};
const char* const kVehicleGravityNames[] = {"local", "celestial"};
const char* const kViewNames[] = {"third-person", "first-person"};


void DrawUIComponent(EditorDocument& doc,SceneObject& o,EditorPanelState& state){
    auto& u=*o.ui;AssetField(doc,"UI document",u.asset,AssetType::UI,false,state);Checkbox(doc,"Enabled",u.enabled);
    TextField(doc,"Runtime document name",u.name);
    ImGui::TextWrapped("Select the UI asset in the Asset Browser to edit its hierarchy. Runtime changes never edit this source.");
}
void DrawWheel(EditorDocument& doc,SceneObject& object,EditorPanelState& state){
    auto& v=*object.wheel;Checkbox(doc,"Wheel enabled",v.enabled);
    const auto* chassis=doc.GetScene().Find(v.chassis);
    if(ImGui::BeginCombo("Chassis",chassis?chassis->name.c_str():"Select dynamic body")){
        for(const auto& candidate:doc.GetScene().Objects())if(((candidate.body&&candidate.body->motion==SceneBodyMotion::Dynamic)||candidate.deformable)&&candidate.id!=object.id&&ImGui::Selectable((candidate.name+" ##"+std::to_string(candidate.id)).c_str(),candidate.id==v.chassis)){
            doc.BeginEdit();v.chassis=candidate.id;doc.CommitEdit();}
        ImGui::EndCombo();
    }
    TextField(doc,"Rigid partition part key (optional)",v.part);
    DragVec3(doc,"Mount (chassis/part local)",v.mount);
    auto degrees=glm::degrees(glm::eulerAngles(glm::normalize(v.orientation)));
    if(ImGui::DragFloat3("Frame rotation degrees",&degrees.x,.5f))v.orientation=glm::normalize(glm::quat(glm::radians(degrees)));
    TrackEdit(doc);
    DragScalar(doc,"Wheel radius m",v.radius,.01f);
    DragScalar(doc,"Tread width m",v.width,.01f);
    DragScalar(doc,"Minimum suspension m",v.minLength,.01f);DragScalar(doc,"Rest suspension m",v.restLength,.01f);DragScalar(doc,"Maximum suspension m",v.maxLength,.01f);
    DragScalar(doc,"Spring N/m",v.springStiffness,100);DragScalar(doc,"Damping N s/m",v.damping,20);
    DragScalar(doc,"Spin inertia kg m2",v.spinInertia,.1f);DragScalar(doc,"Tyre friction",v.friction,.05f);DragScalar(doc,"Rolling resistance",v.rollingResistance,.001f);
    if(state.project){const auto& categories=state.project->Settings().classification;
        DrawCategoryLayer(doc,"Wheel collision layer",v.collisionLayer,categories.collision);DrawCategoryMask(doc,"Wheel collision mask",v.collisionMask,categories.collision);
        DrawCategoryMask(doc,"Wheel required tags",v.requiredTags,categories.tags,false);DrawCategoryMask(doc,"Wheel excluded tags",v.excludedTags,categories.tags,false);}
    ImGui::TextWrapped("Local frame: suspension -Y, axle +X, rolling forward -Z. Mount and frame are relative to the chassis; no world-up assumption. This entity presents the wheel only. fixedUpdate supplies torque, brake and steering.");
    if(ImGui::TreeNode("Tyre response and rebound")) {
        DragScalar(doc,"Rebound damping multiplier",v.reboundRatio,.05f);
        DragScalar(doc,"Longitudinal slip stiffness",v.longitudinalStiffness,.25f);
        DragScalar(doc,"Lateral slip stiffness",v.lateralStiffness,.25f);
        ImGui::TextWrapped("Load-scaled running tyre stiffness; low-speed contact uses static grip. Higher values resist slip more strongly. Grip still shares a combined friction limit.");
        ImGui::TreePop();
    }
    if(state.runtime)if(const auto* live=state.runtime->RuntimeWheel(object.id)) {
        const auto& current=live->state;
        ImGui::SeparatorText("Live wheel");
        ImGui::TextUnformatted(current.sleeping?"Sleeping":current.contact?"Ground contact":"No support");
        const float compression=(v.restLength-current.length)/std::max(1e-4f,v.restLength-v.minLength);
        ImGui::ProgressBar(std::clamp(compression,0.f,1.f),ImVec2(-1,0),"Suspension compression");
        ImGui::Text("Travel %.3f m | Support %.0f N",current.length,current.normalForce);
        ImGui::Text("Spin %.1f rpm | Grip %.0f%%",current.angularVelocity*9.5492966f,current.gripUtilization*100);
        ImGui::Text("Slip ratio %.3f | Lateral angle %.1f deg",current.slipRatio,glm::degrees(current.slipAngle));
        ImGui::Text("Tyre impulse %.3f / %.3f N s",current.longitudinalImpulse,current.lateralImpulse);
        if(!current.diagnostic.empty())ImGui::TextWrapped("%s",current.diagnostic.c_str());
    }
    std::string error;if(!ValidateWheelSettings(v,error))ImGui::TextColored(ImVec4(1,.3f,.2f,1),"%s",error.c_str());
}
void PowertrainNumber(EditorDocument& doc,const char* label,double& value){double next=value;if(ImGui::InputDouble(label,&next,0,0,"%.8g")&&std::isfinite(next)){doc.BeginEdit();value=next;doc.CommitEdit();}}
void DrawPowertrain(EditorDocument& doc,SceneObject& object,EditorPanelState& state){
    auto& train=*object.powertrain;Checkbox(doc,"Mechanical assembly enabled",train.enabled);
    auto entity=[&](const char* label,uint64_t& id,bool wheel,bool allowNone){const auto* target=doc.GetScene().Find(id);if(ImGui::BeginCombo(label,target?target->name.c_str():"None / missing")){if(allowNone&&ImGui::Selectable("World / no bearing",id==0)){doc.BeginEdit();id=0;doc.CommitEdit();}for(const auto& candidate:doc.GetScene().Objects())if((wheel?bool(candidate.wheel):bool(candidate.body))&&ImGui::Selectable((candidate.name+" ##"+std::to_string(candidate.id)).c_str(),candidate.id==id)){doc.BeginEdit();id=candidate.id;doc.CommitEdit();}ImGui::EndCombo();}};
    auto shaft=[&](const char* label,unsigned& index){const char* preview=index<train.shafts.size()?train.shafts[index].name.c_str():"Missing shaft";if(ImGui::BeginCombo(label,preview)){for(unsigned i=0;i<train.shafts.size();++i)if(ImGui::Selectable((train.shafts[i].name+" ##"+std::to_string(i)).c_str(),i==index)){doc.BeginEdit();index=i;doc.CommitEdit();}ImGui::EndCombo();}};
    const char* shaftKinds[]={"Internal rotor","Wheel terminal","Body rotor"};
    for(size_t i=0;i<train.shafts.size();++i){ImGui::PushID(int(i));auto& v=train.shafts[i];if(ImGui::TreeNode("shaft", "%zu: %s",i,v.name.c_str())){
        TextField(doc,"Shaft name",v.name);int representation=int(v.kind);if(EnumCombo("Shaft representation",representation,shaftKinds,3)){doc.BeginEdit();v.kind=PowertrainShaftKind(representation);if(v.kind==PowertrainShaftKind::Internal)v.target=0;if(v.kind==PowertrainShaftKind::Wheel)v.bearing=0;doc.CommitEdit();}
        if(v.kind==PowertrainShaftKind::Internal){PowertrainNumber(doc,"Rotor inertia kg m2",v.inertia);PowertrainNumber(doc,"Initial speed rad/s",v.initialAngularVelocity);}
        else entity("Physical terminal",v.target,v.kind==PowertrainShaftKind::Wheel,false);
        PowertrainNumber(doc,"Shaft viscous drag N m s",v.viscousDrag);
        if(v.kind!=PowertrainShaftKind::Wheel){entity("Reaction bearing",v.bearing,false,true);DragVec3(doc,"Rotation axis (bearing local / world)",v.axis);}else ImGui::TextWrapped("Wheel axle, inertia and chassis reaction come from the selected M81 component.");
        if(ImGui::Button("Remove shaft and incident elements")){doc.BeginEdit();const auto affected=[&](unsigned n){return n==i;};train.sources.erase(std::remove_if(train.sources.begin(),train.sources.end(),[&](const auto& x){return affected(x.shaft);}),train.sources.end());train.brakes.erase(std::remove_if(train.brakes.begin(),train.brakes.end(),[&](const auto& x){return affected(x.shaft);}),train.brakes.end());train.links.erase(std::remove_if(train.links.begin(),train.links.end(),[&](const auto& x){return affected(x.input)||affected(x.output)||(x.kind==PowertrainLinkKind::Differential&&affected(x.outputB));}),train.links.end());for(auto& x:train.sources)if(x.shaft>i)--x.shaft;for(auto& x:train.brakes)if(x.shaft>i)--x.shaft;for(auto& x:train.links){if(x.input>i)--x.input;if(x.output>i)--x.output;if(x.outputB>i)--x.outputB;}train.shafts.erase(train.shafts.begin()+i);doc.CommitEdit();ImGui::TreePop();ImGui::PopID();break;}
        ImGui::TreePop();}ImGui::PopID();}
    if(train.shafts.size()<32&&ImGui::Button("Add internal shaft")){doc.BeginEdit();PowertrainShaft v;v.name="shaft_"+std::to_string(train.shafts.size());train.shafts.push_back(v);doc.CommitEdit();}
    const char* sourceKinds[]={"Combustion curve","Electric torque / power","Custom script torque"};
    for(size_t i=0;i<train.sources.size();++i){ImGui::PushID(100+int(i));auto& v=train.sources[i];if(ImGui::TreeNode("source","Source: %s",v.name.c_str())){TextField(doc,"Source name",v.name);shaft("Driven shaft",v.shaft);Combo(doc,"Power source",v.kind,sourceKinds,3);PowertrainNumber(doc,"Maximum torque N m",v.maxTorque);PowertrainNumber(doc,"Maximum power W",v.maxPower);PowertrainNumber(doc,"Starter torque N m",v.starterTorque);
        for(size_t p=0;p<v.torqueCurve.size();++p){ImGui::PushID(int(p));PowertrainNumber(doc,"Curve RPM",v.torqueCurve[p].rpm);PowertrainNumber(doc,"Curve torque N m",v.torqueCurve[p].torque);if(ImGui::Button("Remove curve point")){doc.BeginEdit();v.torqueCurve.erase(v.torqueCurve.begin()+p);doc.CommitEdit();ImGui::PopID();break;}ImGui::PopID();}if(v.torqueCurve.size()<64&&ImGui::Button("Add torque point")){doc.BeginEdit();v.torqueCurve.push_back({v.torqueCurve.empty()?0:v.torqueCurve.back().rpm+1000,100});doc.CommitEdit();}
        if(ImGui::Button("Remove source")){doc.BeginEdit();train.sources.erase(train.sources.begin()+i);doc.CommitEdit();ImGui::TreePop();ImGui::PopID();break;}ImGui::TreePop();}ImGui::PopID();}
    if(!train.shafts.empty()&&train.sources.size()<32&&ImGui::Button("Add custom source")){doc.BeginEdit();PowertrainSource v;v.name="source_"+std::to_string(train.sources.size());train.sources.push_back(v);doc.CommitEdit();}
    const char* linkKinds[]={"Ideal gear","Torque-limited clutch","Open differential"};
    for(size_t i=0;i<train.links.size();++i){ImGui::PushID(200+int(i));auto& v=train.links[i];if(ImGui::TreeNode("link","Coupling: %s",v.name.c_str())){TextField(doc,"Coupling name",v.name);Combo(doc,"Coupling mechanism",v.kind,linkKinds,3);shaft("Input shaft",v.input);shaft("Output shaft",v.output);if(v.kind==PowertrainLinkKind::Differential)shaft("Second output shaft",v.outputB);PowertrainNumber(doc,"Output / input speed ratio",v.ratio);if(v.kind==PowertrainLinkKind::Clutch)PowertrainNumber(doc,"Clutch output torque capacity N m",v.maxTorque);if(ImGui::TreeNode("Physical connection prerequisite (optional)")){
            auto& connection=v.connection;
            const auto* owner=doc.GetScene().Find(connection.owner);
            if(ImGui::BeginCombo("Material owner",owner?owner->name.c_str():"None (M82 independent link)")){
                if(ImGui::Selectable("None",connection.owner==0)){doc.BeginEdit();connection={};doc.CommitEdit();}
                for(const auto& candidate:doc.GetScene().Objects())if(candidate.deformable&&ImGui::Selectable((candidate.name+" ##material"+std::to_string(candidate.id)).c_str(),candidate.id==connection.owner)){
                    doc.BeginEdit();connection={};connection.owner=candidate.id;doc.CommitEdit();
                }
                ImGui::EndCombo();
            }
            if(connection.owner){
                owner=doc.GetScene().Find(connection.owner);
                Checkbox(doc,"Use material groups (no attachment constraint)",connection.groups);
                auto group=[&](const char* label,std::string& key,bool optional){
                    if(ImGui::BeginCombo(label,key.empty()?(optional?"None (attachment only)":"Select attachment"):key.c_str())){
                        if(optional&&ImGui::Selectable("None",key.empty())){doc.BeginEdit();key.clear();doc.CommitEdit();}
                        if(connection.groups)TextField(doc,label,key);
                        if(!connection.groups&&owner&&owner->deformable)for(const auto& attachment:owner->deformable->attachments)if(ImGui::Selectable(attachment.group.c_str(),attachment.group==key)){doc.BeginEdit();key=attachment.group;doc.CommitEdit();}
                        ImGui::EndCombo();
                    }
                };
                group("Physical terminal attachment",connection.attachment,false);
                group("Material path to root attachment",connection.rootAttachment,true);
                ImGui::TextWrapped("Released, disabled or missing attachments disconnect this link. A root also requires an intact material path. Commands do not heal damage.");
            }
            ImGui::TreePop();
        }
        if(ImGui::Button("Remove coupling")){doc.BeginEdit();train.links.erase(train.links.begin()+i);doc.CommitEdit();ImGui::TreePop();ImGui::PopID();break;}ImGui::TreePop();}ImGui::PopID();}
    if(train.shafts.size()>=2&&train.links.size()<64&&ImGui::Button("Add gear / coupling")){doc.BeginEdit();PowertrainLink v;v.name="coupling_"+std::to_string(train.links.size());train.links.push_back(v);doc.CommitEdit();}
    for(size_t i=0;i<train.brakes.size();++i){ImGui::PushID(300+int(i));auto& v=train.brakes[i];if(ImGui::TreeNode("brake","Brake: %s",v.name.c_str())){TextField(doc,"Brake name",v.name);shaft("Braked shaft",v.shaft);PowertrainNumber(doc,"Brake torque capacity N m",v.maxTorque);if(ImGui::Button("Remove brake")){doc.BeginEdit();train.brakes.erase(train.brakes.begin()+i);doc.CommitEdit();ImGui::TreePop();ImGui::PopID();break;}ImGui::TreePop();}ImGui::PopID();}
    if(!train.shafts.empty()&&train.brakes.size()<32&&ImGui::Button("Add mechanical brake")){doc.BeginEdit();PowertrainBrake v;v.name="brake_"+std::to_string(train.brakes.size());train.brakes.push_back(v);doc.CommitEdit();}
    ImGui::TextWrapped("Mechanical graph only: fixedUpdate supplies source inputs, clutch engagement, ratio selection and brakes. Internal/body axes are bearing-local, or world-space without a bearing. An open differential constrains average output speed; it does not force equal wheel speed.");
    std::string error;if(!ValidatePowertrainSettings(train,error))ImGui::TextColored(ImVec4(1,.3f,.2f,1),"%s",error.c_str());
    if(state.runtime)if(const auto* runtime=state.runtime->RuntimePowertrain(object.id)){ImGui::Text("Mechanical solve: %s | residual %.5g",runtime->state.converged?"converged":"failed",runtime->state.residual);for(size_t i=0;i<runtime->state.shafts.size();++i)ImGui::Text("%s: %.1f RPM",runtime->settings.shafts[i].name.c_str(),runtime->state.shafts[i].angularVelocity*60/(2*3.141592653589793));}
}
void DrawCharacter(EditorDocument &doc, SceneObject &object, EditorPanelState &state) {
    auto &m = *object.characterMotor;
    Checkbox(doc, "Motor enabled", m.enabled);
    DragScalar(doc, "Radius", m.radius, .01f);
    ImGui::SetItemTooltip("Width of the character's collision capsule, in metres.");
    DragScalar(doc, "Cylinder half height", m.halfHeight, .01f);
    ImGui::SetItemTooltip(
        "Half the straight middle section; total height is twice this value plus twice the radius.");
    DragScalar(doc, "Step height", m.stepHeight, .01f);
    DragScalar(doc, "Maximum slope degrees", m.maxSlopeDegrees, 1);
    ImGui::TextWrapped("Walking speed, jumping and input are set by the character's script.");
    if (InspectorDisclosure("Advanced character settings")) {
        DragScalar(doc, "Support probe", m.supportDistance, .01f);
        DragScalar(doc, "Skin", m.skin, .001f);
        DragScalar(doc, "Gravity multiplier", m.gravityScale, .1f);
        DragScalar(doc, "Reorientation degrees/sec", m.reorientationDegreesPerSecond, 1);
        DragScalar(doc, "Interaction mass kg", m.interactionMass, 1);
        DragScalar(doc, "Max push impulse", m.maxPushImpulse, 1);
        DragVec3(doc, "Shape offset", m.offset);
        if (state.project) {
            const auto &categories = state.project->Settings().classification;
            DrawCategoryLayer(doc, "Collision layer", m.collisionLayer, categories.collision);
            DrawCategoryMask(doc, "Collision mask", m.collisionMask, categories.collision);
            DrawCategoryMask(doc, "Required collider tags", m.requiredTags, categories.tags, false);
            DrawCategoryMask(doc, "Excluded collider tags", m.excludedTags, categories.tags, false);
        }
    }
}

std::string ScriptPropertyLabel(const std::string &key) {
    std::string result;
    for (size_t i = 0; i < key.size(); ++i) {
        const auto c = static_cast<unsigned char>(key[i]);
        const bool separator = c == '_' || c == '-';
        const bool boundary = i && std::isupper(c) &&
                              (std::islower(static_cast<unsigned char>(key[i - 1])) ||
                               (i + 1 < key.size() && std::isupper(static_cast<unsigned char>(key[i - 1])) &&
                                std::islower(static_cast<unsigned char>(key[i + 1]))));
        if ((separator || boundary) && !result.empty() && result.back() != ' ')
            result += ' ';
        if (!separator)
            result += char(c);
    }
    if (!result.empty())
        result[0] = char(std::toupper(static_cast<unsigned char>(result[0])));
    return result;
}

void DrawScripts(EditorDocument &doc, SceneObject &o, EditorPanelState &state) {
    size_t remove = o.scripts.size();
    for (size_t i = 0; i < o.scripts.size(); ++i) {
        auto &slot = o.scripts[i];
        ImGui::PushID(static_cast<int>(i));
        const auto *record = state.assets ? state.assets->Find(slot.asset) : nullptr;
        if (record && !record->missing) {
            const auto name = std::filesystem::u8path(record->relativePath).filename().u8string();
            ImGui::TextWrapped("%s", name.c_str());
            ImGui::SetItemTooltip("%s", record->relativePath.c_str());
        } else if (slot.asset.empty()) {
            ImGui::TextDisabled("Choose a script for this slot");
            AssetField(doc, "Script asset", slot.asset, AssetType::Script, false, state);
        } else if (state.assets) {
            ImGui::TextWrapped("Script is missing. Open Script options to choose a replacement.");
        } else {
            ImGui::TextWrapped("Script attached. Open its project to inspect the source.");
        }
        Checkbox(doc, "Enabled", slot.enabled);
        EditorPanelWidgets::NextActionOnRow("Edit script");
        ImGui::BeginDisabled(slot.asset.empty());
        if (EditorDesign::ActionButton("Edit script", EditorDesign::Tone::Secondary))
            state.editScriptAsset = slot.asset;
        ImGui::EndDisabled();
        if (record && !record->missing) {
            struct Metadata {
                std::filesystem::file_time_type time;
                std::string schema, error;
            };
            static std::map<std::string, Metadata> cache;
            std::error_code ec;
            const auto stamp = std::filesystem::last_write_time(record->path, ec);
            auto it = cache.find(record->path);
            if (it == cache.end() || it->second.time != stamp) {
                Metadata metadata;
                metadata.time = stamp;
                ScriptSystem::Inspect(*state.assets, slot.asset, metadata.schema, metadata.error);
                it = cache.insert_or_assign(record->path, std::move(metadata)).first;
            }
            auto &metadata = it->second;
            std::vector<ScriptProperty> fields;
            std::string error = metadata.error;
            if (error.empty() &&
                ScriptSystem::ReadProperties(metadata.schema, slot.properties, fields, error)) {
                const auto settingsLabel = "Settings (" + std::to_string(fields.size()) + ")";
                if (!fields.empty() &&
                    InspectorDisclosure(settingsLabel.c_str(), false, ImGui::GetID("Script settings"))) {
                    for (auto &f : fields) {
                        const auto fieldId = ImGui::GetID(f.name.c_str());
                        const auto label = ScriptPropertyLabel(f.name);
                        ImGui::PushTextWrapPos(0);
                        ImGui::TextUnformatted(label.c_str());
                        ImGui::PopTextWrapPos();
                        ImGui::SetItemTooltip("Script property: %s", f.name.c_str());
                        ImGui::SetNextItemWidth(-1);
                        ImGui::PushOverrideID(fieldId);
                        bool changed = false;
                        if (f.type == "boolean") {
                            changed = EditorDesign::Toggle("", &f.boolean);
                            ImGui::SameLine();
                            ImGui::TextUnformatted(f.boolean ? "On" : "Off");
                        } else if (f.type == "number") {
                            EditorDesign::FieldFrame field;
                            changed = ImGui::InputDouble("", &f.number);
                        } else if (f.type == "entity") {
                            SceneObjectId id = 0;
                            try {
                                id = std::stoull(f.text);
                            } catch (...) {
                            }
                            const auto *target = doc.GetScene().Find(id);
                            bool open;
                            {
                                EditorDesign::FieldFrame field;
                                open =
                                    ImGui::BeginCombo("", target ? target->name.c_str() : "None / missing");
                            }
                            if (open) {
                                if (ImGui::Selectable("None", id == 0)) {
                                    f.text = "0";
                                    changed = true;
                                }
                                for (const auto &candidate : doc.GetScene().Objects())
                                    if (ImGui::Selectable(
                                            (candidate.name + " ##" + std::to_string(candidate.id)).c_str(),
                                            candidate.id == id)) {
                                        f.text = std::to_string(candidate.id);
                                        changed = true;
                                    }
                                ImGui::EndCombo();
                            }
                        } else {
                            char value[1024];
                            std::snprintf(value, sizeof(value), "%s", f.text.c_str());
                            EditorDesign::FieldFrame field;
                            changed = ImGui::InputText("", value, sizeof(value));
                            if (changed)
                                f.text = value;
                        }
                        ImGui::PopID();
                        if (changed) {
                            doc.BeginEdit();
                            slot.properties = ScriptSystem::WriteProperties(fields);
                            doc.CommitEdit();
                        }
                    }
                }
            }
            if (!error.empty())
                ImGui::TextWrapped("Script metadata: %s", error.c_str());
        }
        if (InspectorDisclosure("Script options")) {
            if (!slot.asset.empty())
                AssetField(doc, "Script asset", slot.asset, AssetType::Script, false, state);
            if (EditorDesign::ActionButton("Remove behaviour", EditorDesign::Tone::Danger))
                remove = i;
        }
        if (i + 1 < o.scripts.size())
            ImGui::Separator();
        ImGui::PopID();
    }
    if (remove < o.scripts.size()) {
        doc.BeginEdit();
        o.scripts.erase(o.scripts.begin() + remove);
        doc.CommitEdit();
    }
    if (InspectorDisclosure("Advanced script slots")) {
        if (ImGui::Button("Add empty slot")) {
            doc.BeginEdit();
            uint64_t id = 1;
            for (const auto &slot : o.scripts)
                id = std::max(id, slot.id + 1);
            o.scripts.push_back({id, "", true, "{}"});
            doc.CommitEdit();
        }
    }
}


void DrawAudioEmitter(EditorDocument& doc,SceneObject& o,EditorPanelState& state){
    auto& a=*o.audioEmitter;
    AssetField(doc,"Audio clip",a.asset,AssetType::Audio,true,state);
    Checkbox(doc,"Enabled",a.enabled);Checkbox(doc,"Play on start",a.playOnStart);
    const char* const policies[]={"Buffered (short effects)","Streamed (bounded PCM)"};Combo(doc,"Loading policy",a.loading,policies,2);
    if(a.loading==AudioLoading::Streamed){int frames=int(a.streamPageFrames);bool changed=ImGui::InputInt("Stream page frames (4 pages)",&frames);if(ImGui::IsItemActivated())doc.BeginEdit();if(changed)a.streamPageFrames=unsigned(std::clamp(frames,1024,16384));if(ImGui::IsItemDeactivatedAfterEdit())doc.CommitEdit();else if(ImGui::IsItemDeactivated())doc.CancelEdit();}
    TextField(doc,"Sound group (empty = master)",a.group);
    Checkbox(doc,"Effect bypass",a.bypass);DragScalar(doc,"Environment send",a.send,.01f,0,1);
    Checkbox(doc,"Loop",a.loop);Checkbox(doc,"Spatial (3D)",a.spatial);
    DragScalar(doc,"Volume",a.volume,.01f,0,1);DragScalar(doc,"Pitch",a.pitch,.01f,.125f,8);
    if(a.spatial){
        const char* const models[]={"None","Inverse distance","Linear distance"};
        Combo(doc,"Attenuation",a.attenuation,models,3);
        DragScalar(doc,"Reference distance",a.referenceDistance,.1f,.001f,a.maximumDistance-.001f);
        DragScalar(doc,"Maximum distance",a.maximumDistance,.1f,a.referenceDistance+.001f,100000);
        DragScalar(doc,"Rolloff",a.rolloff,.01f,0,100);
        DragScalar(doc,"Doppler scale (0 = off)",a.doppler,.01f,0,4);
        Checkbox(doc,"Geometry obstruction",a.occlusion);
        if(a.occlusion){if(state.project)DrawCategoryMask(doc,"Sound obstruction layers",a.occlusionMask,state.project->Settings().classification.collision);DragScalar(doc,"Obstructed gain",a.occludedGain,.01f,0,1);DragScalar(doc,"Obstructed cutoff Hz",a.occludedCutoff,10,40,24000);}

    }
}
void DrawAudioZone(EditorDocument& doc,SceneObject& o,EditorPanelState& state){
 auto& z=*o.audioZone;AssetField(doc,"Reverb settings",z.asset,AssetType::AudioEffect,true,state);Checkbox(doc,"Enabled",z.enabled);
 const char* const shapes[]={"Sphere","Box"};Combo(doc,"Zone shape",z.shape,shapes,2);
 if(z.shape==SceneRegionShape::Box)DragVec3(doc,"Half extents",z.halfExtents,.1f);else DragScalar(doc,"Radius",z.radius,.1f,.01f,10000);
 DragScalar(doc,"Blend distance",z.blendDistance,.1f,0,1000);DragScalar(doc,"Amount",z.amount,.01f,0,1);
 DragInt(doc,"Zone priority",z.priority,-100000,100000);
 ImGui::TextWrapped("Listener-weighted reverb, independent of gravity and render visibility. Equal priorities blend; higher priority wins. Use the entity pose for orientation.");
}
void DrawAudioListener(EditorDocument& doc,SceneObject& o,EditorPanelState&){
    auto& l=*o.audioListener;Checkbox(doc,"Enabled",l.enabled);Checkbox(doc,"Follow active camera view",l.followActiveView);
    ImGui::TextDisabled("One enabled listener per scene. Local -Z forward / +Y up.");
}

void DrawRenderCamera(EditorDocument& doc, SceneObject& o, EditorPanelState& state) {
    auto& c = *o.renderCamera;
    if(state.project)DrawCategoryMask(doc,"Render mask",c.renderMask,state.project->Settings().classification.render);
    Checkbox(doc, "Enabled", c.enabled);
    DragInt(doc, "Target width", c.width, 1, 4096);
    DragInt(doc, "Target height", c.height, 1, 4096);
    DragInt(doc, "Every N rendered frames", c.updateEveryFrames, 1, 100000);
    int projection=int(c.lens.projection);
    if(ImGui::Combo("Projection",&projection,"Perspective\0Orthographic (2D)\0")){doc.BeginEdit();c.lens.projection=CameraProjectionKind(projection);if(projection==0)c.lens.pixelPerfect=false;doc.CommitEdit();}
    if(projection==0)DragScalar(doc, "Vertical FOV (deg)", c.verticalFovDegrees, 0.1f, 1.0f, 178.0f);
    else {
        DragScalar(doc,"Visible height (world units)",c.lens.height,.1f,.01f,1e8f);
        Checkbox(doc,"Pixel-perfect canvas",c.lens.pixelPerfect);
        if(c.lens.pixelPerfect){DragInt(doc,"Canvas width",c.lens.referenceWidth,1,4096);DragInt(doc,"Canvas height",c.lens.referenceHeight,1,4096);DragScalar(doc,"Pixels per world unit",c.lens.pixelsPerUnit,.1f,.01f,65536);ImGui::TextWrapped("Canvas / pixels per unit sets the visible size. Integer scaling adds letterboxing; smaller windows downscale.");}
    }
    DragScalar(doc, "Near plane", c.nearPlane, 0.01f, 0.001f, 100000.0f);
    DragScalar(doc, "Far plane", c.farPlane, 1.0f, 0.002f, 1000000.0f);
    ImGui::TextDisabled("Local -Z forward / +Y up. Live output in Play.");
}

void DrawRender(EditorDocument& doc, SceneObject& o, EditorPanelState& state) {
    SceneRenderComponent& r = *o.render;
    Checkbox(doc,"Render component visible",r.visible);
    int shape=r.shape==SceneShape::Sprite?5:int(r.shape);
    if(ImGui::Combo("Shape",&shape,"Box\0Sphere\0Compound\0Mesh\0Terrain\0Sprite (2D)\0")){doc.BeginEdit();r.shape=shape==5?SceneShape::Sprite:SceneShape(shape);doc.CommitEdit();}
    if(r.shape==SceneShape::Sprite) {
        AssetField(doc,"Sprite image",r.textureAsset,AssetType::Texture,true,state);
        if(ImGui::CollapsingHeader("Sprite sheet / animation")){
            if(!r.spritePlayback){if(ImGui::Button("Use a sprite sheet")){doc.BeginEdit();r.spritePlayback=SpritePlayback{};doc.CommitEdit();}}
            if(r.spritePlayback){
                auto& p=*r.spritePlayback;AssetField(doc,"Sheet asset",p.sheet,AssetType::SpriteSheet,true,state);
                if(state.resources&&!p.sheet.empty()){
                    std::string error;if(auto sheet=state.resources->GetSpriteSheet(p.sheet,error)){
                        if(ImGui::BeginCombo("Clip (or still frame)",p.clip.empty()?"Still frame":p.clip.c_str())){
                            if(ImGui::Selectable("Still frame",p.clip.empty())){doc.BeginEdit();p.clip.clear();p.time=0;doc.CommitEdit();}
                            for(const auto& c:sheet->clips)if(ImGui::Selectable(c.name.c_str(),p.clip==c.name)){doc.BeginEdit();p.clip=c.name;p.time=0;doc.CommitEdit();}
                            ImGui::EndCombo();
                        }
                        if(p.clip.empty()&&ImGui::BeginCombo("Named frame",p.frame.c_str())){for(const auto& f:sheet->frames)if(ImGui::Selectable((f.name+"##"+f.id).c_str(),p.frame==f.id)){doc.BeginEdit();p.frame=f.id;doc.CommitEdit();}ImGui::EndCombo();}
                        SpriteSettings sampled=r.sprite;if(SampleSpriteFrame(*sheet,p,sampled,error)&&!(sampled==r.sprite)){doc.BeginEdit();r.sprite=sampled;doc.CommitEdit();}
                    }
                    if(!error.empty())ImGui::TextWrapped("%s",error.c_str());
                }
                Checkbox(doc,"Play on start",p.playing);Checkbox(doc,"Loop clip",p.loop);{float speed=p.speed;if(ImGui::DragFloat("Playback speed",&speed,.05f,-100,100)){doc.BeginEdit();p.speed=speed;doc.CommitEdit();}}
                if(ImGui::Button("Edit sheet / clips")){state.spriteSheetEdit=p.sheet;state.showSpriteSheetEditor=true;}
                ImGui::SameLine();if(ImGui::Button("Remove animation")){doc.BeginEdit();r.spritePlayback.reset();doc.CommitEdit();}
            }
        }
        auto& sprite=r.sprite;
        DragVec2(doc,"Size (world units)",sprite.size,.02f,.001f,1e6f);
        DragVec2(doc,"Pivot (image top-left 0..1)",sprite.pivot,.01f,0,1);
        Checkbox(doc,"Flip horizontally",sprite.flipX);Checkbox(doc,"Flip vertically",sprite.flipY);
        auto tint=sprite.tint;
        const bool tintChanged=ImGui::ColorEdit4("Tint / opacity",&tint.x);
        if(ImGui::IsItemActivated())doc.BeginEdit();
        if(tintChanged)sprite.tint=glm::clamp(tint,glm::vec4(0),glm::vec4(1));
        TrackEdit(doc);
        DragInt(doc,"Sorting layer",sprite.sortingLayer,-32768,32767);
        DragInt(doc,"Order in layer",sprite.order,-32768,32767);
        Checkbox(doc,"Top-down Y sorting",sprite.ySort);
        Checkbox(doc,"Nearest / pixel-art sampling",sprite.nearest);
        if(ImGui::CollapsingHeader("Sprite picking and image rectangle")) {
            Checkbox(doc,"Select visible pixels (off = bounds)",sprite.alphaPick);
            Checkbox(doc,"Snap presentation in pixel-perfect cameras",sprite.pixelSnap);
            auto rect=sprite.rect;
            const bool changed=ImGui::DragFloat4("Source x / y / width / height",&rect.x,.005f,0,1,"%.4g",ImGuiSliderFlags_AlwaysClamp);
            if(ImGui::IsItemActivated())doc.BeginEdit();
            if(changed){auto candidate=sprite;candidate.rect=rect;if(ValidSprite(candidate))sprite=candidate;}
            TrackEdit(doc);
            ImGui::TextWrapped("Normalized top-left image rectangle; width/height must stay positive and inside the image.");
        }
        ImGui::TextWrapped("Drag an image here or into the scene. Sorting changes drawing only; sprite visuals add no collision. Add a Body component and choose Native 2D for real collision.");
        return;
    }
    auto publish=[&](MaterialOverride& destination,const MaterialDefinition& source){
        auto candidate=destination;
        if(DrawMaterialOverrideControls(candidate,source,state)){
            std::string error;if(!ValidateMaterial(ApplyMaterialOverride(MaterialDefinition{},candidate),error)){state.status=error;return;}
            doc.BeginEdit();destination=std::move(candidate);doc.CommitEdit();
        }
    };
    MaterialDefinition wholeBase;wholeBase.model=MaterialModel::Legacy;wholeBase.baseColor=glm::vec4(r.color,r.alpha);
    if(ImGui::CollapsingHeader("Whole renderable material"))publish(r.instanceOverrides,wholeBase);
    if(!r.runtimeMaterials.empty()&&ImGui::Button("Clear saved runtime material bindings")){doc.BeginEdit();r.runtimeMaterials.clear();doc.CommitEdit();}
    if(ImGui::Button("Add material slot")){doc.BeginEdit();if(r.materials.size()<64)r.materials.push_back({});doc.CommitEdit();}
    for(size_t i=0;i<r.materials.size();++i){
        ImGui::PushID(int(i));auto& slot=r.materials[i];
        if(ImGui::TreeNode("Material slot","Material slot %zu (legacy numeric binding)",i)){
            AssetField(doc,"Shared material",slot.asset,AssetType::Material,true,state);
            MaterialDefinition source=wholeBase;
            if(slot.asset.empty()&&state.resources){if(auto imported=state.resources->TryGetMeshMaterial(r.meshAsset,unsigned(i)))source=MaterialSettings(*imported);}
            else if(state.assets){if(auto* record=state.assets->Find(slot.asset)){std::string error;LoadMaterial(record->path,source,error);}}
            publish(slot.overrides,ApplyMaterialOverride(source,r.instanceOverrides));
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if(r.shape==SceneShape::Mesh&&state.resources) {
        state.resources->RequestMesh(r.meshAsset);
        if(const auto* parts=state.resources->TryGetModelParts(r.meshAsset)) {
            ImGui::TextWrapped("Imported render parts (stable identities)");
            for(size_t index=0;index<parts->size();++index) {
                const auto& part=parts->at(index);ImGui::PushID(part.part.c_str());
                bool visible=std::find(r.hiddenParts.begin(),r.hiddenParts.end(),part.part)==r.hiddenParts.end();
                if(ImGui::Checkbox("Part visible",&visible)) {
                    doc.BeginEdit();r.hiddenParts.erase(std::remove(r.hiddenParts.begin(),r.hiddenParts.end(),part.part),r.hiddenParts.end());
                    if(!visible)r.hiddenParts.push_back(part.part);
                    doc.CommitEdit();
                }
                ImGui::TextWrapped("%s",part.part.c_str());
                if(ImGui::TreeNode("Part material")) {
                    auto found=r.partMaterials.find(part.part);
                    if(found==r.partMaterials.end()) {
                        if(ImGui::Button("Add part material override")&&r.partMaterials.size()<64){doc.BeginEdit();r.partMaterials[part.part]={};doc.CommitEdit();}
                    } else {
                        auto& slot=found->second;AssetField(doc,"Shared material",slot.asset,AssetType::Material,true,state);
                        MaterialDefinition source=wholeBase;
                        if(slot.asset.empty()){if(auto imported=state.resources->TryGetMeshMaterial(r.meshAsset,unsigned(index)))source=MaterialSettings(*imported);}
                        else if(state.assets){if(auto* record=state.assets->Find(slot.asset)){std::string error;LoadMaterial(record->path,source,error);}}
                        publish(slot.overrides,ApplyMaterialOverride(source,r.instanceOverrides));
                        if(ImGui::Button("Remove part material binding")){doc.BeginEdit();r.partMaterials.erase(part.part);doc.CommitEdit();}
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            for(const auto& [key,_]:r.partMaterials)if(std::none_of(parts->begin(),parts->end(),[&](const auto& part){return part.part==key;}))ImGui::TextColored(ImVec4(1,.4f,.2f,1),"Missing imported part: %s",key.c_str());
            for(const auto& key:r.hiddenParts)if(std::none_of(parts->begin(),parts->end(),[&](const auto& part){return part.part==key;}))ImGui::TextColored(ImVec4(1,.4f,.2f,1),"Missing hidden part: %s",key.c_str());
        } else ImGui::TextDisabled("Imported parts become available when the mesh is ready.");
    }

    if (r.shape == SceneShape::Box) DragVec3(doc, "Half extents", r.halfExtents, 0.01f);
    if (r.shape == SceneShape::Sphere) DragScalar(doc, "Radius", r.radius, 0.01f, 0.001f, 100000.0f);
    ColorEdit(doc, "Color", r.color);
    DragScalar(doc, "Alpha", r.alpha, 0.01f, 0.0f, 1.0f);
    if(r.shape==SceneShape::Mesh){DragScalar(doc,"LOD tolerance multiplier",r.lodMultiplier,.1f,0,32);ImGui::TextDisabled("Cooked meshes: 0 forces full detail; 1 uses the cooked pixel error.");}
    if (r.shape == SceneShape::Box || r.shape == SceneShape::Sphere || r.shape == SceneShape::Mesh) {
        const auto* selected = doc.GetScene().Find(r.textureCamera);
        const std::string label = selected ? selected->name : (r.textureCamera ? "(missing camera)" : "(disk/untextured)");
        if (ImGui::BeginCombo("Camera texture", label.c_str())) {
            if (ImGui::Selectable("(disk/untextured)", r.textureCamera == 0)) {
                doc.BeginEdit(); r.textureCamera = 0; doc.CommitEdit();
            }
            for (const auto& camera : doc.GetScene().Objects()) {
                if (!camera.renderCamera) continue;
                ImGui::PushID(static_cast<int>(camera.id));
                if (ImGui::Selectable(camera.name.c_str(), r.textureCamera == camera.id)) {
                    doc.BeginEdit(); r.textureCamera = camera.id; r.textureAsset.clear(); doc.CommitEdit();
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
    }
    if (r.shape == SceneShape::Mesh) {
        AssetField(doc, "Mesh", r.meshAsset, AssetType::Mesh, false, state);
        if (!r.textureCamera) AssetField(doc, "Texture", r.textureAsset, AssetType::Texture, true, state);
        ImGui::TextDisabled("Drag an asset from the Asset Browser onto a field.");
    }
    if (r.shape == SceneShape::Compound) {
        ColorEdit(doc, "Wall color", r.secondaryColor);
        DragScalar(doc, "Wall alpha", r.secondaryAlpha, 0.01f, 0.0f, 1.0f);
        ImGui::TextDisabled("Geometry comes from the compound body.");
    }
}

void JointPicker(EditorDocument& doc,SceneObject& source,EditorPanelState& state,const char* label,std::string& key,bool optional=false){
 std::shared_ptr<const SkeletalAsset> asset;if(source.render&&state.resources){state.resources->RequestMesh(source.render->meshAsset);asset=state.resources->TryGetSkeletal(source.render->meshAsset);}
 if(ImGui::BeginCombo(label,key.empty()?"None / select imported joint":key.c_str())){
  if(optional&&ImGui::Selectable("None",key.empty())){doc.BeginEdit();key.clear();doc.CommitEdit();}
  if(asset)for(size_t i=0;i<asset->skeleton.names.size();++i){auto name=SkeletonJointKey(asset->skeleton,int(i));if(ImGui::Selectable(name.c_str(),name==key)){doc.BeginEdit();key=name;doc.CommitEdit();}}
  else ImGui::TextDisabled("Select/load a skinned mesh first");
  ImGui::EndCombo();
 }
 if(!key.empty()&&asset&&FindSkeletonJoint(asset->skeleton,key)<0)ImGui::TextColored(ImVec4(1,.4f,.2f,1),"Missing joint: %s",key.c_str());
}
void DrawSocket(EditorDocument& doc,SceneObject& object,EditorPanelState& state){
 auto& s=*object.socket;auto* target=doc.GetScene().Find(s.target);
 if(ImGui::BeginCombo("Skeleton entity",target?target->name.c_str():"Select skeleton")){for(auto& o:doc.GetScene().Objects())if(o.animation&&o.id!=object.id&&ImGui::Selectable((o.name+" ##"+std::to_string(o.id)).c_str(),o.id==s.target)){doc.BeginEdit();s.target=o.id;doc.CommitEdit();}ImGui::EndCombo();}
 target=doc.GetScene().Find(s.target);if(target)JointPicker(doc,*target,state,"Socket joint",s.joint);
 Checkbox(doc,"Socket enabled",s.enabled);DragVec3(doc,"Socket local offset",s.offset.position,.01f);auto angles=glm::degrees(glm::eulerAngles(s.offset.rotation));if(ImGui::DragFloat3("Socket local orientation",&angles.x,1))s.offset.rotation=glm::quat(glm::radians(angles));TrackEdit(doc);DragVec3(doc,"Socket local scale",s.offset.scale,.01f);
 ImGui::TextWrapped("Visual/query attachment only. Dynamic physical attachment uses joints. Local X/Y/Z axes appear in viewport.");
}
void DrawAnimation(EditorDocument& doc,SceneObject& object,EditorPanelState& state){
    auto& a=*object.animation;Checkbox(doc,"Animation enabled",a.enabled);Checkbox(doc,"Play on start",a.playOnStart);Checkbox(doc,"Loop clip",a.loop);
    TextField(doc,"Clip name (empty = first)",a.clip);DragScalar(doc,"Playback speed",a.speed);DragScalar(doc,"Start time (seconds)",a.time,.05f,0,100000);
    if(ImGui::Button("Add pose layer")&&a.layers.size()<16){doc.BeginEdit();AnimationLayerSettings l;l.id="Layer "+std::to_string(a.layers.size()+1);a.layers.push_back(l);doc.CommitEdit();}
    for(size_t i=0;i<a.layers.size();++i){ImGui::PushID(int(i));auto& l=a.layers[i];if(ImGui::TreeNode(l.id.c_str())){
        TextField(doc,"Layer ID",l.id);TextField(doc,"Clip",l.clip);Checkbox(doc,"Enabled",l.enabled);Checkbox(doc,"Additive",l.additive);DragScalar(doc,"Weight",l.weight,.01f,0,1);DragScalar(doc,"Speed",l.speed);DragScalar(doc,"Time",l.time,.05f,0,100000);TextField(doc,"Reference clip (empty = rest)",l.referenceClip);DragScalar(doc,"Reference time",l.referenceTime,.05f,0,100000);
        if(ImGui::Button("Add masked joint")){doc.BeginEdit();l.mask.push_back("Root");doc.CommitEdit();}for(size_t n=0;n<l.mask.size();++n){ImGui::PushID(int(n));JointPicker(doc,object,state,"Masked joint",l.mask[n]);ImGui::SameLine();if(ImGui::Button("Remove")){doc.BeginEdit();l.mask.erase(l.mask.begin()+n);doc.CommitEdit();ImGui::PopID();break;}ImGui::PopID();}
        if(ImGui::Button("Remove layer")){doc.BeginEdit();a.layers.erase(a.layers.begin()+i);doc.CommitEdit();ImGui::TreePop();ImGui::PopID();break;}ImGui::TreePop();}ImGui::PopID();}
    if(ImGui::Button("Add limb IK")&&a.limbs.size()<16){doc.BeginEdit();LimbIKSettings k;k.id="Limb "+std::to_string(a.limbs.size()+1);a.limbs.push_back(k);doc.CommitEdit();}
    for(size_t i=0;i<a.limbs.size();++i){auto& k=a.limbs[i];ImGui::PushID(int(100+i));if(ImGui::TreeNode(k.id.c_str())){
        TextField(doc,"Contributor ID",k.id);JointPicker(doc,object,state,"Root",k.root);JointPicker(doc,object,state,"Middle",k.middle);JointPicker(doc,object,state,"End",k.end);
        DragVec3(doc,"Target (world metres)",k.target,.01f);DragVec3(doc,"Pole (world point)",k.pole,.01f);DragScalar(doc,"Weight",k.weight,.01f,0,1);Checkbox(doc,"Enabled",k.enabled);int order=k.order;if(ImGui::InputInt("Order 1..999",&order)){doc.BeginEdit();k.order=std::clamp(order,1,999);doc.CommitEdit();}
        if(ImGui::Button("Remove limb")){doc.BeginEdit();a.limbs.erase(a.limbs.begin()+i);doc.CommitEdit();ImGui::TreePop();ImGui::PopID();break;}ImGui::TreePop();}ImGui::PopID();}
    if(!a.fullBodyIK&&ImGui::Button("Add shared full-body IK")){doc.BeginEdit();a.fullBodyIK=FullBodyIKSettings{};doc.CommitEdit();}
    if(a.fullBodyIK&&ImGui::CollapsingHeader("Shared full-body IK")){
        auto& k=*a.fullBodyIK;Checkbox(doc,"Shared IK enabled",k.enabled);JointPicker(doc,object,state,"Skeletal body root",k.bodyRoot);Checkbox(doc,"Root rotation participates",k.rootRotation);DragVec3(doc,"Root correction minimum (model m)",k.rootMin,.01f);DragVec3(doc,"Root correction maximum (model m)",k.rootMax,.01f);
        auto frame=[&](const char* label,glm::quat& q){auto v=glm::degrees(glm::eulerAngles(q));if(ImGui::DragFloat3(label,&v.x,.5f))q=glm::normalize(glm::quat(glm::radians(v)));TrackEdit(doc);};
        if(ImGui::Button("Add spine participant")){doc.BeginEdit();k.spine.push_back("");doc.CommitEdit();}for(size_t n=0;n<k.spine.size();++n){ImGui::PushID(int(1000+n));JointPicker(doc,object,state,"Spine joint",k.spine[n]);ImGui::SameLine();if(ImGui::Button("Remove")){doc.BeginEdit();k.spine.erase(k.spine.begin()+n);doc.CommitEdit();ImGui::PopID();break;}ImGui::PopID();}
        if(ImGui::Button("Add effector chain")&&k.chains.size()<12){doc.BeginEdit();k.chains.push_back({"chain"+std::to_string(k.chains.size()+1),{}});doc.CommitEdit();}
        for(size_t n=0;n<k.chains.size();++n){ImGui::PushID(int(2000+n));auto& chain=k.chains[n];if(ImGui::TreeNode("Chain","%s",chain.id.c_str())){TextField(doc,"Stable chain ID",chain.id);if(ImGui::Button("Add chain joint")){doc.BeginEdit();chain.joints.push_back("");doc.CommitEdit();}for(size_t j=0;j<chain.joints.size();++j){ImGui::PushID(int(j));JointPicker(doc,object,state,"Ancestor / end joint",chain.joints[j]);ImGui::SameLine();if(ImGui::Button("Remove")){doc.BeginEdit();chain.joints.erase(chain.joints.begin()+j);doc.CommitEdit();ImGui::PopID();break;}ImGui::PopID();}if(ImGui::Button("Remove chain")){doc.BeginEdit();k.chains.erase(k.chains.begin()+n);doc.CommitEdit();ImGui::TreePop();ImGui::PopID();break;}ImGui::TreePop();}ImGui::PopID();}
        if(ImGui::Button("Add contact target")&&k.targets.size()<12){doc.BeginEdit();FullBodyIKTarget t;t.id="target"+std::to_string(k.targets.size()+1);if(!k.chains.empty())t.chain=k.chains.front().id;k.targets.push_back(t);doc.CommitEdit();}
        for(size_t n=0;n<k.targets.size();++n){ImGui::PushID(int(3000+n));auto& target=k.targets[n];if(ImGui::TreeNode("Target","%s",target.id.c_str())){TextField(doc,"Stable target ID",target.id);TextField(doc,"Mapped chain ID",target.chain);Checkbox(doc,"Target enabled",target.enabled);const char* spaces[]={"Model","World"};Combo(doc,"Target space",target.space,spaces,2);auto v=glm::vec3(target.position);DragVec3(doc,"Contact position (m)",v,.01f);target.position=v;frame("Contact orientation",target.orientation);DragScalar(doc,"Position influence",target.positionWeight,.01f,0,1);DragScalar(doc,"Orientation influence",target.orientationWeight,.01f,0,1);DragVec3(doc,"Effector-local contact offset",target.offset,.01f);frame("Effector-local contact frame",target.frame);if(ImGui::Button("Remove target")){doc.BeginEdit();k.targets.erase(k.targets.begin()+n);doc.CommitEdit();ImGui::TreePop();ImGui::PopID();break;}ImGui::TreePop();}ImGui::PopID();}
        if(ImGui::Button("Add local joint limit")&&k.limits.size()<64){doc.BeginEdit();k.limits.push_back({});doc.CommitEdit();}
        for(size_t n=0;n<k.limits.size();++n){ImGui::PushID(int(4000+n));auto& limit=k.limits[n];if(ImGui::TreeNode("Limit","%s",limit.joint.c_str())){JointPicker(doc,object,state,"Limited joint",limit.joint);frame("Authored limit frame",limit.frame);DragVec3(doc,"Minimum rotation-vector (rad)",limit.min,.01f);DragVec3(doc,"Maximum rotation-vector (rad)",limit.max,.01f);DragVec3(doc,"Preferred bend (rad)",limit.preferred,.01f);DragScalar(doc,"Bend preference influence",limit.preferenceWeight,.01f,0,1);if(ImGui::Button("Remove limit")){doc.BeginEdit();k.limits.erase(k.limits.begin()+n);doc.CommitEdit();ImGui::TreePop();ImGui::PopID();break;}ImGui::TreePop();}ImGui::PopID();}
        std::string error;if(!ValidFullBodyIKSettings(k,error))ImGui::TextColored(ImVec4(1,.4f,.2f,1),"%s",error.c_str());
        ImGui::TextWrapped("One shared bounded solve; contact frames are explicit. Root correction changes the pose, never the entity or motor. Live residuals are available through animation.ikStatus.");
        if(ImGui::Button("Remove shared IK")){doc.BeginEdit();a.fullBodyIK.reset();doc.CommitEdit();}
    }
    ImGui::TextDisabled("Layers are resolved in list order; empty mask affects all joints.");
    ImGui::TextDisabled("Use a self-contained GLB/glTF mesh. Pose is independent of playback.");
    if(!object.render||object.render->shape!=SceneShape::Mesh)ImGui::TextColored(ImVec4(1,.3f,.2f,1),"Requires a mesh Render component");
}

void DrawRagdoll(EditorDocument& doc,SceneObject& object,EditorPanelState& state){
    auto frame=[&](const char* label,glm::quat& q){auto degrees=glm::degrees(glm::eulerAngles(glm::normalize(q)));if(ImGui::DragFloat3(label,&degrees.x,.5f))q=glm::normalize(glm::quat(glm::radians(degrees)));TrackEdit(doc);};
    auto& r=*object.ragdoll;Checkbox(doc,"Ragdoll enabled",r.enabled);Checkbox(doc,"Start in ragdoll",r.playOnStart);Checkbox(doc,"Self collision",r.selfCollision);Checkbox(doc,"Receive mapped contact events",r.receiveContactEvents);
    ImGui::TextWrapped("Joint keys are imported hierarchy paths or unique names. Parent mapping must precede children. Bodies use normal physics. Optional physical regions drive the separate pose reference. Positive uniform scales only.");
    if(ImGui::Button("Add mapped bone")&&r.bones.size()<32){doc.BeginEdit();RagdollBone b;b.joint="Root";r.bones.push_back(b);doc.CommitEdit();}
    if(object.render&&state.resources){auto asset=state.resources->TryGetSkeletal(object.render->meshAsset);if(asset&&ImGui::CollapsingHeader("Skeleton selection / mapping tools")){static std::set<std::string> selected;static std::string owner;static char search[256]{};static float thickness=.15f,resistance=.05f,lower=-.75f,upper=.75f;static bool editLimits=false,editResistance=false;static RagdollDefinition preview;static bool hasPreview=false;auto key=object.render->meshAsset+":"+std::to_string(object.id);if(owner!=key){owner=key;selected.clear();hasPreview=false;state.skeletonFitPreview.reset();}ImGui::InputText("Filter joints",search,sizeof(search));ImGui::BeginChild("Joint hierarchy",{0,160},true);for(auto n:asset->skeleton.order){auto& name=asset->skeleton.names[n];if(*search&&name.find(search)==std::string::npos)continue;if(ImGui::Selectable(name.c_str(),selected.count(name))){if(!ImGui::GetIO().KeyCtrl)selected.clear();if(selected.count(name))selected.erase(name);else selected.insert(name);hasPreview=false;state.skeletonFitPreview.reset();}}ImGui::EndChild();state.skeletonPicked.assign(selected.begin(),selected.end());ImGui::InputFloat("Fit thickness (joint-local m)",&thickness);if(ImGui::Button("Preview fit selected chain")){std::vector<std::string> keys(selected.begin(),selected.end());std::string error;hasPreview=FitSkeletonChain(asset->skeleton,r,keys,thickness,preview,error);state.skeletonFitPreview=hasPreview?std::optional<RagdollDefinition>(preview):std::nullopt;state.skeletonFitOwner=object.id;state.skeletonFitGeneration=doc.Generation();state.status=hasPreview?"Preview ready. Existing mass, limits and resistance are retained.":error;}if(hasPreview&&state.skeletonFitGeneration==doc.Generation()){ImGui::Text("Preview: %zu mapped bodies; rest-pose fit is a starting estimate",preview.bones.size());if(ImGui::Button("Accept fitted mappings")){doc.BeginEdit();r=preview;doc.CommitEdit();hasPreview=false;state.skeletonFitPreview.reset();}}
    ImGui::Checkbox("Set limits on selected mappings",&editLimits);ImGui::InputFloat("Lower radians/metres",&lower);ImGui::InputFloat("Upper radians/metres",&upper);ImGui::Checkbox("Set rotational resistance",&editResistance);ImGui::InputFloat("Resistance N m s/rad",&resistance);if(ImGui::Button("Apply selected constraint fields")){auto candidate=r;for(auto& b:candidate.bones)if(selected.count(b.joint)){if(editLimits){b.constraint.limits=true;b.constraint.lower=lower;b.constraint.upper=upper;}if(editResistance)b.constraint.rotationalResistance=resistance;}std::string error;if(!ValidRagdollDefinition(candidate,error))state.status=error;else{doc.BeginEdit();r=std::move(candidate);doc.CommitEdit();}}
    }}
    size_t remove=r.bones.size();
    for(size_t i=0;i<r.bones.size();++i){auto& b=r.bones[i];ImGui::PushID(int(i));if(ImGui::TreeNode("Mapped bone","%s",b.joint.c_str())){
        JointPicker(doc,object,state,"Skeleton joint",b.joint);JointPicker(doc,object,state,"Physical parent key",b.parent,true);const char* shapes[]={"Box","Sphere"};Combo(doc,"Shape",b.shape,shapes,2);
        DragVec3(doc,"Shape offset",b.offset,.01f);frame("Shape orientation",b.orientation);DragVec3(doc,"Half extents",b.halfExtents,.01f);DragScalar(doc,"Radius",b.radius,.01f);DragScalar(doc,"Mass",b.mass,.1f);DragScalar(doc,"Friction",b.friction,.01f);DragScalar(doc,"Restitution",b.restitution,.01f);
        if (state.project && InspectorDisclosure("Collision filtering")) {
        DrawCategoryLayer(doc,"Collision layer",b.collisionLayer,state.project->Settings().classification.collision);
        DrawCategoryMask(doc,"Collision mask",b.collisionMask,state.project->Settings().classification.collision);
    }
        Checkbox(doc,"Suppress parent collision",b.suppressParentCollision);Checkbox(doc,"Capture anchors from current pose",b.autoAnchors);
        const char* types[]={"Fixed","Hinge","Ball","Slider"};Combo(doc,"Constraint",b.constraint.type,types,4);Checkbox(doc,"Constraint enabled",b.constraint.enabled);Checkbox(doc,"Limits",b.constraint.limits);DragScalar(doc,"Lower",b.constraint.lower,.01f);DragScalar(doc,"Upper",b.constraint.upper,.01f);DragScalar(doc,"Rotational resistance (N m s/rad)",b.constraint.rotationalResistance,.01f,0,100000);
        if(!b.autoAnchors){DragVec3(doc,"Child anchor",b.constraint.anchorA,.01f);DragVec3(doc,"Parent anchor",b.constraint.anchorB,.01f);}
        frame("Child frame",b.constraint.frameA);frame("Parent frame",b.constraint.frameB);
        if(ImGui::Button("Remove mapping"))remove=i;
        ImGui::TreePop();}ImGui::PopID();
    }
    if(remove<r.bones.size()){doc.BeginEdit();r.bones.erase(r.bones.begin()+remove);doc.CommitEdit();}
    if(!r.physicalAnimation&&ImGui::Button("Add physical-animation regions")){doc.BeginEdit();r.physicalAnimation=PhysicalAnimationSettings{};doc.CommitEdit();}
    if(r.physicalAnimation&&ImGui::CollapsingHeader("Physical-animation regions")){
        auto& settings=*r.physicalAnimation;Checkbox(doc,"Physical animation enabled",settings.enabled);
        if(ImGui::Button("Add region")&&settings.regions.size()<8){doc.BeginEdit();PhysicalAnimationRegion region;region.id="region"+std::to_string(settings.regions.size()+1);settings.regions.push_back(region);doc.CommitEdit();}
        for(size_t n=0;n<settings.regions.size();++n){ImGui::PushID(int(5000+n));auto& region=settings.regions[n];if(ImGui::TreeNode("Physical region","%s",region.id.c_str())){TextField(doc,"Stable region ID",region.id);Checkbox(doc,"Region enabled",region.enabled);DragScalar(doc,"Stiffness (N m/rad)",region.stiffness,.5f,0,10000);DragScalar(doc,"Damping (N m s/rad)",region.damping,.1f,0,1000);DragScalar(doc,"Torque cap (N m)",region.maxTorque,.5f,0,10000);DragScalar(doc,"Drive effort",region.effortWeight,.01f,0,1);DragScalar(doc,"Physical pose influence",region.poseWeight,.01f,0,1);if(ImGui::Button("Add driven mapped joint")){doc.BeginEdit();region.joints.push_back("");doc.CommitEdit();}for(size_t j=0;j<region.joints.size();++j){ImGui::PushID(int(j));JointPicker(doc,object,state,"Driven mapped joint",region.joints[j]);ImGui::SameLine();if(ImGui::Button("Remove")){doc.BeginEdit();region.joints.erase(region.joints.begin()+j);doc.CommitEdit();ImGui::PopID();break;}ImGui::PopID();}if(ImGui::Button("Remove region")){doc.BeginEdit();settings.regions.erase(settings.regions.begin()+n);doc.CommitEdit();ImGui::TreePop();ImGui::PopID();break;}ImGui::TreePop();}ImGui::PopID();}
        ImGui::TextWrapped("Later enabled regions win overlaps. Drive effort and visual influence are separate. Partial mode preserves motor authority; full active/passive modes need an explicit handoff. Live torque/error/saturation is available through ragdoll.physicalState.");
        if(ImGui::Button("Remove physical settings")){doc.BeginEdit();r.physicalAnimation.reset();doc.CommitEdit();}
    }
    if(!r.balance&&ImGui::Button("Add physical balance mapping")){doc.BeginEdit();BalanceDefinition value;for(auto& e:value.settings.effectors)e.patches.push_back({{0,-.2f,0},{0,-1,0},.2f});r.balance=std::move(value);doc.CommitEdit();}
    if(r.balance&&ImGui::CollapsingHeader("Physical balance / recovery")){
        auto& d=*r.balance;auto& s=d.settings;Checkbox(doc,"Balance configuration enabled",d.enabled);Checkbox(doc,"Draw physical balance diagnostics",d.visualize);
        ImGui::TextWrapped("Explicit two-leg mapping. Configuring does not activate a task. JS must hand off the motor and request stand/recover. Real bodies/contact effort determine success; no root position servo. Non-adjacent self collision remains enabled.");
        DragVec3(doc,"Root-local up",s.up,.01f);DragVec3(doc,"Root-local forward",s.forward,.01f);
        for(unsigned f=0;f<2;++f){ImGui::PushID(int(7000+f));if(ImGui::TreeNode("Support chain","Support chain %u",f)){auto& c=d.chains[f];auto& e=s.effectors[f];JointPicker(doc,object,state,"Upper physical joint",c.upper);JointPicker(doc,object,state,"Lower physical joint",c.lower);JointPicker(doc,object,state,"Foot physical joint",c.foot);DragVec3(doc,"Foot contact offset (m)",e.contactOffset,.01f);DragVec3(doc,"Foot clearance half extents (m)",e.halfExtents,.01f);DragScalar(doc,"Accepted support normal dot",e.normalAcceptance,.01f,0,1);
            for(size_t n=0;n<e.patches.size();++n){ImGui::PushID(int(n));auto& p=e.patches[n];DragVec3(doc,"Patch centre",p.offset,.01f);DragVec3(doc,"Outward patch normal",p.normal,.01f);DragScalar(doc,"Patch radius (m)",p.radius,.01f,.001f,2);if(ImGui::Button("Remove patch")){doc.BeginEdit();e.patches.erase(e.patches.begin()+n);doc.CommitEdit();ImGui::PopID();break;}ImGui::PopID();}
            if(e.patches.size()<8&&ImGui::Button("Add patch")){doc.BeginEdit();e.patches.push_back({{0,-.2f,0},{0,-1,0},.2f});doc.CommitEdit();}ImGui::TreePop();}ImGui::PopID();}
        DragScalar(doc,"Balance stiffness (N m/rad)",s.stiffness,1,.001f,10000);DragScalar(doc,"Balance damping (N m s/rad)",s.damping,.5f,.001f,1000);DragScalar(doc,"Balance torque cap (N m)",s.maxTorque,1,.001f,10000);
        DragScalar(doc,"Standing root/support height (m)",s.standingHeight,.01f,.01f,10);DragScalar(doc,"Standing height tolerance (m)",s.standingTolerance,.01f,.001f,2);DragScalar(doc,"Settled relative speed (m/s)",s.standingSpeed,.01f,.001f,2);DragScalar(doc,"Physical standing dwell (s)",s.settleSeconds,.1f,.01f,30);DragScalar(doc,"Attempt deadline (s)",s.attemptSeconds,.1f,.01f,120);DragScalar(doc,"Initial reference fade (s)",s.referenceSeconds,.1f,.01f,30);
        DragScalar(doc,"COM position response (rad/m)",s.positionResponse,.01f,.001f,10);DragScalar(doc,"COM velocity response (rad s/m)",s.velocityResponse,.01f,.001f,10);DragScalar(doc,"Maximum lean (rad)",s.maximumLean,.01f,.001f,1);DragScalar(doc,"Leg lean fraction",s.legLeanFraction,.01f,0,1);DragScalar(doc,"Trunk lean fraction",s.trunkLeanFraction,.01f,0,1);DragScalar(doc,"Planted orientation weight",s.plantedOrientationWeight,.01f,0,1);DragScalar(doc,"Per-body gravity coherence dot",s.gravityCoherence,.001f,.001f,1);
        DragScalar(doc,"Capture step trigger (m)",s.stepTrigger,.01f,.001f,2);DragScalar(doc,"Maximum step reach (m)",s.stepReach,.01f,.001f,3);DragScalar(doc,"Swing clearance (m)",s.stepHeight,.01f,.001f,2);DragScalar(doc,"Swing reference duration (s)",s.stepSeconds,.01f,.01f,5);DragScalar(doc,"Touchdown tolerance (m)",s.touchdownTolerance,.01f,.001f,1);DragScalar(doc,"Stance load fraction",s.transferLoadFraction,.01f,.001f,1);DragScalar(doc,"Stance patch radius fraction",s.transferRadiusFraction,.01f,.001f,1);DragScalar(doc,"Load transfer deadline (s)",s.transferSeconds,.1f,.01f,10);
        int steps=int(s.maximumSteps);if(ImGui::SliderInt("Maximum corrective landings",&steps,0,8)){doc.BeginEdit();s.maximumSteps=unsigned(steps);doc.CommitEdit();}
        if(state.project){DrawCategoryMask(doc,"Foothold layers",s.query.includeLayers,state.project->Settings().classification.collision);DrawCategoryMask(doc,"Excluded foothold layers",s.query.excludeLayers,state.project->Settings().classification.collision);DrawCategoryMask(doc,"Required foothold tags",s.query.requiredTags,state.project->Settings().classification.tags);DrawCategoryMask(doc,"Excluded foothold tags",s.query.excludedTags,state.project->Settings().classification.tags);}
        if(ImGui::Button("Remove balance configuration")){doc.BeginEdit();r.balance.reset();doc.CommitEdit();}
    }
    std::string error;if(!ValidRagdollDefinition(r,error))ImGui::TextColored(ImVec4(1,.3f,.2f,1),"%s",error.c_str());
}

void DrawGravitySelection(EditorDocument& doc,SceneObject& object,EditorPanelState&){
    auto candidate=*object.gravitySelection;
    const char* modes[]={"Uniform acceleration","Selected field"};
    int mode=int(candidate.mode);bool changed=false;
    if(ImGui::Combo("Gravity selection mode",&mode,modes,2)){
        GravitySelection next;next.mode=GravitySelection::Mode(mode);
        if(next.mode==GravitySelection::Mode::Field){
            for(const auto& source:doc.GetScene().Objects())if(source.gravity){next.source=source.id;break;}
        }
        std::string error;
        if(ValidGravitySelection(next,error)){candidate=next;changed=true;}
    }
    if(candidate.mode==GravitySelection::Mode::Uniform)
        changed|=ImGui::DragFloat3("Acceleration (world m/s^2)",&candidate.acceleration.x,.05f);
    else{
        auto* source=doc.GetScene().Find(candidate.source);
        if(ImGui::BeginCombo("Selected gravity field",source?source->name.c_str():"Missing field")){
            for(const auto& field:doc.GetScene().Objects())if(field.gravity&&ImGui::Selectable((field.name+" ##"+std::to_string(field.id)).c_str(),field.id==candidate.source)){
                candidate.source=field.id;changed=true;}
            ImGui::EndCombo();}
    }
    ImGui::TextWrapped("Optional entity-local gravity. Remove this component to restore spatial routing. Does not change children, camera or support ownership. Create a Gravity region before choosing Selected field.");
    std::string error;
    if(!ValidGravitySelection(candidate,error)){ImGui::TextColored(ImVec4(1,.3f,.2f,1),"%s",error.c_str());return;}
    // One valid history action, including mode/source/vector together.
    if(changed){doc.BeginEdit();object.gravitySelection=candidate;doc.CommitEdit();}
}
void DrawJoint(EditorDocument& doc, SceneObject& object, EditorPanelState&) {
    auto& joint=*object.joint;auto& settings=joint.settings;
    const char* names[]={"Fixed","Hinge","Ball/socket","Slider","2D pin","2D weld"};
    int jointType=int(settings.type);
    if(ImGui::Combo("Joint type",&jointType,names,6)) {
        doc.BeginEdit();settings.type=JointType(jointType);
        if(settings.type==JointType::PlanarWeld){settings.motor=settings.spring=settings.limits=false;settings.rotationalResistance=0;}
        doc.CommitEdit();
    }
    auto bodyField=[&](const char* label,SceneObjectId& id,bool world){
        const auto* body=doc.GetScene().Find(id);const std::string preview=body?body->name:world&&id==0?"World anchor":"Select body";
        if(ImGui::BeginCombo(label,preview.c_str())){
            if(world&&ImGui::Selectable("World anchor",id==0)){doc.BeginEdit();id=0;doc.CommitEdit();}
            for(const auto& candidate:doc.GetScene().Objects())if(candidate.body&&ImGui::Selectable((candidate.name+" ##"+std::to_string(candidate.id)).c_str(),candidate.id==id)){doc.BeginEdit();id=candidate.id;doc.CommitEdit();}
            ImGui::EndCombo();
        }
    };
    bodyField("Body A",joint.bodyA,false);bodyField("Body B",joint.bodyB,true);
    DragVec3(doc,"Anchor A (body local)",settings.anchorA);
    DragVec3(doc,joint.bodyB?"Anchor B (body local)":"Anchor B (owner local)",settings.anchorB);
    auto frame=[&](const char* label,glm::quat& q){auto degrees=glm::degrees(glm::eulerAngles(glm::normalize(q)));if(ImGui::DragFloat3(label,&degrees.x,.5f))q=glm::normalize(glm::quat(glm::radians(degrees)));TrackEdit(doc);};
    frame("Frame A (degrees)",settings.frameA);frame(joint.bodyB?"Frame B (degrees)":"World frame (owner local)",settings.frameB);
    Checkbox(doc,"Joint enabled",settings.enabled);
    if(settings.type==JointType::PlanarPin||settings.type==JointType::PlanarWeld)
        ImGui::TextWrapped("Native 2D: anchors use local XY and frame Z is the plane normal. Pin angle/speed are B minus A in radians/radians per second.");
    else ImGui::TextDisabled("Local frame X is the hinge/slider axis. Hinge values are radians.");
    if(settings.type==JointType::Hinge||settings.type==JointType::Slider||settings.type==JointType::PlanarPin){
        Checkbox(doc,"Limits",settings.limits);DragScalar(doc,"Lower",settings.lower);DragScalar(doc,"Upper",settings.upper);
        Checkbox(doc,"Motor",settings.motor);DragScalar(doc,"Target speed",settings.speed);
        DragScalar(doc,"Maximum force / torque",settings.maxForce,.1f,0,100000);
        Checkbox(doc,"Spring",settings.spring);DragScalar(doc,"Rest coordinate",settings.rest);
        DragScalar(doc,"Stiffness",settings.stiffness,.1f,0,100000);DragScalar(doc,"Damping",settings.damping,.1f,0,100000);
    }
    if(settings.type!=JointType::PlanarWeld)DragScalar(doc,"Rotational resistance (N m s/rad)",settings.rotationalResistance,.01f,0,100000);
    if(!ValidJointSettings(settings)||joint.bodyA==0||joint.bodyA==joint.bodyB)ImGui::TextColored(ImVec4(1,.3f,.2f,1),"Invalid joint settings or body references");
}

// Copy-on-edit keeps the immutable native collider valid throughout undo/redo.
void DrawPlanarCollider(EditorDocument& doc,SceneObject& o,EditorPanelState& state) {
    auto& body=*o.body;auto shape=body.planar?*body.planar:*PlanarBox({.5,.5});
    int kind=int(shape.kind);bool changed=false;
    if(ImGui::Combo("2D geometry",&kind,"Circle\0Box\0Capsule\0Convex polygon\0Static chain\0")){
        shape=kind==0?*PlanarCircle(.5):kind==1?*PlanarBox({.5,.5}):kind==2?*PlanarCapsule(.3,.4):
            kind==3?*PlanarConvex({{-.5,-.5},{.5,-.5},{0,.5}}):*PlanarChain({{-2,0},{0,0},{2,1}});changed=true;
    }
    changed|=ImGui::Checkbox("Lock 2D rotation",&shape.lockRotation);
    if(shape.kind==PlanarKind::Chain&&shape.vertices.size()==2){changed|=ImGui::Checkbox("One-way platform (jump through)",&shape.oneWay);ImGui::TextWrapped("Jump through the non-blocking side; land on the blocking side. Enable Preview collision to see its arrow. A left-to-right edge blocks from above. Static straight segments only.");}
    if(shape.kind==PlanarKind::Circle||shape.kind==PlanarKind::Capsule){float r=float(shape.radius);if(ImGui::DragFloat("Radius (m)",&r,.01f,.001f,10000)){shape.radius=r;changed=true;}}
    if(shape.kind==PlanarKind::Box){glm::vec2 half=-glm::vec2(shape.vertices[0]);if(ImGui::DragFloat2("Half size (m)",&half.x,.01f,.001f,10000)){shape.vertices={{-half.x,-half.y},{half.x,-half.y},{half.x,half.y},{-half.x,half.y}};changed=true;}}
    if(shape.kind==PlanarKind::Capsule){float h=float(shape.vertices[1].y);if(ImGui::DragFloat("Core half height (m)",&h,.01f,0,10000)){shape.vertices={{0,-h},{0,h}};changed=true;}}
    if(shape.kind==PlanarKind::Convex||shape.kind==PlanarKind::Chain){
        for(size_t i=0;i<shape.vertices.size();++i){ImGui::PushID(int(i));glm::vec2 v(shape.vertices[i]);if(ImGui::DragFloat2("Vertex XY",&v.x,.01f)){shape.vertices[i]=v;changed=true;}ImGui::PopID();}
        if(shape.vertices.size()<(shape.kind==PlanarKind::Chain?1024u:64u)&&ImGui::Button("Add vertex")) {
            if(shape.kind==PlanarKind::Chain) {
                shape.vertices.push_back(2.*shape.vertices.back()-shape.vertices[shape.vertices.size()-2]);changed=true;
            } else {
                // Split the closing edge slightly outward. Appending an
                // arbitrary +X point usually creates a concave polygon which
                // the validator correctly rejects, making Add unusable.
                const auto a=shape.vertices.back(),b=shape.vertices.front(),edge=b-a;
                double winding=0;for(size_t i=0;i<shape.vertices.size();++i){const auto p=shape.vertices[i],q=shape.vertices[(i+1)%shape.vertices.size()];winding+=p.x*q.y-p.y*q.x;}
                const auto outward=glm::dvec2(edge.y,-edge.x)*(winding>0?1.:-1.);
                for(int attempt=0;attempt<16;++attempt){auto candidate=shape;
                    candidate.vertices.push_back((a+b)*.5+outward*(.1/std::pow(2.,attempt)));std::string error;
                    if(ValidatePlanar(candidate,error)){shape=std::move(candidate);changed=true;break;}}
            }
        }
        ImGui::SameLine();if(shape.vertices.size()>(shape.kind==PlanarKind::Chain?2u:3u)&&ImGui::Button("Remove last vertex")){shape.vertices.pop_back();changed=true;}
    }
    std::string error;
    if(changed&&ValidatePlanar(shape,error)){doc.BeginEdit();body.planar=std::make_shared<const PlanarCollider>(shape);if(shape.kind==PlanarKind::Chain)body.motion=SceneBodyMotion::Static;doc.CommitEdit();}
    else if(!error.empty())ImGui::TextColored(ImVec4(1,.4f,.2f,1),"%s (last valid shape retained)",error.c_str());
    ImGui::TextWrapped("Real collision in local XY, spin about local Z. Keep transform scale 1. Sprite size, sorting and flips do not change collision. Chains are static and open; straight two-vertex chains can opt into one-way response. 3D physics cannot share this runtime world.");
    if(ImGui::Button("Preview 2D collision")) {
        state.collisionPreview.Clear();const auto q=o.transform.rotation;const auto p=o.transform.position;
        auto line=[&](glm::dvec2 a,glm::dvec2 b){state.collisionPreview.Line(p+q*glm::vec3(a,0),p+q*glm::vec3(b,0),{.3f,1,.5f});};
        const auto& c=*body.planar;
        if(c.oneWay){const auto e=glm::normalize(c.vertices[1]-c.vertices[0]),n=glm::dvec2(-e.y,e.x),mid=(c.vertices[0]+c.vertices[1])*.5,tip=mid+n*.5;line(mid,tip);line(tip,tip-n*.15+e*.1);line(tip,tip-n*.15-e*.1);}
        const double pi=3.141592653589793;
        if(c.kind==PlanarKind::Circle)for(int k=0;k<32;++k){double a=k*2*pi/32,b=(k+1)*2*pi/32;line(c.radius*glm::dvec2(cos(a),sin(a)),c.radius*glm::dvec2(cos(b),sin(b)));}
        else if(c.kind==PlanarKind::Capsule){const double h=c.vertices[1].y;
            line({-c.radius,-h},{-c.radius,h});line({c.radius,-h},{c.radius,h});
            for(int side:{-1,1})for(int k=0;k<16;++k){double a=k*pi/16,b=(k+1)*pi/16;
                line({c.radius*cos(a),side*(h+c.radius*sin(a))},{c.radius*cos(b),side*(h+c.radius*sin(b))});}}
        else for(size_t i=0;i+1<c.vertices.size();++i)line(c.vertices[i],c.vertices[i+1]);
        if(c.kind==PlanarKind::Box||c.kind==PlanarKind::Convex)line(c.vertices.back(),c.vertices.front());
    }
}

void DrawBody(EditorDocument& doc, SceneObject& o, EditorPanelState& state) {
    SceneBodyComponent& b = *o.body;
    ImGui::SeparatorText("Collision and motion");
    Checkbox(doc,"Sensor (events, no response)",b.sensor);
    Checkbox(doc,"Collider enabled",b.enabled);
    if (state.project && InspectorDisclosure("Collision filtering")) {
        DrawCategoryLayer(doc,"Collision layer",b.collisionLayer,state.project->Settings().classification.collision);
        DrawCategoryMask(doc,"Collision mask",b.collisionMask,state.project->Settings().classification.collision);
    }
    Combo(doc, "Motion", b.motion, kMotionNames, 3);
    ImGui::SetItemTooltip("Static holds its pose. Dynamic responds to forces and contacts. Kinematic follows explicit script targets or velocities.");
    ImGui::SeparatorText("Collider shape");
    const char* shapes[]={kBodyShapeNames[0],kBodyShapeNames[1],kBodyShapeNames[2],kBodyShapeNames[3],kBodyShapeNames[4],kBodyShapeNames[5],kBodyShapeNames[6],"Native 2D"};
    int shape=b.shape==SceneShape::Planar?7:int(b.shape);
    if(ImGui::Combo("Collider",&shape,shapes,8)) {
        doc.BeginEdit();b.shape=shape==7?SceneShape::Planar:SceneShape(shape);
        b.planar=shape==7?PlanarBox({.5,.5}):nullptr;doc.CommitEdit();
    }
    if(b.shape==SceneShape::Planar)DrawPlanarCollider(doc,o,state);

    if(b.shape==SceneShape::ConvexHull||b.shape==SceneShape::TriangleMesh){
        AssetField(doc,"Cooked geometry",b.collisionAsset,AssetType::Collision,false,state);
        ImGui::TextWrapped("Cook physical geometry in Assets. Rigid instances require unit scale. Triangle meshes are static surfaces and cannot be sensors.");
        if(state.assets){auto record=state.assets->Find(b.collisionAsset);CollisionAsset asset;std::string error;if(record&&LoadCollisionAsset(record->path,asset,error)){
            ImGui::Text("%zu vertices, %zu faces, %zu BVH nodes",asset.vertices.size(),asset.faces.size(),asset.nodes.size());
            if(ImGui::Button("Preview collision wireframe / normals / bounds")){state.collisionPreview.Clear();auto point=[&](glm::dvec3 p){return o.transform.position+o.transform.rotation*glm::vec3(p);};for(auto& face:asset.faces){for(unsigned k=0;k<3;++k)state.collisionPreview.Line(point(asset.vertices[face.vertices[k]]),point(asset.vertices[face.vertices[(k+1)%3]]),face.active[k]?glm::vec3(1,.5f,.2f):glm::vec3(.2f,.8f,1));auto center=(asset.vertices[face.vertices[0]]+asset.vertices[face.vertices[1]]+asset.vertices[face.vertices[2]])/3.;state.collisionPreview.Line(point(center),point(center+face.normal*.15),{.4f,1,.4f});}state.collisionPreview.Box(point((asset.minimum+asset.maximum)*.5),o.transform.rotation,glm::vec3((asset.maximum-asset.minimum)*.5),{1,1,.1f});}
            if(ImGui::Button("Clear collision preview"))state.collisionPreview.Clear();
        }}
    }
    if (b.shape == SceneShape::Box) DragVec3(doc, "Half extents##body", b.halfExtents, 0.01f);
    if (b.shape == SceneShape::Sphere) DragScalar(doc, "Radius##body", b.radius, 0.01f, 0.001f, 100000.0f);
    if (b.shape == SceneShape::Compound) {
        ImGui::Text("%zu children (one body; summed mass volumes)", b.compoundBoxes.size());
        for (std::size_t i = 0; i < b.compoundBoxes.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            const auto key=b.compoundBoxes[i].key?b.compoundBoxes[i].key:unsigned(i+1);
            if(!ImGui::TreeNodeEx("Child details",ImGuiTreeNodeFlags_DefaultOpen|ImGuiTreeNodeFlags_NoTreePushOnOpen,"Child %zu  (key %u)",i+1,key)){ImGui::PopID();continue;}
            // Folding changes only layout: the child widgets keep their
            // existing index scope and edit/undo identities.
            DragVec3(doc, "Center", b.compoundBoxes[i].localCenter, 0.005f);
            auto& child=b.compoundBoxes[i];int kind=child.type==ShapeType::Sphere?1:child.type==ShapeType::ConvexHull?2:0;const char* kinds[]={"Box","Sphere","Convex hull"};if(ImGui::Combo("Child type",&kind,kinds,3)){doc.BeginEdit();child.type=kind==1?ShapeType::Sphere:kind==2?ShapeType::ConvexHull:ShapeType::Box;if(kind==1&&child.radius<=0)child.radius=.1f;doc.CommitEdit();}
            glm::vec3 angles=glm::degrees(glm::eulerAngles(child.rotation));if(ImGui::DragFloat3("Local rotation (degrees)",&angles.x,.5f)){doc.BeginEdit();child.rotation=glm::normalize(glm::quat(glm::radians(angles)));doc.CommitEdit();}
            ImGui::Text("Child key: %u",child.key?child.key:unsigned(i+1));
            if(child.type==ShapeType::Box)DragVec3(doc, "Half extents", child.halfExtents, 0.005f);
            else if(child.type==ShapeType::Sphere)DragScalar(doc,"Radius",child.radius,.005f,.001f,100000.f);
            else AssetField(doc,"Hull geometry",child.assetId,AssetType::Collision,false,state);
            bool remove=ImGui::SmallButton("Remove child");
            if(remove){doc.BeginEdit();b.compoundBoxes.erase(b.compoundBoxes.begin()+i);doc.CommitEdit();ImGui::PopID();break;}
            ImGui::PopID();
        }
        if (ImGui::SmallButton("Add child box")) {
            doc.BeginEdit();
            CompoundBox child{glm::vec3(0),glm::vec3(.1f)};child.key=1;for(size_t i=0;i<b.compoundBoxes.size();++i)child.key=std::max(child.key,(b.compoundBoxes[i].key?b.compoundBoxes[i].key:unsigned(i+1))+1);b.compoundBoxes.push_back(child);
            doc.CommitEdit();
        }
    }

}

void DrawGravity(EditorDocument& doc, SceneObject& o, EditorPanelState&) {
    SceneGravityComponent& g = *o.gravity;
    Combo(doc, "Kind", g.kind, kGravityKindNames, 2);
    DragScalar(doc, "Magnitude (m/s^2)", g.magnitude, 0.01f, 0.0f, 1000.0f);
    Combo(doc, "Region", g.regionShape, kRegionNames, 2);
    if (g.regionShape == SceneRegionShape::Sphere) DragScalar(doc, "Region radius", g.regionRadius, 0.1f, 0.0f, 1.0e6f);
    else DragVec3(doc, "Region half extents", g.regionHalfExtents, 0.1f);
    ImGui::TextDisabled(g.kind == SceneGravityKind::Radial ? "Pulls toward this object's position."
                                                            : "Pulls along this object's local -Y.");
    ImGui::TextDisabled("Earlier objects win where regions overlap.");
}

void DrawLight(EditorDocument& doc, SceneObject& o, EditorPanelState&) {
    SceneLightComponent& l = *o.light;
    Checkbox(doc,"Enabled##light",l.enabled);
    Combo(doc, "Kind##light", l.kind, kLightKindNames, 2);
    DragScalar(doc,"Intensity##light",l.intensity,.1f,0,10000);
    Checkbox(doc,"Request shadow##light",l.castsShadow);
    DragScalar(doc,"Shadow priority##light",l.shadowPriority,.1f,-10000,10000);
    ImGui::TextDisabled("Spot shadows use a per-view relevance budget; points remain unshadowed.");
    ColorEdit(doc, "Color##light", l.color);
    DragScalar(doc, "Range", l.range, 0.1f, 0.0f, 10000.0f);
    if (l.kind == SceneLightKind::Spot) {
        DragScalar(doc, "Inner cone (deg)", l.innerConeDegrees, 0.5f, 0.0f, 89.0f);
        DragScalar(doc, "Outer cone (deg)", l.outerConeDegrees, 0.5f, 0.0f, 89.0f);
        ImGui::TextDisabled("A spot light faces this object's local -Z.");
    }
}







void DrawCelestial(EditorDocument& doc, SceneObject& o, EditorPanelState&) {
    DragScalar(doc, "Gravitational parameter (static, m^3/s^2)", o.celestial->gravitationalParameter, 10.0f, 0.0f, 1.0e30f);
    ImGui::TextDisabled("Dynamic: joins pairwise Newtonian gravity by its mass.");
}

void DrawAtmosphere(EditorDocument& doc, SceneObject& o, EditorPanelState&) {
    SceneAtmosphereComponent& a = *o.atmosphere;
    DragScalar(doc, "Reference radius (m)", a.referenceRadius, 0.1f, 0.001f, 1.0e9f);
    DragScalar(doc, "Top radius (m)", a.topRadius, 0.1f, 0.001f, 1.0e9f);
    DragScalar(doc, "Reference density (kg/m^3)", a.referenceDensity, 0.001f, 0.0f, 1000.0f);
    DragScalar(doc, "Polytropic exponent", a.polytropicExponent, 0.001f, 1.001f, 1.999f);
    DragScalar(doc, "Oxidizer mass fraction", a.oxidizerMassFraction, 0.001f, 0.0f, 1.0f);
    DragScalar(doc, "Reference temperature (K)", a.referenceTemperatureKelvin, 1.0f, 1.0f, 10000.0f);
    ImGui::TextDisabled("Needs a static Celestial gravitational parameter.");
}

void DrawCombustible(EditorDocument& doc, SceneObject& o, EditorPanelState&) {
    SceneCombustibleComponent& c = *o.combustible;
    DragScalar(doc, "Heat capacity (J/K)", c.heatCapacityJPerK, 1.0f, 0.001f, 1.0e9f);
    DragScalar(doc, "Fuel mass (kg)", c.initialFuelMassKg, 0.001f, 0.0f, 1.0e6f);
    DragScalar(doc, "Ignition temperature (K)", c.ignitionTemperatureK, 1.0f, 0.0f, 10000.0f);
    DragScalar(doc, "Max fuel rate (kg/s)", c.maximumFuelRateKgPerSecond, 0.0001f, 0.0f, 1000.0f);
    DragScalar(doc, "Radiative area (m^2)", c.radiativeAreaSquareMeters, 0.01f, 0.0f, 1.0e6f);
    DragScalar(doc, "Retained heat fraction", c.retainedCombustionHeatFraction, 0.01f, 0.0f, 1.0f);
}

void DrawFluidVolume(EditorDocument& doc, SceneObject& o, EditorPanelState&) {
    SceneFluidVolumeComponent& f = *o.fluidVolume;
        ImGui::TextWrapped("Experimental: conserved game-resolution water. Requires Project Settings > Experimental > Advanced Fluid Dynamics; Alpha remains locked.");
        DragScalar(doc,"Cell size (m)",f.spacing,.01f,.02f,10);
        int dimensions[3]={f.countX,f.countY,f.countZ};
        if(ImGui::DragInt3("Domain cells",dimensions,1,2,256)){f.countX=dimensions[0];f.countY=dimensions[1];f.countZ=dimensions[2];}
        TrackEdit(doc);
        const auto cells=std::int64_t(f.countX)*f.countY*f.countZ;
        ImGui::Text("Domain: %.2f x %.2f x %.2f m | %lld cells",f.countX*f.spacing,f.countY*f.spacing,f.countZ*f.spacing,(long long)cells);
        if(cells>262144)ImGui::TextWrapped("Too many cells. Reduce the domain dimensions or increase cell size before Play.");
        DragScalar(doc,"Initial fill height (m)",f.fillHeight,.05f,0,f.countY*f.spacing);
        DragScalar(doc,"Density (kg/m3)",f.density,1,1,10000);
        ImGui::ColorEdit3("Water colour",&f.color.x);TrackEdit(doc);
        DragScalar(doc,"Opacity",f.opacity,.01f,0,1);
        ImGui::TextWrapped("The owner is the bottom centre of a stationary domain. Rotate local +Y away from gravity. Cells below your resolution are approximate; use larger domains sparingly.");

}




void DrawBasicWater(EditorDocument& doc,SceneObject& o,EditorPanelState&){
    auto& w=*o.basicWater;Checkbox(doc,"Enabled",w.enabled);
    DragVec3(doc,"Half width / depth / half length",w.halfExtents,.1f);
    ColorEdit(doc,"Water colour",w.color);
    DragScalar(doc,"Wave amplitude (visual metres)",w.waveAmplitude,.01f,0,5);
    DragScalar(doc,"Wavelength (metres)",w.wavelength,.1f,.1f,10000);
    DragScalar(doc,"Wave speed",w.waveSpeed,.05f,-20,20);
    DragScalar(doc,"Opacity",w.opacity,.01f,0,1);
    DragVec3(doc,"Local swimming current",w.flow,.05f);
    DragScalar(doc,"Swimming density",w.density,10,1,10000);
    ImGui::TextWrapped("Lightweight prescribed volume: local +Y surface, water below it. Visual waves do not simulate flow or change the mean swimming volume. Scale must be one; use explicit extents. No rigid-body buoyancy or conserved quantity.");
    std::string error;if(!ValidateBasicWater(w,error))ImGui::TextColored({1,.4f,.3f,1},"%s",error.c_str());
}





void NavProfile(EditorDocument& doc,unsigned& profile,EditorPanelState& state){if(!state.project)return;CategoryRegistry registry;registry.names.clear();for(auto [id,p]:state.project->Settings().navigation.profiles)registry.names[id]=p.name;DrawCategoryLayer(doc,"Agent profile",profile,registry);}
void DrawNavSurface(EditorDocument& doc,SceneObject& o,EditorPanelState& state){if(o.landscape){DrawTerrainV2Navigation(doc,o,state);return;}auto& n=*o.navigationSurface;Checkbox(doc,"Enabled",n.enabled);NavProfile(doc,n.profile,state);Checkbox(doc,"Include dynamic bake sources",n.includeDynamic);if(state.project)DrawCategoryMask(doc,"Physical source layers",n.sources,state.project->Settings().classification.collision);DragVec3(doc,"Local bounds half extents",n.halfExtents);DragScalar(doc,"Cell size",n.cellSize,.01f,.02f,2);DragScalar(doc,"Cell height",n.cellHeight,.01f,.01f,1);DragInt(doc,"Tile size (cells)",n.tileSize,16,128);DragInt(doc,"Minimum region (cells)",n.minRegion,0,100);DragScalar(doc,"Simplification",n.simplification,.1f,.1f,10);AssetField(doc,"Baked navigation",n.asset,AssetType::Navigation,true,state);ImGui::TextWrapped("Object rotation defines the navigation frame; local +Y is its traversal up. Gravity is independent.");
 if(state.mode==EditorMode::Edit&&state.project&&state.assets){Scene resolved,flat;std::string error;NavigationGeometry geometry;const bool sources=ResolvePrefabs(doc.GetScene(),state.assets,resolved,error)&&FlattenHierarchy(resolved,flat,error);const SceneObject* surface=sources?flat.Find(o.id):nullptr;if(surface&&!n.asset.empty()){auto* record=state.assets->Find(n.asset);NavigationData data;if(record&&LoadNavigation(record->path,data,error)&&CollectNavigationGeometry(flat,*surface,state.project->Settings().navigation,geometry,error,state.assets))ImGui::Text("Bake: %s | %zu tile layers",data.fingerprint==geometry.fingerprint?"current":"STALE",data.layers.size());else ImGui::TextWrapped("Bake error: %s",error.c_str());}
 if(ImGui::Button("Bake selected surface"))BakeEditorNavigation(doc,o.id,state);
 if(ImGui::Button("Preview baked polygons")){auto* record=state.assets->Find(n.asset);NavigationData data;if(record&&surface&&LoadNavigation(record->path,data,error)){NavigationSystem preview(state.project->Settings().navigation);if(preview.LoadSurface(o.id,surface->transform,std::make_shared<NavigationData>(data),error)){state.navigationPreview.Clear();preview.Debug(state.navigationPreview);state.debug.navigation=true;state.status="Baked polygons displayed (preview snapshot).";}}if(!error.empty())state.status=error;}
 if(ImGui::Button("Clear baked reference")){doc.BeginEdit();n.asset.clear();doc.CommitEdit();state.status="Navigation reference cleared; asset retained for other users.";}}
 if(state.runtime){for(auto [id,error]:state.runtime->Navigation().Errors())if(id==o.id)ImGui::TextWrapped("Runtime: %s",error.c_str());}
}
void DrawNavAgent(EditorDocument& doc,SceneObject& o,EditorPanelState& state){auto& n=*o.navigationAgent;Checkbox(doc,"Enabled",n.enabled);NavProfile(doc,n.profile,state);if(state.project){DrawCategoryMask(doc,"Allowed navigation areas",n.areas,state.project->Settings().navigation.areas);for(auto [id,name]:state.project->Settings().navigation.areas.names){float cost=n.costs.count(id)?n.costs[id]:1;ImGui::PushID(int(id));if(ImGui::DragFloat((name+" cost").c_str(),&cost,.1f,1,100)){doc.BeginEdit();n.costs[id]=cost;doc.CommitEdit();}ImGui::PopID();}}Checkbox(doc,"Local avoidance",n.avoidance);DragScalar(doc,"Guidance speed",n.speed,.1f,.1f,100);DragScalar(doc,"Arrival distance",n.arrival,.05f,.01f,10);DragScalar(doc,"Corner tolerance",n.cornerDistance,.05f,.01f,5);DragScalar(doc,"Repath cadence (s)",n.repathSeconds,.05f,.02f,10);ImGui::TextWrapped("Guidance only. Project scripts feed CharacterMotor; navigation never moves this entity.");}
void DrawNavObstacle(EditorDocument& doc,SceneObject& o,EditorPanelState&){auto& n=*o.navigationObstacle;Checkbox(doc,"Enabled",n.enabled);Checkbox(doc,"Cylinder",n.cylinder);DragVec3(doc,"Box half extents",n.halfExtents);DragScalar(doc,"Radius",n.radius,.05f,.01f,100);DragScalar(doc,"Height",n.height,.05f,.01f,100);DragScalar(doc,"Update distance",n.updateDistance,.01f,.01f,10);ImGui::TextWrapped("Tile-cache carving; physical collision is a separate Body component.");}
void DrawNavLink(EditorDocument& doc,SceneObject& o,EditorPanelState& state){auto& n=*o.navigationLink;Checkbox(doc,"Enabled",n.enabled);Checkbox(doc,"Bidirectional",n.bidirectional);DragVec3(doc,"Local start",n.start);DragVec3(doc,"Local end",n.end);DragScalar(doc,"Endpoint tolerance",n.radius,.05f,.01f,5);if(state.project)DrawCategoryLayer(doc,"Navigation area",n.area,state.project->Settings().navigation.areas);ImGui::TextWrapped("Traversal is explicit script behaviour. No automatic teleport.");}
void DrawNavModifier(EditorDocument& doc,SceneObject& o,EditorPanelState& state){auto& n=*o.navigationModifier;Checkbox(doc,"Enabled",n.enabled);Checkbox(doc,"Exclude own collider source",n.excludeSource);Checkbox(doc,"Blocked volume",n.blocked);DragVec3(doc,"Local volume half extents",n.halfExtents);if(state.project)DrawCategoryLayer(doc,"Navigation area",n.area,state.project->Settings().navigation.areas);ImGui::TextWrapped("Volume is conservatively projected into each surface frame during bake.");}

template <typename T>
ComponentEditor Make(const char* name, char indicator, std::optional<T> SceneObject::*member,
                     void (*draw)(EditorDocument&, SceneObject&, EditorPanelState&)) {
    // Function pointers cannot capture, so the member pointer is threaded
    // through a static per-instantiation slot; each component type is a
    // distinct T, so each instantiation has its own slot.
    static std::optional<T> SceneObject::*slot = nullptr;
    slot = member;
    ComponentEditor editor;
    editor.name = name;
    editor.indicator = indicator;
    editor.has = [](const SceneObject& o) { return (o.*slot).has_value(); };
    editor.add = [](SceneObject& o) { o.*slot = T{}; };
    editor.remove = [](SceneObject& o) { (o.*slot).reset(); };
    editor.draw = draw;
    return editor;
}
ComponentEditor GridWaterEditor(){
    auto editor=Make<SceneFluidVolumeComponent>("Fluid volume",'W',&SceneObject::fluidVolume,DrawFluidVolume);
    editor.add=[](SceneObject& object){SceneFluidVolumeComponent component;
        component.spacing=.4f;component.countX=30;component.countY=12;component.countZ=20;object.fluidVolume=component;};
    return editor;
}
}  // namespace

namespace {
void DrawWeather(EditorDocument& doc,SceneObject& o,EditorPanelState&) {
    auto candidate=*o.weather;
    bool changed=false;
    changed|=ImGui::Checkbox("Enabled##weather",&candidate.enabled);
    if(ImGui::BeginCombo("Apply profile preset","Choose explicit starting values")) {
        for(const char* preset:{"sunny","cloudy","rain","snow","blizzard","thunderstorm","tornado","fog"}) {
            if(ImGui::Selectable(preset))changed|=WeatherPreset(preset,candidate.profile);
        }
        ImGui::EndCombo();
    }
    ImGui::TextWrapped("Presets populate the profile below. Regions and transitions use these ordinary values; no weather sequence runs automatically.");
    auto number=[&](const char* name,double& value,double step=.01) {
        changed|=ImGui::InputDouble(name,&value,step,step*10,"%.8g");
    };
    auto vector=[&](const char* name,glm::dvec3& value) {
        changed|=ImGui::InputScalarN(name,ImGuiDataType_Double,&value.x,3,nullptr,nullptr,"%.8g");
    };
    if(ImGui::TreeNodeEx("Region and coordinate frame",ImGuiTreeNodeFlags_DefaultOpen)) {
        changed|=ImGui::InputInt("Region priority",&candidate.priority);
        changed|=ImGui::InputScalar("Deterministic seed",ImGuiDataType_U32,&candidate.seed);
        int shape=int(candidate.regionShape);
        if(ImGui::Combo("Region shape",&shape,"Global\0Sphere\0Box\0")) {
            candidate.regionShape=WeatherRegionShape(shape);changed=true;
        }
        if(candidate.regionShape!=WeatherRegionShape::Global) {
            vector("Region centre (owner local metres)",candidate.regionCenter);
            if(candidate.regionShape==WeatherRegionShape::Sphere)number("Region radius (m)",candidate.regionRadius,1);
            else {
                vector("Region half extents (local m)",candidate.regionHalfExtents);
                glm::vec3 degrees=glm::degrees(glm::eulerAngles(glm::quat(candidate.regionRotation)));
                if(ImGui::DragFloat3("Region rotation (local degrees)",&degrees.x,.5f)) {
                    candidate.regionRotation=glm::dquat(glm::quat(glm::radians(degrees)));changed=true;
                }
            }
            number("Boundary blend distance (m)",candidate.blendDistance,1);
        }
        changed|=ImGui::Checkbox("Radial weather frame",&candidate.frame.radial);
        vector(candidate.frame.radial?"Planet centre (absolute metres)":"Ground-plane anchor (owner local m)",candidate.frame.center);
        if(candidate.frame.radial)number("Planet reference radius (m)",candidate.frame.radius,100);
        else vector("Frame up (owner local direction)",candidate.frame.up);
        vector("Frame north (owner local direction)",candidate.frame.north);
        number("Maximum weather altitude (m)",candidate.topAltitude,100);
        ImGui::TextWrapped("Region bounds follow this entity's transform. A flat frame follows its owner; a radial frame references an absolute planet centre. Weather does not select or replace gravity.");
        ImGui::TreePop();
    }
    auto& p=candidate.profile;
    if(ImGui::TreeNodeEx("Clouds",ImGuiTreeNodeFlags_DefaultOpen)) {
        number("Cloud coverage (0..1)",p.cloudCoverage);
        number("Cloud density (0..2)",p.cloudDensity);
        number("Cloud base altitude (m)",p.cloudBase,10);
        number("Cloud thickness (m)",p.cloudThickness,10);
        ImGui::TreePop();
    }
    if(ImGui::TreeNodeEx("Wind and gusts",ImGuiTreeNodeFlags_DefaultOpen)) {
        vector("Wind (owner local m/s)",p.wind);
        number("Peak gust speed (m/s)",p.gust,.5);
        ImGui::TextWrapped("The sampled field also drives opted-in physical bodies. Graphics quality does not change wind forces.");
        ImGui::TreePop();
    }
    if(ImGui::TreeNodeEx("Rain, snow and fog",ImGuiTreeNodeFlags_DefaultOpen)) {
        number("Rain intensity (0..1)",p.rain);
        number("Snow intensity (0..1)",p.snow);
        number("Fog intensity (0..1)",p.fog);
        ImGui::TreePop();
    }
    if(ImGui::TreeNode("Lightning and tornado")) {
        number("Lightning strikes per minute",p.lightningRate,1);
        number("Tornado intensity (0..1)",p.tornado);
        number("Tornado radius (m)",p.tornadoRadius,1);
        number("Tornado height (m)",p.tornadoHeight,10);
        number("Tornado travel speed (m/s)",p.tornadoSpeed,.5);
        changed|=ImGui::InputScalarN("Tornado north/east offset (m)",ImGuiDataType_Double,&candidate.tornadoOffset.x,2,nullptr,nullptr,"%.8g");
        vector("Tornado travel direction (owner local)",candidate.tornadoDirection);
        ImGui::TreePop();
    }
    static SceneObjectId errorOwner=0;static std::string error;
    if(errorOwner!=o.id){errorOwner=o.id;error.clear();}
    if(changed&&ValidateWeatherSettings(candidate,error)) {
        doc.BeginEdit();o.weather=std::move(candidate);doc.CommitEdit();error.clear();
    }
    if(!error.empty())ImGui::TextWrapped("Weather edit rejected: %s",error.c_str());
}

void DrawWeatherResponse(EditorDocument& doc,SceneObject& o,EditorPanelState& state) {
    auto candidate=*o.weatherResponse;
    bool changed=ImGui::Checkbox("Enabled##weather-response",&candidate.enabled);
    changed|=ImGui::InputDouble("Effective wind area (m2)",&candidate.area,.1,1,"%.6g");
    changed|=ImGui::InputDouble("Wind drag coefficient",&candidate.dragCoefficient,.1,1,"%.6g");
    changed|=ImGui::InputDouble("Maximum wind acceleration (m/s2)",&candidate.maxAcceleration,1,10,"%.6g");
    static SceneObjectId errorOwner=0;static std::string error;
    if(errorOwner!=o.id){errorOwner=o.id;error.clear();}
    if(changed&&ValidateWeatherResponseSettings(candidate,error)) {
        doc.BeginEdit();o.weatherResponse=candidate;doc.CommitEdit();error.clear();
    }
    if(state.project)DrawCategoryMask(doc,"Wind shelter collision layers",o.weatherResponse->occlusionMask,state.project->Settings().classification.collision);
    if(!error.empty())ImGui::TextWrapped("Wind response edit rejected: %s",error.c_str());
    ImGui::TextWrapped("Opt-in centred pressure drag on an ordinary dynamic body. This component does not create a rigid body or replace gravity. Avoid applying equivalent atmospheric drag twice.");
}

void DrawPlanet(EditorDocument& doc,SceneObject& o,EditorPanelState&){
 auto candidate=*o.planet;bool changed=false;
 changed|=ImGui::InputDouble("Radius (m)",&candidate.radius,10,1000,"%.8f");
 changed|=ImGui::InputScalarN("Absolute centre",ImGuiDataType_Double,&candidate.center.x,3);
 changed|=ImGui::InputDouble("Screen error (pixels)",&candidate.pixelError,.25,1);
 changed|=ImGui::InputDouble("Collision spacing (m)",&candidate.collisionSpacing,.05,.25);
 changed|=ImGui::InputDouble("Collision interest radius (m)",&candidate.collisionRadius,1,10);
 int budget=int(candidate.maxTiles);if(ImGui::InputInt("Visual leaf budget",&budget)){candidate.maxTiles=unsigned(std::max(6,budget));candidate.maxResident=std::max(candidate.maxResident,candidate.maxTiles*2);changed=true;}
 if(ImGui::TreeNode("Procedural height bands")){
  for(size_t i=0;i<candidate.bands.size();++i){ImGui::PushID(int(i));auto& b=candidate.bands[i];changed|=ImGui::InputDouble("Amplitude (m)",&b.amplitude,.1,10,"%.9g");changed|=ImGui::InputScalarN("Wave (radians/metre)",ImGuiDataType_Double,&b.wave.x,3);if(ImGui::SmallButton("Remove band")){candidate.bands.erase(candidate.bands.begin()+i);changed=true;ImGui::PopID();break;}ImGui::Separator();ImGui::PopID();}
  if(candidate.bands.size()<16&&ImGui::Button("Add height band")){candidate.bands.push_back({10,{.0001,0,.0001}});changed=true;}ImGui::TreePop();
 }
 if(ImGui::TreeNode("Authored height patches")){
  for(size_t i=0;i<candidate.patches.size();++i){ImGui::PushID(int(i));auto& p=candidate.patches[i];changed|=ImGui::InputScalarN("Model direction",ImGuiDataType_Double,&p.direction.x,3);changed|=ImGui::InputDouble("Half width (m)",&p.extent,1,10);
   static int cell[2]{0,0};ImGui::InputInt2("Sample column / row",cell);for(auto& c:cell)c=std::clamp(c,0,int(p.resolution)-1);changed|=ImGui::InputDouble("Sample elevation (m)",&p.heights[size_t(cell[1])*p.resolution+cell[0]],.01,.1,"%.9g");ImGui::Text("%u x %u embedded source samples",p.resolution,p.resolution);
   if(ImGui::SmallButton("Remove patch")){candidate.patches.erase(candidate.patches.begin()+i);changed=true;ImGui::PopID();break;}ImGui::Separator();ImGui::PopID();}
  if(candidate.patches.size()<64&&ImGui::Button("Add 5 x 5 height patch")){PlanetHeightPatch p;p.extent=20;p.resolution=5;p.heights.assign(25,0);candidate.patches.push_back(std::move(p));changed=true;}ImGui::TreePop();
 }
 static std::string error;if(changed&&ValidatePlanetSettings(candidate,error)){doc.BeginEdit();o.planet=candidate;doc.CommitEdit();}
 if(!error.empty())ImGui::TextWrapped("%s",error.c_str());
 ImGui::TextWrapped("Absolute cube-sphere. Owner transform must remain identity. Bands and raster samples are editable above; external data can use planet.surface in the scene text. Physical collision interests are supplied by project scripts; no global physics mesh is allocated.");
}
}
const std::vector<ComponentEditor>& ComponentEditorRegistry() {
    static const std::vector<ComponentEditor> registry = {
        TerrainV2ComponentEditor(),
        Make<WeatherSettings>("Weather region",'W',&SceneObject::weather,DrawWeather),
        Make<WeatherResponseSettings>("Weather wind response",'W',&SceneObject::weatherResponse,DrawWeatherResponse),
        Make<PlanetSettings>("Planet terrain",'T',&SceneObject::planet,DrawPlanet),
        Make<DeformableSettings>("Deformable",'D',&SceneObject::deformable,DrawDeformable),
        Make<BasicWaterSettings>("Basic water",'W',&SceneObject::basicWater,DrawBasicWater),
        Make<NavigationSurfaceSettings>("Navigation surface",'N',&SceneObject::navigationSurface,DrawNavSurface),
        Make<NavigationAgentSettings>("Navigation agent",'N',&SceneObject::navigationAgent,DrawNavAgent),
        Make<NavigationObstacleSettings>("Navigation obstacle",'N',&SceneObject::navigationObstacle,DrawNavObstacle),
        Make<NavigationLinkSettings>("Navigation link",'N',&SceneObject::navigationLink,DrawNavLink),
        Make<NavigationModifierSettings>("Navigation modifier",'N',&SceneObject::navigationModifier,DrawNavModifier),
        Make<SceneUIComponent>("Runtime UI",'U',&SceneObject::ui,DrawUIComponent),
        {"Scripts",'J',[](const SceneObject& o){return !o.scripts.empty();},[](SceneObject& o){o.scripts.push_back({1,"",true,"{}"});},[](SceneObject& o){o.scripts.clear();},DrawScripts},
        Make<ParticleEmitterSettings>("Particle emitter", 'E', &SceneObject::particleEmitter, DrawParticleEmitter),
        Make<SceneAudioEmitterComponent>("Audio emitter", 'U', &SceneObject::audioEmitter, DrawAudioEmitter),
        Make<SceneAudioZoneComponent>("Audio environment", 'Z', &SceneObject::audioZone, DrawAudioZone),
        Make<SceneAudioListenerComponent>("Audio listener", 'N', &SceneObject::audioListener, DrawAudioListener),
        Make<SceneRenderCameraComponent>("Render camera", 'K', &SceneObject::renderCamera, DrawRenderCamera),
        Make<SceneRenderComponent>("Render", 'R', &SceneObject::render, DrawRender),
        Make<GravitySelection>("Gravity selection",'G',&SceneObject::gravitySelection,DrawGravitySelection),
        Make<WheelSettings>("Wheel",'W',&SceneObject::wheel,DrawWheel),
        {"Powertrain",'P',[](const SceneObject& o){return o.powertrain.has_value();},[](SceneObject& o){o.powertrain=PowertrainSettings{};PowertrainShaft shaft;shaft.name="rotor";o.powertrain->shafts.push_back(shaft);},[](SceneObject& o){o.powertrain.reset();},DrawPowertrain},
        Make<CharacterMotorSettings>("Character motor",'M',&SceneObject::characterMotor,DrawCharacter),
        Make<SceneAnimationComponent>("Animation",'A',&SceneObject::animation,DrawAnimation),
        Make<SceneSocketComponent>("Visual socket",'S',&SceneObject::socket,DrawSocket),
        Make<RagdollDefinition>("Ragdoll",'R',&SceneObject::ragdoll,DrawRagdoll),
        Make<SceneJointComponent>("Joint",'J',&SceneObject::joint,DrawJoint),
        Make<SceneBodyComponent>("Body", 'B', &SceneObject::body, DrawBody),
        Make<SceneGravityComponent>("Gravity region", 'G', &SceneObject::gravity, DrawGravity),
        Make<SceneLightComponent>("Light", 'L', &SceneObject::light, DrawLight),
        Make<SceneCelestialComponent>("Celestial", 'C', &SceneObject::celestial, DrawCelestial),
        Make<SceneAtmosphereComponent>("Atmosphere", 'A', &SceneObject::atmosphere, DrawAtmosphere),
        Make<SceneCombustibleComponent>("Combustible", 'F', &SceneObject::combustible, DrawCombustible),
        GridWaterEditor(),
    };
    return registry;
}

void DrawTransformEditor(EditorDocument& doc, SceneObject& o) {
    ImGui::SeparatorText("Transform");
    DragVec3(doc, "Position", o.transform.position);
    DragRotation(doc, "Rotation (deg)", o.transform.rotation);
    DragVec3(doc, "Scale", o.transform.scale, 0.01f);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scale applies to mesh rendering only; physical shape dimensions are configured in their component.");
}

std::string ComponentIndicators(const SceneObject& object) {
    std::string out;
    for (const ComponentEditor& editor : ComponentEditorRegistry()) {
        if (editor.has(object)) out += editor.indicator;
    }
    return out;
}

void DrawCategoryLayer(EditorDocument& doc,const char* label,unsigned& value,const CategoryRegistry& registry){
    auto it=registry.names.find(value);const std::string preview=it==registry.names.end()?"[unregistered ID "+std::to_string(value)+"]":it->second;
    ImGui::TextWrapped("%s",label);
    ImGui::SetNextItemWidth(-1);
    ImGui::PushOverrideID(ImGui::GetID(label));
    if(ImGui::BeginCombo("",preview.c_str())){
        for(const auto& [id,name]:registry.names)if(ImGui::Selectable(name.c_str(),id==value)){doc.BeginEdit();value=id;doc.CommitEdit();}
        ImGui::EndCombo();
    }
    ImGui::PopID();
}
void DrawCategoryMask(EditorDocument& doc,const char* label,CategoryMask& mask,const CategoryRegistry& registry,bool allowAll){
    if(ImGui::TreeNode(label)){
        if(allowAll&&ImGui::Button("All (including future layers)")){doc.BeginEdit();mask=kAllCategories;doc.CommitEdit();}
        if(ImGui::Button("None")){doc.BeginEdit();mask=0;doc.CommitEdit();}
        for(const auto& [id,name]:registry.names){bool selected=(mask&CategoryBit(id))!=0;ImGui::PushID(int(id));
            if(ImGui::Checkbox(name.c_str(),&selected)){doc.BeginEdit();if(selected)mask|=CategoryBit(id);else mask&=~CategoryBit(id);doc.CommitEdit();}
            ImGui::PopID();
        }
        if(!allowAll&&(mask&~registry.ActiveMask()))ImGui::TextColored(ImVec4(1,.3f,.2f,1),"Contains retired/unregistered tags");
        ImGui::TreePop();
    }
}

bool BakeEditorNavigation(EditorDocument& doc,SceneObjectId id,EditorPanelState& state){
    if(state.mode!=EditorMode::Edit||!state.project||!state.assets){state.status="Open an editable project before baking";return false;}
    Scene resolved,flat;std::string error;NavigationData data;
    if(!ResolvePrefabs(doc.GetScene(),state.assets,resolved,error)||!FlattenHierarchy(resolved,flat,error)){state.status=error;return false;}
    auto* surface=flat.Find(id);if(!surface||!surface->navigationSurface){state.status="Selected entity has no navigation surface";return false;}
    if(!BakeNavigation(flat,*surface,state.project->Settings().navigation,data,error,state.assets)){state.status=error;return false;}
    if(surface->landscape){
        NavigationSystem preview(state.project->Settings().navigation);
        if(!preview.LoadSurface(id,surface->transform,std::make_shared<NavigationData>(data),error)){state.status=error;return false;}
        state.navigationPreview.Clear();preview.Debug(state.navigationPreview);state.debug.navigation=true;
        state.status="Live terrain navigation preview: "+std::to_string(data.layers.size())+" layers. Play rebuilds after source edits; no static asset is written.";
        return true;
    }
    auto path=std::filesystem::path(state.project->AssetsDir())/"navigation"/("surface-"+std::to_string(id)+".judasnav");
    std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);if(ec){state.status=ec.message();return false;}
    if(!SaveNavigation(path.string(),data,error)){state.status=error;return false;}
    auto* existing=state.assets->FindByRelativePath(std::filesystem::relative(path,state.project->RootDir()).generic_string());AssetRecord asset;
    if(existing)asset=*existing;else if(!state.assets->Track(path.string(),asset,error)){state.status=error;return false;}
    doc.BeginEdit();doc.GetScene().Find(id)->navigationSurface->asset=asset.id;doc.CommitEdit();if(state.resources)state.resources->Invalidate(asset.id);
    state.navigationPreview.Clear();NavigationSystem preview(state.project->Settings().navigation);if(preview.LoadSurface(id,surface->transform,std::make_shared<NavigationData>(data),error))preview.Debug(state.navigationPreview);state.debug.navigation=true;
    state.status="Baked "+std::to_string(data.layers.size())+" tile layers. Save scene to persist reference.";return true;
}
