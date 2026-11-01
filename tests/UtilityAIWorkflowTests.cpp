#include "editor/EditorAIWorkflow.h"
#include "editor/UtilityAIEditor.h"
#include "editor/EditorDocument.h"
#include "Project.h"
#include "SceneSerialization.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "GameSession.h"
#include "Simulation.h"
#include "Window.h"
#include "WorldPersistence.h"
#include "CharacterMotor.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iostream>
#include <cmath>

namespace fs=std::filesystem;
using Json=nlohmann::json;
unsigned checks=0;
void Check(bool condition,const std::string& label){++checks;if(!condition)throw std::runtime_error(label);std::cout<<"PASS "<<label<<'\n';}
Json State(RuntimeWorld& world,SceneObjectId owner){for(const auto& record:world.Scripts()->Capture())if(record.entity==owner)return Json::parse(record.json);throw std::runtime_error("missing AI actor state");}
void Tick(GameSession& session,Window& window,int frames){for(int i=0;i<frames;++i){session.HandleFrameInput(window,false,false,false,false,false);StepPlayedWorld(session,window,1.f/60);}}
int main(int argc,char** argv)try{
 const bool keep=argc==2;
 const auto root=keep?fs::absolute(argv[1]):fs::temp_directory_path()/("judas-ai-workflow-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 Check(!fs::exists(root),"test output is a fresh isolated directory");
 Project project;std::string error;
 Check(Project::CreateNew(root.string(),"FreshAI",project,error),"create ordinary project: "+error);
 project.Settings().legacyGameplay=false;
 AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());
 EditorDocument doc;
 const auto floor=doc.GetScene().CreateObject("Floor").id;
 doc.GetScene().Find(floor)->body=SceneBodyComponent{};doc.GetScene().Find(floor)->body->halfExtents={12,.5,12};doc.GetScene().Find(floor)->transform.position={0,-.5,0};
 doc.GetScene().Find(floor)->render=SceneRenderComponent{};doc.GetScene().Find(floor)->render->halfExtents={12,.5,12};
 const auto owner=doc.GetScene().CreateObject("Fresh NPC").id;doc.GetScene().Find(owner)->transform.position={0,.95f,0};doc.Select(owner);
 EditorAIBehaviourAssets created;
 Check(CreateAndAttachEditorAI(doc,owner,project,assets,"Guard",created,error),"fresh object creates linked definition, behaviour and actual actor: "+error);
 Check(doc.GetScene().Find(owner)->characterMotor.has_value()&&doc.GetScene().Find(owner)->render.has_value(),"fresh NPC has ordinary visible capsule and CharacterMotor");
 Check(doc.GetScene().Find(owner)->scripts.size()==1&&created.patrolPoint&&created.returnPoint&&created.coverPoint,"actual script and all explicit marker references attached");
 Check(doc.GetScene().Find(created.patrolPoint)->parent==0&&doc.GetScene().Find(created.coverPoint)->parent==0,"targets stay at root rather than following the NPC");
 AssetRecord linked;Check(FindEditorAIDefinition(assets,created.behaviour,linked,error)&&linked.id==created.definition.id,"source links explicit definition identity and matching import");
 std::string schema;std::vector<ScriptProperty> fields;
 Check(ScriptSystem::Inspect(assets,created.behaviour.id,schema,error)&&ScriptSystem::ReadProperties(schema,"{}",fields,error)&&
       ScriptSystem::ReadProperties(schema,doc.GetScene().Find(owner)->scripts[0].properties,fields,error),"actual entity-property defaults and authored references validate, not just module syntax");
 Check(ScriptSystem::CheckProjectSources(assets).empty(),"generated modules compile/link with the real public module");
 doc.Undo();Check(doc.GetScene().Find(owner)->scripts.empty()&&!doc.GetScene().Find(owner)->characterMotor&&!doc.GetScene().Find(created.coverPoint),"one Undo restores fresh object and removes all generated scene setup");
 Check(fs::exists(created.behaviour.path)&&fs::exists(created.definition.path),"Undo preserves project source assets");
 doc.Redo();Check(doc.GetScene().Find(owner)->scripts.size()==1&&doc.GetScene().Find(created.coverPoint),"one Redo restores the full actor setup");
 const auto before=doc.GetScene();const auto bytes=fs::file_size(created.behaviour.path);
 Check(!CreateAndAttachEditorAI(doc,owner,project,assets,"Guard",created,error)&&ScenesEqual(before,doc.GetScene())&&fs::file_size(created.behaviour.path)==bytes,"existing files and scene survive name collision");
 Check(!CreateAndAttachEditorAI(doc,owner,project,assets,"../escape",created,error)&&ScenesEqual(before,doc.GetScene()),"path escape rejected before changing scene");
 const auto rigid=doc.GetScene().CreateObject("Existing dynamic assembly").id;doc.GetScene().Find(rigid)->body=SceneBodyComponent{};doc.GetScene().Find(rigid)->body->motion=SceneBodyMotion::Dynamic;
 Check(!CreateAndAttachEditorAI(doc,rigid,project,assets,"WrongOwner",created,error)&&doc.GetScene().Find(rigid)->body->motion==SceneBodyMotion::Dynamic&&!fs::exists(fs::path(assets.AssetsDir())/"Scripts/WrongOwner.js"),"existing physical ownership is never silently converted");doc.GetScene().DestroyObject(rigid);
 // Transaction negatives prove files, scene and the existing undo history survive.
 const auto invalid=doc.GetScene().CreateObject("Invalid existing motor").id;doc.GetScene().Find(invalid)->characterMotor=CharacterMotorSettings{};doc.GetScene().Find(invalid)->characterMotor->radius=-1;
 const auto invalidBefore=doc.GetScene();Check(!CreateAndAttachEditorAI(doc,owner,project,assets,"InvalidScene",created,error)&&ScenesEqual(invalidBefore,doc.GetScene())&&!fs::exists(fs::path(assets.AssetsDir())/"Scripts/InvalidScene.js"),"invalid preexisting scene cannot leave partial AI assets or edits");doc.GetScene().DestroyObject(invalid);
 const auto transactionBefore=doc.GetScene();const auto recordsBefore=assets.Records().size();unsigned registrations=0;
 assets.SetIdentityQualifier([&](const AssetId& id){if(++registrations==2)throw std::runtime_error("injected second registration failure");return id;});
 const bool failed=CreateAndAttachEditorAI(doc,owner,project,assets,"Interrupted",created,error);assets.SetIdentityQualifier({});
 Check(!failed&&ScenesEqual(transactionBefore,doc.GetScene())&&assets.Records().size()==recordsBefore&&!fs::exists(fs::path(assets.AssetsDir())/"Scripts/Interrupted.ai.js")&&!fs::exists(fs::path(assets.AssetsDir())/"Scripts/Interrupted.js"),"second publication failure rolls back only its own newly minted files and records");
 // The human Scripts-component-first path leaves an empty placeholder.
 EditorDocument placeholder;const auto blankOwner=placeholder.GetScene().CreateObject("Blank script owner").id;
 placeholder.GetScene().Find(blankOwner)->scripts.push_back({7,"",true,"{}"});
 EditorAIBehaviourAssets blankAssets;const bool blankCreated=CreateAndAttachEditorAI(placeholder,blankOwner,project,assets,"PlaceholderGuard",blankAssets,error);
 Check(blankCreated,"Scripts-component-first workflow fills an empty slot: "+error);
 Check(placeholder.GetScene().Find(blankOwner)->scripts.size()==1&&placeholder.GetScene().Find(blankOwner)->scripts[0].id==7&&placeholder.GetScene().Find(blankOwner)->scripts[0].asset==blankAssets.behaviour.id,"empty slot identity is retained without a dangling enabled placeholder");
 placeholder.Undo();Check(placeholder.GetScene().Find(blankOwner)->scripts.size()==1&&placeholder.GetScene().Find(blankOwner)->scripts[0].id==7&&placeholder.GetScene().Find(blankOwner)->scripts[0].asset.empty()&&!placeholder.GetScene().Find(blankOwner)->characterMotor,"one Undo restores the original placeholder and actor");
 placeholder.Redo();Check(placeholder.GetScene().Find(blankOwner)->scripts.size()==1&&placeholder.GetScene().Find(blankOwner)->scripts[0].asset==blankAssets.behaviour.id,"one Redo restores the reuse without appending a duplicate slot");
 EditorDocument unrelatedPlaceholders;const auto chosen=unrelatedPlaceholders.GetScene().CreateObject("Chosen").id;unrelatedPlaceholders.GetScene().Find(chosen)->scripts.push_back({7,"",true,"{}"});
 const auto unrelated=unrelatedPlaceholders.GetScene().CreateObject("Unrelated incomplete script").id;unrelatedPlaceholders.GetScene().Find(unrelated)->scripts.push_back({1,"",true,"{}"});
 const auto unrelatedBefore=unrelatedPlaceholders.GetScene();EditorAIBehaviourAssets unrelatedAssets;
 Check(!CreateAndAttachEditorAI(unrelatedPlaceholders,chosen,project,assets,"OtherIncomplete",unrelatedAssets,error)&&ScenesEqual(unrelatedBefore,unrelatedPlaceholders.GetScene())&&!fs::exists(fs::path(assets.AssetsDir())/"Scripts/OtherIncomplete.js"),"placeholder preflight exemption is limited to the one selected slot being filled");
 // Ordinary damage publisher, no direct mutation of the selected AI action or health.
 const auto emitter=fs::path(project.AssetsDir())/"Damage.js";
 {std::ofstream file(emitter);file<<"import {signals,entity,time,world} from 'judas';export default class {constructor(){this.state={sent:false};}start(){world.setView({position:{x:0,y:7,z:12},rotation:{x:-.2474,y:0,z:0,w:.9689}},60);}restore(){this.start();}fixedUpdate(){if(!this.state.sent&&time.elapsed>1){signals.send(entity('"<<owner<<"'),'ai.damage',{amount:.7});this.state.sent=true;}}}";}
 AssetRecord publisher;Check(assets.Track(emitter.string(),publisher,error),"register project damage publisher");doc.GetScene().Find(floor)->scripts.push_back({1,publisher.id,true,"{}"});
 project.Settings().startupScene="Scenes/main.judas";
 Check(project.Save(error)&&SaveSceneToFile(doc.GetScene(),project.StartupScenePath(),error),"save actual fresh project and scene");
 Scene scene;Check(LoadSceneFromFile(project.StartupScenePath(),scene,error),"reopen saved fresh project scene");
 ResourceManager resources(nullptr,&assets);resources.SetBlockingMode(true);RuntimeWorld world;world.ConfigureProject(project.Settings());
 Check(world.Build(scene,&resources,error),"build actual generated actor: "+error);
 Window window;window.SetTestInputMode(true);GameSession session;Check(session.Begin(world,error),"begin normal project session");
 Tick(session,window,45);
 Check(world.Scripts()->Diagnostics().empty(),"generated script starts without VM faults");
 Check(world.RuntimeCharacter(owner)->position.x>1&&State(world,owner).at("brain").at("selected").at("action")=="patrol","native utility choice drives real CharacterMotor movement");
 Tick(session,window,60);
 Check(world.Scripts()->Diagnostics().empty()&&State(world,owner).at("health").get<double>()<.9&&State(world,owner).at("brain").at("selected").at("action")=="hide","real targeted damage causes hide decision");
 std::map<std::string,SaveChunk> chunks;Check(WorldPersistence::Capture(world,chunks,error),"capture modern AI/physical state");
 const auto state=State(world,owner);const auto position=world.RuntimeCharacter(owner)->position;
 Scene saved;Check(WorldPersistence::SceneFrom(chunks,saved,error),"decode normal saved scene");RuntimeWorld restored;restored.ConfigureProject(project.Settings());
 Check(restored.Build(saved,&resources,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(restored,error)&&WorldPersistence::Restore(restored,chunks,error),"private world restores normal participants: "+error);
 Check(restored.RuntimeCharacter(owner)->position==position,"physical motor endpoint restores exactly before continuation");
 GameSession resumed;Check(resumed.Begin(restored,error),"resume normal restored session");Tick(resumed,window,1);
 const auto resumedState=State(restored,owner);
 Check(restored.Scripts()->Diagnostics().empty()&&std::abs(resumedState.at("brain").at("elapsed").get<double>()-state.at("brain").at("elapsed").get<double>()-1./60)<1e-6,"native agent clock genuinely continues through modern save restoration");
 const auto selected=EditorAIAgentsForEntity(restored.Scripts()->AIInspections(),owner);
 Check(selected.size()==1&&selected[0].entity==owner&&EditorAIAgentsForEntity(restored.Scripts()->AIInspections(),floor).empty(),"selected-object inspection filters actual ownership without inferring visual targets");
 restored.DestroyEntity(created.coverPoint,&error);Tick(resumed,window,20);
 Check(restored.Scripts()->Diagnostics().empty()&&State(restored,owner).at("brain").at("selected").at("action")!="hide","deleted cover target invalidates the action safely");
 resumed.End();restored.Destroy();
 Tick(session,window,600);
 Check(world.Scripts()->Diagnostics().empty()&&State(world,owner).at("health").get<double>()>=.9,"cover arrival performs actual recovery and returns to patrol");
 session.End();world.Destroy();
 Scene disabled=scene;disabled.Find(owner)->scripts[0].enabled=false;RuntimeWorld dormant;dormant.ConfigureProject(project.Settings());Check(dormant.Build(disabled,&resources,error),"disabled behaviour still builds ordinary actor");GameSession idle;Check(idle.Begin(dormant,error),"disabled behaviour session starts");Tick(idle,window,20);
 Check(dormant.Scripts()&&EditorAIAgentsForEntity(dormant.Scripts()->AIInspections(),owner).empty()&&std::abs(dormant.RuntimeCharacter(owner)->position.x)<1e-6,"disabled script creates no AI or hidden movement");idle.End();dormant.Destroy();
 // Different marker heights must not prevent a true patrol loop: arrival
 // and candidate eligibility both use the same gravity-tangent displacement.
 Scene raised=scene;raised.Find(floor)->scripts.clear();raised.Find(created.patrolPoint)->transform.position.y+=4;raised.Find(created.returnPoint)->transform.position.y+=2;
 RuntimeWorld heights;heights.ConfigureProject(project.Settings());Check(heights.Build(raised,&resources,error),"height-offset target scene builds");GameSession heightSession;Check(heightSession.Begin(heights,error),"height-offset session begins");float farthest=0,minimumAfterFar=1000;bool reachedFar=false;
 for(int i=0;i<240;++i){Tick(heightSession,window,1);const float x=heights.RuntimeCharacter(owner)->position.x;farthest=std::max(farthest,x);if(x>2.7f)reachedFar=true;if(reachedFar)minimumAfterFar=std::min(minimumAfterFar,x);}
 std::cout<<"HEIGHT_TRACE max_x="<<farthest<<" final_x="<<heights.RuntimeCharacter(owner)->position.x<<" min_after_far="<<minimumAfterFar<<'\n';
 Check(heights.Scripts()->Diagnostics().empty()&&farthest>2.7f&&minimumAfterFar<.3f,"height-offset patrol markers still produce outward and return movement");heightSession.End();heights.Destroy();
 // The same scaffold respects actual native planar collider geometry.
 EditorDocument planarDoc;const auto planarOwner=planarDoc.GetScene().CreateObject("Planar NPC").id;
 auto* planarActor=planarDoc.GetScene().Find(planarOwner);planarActor->body=SceneBodyComponent{};planarActor->body->shape=SceneShape::Planar;planarActor->body->planar=PlanarBox({.3,.3},true);planarActor->body->motion=SceneBodyMotion::Kinematic;
 EditorAIBehaviourAssets planarAssets;Check(CreateAndAttachEditorAI(planarDoc,planarOwner,project,assets,"PlanarGuard",planarAssets,error),"existing kinematic 2D actor uses the same behaviour workflow");
 const auto wall=planarDoc.GetScene().CreateObject("Real planar wall").id;planarDoc.GetScene().Find(wall)->body=SceneBodyComponent{};planarDoc.GetScene().Find(wall)->body->shape=SceneShape::Planar;planarDoc.GetScene().Find(wall)->body->planar=PlanarBox({.2,2},true);planarDoc.GetScene().Find(wall)->transform.position={1.4f,0,0};
 RuntimeWorld planarWorld;planarWorld.ConfigureProject(project.Settings());Check(planarWorld.Build(planarDoc.GetScene(),&resources,error),"native 2D starter and wall build");GameSession planarSession;Check(planarSession.Begin(planarWorld,error),"native planar session begins");Tick(planarSession,window,90);
 const auto planarPosition=planarWorld.Physics().GetTransform(planarWorld.RuntimeBody(planarOwner)).position;
 Check(planarWorld.Scripts()->Diagnostics().empty()&&planarPosition.x>.3f&&planarPosition.x<1.1f&&std::abs(planarPosition.z)<1e-6,"starter moves physically in its native plane and stops before the real wall");planarSession.End();planarWorld.Destroy();
 if(!keep)fs::remove_all(root);else std::cout<<"FRESH_PROJECT "<<project.RootDir()<<'\n';
 std::cout<<"Utility AI workflow: "<<checks<<" checks, 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
