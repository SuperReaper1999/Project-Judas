#include "DeformableEditor.h"
#include "EditorWidgets.h"
#include "EditorPanels.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "Project.h"
#include "ModelLoader.h"
#include "SceneFingerprint.h"
#include "deformables/PanelCook.h"
#include "imgui.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cmath>

namespace {
void NonnegativeNumber(EditorDocument& doc,const char* label,double& value){double temp=value;if(ImGui::InputDouble(label,&temp,.001,.1,"%.8g")&&std::isfinite(temp)&&temp>=0){doc.BeginEdit();value=temp;doc.CommitEdit();}}
}
void DrawDeformable(EditorDocument& doc,SceneObject& o,EditorPanelState& state){
    auto& d=*o.deformable;
    Checkbox(doc,"Enabled",d.enabled);Checkbox(doc,"Cloth self-contact",d.selfContact);
    AssetField(doc,"Simulation / render binding",d.asset,AssetType::Deformable,false,state);
    int substeps=int(d.substeps),iterations=int(d.iterations);
    if(ImGui::CollapsingHeader("Simulation quality and collision filtering")){
        if(DragInt(doc,"Local material substeps",substeps,1,16))d.substeps=unsigned(substeps);
        if(DragInt(doc,"Local constraint iterations",iterations,1,16))d.iterations=unsigned(iterations);
        ImGui::TextWrapped("These settings refine soft material locally. Rigid structural sections use MK2's own solver budget.");
        if(state.project){DrawCategoryLayer(doc,"Collision layer",d.collisionLayer,state.project->Settings().classification.collision);DrawCategoryMask(doc,"Collision mask",d.collisionMask,state.project->Settings().classification.collision);}
    }
    if(ImGui::Button("Woven cloth preset")){
        doc.BeginEdit();d.material=DeformableMaterial{};d.panel.enabled=false;
        for(auto& attachment:d.attachments)attachment.tearWork=0;
        doc.CommitEdit();
    }
    ImGui::SameLine();
    if(ImGui::Button("Steel body-panel preset")){
        doc.BeginEdit();d.material=DeformableMaterial{};d.material.density=6.28;
        d.material.stretchCompliance=d.material.shearCompliance=1e-9;d.material.thickness=.012;
        d.panel=DeformablePanelMaterial{};d.panel.enabled=true;doc.CommitEdit();
    }
    auto& m=d.material;
    NonnegativeNumber(doc,"Density (kg/m2 cloth; kg/m3 solid)",m.density);
    NonnegativeNumber(doc,"Contact thickness (m)",m.thickness);NonnegativeNumber(doc,"Friction",m.friction);
    if(ImGui::CollapsingHeader("Advanced material response")){
    NonnegativeNumber(doc,"Stretch compliance",m.stretchCompliance);NonnegativeNumber(doc,"Shear compliance",m.shearCompliance);
    NonnegativeNumber(doc,"Bend compliance",m.bendCompliance);NonnegativeNumber(doc,"Volume compliance",m.volumeCompliance);
    NonnegativeNumber(doc,"Damping (1/s)",m.damping);
    NonnegativeNumber(doc,"Air drag (1/s)",m.airDrag);glm::vec3 air(m.airVelocity);if(DragVec3(doc,"World air velocity",air))m.airVelocity=air;
    NonnegativeNumber(doc,"Yield strain (0 disables)",m.yieldStrain);NonnegativeNumber(doc,"Plastic rate (1/s)",m.plasticRate);NonnegativeNumber(doc,"Maximum plastic strain",m.maximumPlasticStrain);
    }
    Checkbox(doc,"Physical panel bending / plasticity",d.panel.enabled);
    if(d.panel.enabled){
        NonnegativeNumber(doc,"Bending stiffness (N m / rad)",d.panel.stiffness);
        NonnegativeNumber(doc,"Yield moment (N m)",d.panel.yieldMoment);
        NonnegativeNumber(doc,"Plastic hardening (N m / rad)",d.panel.hardening);
        ImGui::TextWrapped("Independent thin panels retain local permanent bends. Their detailed mesh follows a coarse physical sheet; it is not a second chassis.");
    }
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
            changed|=ImGui::Checkbox("Structural section stress (M79/M80 opt-in)",&cook.structural);
            if(cook.structural&&!state.fractureInterface.empty())for(auto& bond:cook.bonds)if(bond.key==state.fractureInterface){
                ImGui::TextWrapped("Interface overrides: 0 inherits family strength. Capacity follows actual area and bending section.");
                changed|=ImGui::InputDouble("Interface tension (Pa)",&bond.tension)|ImGui::InputDouble("Interface shear (Pa)",&bond.shear)|ImGui::InputDouble("Interface compression (Pa)",&bond.compression);
                ImGui::Text("Area %.4g m2 | polar moment %.4g m4",bond.area,bond.polarMoment);
            }
            if(cook.rigid&&cook.HasMaterials())changed|=ImGui::Checkbox("Filter intact material and rigid support neighbours",&cook.filterNeighbours);
            if(cook.rigid&&!state.fractureInterface.empty())for(auto& bond:cook.bonds)if(bond.key==state.fractureInterface){
                auto& m=bond.material;changed|=ImGui::Checkbox("Finite elastic/plastic connection (M83)",&m.enabled);
                if(m.enabled){
                    ImGui::TextWrapped("Six physical spring/damper axes. Plastic rest strain changes collider/mount geometry; capacity failure releases ordinary rigid pieces. Values are per connection, not a vehicle preset.");
                    changed|=ImGui::InputFloat("Translation stiffness N/m",&m.stiffness)|ImGui::InputFloat("Rotation stiffness N m/rad",&m.angularStiffness);
                    changed|=ImGui::InputFloat("Translation damping N s/m",&m.damping)|ImGui::InputFloat("Rotation damping N m s/rad",&m.angularDamping);
                    changed|=ImGui::InputFloat("Yield force N",&m.yieldForce)|ImGui::InputFloat("Yield torque N m",&m.yieldTorque);
                    changed|=ImGui::InputFloat("Force hardening N/m",&m.hardeningForce)|ImGui::InputFloat("Torque hardening N m/rad",&m.hardeningTorque);
                    changed|=ImGui::InputFloat("Plastic travel capacity m",&m.maximumTranslation)|ImGui::InputFloat("Plastic angle capacity rad",&m.maximumAngle);
                }
            }
            if(changed&&state.mode==EditorMode::Edit){auto baked=*asset;baked.fracture=std::make_shared<FractureCook>(cook);auto* record=state.assets->Find(d.asset);if(record&&PrepareDeformableAsset(baked,error)){std::ofstream output(record->path,std::ios::binary);auto bytes=EncodeDeformableAsset(baked);output.write(bytes.data(),std::streamsize(bytes.size()));output.close();if(output){state.resources->Invalidate(d.asset);state.status="Fracture material baked; asset content changes save compatibility.";}}else state.status=error;}
            if(ImGui::Button("Preview physical cells / interfaces / interiors")){state.deformablePreview.Clear();auto point=[&](glm::dvec3 v){return o.transform.position+o.transform.rotation*glm::vec3(v);};for(auto& part:cook.parts)state.deformablePreview.Box(point(part.proxyCenter),o.transform.rotation,glm::vec3(part.proxyHalf),{.2f,.8f,.6f});for(auto& bond:cook.bonds){if(!state.fractureInterface.empty()&&state.fractureInterface!=bond.key)continue;auto c=point(bond.centroid);state.deformablePreview.Line(c,c+o.transform.rotation*glm::vec3(bond.normal)*.35f,{1,.3f,.1f});}for(unsigned f=0;f<asset->triangles.size();++f)if(cook.faceBond[f]>=0&&(state.fractureInterface.empty()||cook.bonds[cook.faceBond[f]].key==state.fractureInterface)){auto face=asset->triangles[f];for(int k=0;k<3;++k)state.deformablePreview.Line(point(asset->nodes[face[k]]),point(asset->nodes[face[(k+1)%3]]),{1,.6f,.1f});}}
        }
        for(auto& [name,nodes]:asset->groups){ImGui::PushID(name.c_str());ImGui::Text("%s: %zu nodes",name.c_str(),nodes.size());
            ImGui::SameLine();if(ImGui::Button("Show patch")){state.deformablePreview.Clear();for(auto n:nodes)state.deformablePreview.Box(o.transform.position+o.transform.rotation*glm::vec3(asset->nodes[n]),glm::quat(1,0,0,0),glm::vec3(.035f),{1,.7f,.1f});}ImGui::SameLine();if(ImGui::Button("Attach group")){doc.BeginEdit();auto found=std::find_if(d.attachments.begin(),d.attachments.end(),[&](auto& a){return a.group==name;});if(found==d.attachments.end()){DeformableAttachment a;a.group=name;d.attachments.push_back(a);}doc.CommitEdit();}ImGui::PopID();}
        TextField(doc,"New group name",state.deformableGroup);DragVec3(doc,"Local selection minimum",state.deformableSelectionMin);DragVec3(doc,"Local selection maximum",state.deformableSelectionMax);
        if(ImGui::Button("Preview selection volume")){
            state.deformablePreview.Clear();auto center=(state.deformableSelectionMin+state.deformableSelectionMax)*.5f;
            state.deformablePreview.Box(o.transform.position+o.transform.rotation*center,o.transform.rotation,(state.deformableSelectionMax-state.deformableSelectionMin)*.5f,{.2f,1,.5f});
            for(auto p:asset->nodes)if(glm::all(glm::greaterThanEqual(p,glm::dvec3(state.deformableSelectionMin)))&&glm::all(glm::lessThanEqual(p,glm::dvec3(state.deformableSelectionMax))))state.deformablePreview.Box(o.transform.position+o.transform.rotation*glm::vec3(p),glm::quat(1,0,0,0),glm::vec3(.035f),{1,.7f,.1f});
        }
        if(state.mode==EditorMode::Edit&&ImGui::Button("Bake node group from selection box")){
            auto changed=*asset;auto& group=changed.groups[state.deformableGroup];group.clear();for(unsigned i=0;i<changed.nodes.size();++i)if(glm::all(glm::greaterThanEqual(changed.nodes[i],glm::dvec3(state.deformableSelectionMin)))&&glm::all(glm::lessThanEqual(changed.nodes[i],glm::dvec3(state.deformableSelectionMax))))group.push_back(i);
            auto* record=state.assets->Find(d.asset);if(group.empty())state.status="Selection contains no nodes.";else if(record&&PrepareDeformableAsset(changed,error)){std::ofstream output(record->path,std::ios::binary);auto bytes=EncodeDeformableAsset(changed);output.write(bytes.data(),std::streamsize(bytes.size()));output.close();if(output){state.resources->Invalidate(d.asset);state.status="Named group baked; existing saves require matching asset content.";}else state.status="Failed to write deformable asset.";}else state.status=error;
        }
    }
    for(size_t i=0;i<d.attachments.size();){auto& a=d.attachments[i];ImGui::PushID(int(i));ImGui::Separator();ImGui::Text("Attachment %s",a.group.c_str());Checkbox(doc,"Attachment enabled",a.enabled);const char* kinds[]={"world (authored root anchor)","rigid body (finite mass)","skeleton joint (prescribed)"};Combo(doc,"Target kind",a.kind,kinds,3);
        if(a.kind!=DeformableAttachment::Kind::World){std::vector<std::string> labels,values;for(auto& target:doc.GetScene().Objects())if(a.kind==DeformableAttachment::Kind::Body?(bool(target.body)||bool(target.deformable)):bool(target.animation)){labels.push_back(target.name);values.push_back(std::to_string(target.id));}std::string id=std::to_string(a.target);if(LabelledCombo(doc,"Target entity",id,labels,values,false))a.target=std::stoull(id);if(a.kind==DeformableAttachment::Kind::Bone)TextField(doc,"Stable skeleton joint key",a.joint);}
        if(a.kind==DeformableAttachment::Kind::Body){
            std::vector<std::string> parts;
            if(auto* target=doc.GetScene().Find(a.target);target&&target->deformable&&state.resources){
                auto data=state.resources->GetDeformable(target->deformable->asset,error);
                if(data&&data->fracture&&data->fracture->rigid)for(const auto& p:data->fracture->parts)parts.push_back(p.key);
            }
            StringCombo(doc,"Structural section (empty = whole body)",a.part,parts,true);
            if(ImGui::Button("Hinge / seam mount preset")){doc.BeginEdit();a.forceLimit=20000;a.torqueLimit=8000;a.tearWork=d.panel.enabled?20:0;doc.CommitEdit();}
            ImGui::SameLine();if(ImGui::Button("Latch mount preset")){doc.BeginEdit();a.forceLimit=8000;a.torqueLimit=8000;a.tearWork=d.panel.enabled?20:0;doc.CommitEdit();}
            ImGui::TextWrapped("Use an edge group for a hinge or seam, and a small patch for a latch. These example capacities must be tuned to your panel and carrier; motion follows the actual constrained nodes.");
        }
        if(d.panel.enabled)NonnegativeNumber(doc,"Mounting-sheet tear energy J (0 = disabled)",a.tearWork);
        glm::vec3 offset(a.offset);if(DragVec3(doc,"Target-local offset",offset))a.offset=offset;
        NonnegativeNumber(doc,"Attachment force limit N (0 = unlimited)",a.forceLimit);
        NonnegativeNumber(doc,"Attachment moment limit N m (0 = unlimited)",a.torqueLimit);
        if(a.forceLimit>0||a.torqueLimit>0){
            const auto* target=doc.GetScene().Find(a.target);
            if(a.kind!=DeformableAttachment::Kind::Body||!target||!target->body||target->body->motion!=SceneBodyMotion::Dynamic)
                if(a.part.empty())ImGui::TextColored(ImVec4(1,.3f,.2f,1),"Finite strength requires a dynamic body or a physical structural section.");
            ImGui::TextWrapped("Limits measure the actual signed patch reaction, including moment about its centroid. Exceeding either releases after the completed step; no position or velocity reset.");
        }
        if(state.runtime){std::string diagnostic;if(const auto* runtime=state.runtime->RuntimeDeformable(o.id,diagnostic))if(i<runtime->attachmentState.size()){
            const auto& observation=runtime->attachmentState[i];
            if(runtime->panelBending)ImGui::Text("Local irreversible work %.4g / %.4g J",runtime->AttachmentWork(unsigned(i)),a.tearWork);
            ImGui::Text("Demand %.3g N / %.3g N m | %s",observation.peakForce,observation.peakTorque,i<runtime->released.size()&&runtime->released[i]?"released":"retained");
        }}
        bool remove=ImGui::Button("Remove attachment");ImGui::PopID();if(remove){doc.BeginEdit();d.attachments.erase(d.attachments.begin()+i);doc.CommitEdit();}else ++i;
    }
    if(state.mode==EditorMode::Edit&&state.project&&state.assets&&ImGui::CollapsingHeader("Create/import deformable asset")){
        TextField(doc,"Assets-relative destination",state.deformableDestination);DragInt(doc,"Sheet columns / block X",state.deformableColumns,1,48);DragInt(doc,"Sheet rows / block Y",state.deformableRows,1,48);DragInt(doc,"Render subdivision / block Z",state.deformableSubdivision,1,8);DragVec3(doc,"Size (metres)",state.deformableSize);AssetField(doc,"Source mesh",state.deformableSource,AssetType::Mesh,true,state);
        int action=0;if(ImGui::Button("Create sheet"))action=1;ImGui::SameLine();if(ImGui::Button("Create tetrahedral block"))action=2;if(ImGui::Button("Import indexed cloth mesh"))action=3;
        if(ImGui::Button("Create 24-section physical vehicle frame")){state.deformableColumns=2;state.deformableRows=2;state.deformableSubdivision=6;state.deformableSize={2,1.2f,5};action=10;}
        if(ImGui::Button("Create cohesive fracture block"))action=4;
        if(ImGui::Button("Create bendable structural block"))action=7;
        ImGui::SameLine();if(ImGui::Button("Create rigid fracture block"))action=5;TextField(doc,"Partition source (assets-relative path)",state.deformableSource);if(ImGui::Button("Import explicit volumetric partition"))action=6;
        if(state.resources&&!state.deformableSource.empty()){
            if(auto* source=state.assets->Find(state.deformableSource);source&&source->type==AssetType::Mesh)state.resources->RequestMesh(source->id);
            const auto* parts=state.resources->TryGetModelParts(state.deformableSource);
            if(parts){std::vector<std::string> names;for(const auto& part:*parts)if(!part.part.empty()&&std::find(names.begin(),names.end(),part.part)==names.end())names.push_back(part.part);StringCombo(doc,"Imported panel region",state.deformableSourcePart,names,true);}
        }
        const char* axes[]={"X (side panel)","Y (bonnet / roof)","Z (front / rear)"};
        int axis=state.deformableNormalAxis;if(ImGui::Combo("Panel normal",&axis,axes,3))state.deformableNormalAxis=axis;
        ImGui::TextWrapped("Select a separated imported part. The panel grid uses rows/columns above; its render detail remains unchanged. Split closed or folded shells into regions.");
        if(ImGui::Button("Fit physical panel to imported region"))action=8;
        if(asset&&asset->fracture&&asset->fracture->rigid){
            ImGui::TextWrapped("Detailed exterior: the selected mesh follows physical sections and tears at authored mesh islands. Use separate physical panels for local sheet-metal bends.");
            if(ImGui::Button("Bind detailed exterior to current structure"))action=9;
        }
        if(action)BakeEditorDeformable(doc,o.id,state,action);
    }
    if(!ValidDeformableSettings(d,error))ImGui::TextWrapped("Invalid configuration: %s",error.c_str());
    if(state.runtime){auto* runtime=state.runtime->RuntimeDeformable(o.id,error);if(runtime){
        ImGui::Text("Runtime: %s | %.3g kg | %zu contacts | strain %.4g | min J %.3f",runtime->sleeping?"sleeping":"active",runtime->Mass(),runtime->stats.contacts,runtime->stats.maximumStrain,runtime->stats.minimumJacobian);
        if(!runtime->error.empty())ImGui::TextWrapped("Stopped: %s",runtime->error.c_str());
        if(ImGui::Checkbox("Live physical mesh, mounts and collision proxies",&state.deformableLivePreview)&&!state.deformableLivePreview)state.deformablePreview.Clear();
        if(state.deformableLivePreview){auto& lines=state.deformablePreview;lines.Clear();
            for(auto edge:runtime->asset->edges)lines.Line(glm::vec3(runtime->positions[edge.x]),glm::vec3(runtime->positions[edge.y]),{.1f,.7f,1});
            for(unsigned a=0;a<runtime->settings.attachments.size();++a){auto group=runtime->asset->groups.find(runtime->settings.attachments[a].group);if(group!=runtime->asset->groups.end())for(auto n:group->second)lines.Cross(glm::vec3(runtime->positions[n]),.035f,runtime->released[a]?glm::vec3(1,.2f,.2f):glm::vec3(1,.7f,.1f));}
            if(runtime->asset->fracture&&runtime->asset->fracture->rigid)for(unsigned part=0;part<runtime->asset->fracture->parts.size();++part){
                auto body=state.runtime->RuntimeBody(state.runtime->FracturePartEntity(o.id,part));Shape shape;BodyTransform pose;
                if(state.runtime->Physics().GetBodyShape(body,shape,pose,false)){
                    if(shape.type==ShapeType::Box)lines.Box(pose.position,pose.rotation,shape.halfExtents,{.6f,1,.3f});
                    else if(shape.type==ShapeType::Sphere)lines.Sphere(pose.position,shape.radius,{.6f,1,.3f});
                }
            }
        }
    }else ImGui::TextWrapped("Runtime: %s",error.c_str());}
    ImGui::TextWrapped("Nodes simulate in world space; root is initial placement. Explicit reset moves the entire state. Bone pins consume the fixed-step resolved pose. Render materials use normal Render slots.");
}



bool BakeEditorDeformable(EditorDocument& doc,SceneObjectId id,EditorPanelState& state,int action){
    auto* object=doc.GetScene().Find(id);std::string error;
    if(!object||!object->deformable||!state.project||!state.assets||action<1||action>10){state.status="Select a deformable and an open project.";return false;}
    try{
            auto path=std::filesystem::path(state.project->AssetsDir())/state.deformableDestination;auto relative=path.lexically_normal().lexically_relative(state.project->AssetsDir());if(relative.empty()||relative.is_absolute()||*relative.begin()==".."||path.extension()!=".judasdeform")throw std::runtime_error("Choose a .judasdeform path inside project assets.");
            if(std::filesystem::exists(path))throw std::runtime_error("Destination exists; choose a new name.");
            DeformableAsset baked;
            if(action==10){
                baked=MakeFractureBlock({state.deformableColumns,state.deformableRows,state.deformableSubdivision},glm::dvec3(state.deformableSize),true);baked.fracture->structural=true;baked.fracture->filterNeighbours=true;
                for(auto& connection:baked.fracture->bonds)connection.material.enabled=true;
                if(!PrepareDeformableAsset(baked,error))throw std::runtime_error(error);
            }
            else if(action==1)baked=MakeDeformableSheet(unsigned(state.deformableColumns),unsigned(state.deformableRows),state.deformableSize.x,state.deformableSize.y,unsigned(state.deformableSubdivision));
            else if(action==2)baked=MakeDeformableBlock({state.deformableColumns,state.deformableRows,state.deformableSubdivision},glm::dvec3(state.deformableSize));
            else if(action==4||action==5||action==7){baked=MakeFractureBlock({state.deformableColumns,state.deformableRows,state.deformableSubdivision},glm::dvec3(state.deformableSize),action==5);if(action==7){baked.fracture->structural=true;if(!PrepareDeformableAsset(baked,error))throw std::runtime_error(error);}}
            else if(action==6){auto path=std::filesystem::path(state.project->AssetsDir())/state.deformableSource;auto relative=path.lexically_normal().lexically_relative(state.project->AssetsDir());if(relative.empty()||relative.is_absolute()||*relative.begin()=="..")throw std::runtime_error("Partition source must be inside project assets.");std::ifstream input(path);std::string text{std::istreambuf_iterator<char>(input),{}};if(!input||!ImportFracturePartition(text,baked,error))throw std::runtime_error("Partition import: "+error);}
            else if(action==9){
                auto current=state.resources?state.resources->GetDeformable(object->deformable->asset,error):nullptr;
                auto* source=state.assets->Find(state.deformableSource);
                if(!current||!current->fracture||!current->fracture->rigid)throw std::runtime_error("Choose a physical structural asset first.");
                if(!source||source->type!=AssetType::Mesh)throw std::runtime_error("Choose an imported detailed mesh asset.");
                baked=*current;baked.structuralSkin.reset();baked.sourceAsset=source->id;
                if(!SceneFingerprintSha256File(source->path,baked.sourceFingerprint,error))throw std::runtime_error(error);
            }
            else if(action==8){
                auto* source=state.assets->Find(state.deformableSource);MeshData mesh,region;
                if(!source||source->type!=AssetType::Mesh)throw std::runtime_error("Choose an imported mesh asset.");
                if(!LoadModelMesh(source->path,mesh,error)||!SelectDeformablePanelRegion(mesh,state.deformableSourcePart,region,error)||!CookDeformablePanel(region,{unsigned(state.deformableColumns),unsigned(state.deformableRows),unsigned(state.deformableNormalAxis)},baked,error))throw std::runtime_error("Panel import: "+error);
                baked.sourceAsset=source->id;if(!SceneFingerprintSha256File(source->path,baked.sourceFingerprint,error))throw std::runtime_error(error);
            }
            else{auto* source=state.assets->Find(state.deformableSource);MeshData mesh;if(!source||source->type!=AssetType::Mesh||!LoadModelMesh(source->path,mesh,error)||!ImportDeformableCloth(mesh,baked,error))throw std::runtime_error("Cloth import: "+error);baked.sourceAsset=source->id;if(!SceneFingerprintSha256File(source->path,baked.sourceFingerprint,error))throw std::runtime_error(error);}
            auto bytes=EncodeDeformableAsset(baked);std::filesystem::create_directories(path.parent_path());std::ofstream output(path,std::ios::binary);output.write(bytes.data(),std::streamsize(bytes.size()));output.close();if(!output)throw std::runtime_error("Failed to write deformable asset.");AssetRecord record;if(!state.assets->Track(path.string(),record,error))throw std::runtime_error(error);doc.BeginEdit();object->deformable->asset=record.id;if(action==10){object->deformable->panel.enabled=false;double volume=0;for(const auto& element:baked.solids)volume+=element.volume;object->deformable->material.density=1500/volume;}if(action==8){object->deformable->panel.enabled=true;object->deformable->material.stretchCompliance=object->deformable->material.shearCompliance=1e-9;}auto& attachments=object->deformable->attachments;size_t before=attachments.size();attachments.erase(std::remove_if(attachments.begin(),attachments.end(),[&](const auto& a){return !baked.groups.count(a.group);}),attachments.end());doc.CommitEdit();state.status="Deformable created and assigned: "+record.relativePath;if(before!=attachments.size())state.status+="; removed attachments to groups absent from the new topology.";
        return true;
    }catch(const std::exception& e){state.status=e.what();return false;}
}
