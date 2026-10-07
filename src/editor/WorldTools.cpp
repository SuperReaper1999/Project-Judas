#include "EditorPanels.h"
#include "EditorWidgets.h"
#include "ComponentEditors.h"
#include "RecipePreview.h"
#include "AuthoringDocument.h"
#include "NamedAuthoring.h"
#include "SceneSerialization.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "imgui.h"
#include "../../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace {
struct Tools {
    WorldRecipe recipe;
    std::shared_ptr<RecipePreviewTask> task;
    std::string error,project,scene,selectedRecipe;
    bool acceptRemoval=false,sourceDirty=false;
    std::vector<char> source=std::vector<char>(4*1024*1024,0);
    uint64_t sourceGeneration=~uint64_t(0);
    AuthoringDocument file;
    std::vector<char> fileSource=std::vector<char>(4*1024*1024,0);
    std::string fileKind="project",filePath,externalDiff,retainedSource;
};
Tools tools;
std::map<std::string,Tools> retainedDrafts;
void SetBuffer(std::vector<char>& buffer,const std::string& text){std::fill(buffer.begin(),buffer.end(),0);std::copy_n(text.data(),std::min(text.size(),buffer.size()-1),buffer.data());}
bool Commit(EditorDocument& doc,const Scene& candidate,std::string& error){std::string text;SaveSceneToString(candidate,text);return doc.ApplySource(text,error);}
void Cancel(EditorPanelState& state){if(tools.task&&state.importJobs)state.importJobs->Cancel(tools.task->job);tools.task.reset();state.recipePreview.Clear();}
}
void DrawWorldBuildingPanel(EditorDocument& doc,EditorPanelState& state){
    if(!state.showWorldBuilding||state.mode!=EditorMode::Edit)return;
    ImGui::SetNextWindowSize({520,720},ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("World building",&state.showWorldBuilding)){ImGui::End();return;}
    if(tools.project!=(state.project?state.project->ProjectFile():"")||tools.scene!=doc.Path()){
        Cancel(state);
        auto previous=tools.project+":"+tools.scene;
        if(tools.sourceDirty||tools.file.Dirty()||std::string(tools.fileSource.data())!=tools.file.Source()) {
            retainedDrafts[previous]=std::move(tools);
            state.status="Dirty authoring draft retained; return to its project/scene to resume it";
        }
        auto next=(state.project?state.project->ProjectFile():"")+":"+doc.Path();
        auto saved=retainedDrafts.find(next);
        if(saved!=retainedDrafts.end()){tools=std::move(saved->second);retainedDrafts.erase(saved);}else tools=Tools{};
        tools.project=state.project?state.project->ProjectFile():"";tools.scene=doc.Path();
    }
    auto error=[&](bool ok){if(!ok)state.status=tools.error;else tools.error.clear();};
    if(ImGui::CollapsingHeader("Recipes: arrays, paths and profiles",ImGuiTreeNodeFlags_DefaultOpen)){
        if(ImGui::BeginCombo("Scene-owned recipe",tools.selectedRecipe.empty()?"New recipe":tools.selectedRecipe.c_str())){
            if(ImGui::Selectable("New recipe",tools.selectedRecipe.empty())){Cancel(state);tools.recipe=WorldRecipe{};tools.selectedRecipe.clear();}
            for(auto& [id,source]:doc.GetScene().Settings().authoringRecipes)if(ImGui::Selectable(id.c_str(),tools.selectedRecipe==id)){
                WorldRecipe recipe;if(ParseWorldRecipe(source,recipe,tools.error)){Cancel(state);tools.recipe=recipe;tools.selectedRecipe=id;tools.acceptRemoval=false;}
            }
            ImGui::EndCombo();
        }
        if(!tools.selectedRecipe.empty()&&ImGui::Button("Refresh from current document (after undo)")){
            auto it=doc.GetScene().Settings().authoringRecipes.find(tools.selectedRecipe);
            if(it!=doc.GetScene().Settings().authoringRecipes.end()){Cancel(state);ParseWorldRecipe(it->second,tools.recipe,tools.error);}else tools.error="Recipe no longer exists in this undo revision";
        }
        char id[129];std::snprintf(id,sizeof(id),"%s",tools.recipe.id.c_str());if(ImGui::InputText("Recipe ID",id,sizeof(id)))tools.recipe.id=id;
        const char* types[]={"linear","radial","path","sweep","arc"};int type=0;for(int i=0;i<5;++i)if(tools.recipe.type==types[i])type=i;
        if(ImGui::Combo("Operation",&type,types,5))tools.recipe.type=types[type];
        if(ImGui::Button("Use selected entity as template"))tools.recipe.templateId=doc.Selected();
        ImGui::SameLine();ImGui::Text("template #%llu",static_cast<unsigned long long>(tools.recipe.templateId));
        char prefab[96];std::snprintf(prefab,sizeof(prefab),"%s",tools.recipe.prefabAsset.c_str());if(ImGui::InputText("Prefab asset (optional)",prefab,sizeof(prefab)))tools.recipe.prefabAsset=prefab;
        int count=int(tools.recipe.count),segments=int(tools.recipe.segments),proxySegments=int(tools.recipe.collisionSegments);
        if(ImGui::InputInt("Maximum elements",&count))tools.recipe.count=unsigned(std::clamp(count,1,2048));
        if(ImGui::InputInt("Curve segments per span",&segments))tools.recipe.segments=unsigned(std::clamp(segments,1,512));
        if(ImGui::InputInt("Collision segments (0 matches render)",&proxySegments))tools.recipe.collisionSegments=unsigned(std::clamp(proxySegments,0,512));
        ImGui::DragFloat("Spacing (m)",&tools.recipe.spacing,.05f,.01f,1000);
        ImGui::DragFloat("Radius (m)",&tools.recipe.radius,.05f,.01f,1000);
        ImGui::DragFloat("Arc start (degrees)",&tools.recipe.startDegrees,.5f);
        ImGui::DragFloat("Arc end (degrees)",&tools.recipe.endDegrees,.5f);
        ImGui::DragFloat("Transported-frame twist (degrees)",&tools.recipe.twistDegrees,.5f);
        ImGui::Checkbox("Smooth path",&tools.recipe.smooth);
        ImGui::SameLine();ImGui::Checkbox("Closed physical source",&tools.recipe.closed);
        ImGui::DragFloat3("Frame position (m)",&tools.recipe.frame.position.x,.1f);
        glm::vec3 angles=glm::degrees(glm::eulerAngles(tools.recipe.frame.rotation));if(ImGui::DragFloat3("Frame rotation (degrees)",&angles.x,.5f))tools.recipe.frame.rotation=glm::quat(glm::radians(angles));
        ImGui::DragFloat3("Frame scale",&tools.recipe.frame.scale.x,.01f,.01f,100);
        if(ImGui::TreeNode("Path control points (frame local, m)")){for(size_t i=0;i<tools.recipe.path.size();++i){ImGui::PushID(int(i));ImGui::DragFloat3("Point",&tools.recipe.path[i].x,.1f);ImGui::PopID();}if(ImGui::Button("Add point")&&tools.recipe.path.size()<256)tools.recipe.path.push_back(tools.recipe.path.back()+glm::vec3(0,0,-5));
        ImGui::SameLine();if(ImGui::Button("Remove last point")&&tools.recipe.path.size()>2)tools.recipe.path.pop_back();ImGui::TreePop();}
        if(ImGui::TreeNode("Convex profile (frame cross-section, m)")){for(size_t i=0;i<tools.recipe.profile.size();++i){ImGui::PushID(int(i));ImGui::DragFloat2("Vertex",&tools.recipe.profile[i].x,.01f);ImGui::PopID();}ImGui::TextWrapped("Closed convex sections; the M64 cooker rejects crossings, degeneracies and bad topology. Curved collision accuracy follows the authored segment count.");ImGui::TreePop();}
        const auto parameters=SerializeWorldRecipe(tools.recipe);
        const bool haveProject=state.project&&state.project->IsLoaded()&&state.assets&&state.importJobs;
        ImGui::BeginDisabled(!haveProject);
        if(ImGui::Button("Preview / cook")){
            bool cached=tools.task&&state.importJobs->StateOf(tools.task->job)==JobState::Completed&&tools.task->generation==doc.Generation()&&tools.task->parameters==parameters;
            if(!cached){Cancel(state);tools.task=PreviewRecipe(*state.importJobs,tools.recipe,doc.GetScene(),*state.assets,doc.Generation(),state.project->ProjectFile());}
        }
        ImGui::EndDisabled();if(!haveProject)ImGui::TextDisabled("Open a project to publish generated resources.");
        ImGui::SameLine();if(ImGui::Button("Cancel preview"))Cancel(state);
        if(tools.task){auto status=state.importJobs->StateOf(tools.task->job);bool valid=status==JobState::Completed&&tools.task->generation==doc.Generation()&&tools.task->parameters==parameters;
            if(status==JobState::Failed)tools.error=state.importJobs->ErrorOf(tools.task->job);
            if(status==JobState::Queued||status==JobState::Running)ImGui::TextDisabled("Generating and validating on the job pool...");
            if(status==JobState::Completed&&!valid){state.recipePreview.Clear();ImGui::TextDisabled("Preview is stale: regenerate after parameters, undo or document changes.");}
            if(valid){auto& p=tools.task->product;state.recipePreview.Clear();if(!p.mesh.vertices.empty()){auto transform=p.recipe.frame;for(size_t i=0;i<p.mesh.indices.size();i+=3)for(int e=0;e<3;++e){auto a=p.mesh.vertices[p.mesh.indices[i+e]].position,b=p.mesh.vertices[p.mesh.indices[i+(e+1)%3]].position;state.recipePreview.Line(transform.position+transform.rotation*(transform.scale*a),transform.position+transform.rotation*(transform.scale*b),{.2f,.9f,1});}}else for(auto& e:p.recipe.elements){auto* o=p.scene.Find(e.object);if(o)state.recipePreview.Axes(o->transform.position,o->transform.rotation,.5f);}
                ImGui::Text("Validated: %zu elements, %zu vertices, %zu removals",p.recipe.elements.size(),p.mesh.vertices.size(),p.removed.size());
                for(auto id:p.removed)ImGui::Text("Remove authored entity #%llu",static_cast<unsigned long long>(id));
                if(!p.removed.empty())ImGui::Checkbox("Accept listed removals (references must remain valid)",&tools.acceptRemoval);
                ImGui::BeginDisabled(!p.removed.empty()&&!tools.acceptRemoval);
                if(ImGui::Button("Apply accepted preview (one undo)")){
                    if(PublishRecipeResources(*tools.task,*state.project,*state.assets,tools.error)&&Commit(doc,p.scene,tools.error)){tools.recipe=p.recipe;tools.selectedRecipe=p.recipe.id;Cancel(state);state.status="Applied validated recipe; surviving identities and instance edits retained";}
                }ImGui::EndDisabled();
            }
        }
        if(!tools.selectedRecipe.empty()&&ImGui::Button("Detach recipe; keep ordinary generated content")){auto candidate=doc.GetScene();candidate.Settings().authoringRecipes.erase(tools.selectedRecipe);error(Commit(doc,candidate,tools.error));tools.selectedRecipe.clear();Cancel(state);}
    }
    if(ImGui::CollapsingHeader("Placement and batch commands")){
        ImGui::Checkbox("Viewport individual origins",&state.gizmoIndividualOrigins);
        static int axis=0;static float grid=.5f,rotationGrid=15.f,scaleGrid=.1f;static bool normalOrientation=false;
        ImGui::Combo("World axis",&axis,"X\0Y\0Z\0");
        if(ImGui::Button("Align to primary"))error(doc.AlignSelection(unsigned(axis),false,tools.error));
        ImGui::SameLine();if(ImGui::Button("Distribute between extremes"))error(doc.AlignSelection(unsigned(axis),true,tools.error));
        ImGui::InputFloat("Translation snap (m)",&grid);ImGui::InputFloat("Rotation snap (degrees)",&rotationGrid);ImGui::InputFloat("Scale snap",&scaleGrid);
        if(ImGui::Button("Snap numeric transforms"))error(doc.SnapSelection(grid,rotationGrid,scaleGrid,tools.error));
        static glm::vec3 direction{0,-1,0};static float distance=100;
        ImGui::DragFloat3("Surface cast world direction",&direction.x,.01f);ImGui::DragFloat("Surface cast distance (m)",&distance,.1f,.01f,10000);
        ImGui::Checkbox("Orient authored local +Y to hit normal",&normalOrientation);
        if(ImGui::Button("Snap origins to actual collision")){
            RuntimeWorld world;
            if(world.Build(doc.GetScene(),state.resources,tools.error,state.project?&state.project->Settings().classification:nullptr))error(doc.SurfaceSnap(world,direction,distance,normalOrientation,tools.error));else state.status=tools.error;
        }
        ImGui::TextWrapped("Surface normal is placement policy, not gravity. Shapes/assets must already be ready. Transform scale changes visuals; edit body dimensions explicitly. Rigid geometry cannot represent shear.");
        static char prefix[128]="Object";ImGui::InputText("Rename prefix",prefix,sizeof(prefix));if(ImGui::Button("Rename selected in document order"))error(doc.RenameSelection(prefix,tools.error));
        for(const auto& c:ComponentEditorRegistry()){ImGui::PushID(c.name);size_t have=0;for(auto id:doc.Selection())if(auto* o=doc.GetScene().Find(id))have+=c.has(*o);ImGui::Text("%s: %zu/%zu",c.name,have,doc.Selection().size());
        ImGui::SameLine();
            auto batch=[&](bool add){auto candidate=doc.GetScene();for(auto id:doc.Selection())if(auto* o=candidate.Find(id)){if(add&&!c.has(*o))c.add(*o);if(!add&&c.has(*o))c.remove(*o);}error(Commit(doc,candidate,tools.error));};
            if(ImGui::SmallButton("Add to missing"))batch(true);
        ImGui::SameLine();if(ImGui::SmallButton("Remove from all"))batch(false);ImGui::PopID();}
        ImGui::TextWrapped("Counts preview compatibility. The full candidate is validated atomically; failures retain every selected object.");
        auto refs=doc.SelectionReferences();for(auto& line:refs)ImGui::TextWrapped("%s",line.c_str());
        if(ImGui::Button("Delete selected after reference review"))error(doc.DeleteSelection(tools.error));
    }
    if(ImGui::CollapsingHeader("Scene source (same document / undo)")){
        if(tools.sourceGeneration!=doc.Generation()&&!tools.sourceDirty){std::string legacy,named;SaveSceneToString(doc.GetScene(),legacy);if(LegacyToNamed(legacy,"scene",named,tools.error))SetBuffer(tools.source,named);tools.sourceGeneration=doc.Generation();}
        if(doc.ExternalChanged()){
            ImGui::TextColored({1,.7f,.2f,1},"External edit detected. Save is blocked.");
            if(ImGui::Button("Review external semantic diff")){
                Scene disk;std::string a,b,c,d;
                if(LoadSceneFromFile(doc.Path(),disk,tools.error)){
                    SaveSceneToString(doc.GetScene(),a);SaveSceneToString(disk,b);
                    if(LegacyToNamed(a,"scene",c,tools.error)&&LegacyToNamed(b,"scene",d,tools.error))tools.externalDiff=nlohmann::json::diff(nlohmann::json::parse(c),nlohmann::json::parse(d)).dump(2);
                }
            }
            if(ImGui::Button("Reload disk; retain current source draft")){
                tools.retainedSource=tools.source.data();
                if(doc.Load(doc.Path(),tools.error)){tools.sourceDirty=false;tools.sourceGeneration=~uint64_t(0);tools.externalDiff.clear();}
            }
        }
        if(!tools.externalDiff.empty())ImGui::TextWrapped("%s",tools.externalDiff.c_str());
        if(!tools.retainedSource.empty()&&ImGui::Button("Restore retained pre-reload draft")){SetBuffer(tools.source,tools.retainedSource);tools.sourceDirty=true;}
        if(ImGui::InputTextMultiline("##scene-source",tools.source.data(),tools.source.size(),{-1,240}))tools.sourceDirty=true;
        if(tools.sourceGeneration!=doc.Generation()&&tools.sourceDirty)ImGui::TextWrapped("Document changed while this source draft was dirty. Draft retained: inspect it before applying replacement.");
        if(ImGui::Button("Discard draft / read current document")){tools.sourceDirty=false;tools.sourceGeneration=~uint64_t(0);}
        if(ImGui::Button("Validate / apply source as one edit")){if(doc.ApplySource(tools.source.data(),tools.error)){tools.sourceDirty=false;tools.sourceGeneration=~uint64_t(0);}else state.status=tools.error;}ImGui::SameLine();
        if(ImGui::Button("Save current scene as named"))error(doc.SaveNamed(tools.error));
    }
    if(ImGui::CollapsingHeader("Project / world / input / prefab named source")){
        const char* kinds[]={"project","world","input","prefab"};int k=0;for(int i=0;i<4;++i)if(tools.fileKind==kinds[i])k=i;if(ImGui::Combo("Document kind",&k,kinds,4))tools.fileKind=kinds[k];
        char path[1024];std::snprintf(path,sizeof(path),"%s",tools.filePath.c_str());if(ImGui::InputText("Path",path,sizeof(path)))tools.filePath=path;
        if(ImGui::Button("Use current project path")&&state.project)tools.filePath=state.project->ProjectFile();
        if(ImGui::Button("Open validated source")){if(tools.file.Dirty()||(!tools.file.Path().empty()&&std::string(tools.fileSource.data())!=tools.file.Source()))tools.error="Save or explicitly discard the current source draft first";else if(tools.file.Load(tools.fileKind,tools.filePath,tools.error))SetBuffer(tools.fileSource,tools.file.Source());}
        if(!tools.file.Path().empty()){ImGui::Text("%s%s",tools.file.Path().c_str(),tools.file.Dirty()?" *":"");ImGui::InputTextMultiline("##file-source",tools.fileSource.data(),tools.fileSource.size(),{-1,240});if(ImGui::Button("Validate / stage file edit"))error(tools.file.Replace(tools.fileSource.data(),tools.error));
        ImGui::SameLine();if(ImGui::Button("Save staged source")){if(tools.file.Save(tools.error))state.status="Saved source; reopen project/scene explicitly before Play to use changed configuration";else state.status=tools.error;}if(ImGui::Button("Undo file edit")&&tools.file.Undo())SetBuffer(tools.fileSource,tools.file.Source());
        ImGui::SameLine();if(ImGui::Button("Discard / reload file")){if(tools.file.Load(tools.file.Kind(),tools.file.Path(),tools.error))SetBuffer(tools.fileSource,tools.file.Source());}}
    }
    if(!tools.error.empty())ImGui::TextWrapped("%s",tools.error.c_str());
    ImGui::End();
}
