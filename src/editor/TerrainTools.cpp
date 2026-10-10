#include "TerrainTools.h"
#include "Project.h"
#include "Prefab.h"
#include "WorldPresentation.h"
#include "ModelArchive.h"
#include "AsyncFile.h"
#include "../../third_party/nlohmann/json.hpp"
#include "imgui.h"
#include <filesystem>
#include <cstdio>
#include <algorithm>
namespace fs=std::filesystem;
bool TerrainEditor::Busy(const EditorPanelState& s)const{if(!task||!s.importJobs)return false;auto state=s.importJobs->StateOf(task->job);return state==JobState::Running||state==JobState::Queued;}
bool TerrainEditor::Guard(EditorPanelState& s){if(Busy(s)||draft.Active()||(loaded&&draft.Dirty())){s.showTerrain=true;error="Terrain draft is separate from the scene. Save or discard it and finish/cancel the cook before changing project, entering Play or closing.";return false;}return true;}
void TerrainEditor::Shutdown(Renderer& r,JobSystem* jobs){draft.Cancel();if(task&&jobs){jobs->Cancel(task->job);jobs->Wait(task->job);}r.DestroyMesh(mesh);r.DestroyTexture(texture);mesh={};texture={};}
void TerrainEditor::Panel(EditorDocument& doc,EditorPanelState& s,EditorRequests& requests){
 if(!s.showTerrain)return;
 ImGui::SetNextWindowSize({440,650},ImGuiCond_FirstUseEver);
 if(!ImGui::Begin("Terrain - sculpt / paint",&s.showTerrain)){ImGui::End();return;}
 if(!s.project||!s.assets){ImGui::TextUnformatted("Open a project first.");ImGui::End();return;}
 if(projectKey!=s.project->ProjectFile()){if(loaded&&draft.Dirty()){ImGui::TextUnformatted("Previous project's draft is retained. Save/discard before replacing.");}else{loaded=false;editing=false;preview.reset();projectKey=s.project->ProjectFile();}}
 // Keep readiness and recovery above the fold; opening the panel alone used
 // to leave three independent prerequisites hidden among source controls.
 auto* selectedInstance=doc.GetScene().Find(doc.Selected());
 const bool matching=loaded&&selectedInstance&&selectedInstance->render&&selectedInstance->render->meshAsset==draft.source.meshId;
 if(!loaded)ImGui::TextWrapped("Choose a terrain in the hierarchy, then click Edit selected terrain. Or open an editable source below.");
 else if(!matching)ImGui::TextColored({1,.7f,.2f,1},"Select an instance of this source to brush.");
 else if(editing)ImGui::TextColored({.4f,1,.6f,1},"BRUSH ACTIVE - drag left on the terrain");
 else ImGui::TextWrapped("Brush paused. Enable viewport editing below to continue.");
 if(!error.empty())ImGui::TextWrapped("%s",error.c_str());
 if(loaded){
  if(ImGui::Checkbox("Enable viewport sculpt / paint",&editing))draft.Cancel();
  if(!matching&&ImGui::Button("Select matching terrain instance")){
   for(const auto& o:doc.GetScene().Objects())if(o.render&&o.render->meshAsset==draft.source.meshId){doc.Select(o.id);break;}
  }
  const char* modes[]={"Raise","Lower","Flatten (press height)","Smooth","Paint"};int mode=int(brush.kind);if(ImGui::Combo("Brush",&mode,modes,5))brush.kind=TerrainBrushKind(mode);
  ImGui::DragFloat("Radius (local m)",&brush.radius,.1f,.05f,1000);ImGui::DragFloat("Strength per dab (m / blend)",&brush.strength,.01f,0,100);ImGui::DragFloat("Falloff exponent",&brush.falloff,.05f,.1f,8);
  if(brush.kind==TerrainBrushKind::Paint){
   if(draft.source.layers.empty())ImGui::TextWrapped("Add a registered texture layer below before painting.");
   for(unsigned k=0;k<draft.source.layers.size();++k){ImGui::PushID(int(k));if(ImGui::RadioButton(draft.source.layers[k].key.c_str(),brush.layer==k))brush.layer=k;ImGui::PopID();}
  }
  ImGui::TextWrapped("One left drag = one undo. Escape cancels the stroke. Save source, then Cook before Play.");
 }
 ImGui::Separator();
 ImGui::InputText("Editable source (project relative)",path,sizeof(path));
 auto ownedPath=[&]{auto p=s.project->Resolve(path);auto relative=fs::weakly_canonical(p).lexically_relative(fs::weakly_canonical(s.project->RootDir()));if(relative.empty()||relative.is_absolute()||*relative.begin()=="..")throw std::runtime_error("terrain source must belong to current project");return p;};
 auto open=[&]{std::string p;try{p=ownedPath();}catch(const std::exception& e){error=e.what();return;}TerrainDraft next;if(next.Open(p,error)){draft=std::move(next);loaded=true;previewRevision=~uint64_t(0);appearanceDirty=true;editing=false;error.clear();}};
 auto activate=[&]{if(!loaded)return;auto* o=doc.GetScene().Find(doc.Selected());if(!o||!o->render||o->render->meshAsset!=draft.source.meshId){for(const auto& candidate:doc.GetScene().Objects())if(candidate.render&&candidate.render->meshAsset==draft.source.meshId){doc.Select(candidate.id);break;}}o=doc.GetScene().Find(doc.Selected());editing=o&&o->render&&o->render->meshAsset==draft.source.meshId;};
 ImGui::BeginDisabled(Busy(s)||draft.Active()||(loaded&&draft.Dirty()));
 if(ImGui::Button("Edit selected terrain")){auto* selected=doc.GetScene().Find(doc.Selected());auto* asset=selected&&selected->render?s.assets->Find(selected->render->meshAsset):nullptr;if(asset){std::vector<uint8_t> bytes;std::string record;std::vector<MaterialDefinition> materials;if(ReadWholeFile(asset->path,bytes,error)&&ReadModelArchiveMetadata(bytes.data(),bytes.size(),record,materials,error)){try{auto j=nlohmann::json::parse(record);if(j.at("kind")!="terrain")throw std::runtime_error("selected mesh is not authored terrain");std::snprintf(path,sizeof(path),"%s",j.at("source").get<std::string>().c_str());open();activate();}catch(const std::exception& e){error=e.what();}}}else error="Select a cooked terrain instance first";}
 editButtonCenter={(ImGui::GetItemRectMin().x+ImGui::GetItemRectMax().x)*.5f,(ImGui::GetItemRectMin().y+ImGui::GetItemRectMax().y)*.5f};
 if(ImGui::Button("Open source and start editing")){open();activate();}
 ImGui::SameLine();if(ImGui::Button("Create new source")){try{auto p=s.project->Resolve(path);auto relative=fs::weakly_canonical(p).lexically_relative(fs::weakly_canonical(s.project->RootDir()));if(relative.empty()||*relative.begin()==".."||fs::exists(p))throw std::runtime_error("choose a new owned source path");TerrainDraft next;next.source=CreateTerrain(unsigned(resolution),unsigned(resolution),width,depth);next.source.productDirectory=(fs::relative(s.project->AssetsDir(),s.project->RootDir())/"Terrain"/next.source.identity).generic_string();next.SetPath(p);if(next.Save(error)){draft=std::move(next);loaded=true;previewRevision=~uint64_t(0);appearanceDirty=true;}}catch(const std::exception& e){error=e.what();}}
 ImGui::EndDisabled();ImGui::InputInt("Grid samples per side (2..182)",&resolution);resolution=std::clamp(resolution,2,182);ImGui::DragFloat("Width (m)",&width,.5f,.1f,10000);ImGui::DragFloat("Depth (m)",&depth,.5f,.1f,10000);
 if(loaded){ImGui::Separator();ImGui::Text("%u x %u samples | %s",draft.source.nx,draft.source.nz,draft.Dirty()?"UNSAVED source":"source saved");ImGui::TextWrapped("%s",draft.Path().c_str());if(draft.ExternalChanged())ImGui::TextColored({1,.4f,.2f,1},"External source conflict: reopen/discard or fork; save will reject.");
 ImGui::BeginDisabled(Busy(s)||draft.Active());if(ImGui::Button("Save editable source"))draft.Save(error);ImGui::SameLine();if(ImGui::Button("Discard draft / reopen")){draft.Cancel();open();}
 if(ImGui::Button("Fork to the new source path")){std::string p;try{p=ownedPath();}catch(const std::exception& e){error=e.what();}if(p.empty()||fs::exists(p))error="fork requires a new owned source path";else{TerrainDraft next;next.source=ForkTerrain(draft.source);next.SetPath(p);if(next.Save(error)){draft=std::move(next);editing=false;previewRevision=~uint64_t(0);appearanceDirty=true;}}}
 ImGui::SameLine();if(ImGui::Button("Cancel editing")){editing=false;draft.Cancel();}
 if(ImGui::Button("Undo terrain stroke")){if(draft.Undo())appearanceDirty=true;}ImGui::SameLine();if(ImGui::Button("Redo terrain stroke")){if(draft.Redo())appearanceDirty=true;}
 for(unsigned k=0;k<draft.source.layers.size();++k){auto& layer=draft.source.layers[k];ImGui::PushID(int(k));ImGui::TextUnformatted(layer.key.c_str());if(ImGui::BeginCombo("Registered base-color texture",layer.texture.c_str())){for(auto& [id,a]:s.assets->Records())if(a.type==AssetType::Texture&&id!=draft.source.textureId){if(ImGui::Selectable(a.relativePath.c_str(),id==layer.texture)){layer.texture=id;draft.MarkChanged(false);appearanceDirty=true;}}ImGui::EndCombo();}if(ImGui::DragFloat("Tile size (local m)",&layer.tileMetres,.05f,.05f,10000)){draft.MarkChanged(false);appearanceDirty=true;}ImGui::PopID();}
 if(draft.source.layers.size()<4&&ImGui::Button("Add stable texture layer")){for(auto& [id,a]:s.assets->Records())if(a.type==AssetType::Texture&&id!=draft.source.textureId){draft.source.layers.push_back({"layer-"+MintAssetId(),id,4});draft.MarkChanged(false);appearanceDirty=true;break;}}
 ImGui::TextWrapped("Layer identity/order is fixed for this source version; fork or edit explicit weights when replacing a palette. Painting stores normalized weights, not a fill effect. One drag = one undo. Escape cancels. Focus loss finishes the current stroke.");
 ImGui::BeginDisabled(draft.Dirty()||!s.importJobs);if(ImGui::Button("Cook / publish saved source")){auto captured=draft.source;auto sourcePath=draft.Path(),root=s.project->RootDir();auto assets=*s.assets;task=std::make_shared<Task>();auto work=task;work->job=s.importJobs->Submit([work,captured,sourcePath,root,assets](JobContext& context){std::string error;if(!CookTerrain(captured,sourcePath,root,assets,work->result,error,[&]{return context.CancelRequested();})){if(context.CancelRequested())context.ReportCancelled();else context.SetError(error);}},JobPriority::Normal,"Terrain cook");}
 ImGui::EndDisabled();ImGui::SameLine();if(ImGui::Button("Create terrain instance")){auto* asset=s.assets->Find(draft.source.meshId);if(!asset||asset->missing)error="cook/publish and rescan first";else {doc.BeginEdit();auto& o=doc.GetScene().CreateObject("Terrain landscape");o.transform.position=s.cameraFocus;o.render.emplace();o.render->shape=SceneShape::Mesh;o.render->meshAsset=draft.source.meshId;o.render->textureAsset=draft.source.textureId;o.body.emplace();o.body->shape=SceneShape::TriangleMesh;o.body->collisionAsset=draft.source.collisionId;auto id=o.id;doc.CommitEdit();doc.Select(id);}}
 ImGui::EndDisabled();}
 if(task){auto state=s.importJobs->StateOf(task->job);if(state==JobState::Completed){ImGui::Text("Cook %.2f ms / %zu bytes; geometry %s",task->result.milliseconds,task->result.bytes,task->result.geometryChanged?"changed":"unchanged");requests.rescanAssets=true;s.importJobs->Forget(task->job);task.reset();}else if(state==JobState::Failed){error=s.importJobs->ErrorOf(task->job);s.importJobs->Forget(task->job);task.reset();}else if(state==JobState::Cancelled){error="Cancelled; last good products retained";s.importJobs->Forget(task->job);task.reset();}else{ImGui::TextUnformatted("Cooking bounded immutable products on job pool...");if(ImGui::Button("Cancel cook"))s.importJobs->Cancel(task->job);}}
 ImGui::End();if(!s.showTerrain){draft.End();editing=false;}}
void TerrainEditor::Prepare(Renderer& r,const EditorPanelState& s){if(!loaded)return;if(!preview||previewRevision!=draft.GeometryRevision()){preview=std::make_shared<MeshData>(BuildTerrainMesh(draft.source));r.DestroyMesh(mesh);mesh=r.CreateMesh(*preview);previewRevision=draft.GeometryRevision();appearanceDirty=true;}if(appearanceDirty||appearanceRevision!=draft.Revision()){TextureData data;auto previewSource=draft.source;previewSource.textureSize=std::min(previewSource.textureSize,256u);if(PaintTerrainTexture(previewSource,*s.assets,data,error)){r.DestroyTexture(texture);texture=r.CreateTexture(data);appearanceDirty=false;appearanceRevision=draft.Revision();}}}
bool TerrainEditor::Stroke(const Scene& scene,SceneObjectId id,glm::vec3 origin,glm::vec3 direction,bool pressed,bool down,bool cancel,bool focused,std::string& status){
 footprint.Clear();
 if(cancel){draft.Cancel();editing=false;return true;}
 if(!focused||!down){if(draft.Active()){draft.End();appearanceDirty=true;}}
 if(!focused||!loaded||!editing||!preview)return false;
 auto* object=scene.Find(id);
 if(!object||!object->render||object->render->meshAsset!=draft.source.meshId){if(pressed)status="Select an instance of the open terrain source first";return down;}
 if(glm::length(object->transform.scale-glm::vec3(1))>1e-6){status="Terrain sculpt requires rigid unit-scale placement";return down;}
 auto inverse=glm::inverse(object->transform.rotation);glm::vec3 hit;
 if(!PickTerrain(*preview,inverse*(origin-object->transform.position),inverse*direction,hit))return down;
 // The footprint samples the same finite edited triangles as the gesture.
 for(int i=0;i<48;++i){glm::vec3 ends[2];bool found=true;for(int j=0;j<2;++j){float angle=float(i+j)*6.2831853f/48;glm::vec3 p=hit+glm::vec3(std::cos(angle)*brush.radius,0,std::sin(angle)*brush.radius);glm::vec3 surface;if(!TerrainPoint(draft.source,p.x,p.z,surface)){found=false;break;}ends[j]=object->transform.position+object->transform.rotation*(surface+glm::vec3(0,.025f,0));}if(found)footprint.Line(ends[0],ends[1],{1,.85f,.2f});}
 if(!down)return false;
 auto revision=draft.Revision();bool ok=pressed?draft.Begin(hit,brush,error):(draft.Active()?draft.Continue(hit,error):true);
 if(!ok)status=error;
 if(revision!=draft.Revision())appearanceDirty=true;
 return true;
}
void TerrainEditor::Draw(Renderer& r,Scene& scene,ResourceManager& assets){if(!loaded||!preview)return;r.SetRenderMask(scene.Settings().mainCameraRenderMask);for(auto& o:scene.Objects())if(o.render&&o.renderVisible&&o.render->meshAsset==draft.source.meshId&&o.render->visible){r.SetRenderLayer(o.renderLayer);r.SetMaterialBindings(BuildRenderMaterialBindings(&assets,*o.render));r.DrawMesh(mesh,o.transform.position,o.transform.rotation,o.transform.scale,texture,o.render->color,o.render->alpha);o.render->visible=false;}}
