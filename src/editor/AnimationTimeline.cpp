#include "AnimationTimeline.h"
#include "EditorPanels.h"
#include "ModelCook.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "imgui.h"
#include <filesystem>
#include <algorithm>
namespace {
bool GuardDraftSwitch(AnimationEventDraft& draft,std::string& status){
 if(!draft.Dirty())return false;
 ImGui::TextWrapped("Unsaved markers for %s in %s. Save or explicitly discard this draft before switching clips, assets or projects.",draft.clip.c_str(),draft.path.c_str());
 if(ImGui::Button("Save previous marker draft")){if(draft.Save(status))status="Previous marker draft saved. The selected content will open next frame.";}
 ImGui::SameLine();if(ImGui::Button("Discard previous marker draft")){draft={};status="Previous unsaved marker draft discarded.";}
 if(!status.empty())ImGui::TextWrapped("%s",status.c_str());
 return true;
}
bool Text(const char* label,std::string& value){std::vector<char> bytes(std::max(size_t(8192),value.size()+256),0);std::copy(value.begin(),value.end(),bytes.begin());if(!ImGui::InputText(label,bytes.data(),bytes.size()))return false;value=bytes.data();return true;}
}
void AnimationTimeline::Panel(EditorPanelState& state,uint64_t selectedEntity){
 if(!visible)return;
 ImGui::SetNextWindowSize({820,650},ImGuiCond_FirstUseEver);
 if(!ImGui::Begin("Animation timeline",&visible)){ImGui::End();return;}
 if(!state.project||!state.project->IsLoaded()){ImGui::TextUnformatted("Open a project first.");ImGui::End();return;}
 if(state.mode==EditorMode::Play){
  auto* live=state.runtime?state.runtime->RuntimeAnimation(selectedEntity):nullptr;
  ImGui::TextWrapped("Live inspection is read-only. Pause and select an animated entity in the hierarchy to inspect it. Stop Play to edit/preview content.");
  if(live&&live->asset){ImGui::Text("Clip %s | %.5f seconds | speed %.3f | %s",live->playback.clip.c_str(),live->playback.time,live->playback.speed,live->mixer.paused?"clock held":"automatic");ImGui::Text("Blend %.3f | sampled events %zu | overflow %s",live->mixer.Fraction(),live->step.events.size(),live->step.overflow?"yes":"no");for(const auto& hit:live->step.events)ImGui::BulletText("%s / %s @ %.5f s | %s weight %.3f",hit.clip.c_str(),hit.event.name.c_str(),hit.event.time,hit.source.c_str(),hit.weight);if(!live->error.empty())ImGui::TextWrapped("%s",live->error.c_str());}
  else ImGui::TextUnformatted("No ready animated entity selected.");
  ImGui::End();return;
 }
 ImGui::TextWrapped("Edit model clip markers without running gameplay. Preview and scrubbing emit no gameplay events. Save writes the import recipe; Reimport publishes the model.");
 const auto root=std::filesystem::path(state.project->RootDir());
 if(ownerProject!=root.string()&&GuardDraftSwitch(draft,status)){ImGui::End();return;}
 if(ownerProject!=root.string()){draft={};undo.clear();selected=-1;loadedRecipe.clear();loadedClip.clear();ownerProject=root.string();}
 if(ImGui::BeginCombo("Project import recipe",state.modelRecipe.c_str())){
  auto directory=root/"Imports";if(std::filesystem::is_directory(directory))for(const auto& file:std::filesystem::directory_iterator(directory))if(file.path().extension()==".judasimport")if(ImGui::Selectable(file.path().filename().string().c_str(),state.modelRecipe==file.path().string()))state.modelRecipe=file.path().string();ImGui::EndCombo();}
 bool edit=state.mode==EditorMode::Edit;
 bool busy=state.importTask&&!state.importTask->done.load();
 if(edit&&!busy&&ImGui::Button("Load / reimport selected recipe")&&!state.modelRecipe.empty()){state.importTask=QueueModelImport(*state.importJobs,state.modelRecipe);state.importPublished=false;}
 if(busy)ImGui::TextUnformatted("Importing through the ordinary asset pipeline...");
 if(state.importTask&&state.importTask->done.load()&&!state.importTask->success)ImGui::TextWrapped("%s",state.importTask->error.c_str());
 auto rig=state.importAccepted&&state.importAccepted->preview?state.importAccepted->preview->skeletal:nullptr;
 if(rig&&!rig->clips.empty()){
  state.modelPreviewClip=unsigned(std::min(size_t(state.modelPreviewClip),rig->clips.size()-1));auto& clip=rig->clips[state.modelPreviewClip];
  if(ImGui::BeginCombo("Clip",clip.name.c_str())){for(size_t i=0;i<rig->clips.size();++i)if(ImGui::Selectable(rig->clips[i].name.c_str(),i==state.modelPreviewClip)){state.modelPreviewClip=unsigned(i);state.modelPreviewTime=0;}ImGui::EndCombo();}
  ImGui::Checkbox("Preview playback",&state.modelPreviewPlaying);ImGui::SameLine();ImGui::Checkbox("Skeleton",&state.modelPreviewSkeleton);ImGui::SliderFloat("Scrub / seconds",&state.modelPreviewTime,0,clip.duration,"%.4f s");
  ImGui::SliderFloat("Preview orbit",&state.modelPreviewYaw,-3.14159f,3.14159f);
  if(state.modelPreviewToken)ImGui::Image((ImTextureID)(intptr_t)state.modelPreviewToken,{std::min(600.f,ImGui::GetContentRegionAvail().x),300},{0,1},{1,0});
  ImGui::Text("Duration %.4f s | extracted root keys %zu | authored markers %zu",clip.duration,clip.motion.size(),clip.events.size());
  auto motion=SampleRootMotion(clip,state.modelPreviewTime,false);ImGui::Text("Model root at cursor: %.3f, %.3f, %.3f metres",motion.translation.x,motion.translation.y,motion.translation.z);
  if((loadedRecipe!=state.importAccepted->recipe||loadedClip!=clip.name)&&GuardDraftSwitch(draft,status)){ImGui::End();return;}
  if(loadedRecipe!=state.importAccepted->recipe||loadedClip!=clip.name){draft={};undo.clear();selected=-1;loadedRecipe=state.importAccepted->recipe;loadedClip=clip.name;draft.Load(root.string(),loadedRecipe,clip,status);}
  if(ImGui::Button("Reload event draft from recipe")){draft.Load(root.string(),state.importAccepted->recipe,clip,status);undo.clear();selected=-1;}
  if(draft.Dirty())ImGui::TextColored({1,.75f,.3f,1},"Unsaved marker draft");
  if(!draft.path.empty()){
   // A click selects the nearest marker; otherwise scrubs. Dragging the scrubber
   // never changes a marker. Numeric edits are deliberate and can be undone.
   auto origin=ImGui::GetCursorScreenPos();float width=std::max(100.f,ImGui::GetContentRegionAvail().x);ImGui::InvisibleButton("marker track",{width,42});auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled(origin,{origin.x+width,origin.y+42},IM_COL32(25,32,43,255),4);
   for(size_t i=0;i<draft.events.size();++i){float x=origin.x+width*(clip.duration>0?draft.events[i].time/clip.duration:0);draw->AddLine({x,origin.y+5},{x,origin.y+37},int(i)==selected?IM_COL32(255,198,91,255):IM_COL32(78,215,194,255),3);}
   float cursor=origin.x+width*(clip.duration>0?state.modelPreviewTime/clip.duration:0);draw->AddLine({cursor,origin.y},{cursor,origin.y+42},IM_COL32(245,245,245,255));
   if(ImGui::IsItemClicked()){float x=ImGui::GetIO().MousePos.x-origin.x;selected=-1;float nearest=9;for(size_t i=0;i<draft.events.size();++i){float dx=std::abs(x-width*(clip.duration>0?draft.events[i].time/clip.duration:0));if(dx<nearest){nearest=dx;selected=int(i);}}if(selected<0)state.modelPreviewTime=std::clamp(x/width,0.f,1.f)*clip.duration;}
   ImGui::BeginDisabled(!edit);
   if(ImGui::Button("Add marker at cursor")&&draft.events.size()<1024){Remember();std::string id;do{id="event-"+std::to_string(++serial);}while(std::any_of(draft.events.begin(),draft.events.end(),[&](const auto& e){return e.id==id;}));draft.events.push_back({id,"marker","",state.modelPreviewTime});selected=int(draft.events.size())-1;}
   ImGui::SameLine();if(ImGui::Button("Undo marker edit")&&!undo.empty()){draft.events=std::move(undo.back());undo.pop_back();selected=-1;}
   if(ImGui::BeginCombo("Selected marker",selected>=0&&size_t(selected)<draft.events.size()?draft.events[selected].id.c_str():"Choose marker")){for(size_t i=0;i<draft.events.size();++i)if(ImGui::Selectable(draft.events[i].id.c_str(),selected==int(i)))selected=int(i);ImGui::EndCombo();}
   if(selected>=0&&size_t(selected)<draft.events.size()){auto before=draft.events;auto& e=draft.events[selected];bool changed=Text("Stable ID",e.id);changed|=Text("Event name",e.name);changed|=Text("Opaque string data",e.data);changed|=ImGui::InputFloat("Marker seconds",&e.time,1.f/60, .1f,"%.5f");if(changed){if(undo.size()>=64)undo.erase(undo.begin());undo.push_back(std::move(before));}if(ImGui::Button("Move marker to cursor")){Remember();e.time=state.modelPreviewTime;}ImGui::SameLine();if(ImGui::Button("Delete marker")){Remember();draft.events.erase(draft.events.begin()+selected);selected=-1;}}
   if(ImGui::Button("Save event track to recipe")){if(draft.Save(status))status="Recipe saved. Reimport to publish events to runtime content.";}
   ImGui::SameLine();if(ImGui::Button("Discard draft / reload recipe")){draft.Load(root.string(),loadedRecipe,clip,status);undo.clear();selected=-1;}
   ImGui::EndDisabled();
  }
 }else ImGui::TextUnformatted("Choose an animated model recipe and load it to preview its clips.");
 if(!status.empty())ImGui::TextWrapped("%s",status.c_str());
 ImGui::End();
}
