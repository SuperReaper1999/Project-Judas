#include "SkeletonAuthoring.h"
#include <set>
#include "CollisionAsset.h"
#include "NavigationAsset.h"
#include "Prefab.h"
#include "RuntimeWorld.h"
#include "ComponentEditors.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include "ModelLoader.h"
#include "SceneFingerprint.h"
#include "ScriptSystem.h"

#include <glm/gtc/quaternion.hpp>

#include "imgui.h"

#include "AssetDatabase.h"
#include "Project.h"
#include "EditorPanels.h"
#include "CollisionAsset.h"
#include "EditorWidgets.h"

namespace {
const char* const kShapeNames[] = {"box", "sphere", "compound", "mesh", "terrain", "convex hull", "static triangle mesh"};
const char* const kBodyShapeNames[] = {"box", "sphere", "compound", "(mesh: not a body shape)", "terrain", "convex hull", "static triangle mesh"};
const char* const kMotionNames[] = {"static", "dynamic"};
const char* const kGravityKindNames[] = {"radial", "uniform"};
const char* const kRegionNames[] = {"sphere", "box"};
const char* const kLightKindNames[] = {"point", "spot"};
const char* const kVehicleGravityNames[] = {"local", "celestial"};
const char* const kViewNames[] = {"third-person", "first-person"};

// The inspector's asset fields: a combo over the project's assets of one
// type (name shown, id stored), a drop target for the Asset Browser's
// drag payload, and an honest status line for the stored id.
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
    LabelledCombo(doc, label, assetId, labels, values, allowNone);
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
    if (!record) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "  unknown asset id %s", assetId.c_str());
    } else if (record->missing) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "  file missing: %s", record->relativePath.c_str());
    } else {
        ImGui::TextDisabled("  id %s", assetId.c_str());
    }
}

void DrawUIComponent(EditorDocument& doc,SceneObject& o,EditorPanelState& state){
    auto& u=*o.ui;AssetField(doc,"UI document",u.asset,AssetType::UI,false,state);Checkbox(doc,"Enabled",u.enabled);
    char name[128];std::snprintf(name,sizeof(name),"%s",u.name.c_str());if(ImGui::InputText("Runtime document name",name,sizeof(name))){doc.BeginEdit();u.name=name;doc.CommitEdit();}
    ImGui::TextWrapped("Select the UI asset in the Asset Browser to edit its hierarchy. Runtime changes never edit this source.");
}
void DrawCharacter(EditorDocument& doc,SceneObject& object,EditorPanelState& state){
    auto& m=*object.characterMotor;Checkbox(doc,"Motor enabled",m.enabled);
    DragScalar(doc,"Radius",m.radius,.01f);DragScalar(doc,"Cylinder half height",m.halfHeight,.01f);
    DragScalar(doc,"Step height",m.stepHeight,.01f);DragScalar(doc,"Support probe",m.supportDistance,.01f);DragScalar(doc,"Skin",m.skin,.001f);
    DragScalar(doc,"Maximum slope degrees",m.maxSlopeDegrees,1);DragScalar(doc,"Gravity multiplier",m.gravityScale,.1f);DragScalar(doc,"Reorientation degrees/sec",m.reorientationDegreesPerSecond,1);
    DragScalar(doc,"Interaction mass kg",m.interactionMass,1);DragScalar(doc,"Max push impulse",m.maxPushImpulse,1);
    DragVec3(doc,"Shape offset",m.offset);
    if(state.project){const auto& categories=state.project->Settings().classification;
        DrawCategoryLayer(doc,"Collision layer",m.collisionLayer,categories.collision);DrawCategoryMask(doc,"Collision mask",m.collisionMask,categories.collision);
        DrawCategoryMask(doc,"Required collider tags",m.requiredTags,categories.tags,false);DrawCategoryMask(doc,"Excluded collider tags",m.excludedTags,categories.tags,false);}
    ImGui::TextWrapped("Script fixedUpdate supplies world velocity and extra acceleration. No input, camera or gameplay state belongs to this component.");
}
void DrawScripts(EditorDocument& doc,SceneObject& o,EditorPanelState& state){
    size_t remove=o.scripts.size();
    for(size_t i=0;i<o.scripts.size();++i){auto& slot=o.scripts[i];ImGui::PushID(static_cast<int>(i));
        ImGui::Text("Behaviour %zu (slot %llu)",i+1,static_cast<unsigned long long>(slot.id));
        AssetField(doc,"Script asset",slot.asset,AssetType::Script,false,state);Checkbox(doc,"Enabled",slot.enabled);
        if(state.assets&&!slot.asset.empty()){
            struct Metadata {std::filesystem::file_time_type time;std::string schema,error;};
            static std::map<std::string,Metadata> cache;
            const auto* record=state.assets->Find(slot.asset);
            if(record&&!record->missing){std::error_code ec;auto stamp=std::filesystem::last_write_time(record->path,ec);
                auto it=cache.find(record->path);if(it==cache.end()||it->second.time!=stamp){Metadata m;m.time=stamp;
                    ScriptSystem::Inspect(*state.assets,slot.asset,m.schema,m.error);it=cache.insert_or_assign(record->path,std::move(m)).first;}
                auto& metadata=it->second;std::vector<ScriptProperty> fields;std::string error=metadata.error;
                if(error.empty()&&ScriptSystem::ReadProperties(metadata.schema,slot.properties,fields,error)){
                    for(auto& f:fields){bool changed=false;
                        if(f.type=="boolean")changed=ImGui::Checkbox(f.name.c_str(),&f.boolean);
                        else if(f.type=="number")changed=ImGui::InputDouble(f.name.c_str(),&f.number);
                        else if(f.type=="entity"){SceneObjectId id=0;try{id=std::stoull(f.text);}catch(...){}const auto* target=doc.GetScene().Find(id);if(ImGui::BeginCombo(f.name.c_str(),target?target->name.c_str():"None / missing")){if(ImGui::Selectable("None",id==0)){f.text="0";changed=true;}for(auto& candidate:doc.GetScene().Objects())if(ImGui::Selectable((candidate.name+" ##"+std::to_string(candidate.id)).c_str(),candidate.id==id)){f.text=std::to_string(candidate.id);changed=true;}ImGui::EndCombo();}}
                        else {char value[1024];std::snprintf(value,sizeof(value),"%s",f.text.c_str());changed=ImGui::InputText(f.name.c_str(),value,sizeof(value));if(changed)f.text=value;}
                        if(changed){doc.BeginEdit();slot.properties=ScriptSystem::WriteProperties(fields);doc.CommitEdit();}
                    }
                }
                if(!error.empty())ImGui::TextWrapped("Script metadata: %s",error.c_str());
            }
        }
        if(ImGui::Button("Remove behaviour"))remove=i;
        ImGui::PopID();
    }
    if(remove<o.scripts.size()){doc.BeginEdit();o.scripts.erase(o.scripts.begin()+remove);doc.CommitEdit();}
    if(ImGui::Button("Add behaviour")){doc.BeginEdit();uint64_t id=1;for(const auto& slot:o.scripts)id=std::max(id,slot.id+1);o.scripts.push_back({id,"",true,"{}"});doc.CommitEdit();}
}

void DrawParticleEmitter(EditorDocument& doc,SceneObject& o,EditorPanelState& state){
    auto& e=*o.particleEmitter;
    Checkbox(doc,"Enabled",e.enabled);Checkbox(doc,"Continuous/loop",e.loop);
    Checkbox(doc,"Local space",e.localSpace);Checkbox(doc,"Sample Judas gravity",e.useGravity);
    DragScalar(doc,"Rate / second",e.rate,1,0,100000);DragScalar(doc,"Lifetime seconds",e.lifetime,0.05f,0.01f,3600);
    DragScalar(doc,"Start size",e.size,0.01f,0,1000);DragScalar(doc,"End size",e.endSize,0.01f,0,1000);
    auto integer=[&](const char* name,int& v,int lo,int hi){int next=v;if(ImGui::DragInt(name,&next,1,lo,hi)){doc.BeginEdit();v=std::clamp(next,lo,hi);doc.CommitEdit();}};
    integer("Capacity",e.maxParticles,1,100000);integer("Startup burst",e.burst,0,e.maxParticles);
    DragVec3(doc,"Position spread",e.spread,0.01f);DragVec3(doc,"Initial velocity",e.velocity,0.05f);
    DragVec3(doc,"Velocity variation",e.velocityVariation,0.05f);DragVec3(doc,"Acceleration",e.acceleration,0.05f);
    auto color=[&](const char* label,glm::vec4& v){auto next=v;if(ImGui::ColorEdit4(label,&next.x)){doc.BeginEdit();v=next;doc.CommitEdit();}};
    color("Start color/alpha",e.color);color("End color/alpha",e.endColor);
    unsigned seed=e.seed;if(ImGui::InputScalar("Seed",ImGuiDataType_U32,&seed)){doc.BeginEdit();e.seed=seed;doc.CommitEdit();}
    AssetField(doc,"Particle texture",e.textureAsset,AssetType::Texture,true,state);
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
    DragScalar(doc, "Vertical FOV (deg)", c.verticalFovDegrees, 0.1f, 1.0f, 178.0f);
    DragScalar(doc, "Near plane", c.nearPlane, 0.01f, 0.001f, 100000.0f);
    DragScalar(doc, "Far plane", c.farPlane, 1.0f, 0.002f, 1000000.0f);
    ImGui::TextDisabled("Local -Z forward / +Y up. Live output in Play.");
}

void DrawRender(EditorDocument& doc, SceneObject& o, EditorPanelState& state) {
    SceneRenderComponent& r = *o.render;
    Combo(doc, "Shape", r.shape, kShapeNames, 5);
    if(ImGui::Button("Add material slot")){doc.BeginEdit();if(r.materials.size()<64)r.materials.push_back({});doc.CommitEdit();}
    for(size_t i=0;i<r.materials.size();++i){ImGui::PushID(int(i));auto& slot=r.materials[i];ImGui::Text("Material slot %zu (shared asset)",i);AssetField(doc,"Shared material",slot.asset,AssetType::Material,true,state);
        MaterialDefinition source;source.model=MaterialModel::Legacy;if(state.assets){if(auto* record=state.assets->Find(slot.asset)){std::string error;LoadMaterial(record->path,source,error);}}
        auto resolved=ApplyMaterialOverride(source,slot.overrides);
        auto set=[&](bool changed,auto apply){if(changed){doc.BeginEdit();apply();doc.CommitEdit();}};
        auto colour=resolved.baseColor;set(ImGui::ColorEdit4("Instance base colour (linear)",&colour.x),[&]{slot.overrides.baseColor=colour;});
        float rough=resolved.roughness;set(ImGui::SliderFloat("Instance roughness",&rough,0,1),[&]{slot.overrides.roughness=rough;});float metal=resolved.metallic;set(ImGui::SliderFloat("Instance metallic",&metal,0,1),[&]{slot.overrides.metallic=metal;});
        auto emission=resolved.emissive;set(ImGui::ColorEdit3("Instance emission (linear)",&emission.x),[&]{slot.overrides.emissive=emission;});float intensity=resolved.emissiveIntensity;set(ImGui::DragFloat("Instance emission intensity",&intensity,.05f,0,100000),[&]{slot.overrides.emissiveIntensity=intensity;});
        auto uvScale=resolved.uvScale;set(ImGui::DragFloat2("Instance UV scale",&uvScale.x,.05f),[&]{slot.overrides.uvScale=uvScale;});auto uvOffset=resolved.uvOffset;set(ImGui::DragFloat2("Instance UV offset",&uvOffset.x,.01f),[&]{slot.overrides.uvOffset=uvOffset;});
        if(slot.overrides.baseColor||slot.overrides.roughness||slot.overrides.metallic||slot.overrides.emissive||slot.overrides.emissiveIntensity)ImGui::TextColored(ImVec4(1,.8f,.3f,1),"INSTANCE OVERRIDE (does not edit source)");
        if(ImGui::Button("Revert all slot overrides")){doc.BeginEdit();slot.overrides={};doc.CommitEdit();}ImGui::PopID();
    }

    if (r.shape == SceneShape::Box) DragVec3(doc, "Half extents", r.halfExtents, 0.01f);
    if (r.shape == SceneShape::Sphere) DragScalar(doc, "Radius", r.radius, 0.01f, 0.001f, 100000.0f);
    ColorEdit(doc, "Color", r.color);
    DragScalar(doc, "Alpha", r.alpha, 0.01f, 0.0f, 1.0f);
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
    if (r.shape == SceneShape::Terrain) ImGui::TextDisabled("Geometry comes from the terrain body.");
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
    ImGui::TextDisabled("Layers are resolved in list order; empty mask affects all joints.");
    ImGui::TextDisabled("Use a self-contained GLB/glTF mesh. Pose is independent of playback.");
    if(!object.render||object.render->shape!=SceneShape::Mesh)ImGui::TextColored(ImVec4(1,.3f,.2f,1),"Requires a mesh Render component");
}

void DrawRagdoll(EditorDocument& doc,SceneObject& object,EditorPanelState& state){
    auto frame=[&](const char* label,glm::quat& q){auto degrees=glm::degrees(glm::eulerAngles(glm::normalize(q)));if(ImGui::DragFloat3(label,&degrees.x,.5f))q=glm::normalize(glm::quat(glm::radians(degrees)));TrackEdit(doc);};
    auto& r=*object.ragdoll;Checkbox(doc,"Ragdoll enabled",r.enabled);Checkbox(doc,"Start in ragdoll",r.playOnStart);Checkbox(doc,"Self collision",r.selfCollision);
    ImGui::TextWrapped("Joint keys are imported hierarchy paths or unique names. Parent mapping must precede children. Bodies use normal physics; no pose motors. Positive uniform scales only.");
    if(ImGui::Button("Add mapped bone")&&r.bones.size()<32){doc.BeginEdit();RagdollBone b;b.joint="Root";r.bones.push_back(b);doc.CommitEdit();}
    if(object.render&&state.resources){auto asset=state.resources->TryGetSkeletal(object.render->meshAsset);if(asset&&ImGui::CollapsingHeader("Skeleton selection / mapping tools")){static std::set<std::string> selected;static std::string owner;static char search[256]{};static float thickness=.15f,resistance=.05f,lower=-.75f,upper=.75f;static bool editLimits=false,editResistance=false;static RagdollDefinition preview;static bool hasPreview=false;auto key=object.render->meshAsset+":"+std::to_string(object.id);if(owner!=key){owner=key;selected.clear();hasPreview=false;state.skeletonFitPreview.reset();}ImGui::InputText("Filter joints",search,sizeof(search));ImGui::BeginChild("Joint hierarchy",{0,160},true);for(auto n:asset->skeleton.order){auto& name=asset->skeleton.names[n];if(*search&&name.find(search)==std::string::npos)continue;if(ImGui::Selectable(name.c_str(),selected.count(name))){if(!ImGui::GetIO().KeyCtrl)selected.clear();if(selected.count(name))selected.erase(name);else selected.insert(name);hasPreview=false;state.skeletonFitPreview.reset();}}ImGui::EndChild();state.skeletonPicked.assign(selected.begin(),selected.end());ImGui::InputFloat("Fit thickness (joint-local m)",&thickness);if(ImGui::Button("Preview fit selected chain")){std::vector<std::string> keys(selected.begin(),selected.end());std::string error;hasPreview=FitSkeletonChain(asset->skeleton,r,keys,thickness,preview,error);state.skeletonFitPreview=hasPreview?std::optional<RagdollDefinition>(preview):std::nullopt;state.skeletonFitOwner=object.id;state.skeletonFitGeneration=doc.Generation();state.status=hasPreview?"Preview ready. Existing mass, limits and resistance are retained.":error;}if(hasPreview&&state.skeletonFitGeneration==doc.Generation()){ImGui::Text("Preview: %zu mapped bodies; rest-pose fit is a starting estimate",preview.bones.size());if(ImGui::Button("Accept fitted mappings")){doc.BeginEdit();r=preview;doc.CommitEdit();hasPreview=false;state.skeletonFitPreview.reset();}}
    ImGui::Checkbox("Set limits on selected mappings",&editLimits);ImGui::InputFloat("Lower radians/metres",&lower);ImGui::InputFloat("Upper radians/metres",&upper);ImGui::Checkbox("Set rotational resistance",&editResistance);ImGui::InputFloat("Resistance N m s/rad",&resistance);if(ImGui::Button("Apply selected constraint fields")){auto candidate=r;for(auto& b:candidate.bones)if(selected.count(b.joint)){if(editLimits){b.constraint.limits=true;b.constraint.lower=lower;b.constraint.upper=upper;}if(editResistance)b.constraint.rotationalResistance=resistance;}std::string error;if(!ValidRagdollDefinition(candidate,error))state.status=error;else{doc.BeginEdit();r=std::move(candidate);doc.CommitEdit();}}
    }}
    size_t remove=r.bones.size();
    for(size_t i=0;i<r.bones.size();++i){auto& b=r.bones[i];ImGui::PushID(int(i));if(ImGui::TreeNode("Mapped bone","%s",b.joint.c_str())){
        JointPicker(doc,object,state,"Skeleton joint",b.joint);JointPicker(doc,object,state,"Physical parent key",b.parent,true);const char* shapes[]={"Box","Sphere"};Combo(doc,"Shape",b.shape,shapes,2);
        DragVec3(doc,"Shape offset",b.offset,.01f);frame("Shape orientation",b.orientation);DragVec3(doc,"Half extents",b.halfExtents,.01f);DragScalar(doc,"Radius",b.radius,.01f);DragScalar(doc,"Mass",b.mass,.1f);DragScalar(doc,"Friction",b.friction,.01f);DragScalar(doc,"Restitution",b.restitution,.01f);
        if(state.project){DrawCategoryLayer(doc,"Collision layer",b.collisionLayer,state.project->Settings().classification.collision);DrawCategoryMask(doc,"Collision mask",b.collisionMask,state.project->Settings().classification.collision);}
        Checkbox(doc,"Suppress parent collision",b.suppressParentCollision);Checkbox(doc,"Capture anchors from current pose",b.autoAnchors);
        const char* types[]={"Fixed","Hinge","Ball","Slider"};Combo(doc,"Constraint",b.constraint.type,types,4);Checkbox(doc,"Constraint enabled",b.constraint.enabled);Checkbox(doc,"Limits",b.constraint.limits);DragScalar(doc,"Lower",b.constraint.lower,.01f);DragScalar(doc,"Upper",b.constraint.upper,.01f);DragScalar(doc,"Rotational resistance (N m s/rad)",b.constraint.rotationalResistance,.01f,0,100000);
        if(!b.autoAnchors){DragVec3(doc,"Child anchor",b.constraint.anchorA,.01f);DragVec3(doc,"Parent anchor",b.constraint.anchorB,.01f);}
        frame("Child frame",b.constraint.frameA);frame("Parent frame",b.constraint.frameB);
        if(ImGui::Button("Remove mapping"))remove=i;
        ImGui::TreePop();}ImGui::PopID();
    }
    if(remove<r.bones.size()){doc.BeginEdit();r.bones.erase(r.bones.begin()+remove);doc.CommitEdit();}
    std::string error;if(!ValidRagdollDefinition(r,error))ImGui::TextColored(ImVec4(1,.3f,.2f,1),"%s",error.c_str());
}

void DrawJoint(EditorDocument& doc, SceneObject& object, EditorPanelState&) {
    auto& joint=*object.joint;auto& settings=joint.settings;
    const char* names[]={"Fixed","Hinge","Ball/socket","Slider"};
    Combo(doc,"Joint type",settings.type,names,4);
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
    ImGui::TextDisabled("Local frame X is the hinge/slider axis. Hinge values are radians.");
    if(settings.type==JointType::Hinge||settings.type==JointType::Slider){
        Checkbox(doc,"Limits",settings.limits);DragScalar(doc,"Lower",settings.lower);DragScalar(doc,"Upper",settings.upper);
        Checkbox(doc,"Motor",settings.motor);DragScalar(doc,"Target speed",settings.speed);
        DragScalar(doc,"Maximum force / torque",settings.maxForce,.1f,0,100000);
        Checkbox(doc,"Spring",settings.spring);DragScalar(doc,"Rest coordinate",settings.rest);
        DragScalar(doc,"Stiffness",settings.stiffness,.1f,0,100000);DragScalar(doc,"Damping",settings.damping,.1f,0,100000);
    }
    DragScalar(doc,"Rotational resistance (N m s/rad)",settings.rotationalResistance,.01f,0,100000);
    if(!ValidJointSettings(settings)||joint.bodyA==0||joint.bodyA==joint.bodyB)ImGui::TextColored(ImVec4(1,.3f,.2f,1),"Invalid joint settings or body references");
}

void DrawBody(EditorDocument& doc, SceneObject& o, EditorPanelState& state) {
    SceneBodyComponent& b = *o.body;
    Checkbox(doc,"Sensor (events, no response)",b.sensor);
    Checkbox(doc,"Collider enabled",b.enabled);
    if(state.project){DrawCategoryLayer(doc,"Collision layer",b.collisionLayer,state.project->Settings().classification.collision);DrawCategoryMask(doc,"Collision mask",b.collisionMask,state.project->Settings().classification.collision);}
    Combo(doc, "Motion", b.motion, kMotionNames, 2);
    Combo(doc, "Collider", b.shape, kBodyShapeNames, 7);
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
    if (b.shape == SceneShape::Terrain) StringCombo(doc, "Surface", b.terrainSurface, state.terrainSurfaces, false);
    if (b.shape == SceneShape::Compound) {
        ImGui::Text("%zu children (one body; summed mass volumes)", b.compoundBoxes.size());
        for (std::size_t i = 0; i < b.compoundBoxes.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
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
    ImGui::Text("%zu fluid cavities (body-local boxes)", b.fluidCavities.size());
    ImGui::PushID("fluid-cavities");
    for (std::size_t i = 0; i < b.fluidCavities.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        auto& cavity = b.fluidCavities[i];
        DragVec3(doc, "Interior center", cavity.localCenter, 0.005f);
        DragScalar(doc, "Interior half X", cavity.halfExtents.x, 0.005f, 0.001f, 100000.0f);
        DragScalar(doc, "Interior half Y", cavity.halfExtents.y, 0.005f, 0.001f, 100000.0f);
        DragScalar(doc, "Interior half Z", cavity.halfExtents.z, 0.005f, 0.001f, 100000.0f);
        const bool remove = ImGui::SmallButton("Remove cavity");
        ImGui::PopID();
        if (remove) {
            doc.BeginEdit();
            b.fluidCavities.erase(b.fluidCavities.begin() + static_cast<std::ptrdiff_t>(i));
            doc.CommitEdit();
            break;
        }
    }
    if (ImGui::SmallButton("Add fluid cavity")) {
        doc.BeginEdit();
        b.fluidCavities.emplace_back();
        doc.CommitEdit();
    }
    ImGui::PopID();
    if (!b.fluidCavities.empty()) ImGui::TextDisabled("Bounds must describe the resolved interior, up to its opening.");
    if (b.motion == SceneBodyMotion::Dynamic) {
        DragScalar(doc, "Mass (kg)", b.mass, 0.1f, 0.001f, 1.0e30f);
        DragVec3(doc, "Initial velocity", b.initialLinearVelocity, 0.05f);
        Checkbox(doc, "Pickable (G/H)", b.pickable);
        Checkbox(doc, "Managed by fidelity policy (M29)", b.managed);
        if (!b.managed) ImGui::TextDisabled("Unmanaged: always fully simulated; the policy never touches it.");
    }
    AssetField(doc,"Physical material (empty = legacy)",b.physicalMaterial,AssetType::PhysicalMaterial,true,state);
    if(!b.physicalMaterial.empty())Checkbox(doc,"Override shared physical coefficients",b.physicalMaterialOverride);
    if(b.physicalMaterial.empty()||b.physicalMaterialOverride){
    DragScalar(doc, "Friction", b.friction, 0.01f, 0.0f, 5.0f);
    DragScalar(doc, "Restitution", b.restitution, 0.01f, 0.0f, 1.0f);
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
    Combo(doc, "Kind##light", l.kind, kLightKindNames, 2);
    ColorEdit(doc, "Color##light", l.color);
    DragScalar(doc, "Range", l.range, 0.1f, 0.0f, 10000.0f);
    if (l.kind == SceneLightKind::Spot) {
        DragScalar(doc, "Inner cone (deg)", l.innerConeDegrees, 0.5f, 0.0f, 89.0f);
        DragScalar(doc, "Outer cone (deg)", l.outerConeDegrees, 0.5f, 0.0f, 89.0f);
        ImGui::TextDisabled("A spot light faces this object's local -Z.");
    }
}

void DrawDoor(EditorDocument& doc, SceneObject& o, EditorPanelState& state) {
    ImGui::TextDisabled("Historical gameplay adapter; modern projects use ordinary components + JS.");
    if(state.project){auto& d=*o.door;DrawCategoryLayer(doc,"Collision layer",d.collisionLayer,state.project->Settings().classification.collision);DrawCategoryMask(doc,"Collision mask",d.collisionMask,state.project->Settings().classification.collision);}
    DragVec3(doc, "Hinge axis (local)", o.door->localHingeAxis, 0.01f);
    DragScalar(doc, "Open angle (deg)", o.door->openAngleDegrees, 0.5f, 0.0f, 180.0f);
    DragScalar(doc, "Angular speed (deg/s)", o.door->angularSpeedDegreesPerSecond, 1.0f, 0.0f, 3600.0f);
    ImGui::TextDisabled("Needs a box Render for the panel; hinge at the object's transform.");
}

void DrawLightSwitch(EditorDocument& doc, SceneObject& o, EditorPanelState&) {
    ImGui::TextDisabled("Historical gameplay adapter; modern projects use ordinary components + JS.");
    SceneLightSwitchComponent& s = *o.lightSwitch;
    DragVec3(doc, "Hinge axis (local)##sw", s.localHingeAxis, 0.01f);
    DragScalar(doc, "Toggle angle (deg)", s.toggleAngleDegrees, 0.5f, 0.0f, 180.0f);
    DragScalar(doc, "Angular speed (deg/s)##sw", s.angularSpeedDegreesPerSecond, 1.0f, 0.0f, 3600.0f);
    DragVec3(doc, "Lamp offset (local)", s.lampLocalOffset, 0.05f);
    ColorEdit(doc, "Lamp color", s.lampColor);
    DragScalar(doc, "Lamp range", s.lampRange, 0.1f, 0.0f, 1000.0f);
}

void DrawVehicle(EditorDocument& doc, SceneObject& o, EditorPanelState&) {
    ImGui::TextDisabled("Historical gameplay adapter; modern projects use ordinary components + JS.");
    SceneVehicleComponent& v = *o.vehicle;
    Combo(doc, "Gravity source", v.gravity, kVehicleGravityNames, 2);
    Checkbox(doc, "Headlight", v.headlight);
    Checkbox(doc, "Navigation lights", v.navigationLights);
    DragScalar(doc, "Drag coefficient", v.dragCoefficient, 0.01f, 0.0f, 10.0f);
    Checkbox(doc, "Start with pilot attached", v.initialPilotAttached);
    ImGui::TextDisabled("Needs a dynamic box Body. One vehicle per scene.");
}

void DrawCelestial(EditorDocument& doc, SceneObject& o, EditorPanelState&) {
    DragScalar(doc, "Gravitational parameter (static, m^3/s^2)", o.celestial->gravitationalParameter, 10.0f, 0.0f, 1.0e30f);
    DragScalar(doc, "Operator thrust (N)", o.celestial->operatorThrustForce, 1.0e9f, 0.0f, 1.0e30f);
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
    DragScalar(doc, "Particle spacing (m)", f.spacing, 0.001f, 0.001f, 100.0f);
    int count[3] = {f.countX, f.countY, f.countZ};
    if (ImGui::DragInt3("Lattice count", count, 1.0f, 0, 1000)) {
        f.countX = count[0]; f.countY = count[1]; f.countZ = count[2];
    }
    TrackEdit(doc);
    Checkbox(doc, "Emitter (hold B)", f.emitter);
    if (f.emitter) {
        DragVec3(doc, "Emitter offset (local)", f.emitterLocalOffset, 0.05f);
        DragInt(doc, "Max particles", f.maxParticles, 0, 100000);
    }
    ImGui::TextDisabled("Lattice grows along local +Y from the object.");
}

void DrawPlayerStart(EditorDocument& doc, SceneObject& o, EditorPanelState& state) {
    ImGui::TextDisabled("Historical gameplay adapter; modern projects use ordinary components + JS.");
    if(state.project){auto& p=*o.playerStart;DrawCategoryLayer(doc,"Collision layer",p.collisionLayer,state.project->Settings().classification.collision);DrawCategoryMask(doc,"Collision mask",p.collisionMask,state.project->Settings().classification.collision);}
    DragScalar(doc, "Yaw (deg)", o.playerStart->yawDegrees, 0.5f, -360.0f, 360.0f);
    Combo(doc, "View", o.playerStart->view, kViewNames, 2);
    DragScalar(doc, "Density (kg/m^3)", o.playerStart->density, 1.0f, 0.001f, 1.0e6f);
    DragScalar(doc, "Fluid drag (1/s)", o.playerStart->fluidDrag, 0.05f, 0.0f, 1000.0f);
    DragScalar(doc, "Swim acceleration (m/s^2)", o.playerStart->swimAcceleration, 0.1f, 0.0f, 1000.0f);
    ImGui::TextDisabled("Exactly one object may carry a player start.");
}

void PreviewLiquid(const LiquidBasinData& data,const SceneObject& o,double volume,EditorPanelState& state){state.liquidPreview.Clear();MeshData surface;const double q=LiquidInverse(data,volume);glm::vec3 minimum(1e30f),maximum(-1e30f);for(auto& t:data.geometry.cells){for(auto v:t){minimum=glm::min(minimum,glm::vec3(v));maximum=glm::max(maximum,glm::vec3(v));}LiquidClip(t,{data.equilibrium.Coordinate(t[0]),data.equilibrium.Coordinate(t[1]),data.equilibrium.Coordinate(t[2]),data.equilibrium.Coordinate(t[3])},q,&surface);}state.liquidPreview.Box(o.transform.position+o.transform.rotation*(minimum+maximum)*.5f,o.transform.rotation,(maximum-minimum)*.5f,{.15f,.4f,.9f});for(size_t i=0;i+2<surface.vertices.size();i+=3)for(int j=0;j<3;++j)state.liquidPreview.Line(o.transform.position+o.transform.rotation*surface.vertices[i+j].position,o.transform.position+o.transform.rotation*surface.vertices[i+(j+1)%3].position,{.1f,.5f,.95f});}
void LiquidNumber(EditorDocument& doc,const char* label,double& value){double temp=value;if(ImGui::InputDouble(label,&temp,.001,.1,"%.8g")&&std::isfinite(temp)&&temp>=0){doc.BeginEdit();value=temp;doc.CommitEdit();}}
void DrawDeformable(EditorDocument& doc,SceneObject& o,EditorPanelState& state){
    auto& d=*o.deformable;
    Checkbox(doc,"Enabled",d.enabled);Checkbox(doc,"Cloth self-contact",d.selfContact);
    AssetField(doc,"Simulation / render binding",d.asset,AssetType::Deformable,false,state);
    int substeps=int(d.substeps),iterations=int(d.iterations);
    if(DragInt(doc,"Substeps",substeps,1,16))d.substeps=unsigned(substeps);
    if(DragInt(doc,"Iterations",iterations,1,16))d.iterations=unsigned(iterations);
    if(state.project){DrawCategoryLayer(doc,"Collision layer",d.collisionLayer,state.project->Settings().classification.collision);DrawCategoryMask(doc,"Collision mask",d.collisionMask,state.project->Settings().classification.collision);}
    auto& m=d.material;
    LiquidNumber(doc,"Density (kg/m2 cloth; kg/m3 solid)",m.density);
    LiquidNumber(doc,"Stretch compliance",m.stretchCompliance);LiquidNumber(doc,"Shear compliance",m.shearCompliance);
    LiquidNumber(doc,"Bend compliance",m.bendCompliance);LiquidNumber(doc,"Volume compliance",m.volumeCompliance);
    LiquidNumber(doc,"Damping (1/s)",m.damping);LiquidNumber(doc,"Contact thickness (m)",m.thickness);LiquidNumber(doc,"Friction",m.friction);
    LiquidNumber(doc,"Air drag (1/s)",m.airDrag);glm::vec3 air(m.airVelocity);if(DragVec3(doc,"World air velocity",air))m.airVelocity=air;
    LiquidNumber(doc,"Yield strain (0 disables)",m.yieldStrain);LiquidNumber(doc,"Plastic rate (1/s)",m.plasticRate);LiquidNumber(doc,"Maximum plastic strain",m.maximumPlasticStrain);
    std::string error;std::shared_ptr<const DeformableAsset> asset;
    if(state.resources&&!d.asset.empty()){state.resources->RequestDeformable(d.asset);asset=state.resources->GetDeformable(d.asset,error);}
    if(!error.empty())ImGui::TextWrapped("Asset: %s",error.c_str());
    if(asset){
        ImGui::Text("%zu nodes | %zu triangles | %zu tetrahedra",asset->nodes.size(),asset->triangles.size(),asset->tetrahedra.size());
        if(ImGui::Button("Preview wireframe and selected groups")){state.deformablePreview.Clear();auto point=[&](glm::dvec3 p){return o.transform.position+o.transform.rotation*glm::vec3(p);};for(auto edge:asset->edges)state.deformablePreview.Line(point(asset->nodes[edge.x]),point(asset->nodes[edge.y]),{.1f,.7f,1});for(auto& attachment:d.attachments){auto it=asset->groups.find(attachment.group);if(it!=asset->groups.end())for(auto n:it->second)state.deformablePreview.Box(point(asset->nodes[n]),glm::quat(1,0,0,0),glm::vec3(.025f),{1,.7f,.1f});}}
        if(asset->fracture){
            auto cook=*asset->fracture;
            ImGui::Text("Fracture: %zu parts / %zu interfaces (%s)",cook.parts.size(),cook.bonds.size(),cook.rigid?"rigid M45 cells":"deformable cohesive cells");
            if(ImGui::BeginCombo("Interface preview",state.fractureInterface.empty()?"all interfaces":state.fractureInterface.c_str())){if(ImGui::Selectable("all interfaces",state.fractureInterface.empty()))state.fractureInterface.clear();for(const auto& bond:cook.bonds)if(ImGui::Selectable(bond.key.c_str(),state.fractureInterface==bond.key))state.fractureInterface=bond.key;ImGui::EndCombo();}
            bool changed=ImGui::InputDouble("Tensile strength (Pa)",&cook.tension)|ImGui::InputDouble("Shear strength (Pa)",&cook.shear)|ImGui::InputDouble("Compression strength (0 = no crushing)",&cook.compression)|ImGui::InputDouble("Cohesive compliance (m/Pa)",&cook.compliance);
            if(changed&&state.mode==EditorMode::Edit){auto baked=*asset;baked.fracture=std::make_shared<FractureCook>(cook);auto* record=state.assets->Find(d.asset);if(record&&PrepareDeformableAsset(baked,error)){std::ofstream output(record->path,std::ios::binary);auto bytes=EncodeDeformableAsset(baked);output.write(bytes.data(),std::streamsize(bytes.size()));output.close();if(output){state.resources->Invalidate(d.asset);state.status="Fracture material baked; asset content changes save compatibility.";}}else state.status=error;}
            if(ImGui::Button("Preview physical cells / interfaces / interiors")){state.deformablePreview.Clear();auto point=[&](glm::dvec3 v){return o.transform.position+o.transform.rotation*glm::vec3(v);};for(auto& part:cook.parts)state.deformablePreview.Box(point(part.proxyCenter),o.transform.rotation,glm::vec3(part.proxyHalf),{.2f,.8f,.6f});for(auto& bond:cook.bonds){if(!state.fractureInterface.empty()&&state.fractureInterface!=bond.key)continue;auto c=point(bond.centroid);state.deformablePreview.Line(c,c+o.transform.rotation*glm::vec3(bond.normal)*.35f,{1,.3f,.1f});}for(unsigned f=0;f<asset->triangles.size();++f)if(cook.faceBond[f]>=0&&(state.fractureInterface.empty()||cook.bonds[cook.faceBond[f]].key==state.fractureInterface)){auto face=asset->triangles[f];for(int k=0;k<3;++k)state.deformablePreview.Line(point(asset->nodes[face[k]]),point(asset->nodes[face[(k+1)%3]]),{1,.6f,.1f});}}
        }
        for(auto& [name,nodes]:asset->groups){ImGui::PushID(name.c_str());ImGui::Text("%s: %zu nodes",name.c_str(),nodes.size());ImGui::SameLine();if(ImGui::Button("Attach group")){doc.BeginEdit();auto found=std::find_if(d.attachments.begin(),d.attachments.end(),[&](auto& a){return a.group==name;});if(found==d.attachments.end()){DeformableAttachment a;a.group=name;d.attachments.push_back(a);}doc.CommitEdit();}ImGui::PopID();}
        TextField(doc,"New group name",state.deformableGroup);DragVec3(doc,"Local selection minimum",state.deformableSelectionMin);DragVec3(doc,"Local selection maximum",state.deformableSelectionMax);
        if(state.mode==EditorMode::Edit&&ImGui::Button("Bake node group from selection box")){
            auto changed=*asset;auto& group=changed.groups[state.deformableGroup];group.clear();for(unsigned i=0;i<changed.nodes.size();++i)if(glm::all(glm::greaterThanEqual(changed.nodes[i],glm::dvec3(state.deformableSelectionMin)))&&glm::all(glm::lessThanEqual(changed.nodes[i],glm::dvec3(state.deformableSelectionMax))))group.push_back(i);
            auto* record=state.assets->Find(d.asset);if(group.empty())state.status="Selection contains no nodes.";else if(record&&PrepareDeformableAsset(changed,error)){std::ofstream output(record->path,std::ios::binary);auto bytes=EncodeDeformableAsset(changed);output.write(bytes.data(),std::streamsize(bytes.size()));output.close();if(output){state.resources->Invalidate(d.asset);state.status="Named group baked; existing saves require matching asset content.";}else state.status="Failed to write deformable asset.";}else state.status=error;
        }
    }
    for(size_t i=0;i<d.attachments.size();){auto& a=d.attachments[i];ImGui::PushID(int(i));ImGui::Separator();ImGui::Text("Attachment %s",a.group.c_str());Checkbox(doc,"Attachment enabled",a.enabled);const char* kinds[]={"world (authored root anchor)","rigid body (finite mass)","skeleton joint (prescribed)"};Combo(doc,"Target kind",a.kind,kinds,3);
        if(a.kind!=DeformableAttachment::Kind::World){std::vector<std::string> labels,values;for(auto& target:doc.GetScene().Objects())if(a.kind==DeformableAttachment::Kind::Body?bool(target.body):bool(target.animation)){labels.push_back(target.name);values.push_back(std::to_string(target.id));}std::string id=std::to_string(a.target);if(LabelledCombo(doc,"Target entity",id,labels,values,false))a.target=std::stoull(id);if(a.kind==DeformableAttachment::Kind::Bone)TextField(doc,"Stable skeleton joint key",a.joint);}
        glm::vec3 offset(a.offset);if(DragVec3(doc,"Target-local offset",offset))a.offset=offset;
        bool remove=ImGui::Button("Remove attachment");ImGui::PopID();if(remove){doc.BeginEdit();d.attachments.erase(d.attachments.begin()+i);doc.CommitEdit();}else ++i;
    }
    if(state.mode==EditorMode::Edit&&state.project&&state.assets&&ImGui::CollapsingHeader("Create/import deformable asset")){
        TextField(doc,"Assets-relative destination",state.deformableDestination);DragInt(doc,"Sheet columns / block X",state.deformableColumns,1,48);DragInt(doc,"Sheet rows / block Y",state.deformableRows,1,48);DragInt(doc,"Render subdivision / block Z",state.deformableSubdivision,1,8);DragVec3(doc,"Size (metres)",state.deformableSize);TextField(doc,"Cloth source mesh asset ID",state.deformableSource);
        int action=0;if(ImGui::Button("Create sheet"))action=1;ImGui::SameLine();if(ImGui::Button("Create tetrahedral block"))action=2;if(ImGui::Button("Import indexed cloth mesh"))action=3;
        if(ImGui::Button("Create cohesive fracture block"))action=4;
        ImGui::SameLine();if(ImGui::Button("Create rigid fracture block"))action=5;TextField(doc,"Partition source (assets-relative path)",state.deformableSource);if(ImGui::Button("Import explicit volumetric partition"))action=6;
        if(action)BakeEditorDeformable(doc,o.id,state,action);
    }
    if(!ValidDeformableSettings(d,error))ImGui::TextWrapped("Invalid configuration: %s",error.c_str());
    if(state.runtime){auto* runtime=state.runtime->RuntimeDeformable(o.id,error);if(runtime)ImGui::Text("Runtime: %s | %zu contacts | min J %.3f | %s",runtime->sleeping?"sleeping":"active",runtime->stats.contacts,runtime->stats.minimumJacobian,runtime->error.c_str());else ImGui::TextWrapped("Runtime: %s",error.c_str());}
    ImGui::TextWrapped("Nodes simulate in world space; root is initial placement. Explicit reset moves the entire state. Bone pins consume the fixed-step resolved pose. Render materials use normal Render slots.");
}

void LiquidMaterialEditor(EditorDocument& doc,LiquidMaterial& material){TextField(doc,"Liquid material identity",material.id);LiquidNumber(doc,"Density kg/m3",material.density);}
void DrawLiquidBasin(EditorDocument& doc,SceneObject& o,EditorPanelState& state){auto& b=*o.liquidBasin;Checkbox(doc,"Enabled",b.enabled);AssetField(doc,"Physical cavity",b.geometry,AssetType::Liquid,true,state);AssetField(doc,"Baked capacity",b.asset,AssetType::Liquid,true,state);LiquidMaterialEditor(doc,b.material);LiquidNumber(doc,"Initial volume m3",b.initialVolume);LiquidNumber(doc,"Curve volume tolerance m3",b.volumeTolerance);LiquidNumber(doc,"Height tolerance metres",b.heightTolerance);
 Checkbox(doc,"Dynamic surface (rebake required)",b.surface.enabled);
 if(b.surface.enabled){int columns=int(b.surface.columns),rows=int(b.surface.rows);
  if(ImGui::SliderInt("Surface columns",&columns,2,32)){doc.BeginEdit();b.surface.columns=unsigned(columns);doc.CommitEdit();}
  if(ImGui::SliderInt("Surface rows",&rows,2,32)){doc.BeginEdit();b.surface.rows=unsigned(rows);doc.CommitEdit();}
  LiquidNumber(doc,"Surface friction /s",b.surface.friction);LiquidNumber(doc,"Solve relative tolerance",b.surface.tolerance);
  int iterations=int(b.surface.iterations),budget=int(b.surface.parcelBudget);
  if(ImGui::SliderInt("Pressure iteration budget",&iterations,4,512)){doc.BeginEdit();b.surface.iterations=unsigned(iterations);doc.CommitEdit();}
  if(ImGui::SliderInt("Splash parcel budget",&budget,0,1024)){doc.BeginEdit();b.surface.parcelBudget=unsigned(budget);doc.CommitEdit();}
  LiquidNumber(doc,"Splash threshold m/s",b.surface.splashSpeed);LiquidNumber(doc,"Splash fraction",b.surface.splashFraction);
  ImGui::TextWrapped("Fixed volume cells over the physical cavity; no vertical dynamic grid. Static mode remains available. Settings require rebake.");
 }
 if(state.mode==EditorMode::Edit&&state.project&&state.assets){if(ImGui::Button("Bake liquid capacity"))BakeEditorLiquid(doc,o.id,state);if(ImGui::Button("Clear capacity reference")){doc.BeginEdit();b.asset.clear();doc.CommitEdit();}
 auto* asset=state.assets->Find(b.asset);LiquidBasinData data;std::string error;Scene resolved,flat;if(asset&&LoadLiquidBasin(asset->path,data,error)&&ResolvePrefabs(doc.GetScene(),state.assets,resolved,error)&&FlattenHierarchy(resolved,flat,error)){auto* basin=flat.Find(o.id);auto hash=basin?LiquidSourceFingerprint(flat,*basin,*state.assets,error):std::string{};ImGui::Text("%s | %.3f litres capacity | %zu samples",hash==data.fingerprint?"Current":"STALE",data.capacity*1000,data.curve.size());std::vector<float> volumes;for(auto point:data.curve)volumes.push_back(float(point.volume*1000));ImGui::PlotLines("Capacity curve (L)",volumes.data(),int(volumes.size()));ImGui::Text("Solved coordinate %.6f; radial bound %.6f m",LiquidInverse(data,b.initialVolume),data.coordinateError);if(ImGui::Button("Preview basin bounds/surface")&&basin)PreviewLiquid(data,*basin,b.initialVolume,state);}else if(asset)ImGui::TextWrapped("%s",error.c_str());}
 if(state.runtime){auto h=state.runtime->Liquids().Handle(o.id);if(auto* r=state.runtime->Liquids().Get(h))ImGui::Text("Runtime %.3f L | coordinate %.6f",r->volume*1000,r->q);}
}
void DrawLiquidContainer(EditorDocument& doc,SceneObject& o,EditorPanelState& state){auto& a=*o.liquidContainer;Checkbox(doc,"Enabled",a.enabled);AssetField(doc,"Physical cavity",a.geometry,AssetType::Liquid,true,state);LiquidMaterialEditor(doc,a.material);LiquidNumber(doc,"Initial volume m3",a.initialVolume);LiquidNumber(doc,"Opening area m2",a.openingArea);LiquidNumber(doc,"Discharge coefficient",a.discharge);for(size_t i=0;i<a.opening.size();++i){ImGui::PushID(int(i));glm::vec3 temp(a.opening[i]);if(ImGui::DragFloat3("Opening vertex (local metres)",&temp.x,.01f)){doc.BeginEdit();a.opening[i]=temp;doc.CommitEdit();}if(a.opening.size()>3&&ImGui::Button("Remove vertex")){doc.BeginEdit();a.opening.erase(a.opening.begin()+i);doc.CommitEdit();ImGui::PopID();break;}ImGui::PopID();}if(a.opening.size()<32&&ImGui::Button("Add opening vertex")){doc.BeginEdit();a.opening.push_back(a.opening.back());doc.CommitEdit();}ImGui::TextWrapped("Vented air; local uniform gravity. Tilt recomputes actual clipped cavity volume.");if(state.runtime)if(auto* r=state.runtime->Liquids().Get(state.runtime->Liquids().Handle(o.id)))ImGui::Text("%.3f L | stable capacity %.3f L",r->volume*1000,r->stableCapacity*1000);}
void DrawLiquidConnection(EditorDocument& doc,SceneObject& o,EditorPanelState&){auto& c=*o.liquidConnection;Checkbox(doc,"Enabled",c.enabled);Checkbox(doc,"Bidirectional",c.bidirectional);auto ref=[&](const char* name,SceneObjectId& id){auto* selected=doc.GetScene().Find(id);if(ImGui::BeginCombo(name,selected?selected->name.c_str():"Select basin")){for(auto& b:doc.GetScene().Objects())if(b.liquidBasin&&ImGui::Selectable((b.name+" ##"+std::to_string(b.id)).c_str(),b.id==id)){doc.BeginEdit();id=b.id;doc.CommitEdit();}ImGui::EndCombo();}};ref("Source",c.source);ref("Destination",c.destination);LiquidNumber(doc,"Opening area m2",c.openingArea);LiquidNumber(doc,"Discharge",c.discharge);ImGui::TextWrapped("Entity position is the saddle/spill point; its actual gravity potential determines opening.");}
void DrawLiquidInteraction(EditorDocument& doc,SceneObject& o,EditorPanelState&){Checkbox(doc,"Enabled",o.liquidInteraction->enabled);LiquidNumber(doc,"Bulk drag coefficient",o.liquidInteraction->drag);ImGui::TextWrapped("Opt-in box/compound hydrostatics; keep this body outside particle-liquid ownership.");}

void NavProfile(EditorDocument& doc,unsigned& profile,EditorPanelState& state){if(!state.project)return;CategoryRegistry registry;registry.names.clear();for(auto [id,p]:state.project->Settings().navigation.profiles)registry.names[id]=p.name;DrawCategoryLayer(doc,"Agent profile",profile,registry);}
void DrawNavSurface(EditorDocument& doc,SceneObject& o,EditorPanelState& state){auto& n=*o.navigationSurface;Checkbox(doc,"Enabled",n.enabled);NavProfile(doc,n.profile,state);Checkbox(doc,"Include dynamic bake sources",n.includeDynamic);if(state.project)DrawCategoryMask(doc,"Physical source layers",n.sources,state.project->Settings().classification.collision);DragVec3(doc,"Local bounds half extents",n.halfExtents);DragScalar(doc,"Cell size",n.cellSize,.01f,.02f,2);DragScalar(doc,"Cell height",n.cellHeight,.01f,.01f,1);DragInt(doc,"Tile size (cells)",n.tileSize,16,128);DragInt(doc,"Minimum region (cells)",n.minRegion,0,100);DragScalar(doc,"Simplification",n.simplification,.1f,.1f,10);AssetField(doc,"Baked navigation",n.asset,AssetType::Navigation,true,state);ImGui::TextWrapped("Object rotation defines the navigation frame; local +Y is its traversal up. Gravity is independent.");
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
}  // namespace

const std::vector<ComponentEditor>& ComponentEditorRegistry() {
    static const std::vector<ComponentEditor> registry = {
        Make<DeformableSettings>("Deformable",'D',&SceneObject::deformable,DrawDeformable),
        Make<LiquidBasinSettings>("Liquid basin",'Q',&SceneObject::liquidBasin,DrawLiquidBasin),
        Make<LiquidContainerSettings>("Liquid container",'C',&SceneObject::liquidContainer,DrawLiquidContainer),
        Make<LiquidConnectionSettings>("Liquid connection",'S',&SceneObject::liquidConnection,DrawLiquidConnection),
        Make<LiquidInteractionSettings>("Liquid interaction",'B',&SceneObject::liquidInteraction,DrawLiquidInteraction),
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
        Make<CharacterMotorSettings>("Character motor",'M',&SceneObject::characterMotor,DrawCharacter),
        Make<SceneAnimationComponent>("Animation",'A',&SceneObject::animation,DrawAnimation),
        Make<SceneSocketComponent>("Visual socket",'S',&SceneObject::socket,DrawSocket),
        Make<RagdollDefinition>("Ragdoll",'R',&SceneObject::ragdoll,DrawRagdoll),
        Make<SceneJointComponent>("Joint",'J',&SceneObject::joint,DrawJoint),
        Make<SceneBodyComponent>("Body", 'B', &SceneObject::body, DrawBody),
        Make<SceneGravityComponent>("Gravity region", 'G', &SceneObject::gravity, DrawGravity),
        Make<SceneLightComponent>("Light", 'L', &SceneObject::light, DrawLight),
        Make<SceneDoorComponent>("Door", 'D', &SceneObject::door, DrawDoor),
        Make<SceneLightSwitchComponent>("Light switch", 'S', &SceneObject::lightSwitch, DrawLightSwitch),
        Make<SceneVehicleComponent>("Vehicle", 'V', &SceneObject::vehicle, DrawVehicle),
        Make<SceneCelestialComponent>("Celestial", 'C', &SceneObject::celestial, DrawCelestial),
        Make<SceneAtmosphereComponent>("Atmosphere", 'A', &SceneObject::atmosphere, DrawAtmosphere),
        Make<SceneCombustibleComponent>("Combustible", 'F', &SceneObject::combustible, DrawCombustible),
        Make<SceneFluidVolumeComponent>("Fluid volume", 'W', &SceneObject::fluidVolume, DrawFluidVolume),
        Make<ScenePlayerStartComponent>("Player start", 'P', &SceneObject::playerStart, DrawPlayerStart),
    };
    return registry;
}

void DrawTransformEditor(EditorDocument& doc, SceneObject& o) {
    if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) return;
    DragVec3(doc, "Position", o.transform.position);
    // Rotation is edited as yaw/pitch/roll degrees for humans; the
    // authored value stays a quaternion.
    glm::vec3 euler = glm::degrees(glm::eulerAngles(glm::normalize(o.transform.rotation)));
    if (ImGui::DragFloat3("Rotation (deg)", &euler.x, 0.5f, 0.0f, 0.0f, "%.3g")) {
        o.transform.rotation = glm::normalize(glm::quat(glm::radians(euler)));
    }
    TrackEdit(doc);
    DragVec3(doc, "Scale", o.transform.scale, 0.01f);
    ImGui::TextDisabled("Scale applies to mesh rendering only.");
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
    if(ImGui::BeginCombo(label,preview.c_str())){
        for(const auto& [id,name]:registry.names)if(ImGui::Selectable(name.c_str(),id==value)){doc.BeginEdit();value=id;doc.CommitEdit();}
        ImGui::EndCombo();
    }
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
    auto path=std::filesystem::path(state.project->AssetsDir())/"navigation"/("surface-"+std::to_string(id)+".judasnav");
    std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);if(ec){state.status=ec.message();return false;}
    if(!SaveNavigation(path.string(),data,error)){state.status=error;return false;}
    auto* existing=state.assets->FindByRelativePath(std::filesystem::relative(path,state.project->RootDir()).generic_string());AssetRecord asset;
    if(existing)asset=*existing;else if(!state.assets->Track(path.string(),asset,error)){state.status=error;return false;}
    doc.BeginEdit();doc.GetScene().Find(id)->navigationSurface->asset=asset.id;doc.CommitEdit();if(state.resources)state.resources->Invalidate(asset.id);
    state.navigationPreview.Clear();NavigationSystem preview(state.project->Settings().navigation);if(preview.LoadSurface(id,surface->transform,std::make_shared<NavigationData>(data),error))preview.Debug(state.navigationPreview);state.debug.navigation=true;
    state.status="Baked "+std::to_string(data.layers.size())+" tile layers. Save scene to persist reference.";return true;
}

bool BakeEditorLiquid(EditorDocument& doc,SceneObjectId id,EditorPanelState& state){
 if(state.mode!=EditorMode::Edit||!state.project||!state.assets){state.status="Open an editable project to bake";return false;}
 Scene resolved,flat;std::string error;if(!ResolvePrefabs(doc.GetScene(),state.assets,resolved,error)||!FlattenHierarchy(resolved,flat,error)){state.status=error;return false;}auto* o=flat.Find(id);if(!o||!o->liquidBasin){state.status="Select a liquid basin";return false;}LiquidGeometry geometry;GravityEquilibrium eq;std::string hash;LiquidBasinData data;if(!PrepareLiquidBake(flat,*o,*state.assets,geometry,eq,hash,error)||!BakeLiquidBasin(geometry,eq,o->liquidBasin->volumeTolerance,o->liquidBasin->heightTolerance,hash,data,error)){state.status=error;return false;}
 if(!BakeLiquidSurface(data,o->liquidBasin->surface,data.dynamicSurface,error,&geometry)){state.status=error;return false;}
 auto path=std::filesystem::path(state.project->AssetsDir())/"liquid"/("basin-"+std::to_string(id)+".judasbasin");std::filesystem::create_directories(path.parent_path());if(!SaveLiquidBasin(path.string(),data,error)){state.status=error;return false;}AssetRecord asset;auto* previous=state.assets->FindByRelativePath(std::filesystem::relative(path,state.project->RootDir()).generic_string());if(previous)asset=*previous;else if(!state.assets->Track(path.string(),asset,error)){state.status=error;return false;}doc.BeginEdit();doc.GetScene().Find(id)->liquidBasin->asset=asset.id;doc.CommitEdit();if(state.resources)state.resources->Invalidate(asset.id);PreviewLiquid(data,*o,o->liquidBasin->initialVolume,state);
 state.status="Baked "+std::to_string(data.capacity*1000)+" L capacity / "+std::to_string(data.curve.size())+" samples. Save scene.";return true;
}

bool BakeEditorDeformable(EditorDocument& doc,SceneObjectId id,EditorPanelState& state,int action){
    auto* object=doc.GetScene().Find(id);std::string error;
    if(!object||!object->deformable||!state.project||!state.assets||action<1||action>6){state.status="Select a deformable and an open project.";return false;}
    try{
            auto path=std::filesystem::path(state.project->AssetsDir())/state.deformableDestination;auto relative=path.lexically_normal().lexically_relative(state.project->AssetsDir());if(relative.empty()||relative.is_absolute()||*relative.begin()==".."||path.extension()!=".judasdeform")throw std::runtime_error("Choose a .judasdeform path inside project assets.");
            if(std::filesystem::exists(path))throw std::runtime_error("Destination exists; choose a new name.");
            DeformableAsset baked;
            if(action==1)baked=MakeDeformableSheet(unsigned(state.deformableColumns),unsigned(state.deformableRows),state.deformableSize.x,state.deformableSize.y,unsigned(state.deformableSubdivision));
            else if(action==2)baked=MakeDeformableBlock({state.deformableColumns,state.deformableRows,state.deformableSubdivision},glm::dvec3(state.deformableSize));
            else if(action==4||action==5)baked=MakeFractureBlock({state.deformableColumns,state.deformableRows,state.deformableSubdivision},glm::dvec3(state.deformableSize),action==5);
            else if(action==6){auto path=std::filesystem::path(state.project->AssetsDir())/state.deformableSource;auto relative=path.lexically_normal().lexically_relative(state.project->AssetsDir());if(relative.empty()||relative.is_absolute()||*relative.begin()=="..")throw std::runtime_error("Partition source must be inside project assets.");std::ifstream input(path);std::string text{std::istreambuf_iterator<char>(input),{}};if(!input||!ImportFracturePartition(text,baked,error))throw std::runtime_error("Partition import: "+error);}
            else{auto* source=state.assets->Find(state.deformableSource);MeshData mesh;if(!source||source->type!=AssetType::Mesh||!LoadModelMesh(source->path,mesh,error)||!ImportDeformableCloth(mesh,baked,error))throw std::runtime_error("Cloth import: "+error);baked.sourceAsset=source->id;if(!SceneFingerprintSha256File(source->path,baked.sourceFingerprint,error))throw std::runtime_error(error);}
            auto bytes=EncodeDeformableAsset(baked);std::filesystem::create_directories(path.parent_path());std::ofstream output(path,std::ios::binary);output.write(bytes.data(),std::streamsize(bytes.size()));output.close();if(!output)throw std::runtime_error("Failed to write deformable asset.");AssetRecord record;if(!state.assets->Track(path.string(),record,error))throw std::runtime_error(error);doc.BeginEdit();object->deformable->asset=record.id;auto& attachments=object->deformable->attachments;size_t before=attachments.size();attachments.erase(std::remove_if(attachments.begin(),attachments.end(),[&](const auto& a){return !baked.groups.count(a.group);}),attachments.end());doc.CommitEdit();state.status="Deformable created and assigned: "+record.relativePath;if(before!=attachments.size())state.status+="; removed attachments to groups absent from the new topology.";
        return true;
    }catch(const std::exception& e){state.status=e.what();return false;}
}
