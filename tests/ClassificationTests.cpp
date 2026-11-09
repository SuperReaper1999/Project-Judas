#include "Classification.h"
#include "Project.h"
#include "Prefab.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "EngineHost.h"
#include "WorldPresentation.h"
#include "editor/EditorDocument.h"
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <chrono>
#include <cstdio>
namespace fs=std::filesystem;
namespace {
int checks=0,failures=0;
void Check(bool ok,const char* label){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",label);}
ProjectClassification Categories(){ProjectClassification c;unsigned id;c.tags.Add("interactable",id);c.tags.Add("target",id);c.collision.Add("Ghost",id);c.collision.Add("QueryOnly",id);c.render.Add("MainOnly",id);c.render.Add("CameraOnly",id);return c;}
Scene Source(){Scene s;auto& prop=s.CreateObject("Tagged assembly");prop.tags=3;prop.renderLayer=1;prop.render=SceneRenderComponent{};prop.render->color={1,.2f,.1f};prop.body=SceneBodyComponent{};prop.body->motion=SceneBodyMotion::Dynamic;prop.body->collisionLayer=1;prop.body->collisionMask=0;
 const auto rootId=prop.id;auto& child=s.CreateObject("Child marker");child.parent=rootId;child.transform.position={0,1.5f,0};child.render=SceneRenderComponent{};child.render->halfExtents={.2f,.2f,.2f};child.renderLayer=2;child.tags=2;return s;}
bool Demo(const fs::path& path,std::string& error){
 Project project;if(!Project::CreateNew(path.string(),"Classification and filtering",project,error))return false;project.Settings().classification=Categories();project.Settings().startupScene="Scenes/main.judas";
 Scene source=Source();source.Find(1)->body->motion=SceneBodyMotion::Static;source.Find(1)->body->collisionLayer=0;source.Find(1)->body->collisionMask=kAllCategories;
 if(!SaveSceneToFile(source,(path/"Assets/assembly.judasprefab").string(),error))return false;
 AssetDatabase assets;assets.Scan(path.string(),project.AssetsDir());AssetRecord asset;if(!assets.Track((path/"Assets/assembly.judasprefab").string(),asset,error,"39393939393939393939393939393939"))return false;
 Scene s;s.Settings().name="M39 tags, collision and camera layers";s.Settings().ambientColor={.6f,.6f,.6f};s.Settings().mainCameraRenderMask=CategoryBit(0)|CategoryBit(1);
 auto& floor=s.CreateObject("Floor");floor.transform.position={0,-.25f,0};floor.render=SceneRenderComponent{};floor.render->halfExtents={15,.25f,15};floor.body=SceneBodyComponent{};floor.body->halfExtents=floor.render->halfExtents;
 auto& player=s.CreateObject("Player start");player.characterMotor=CharacterMotorSettings{};player.transform.position={0,1,8};
 auto& gravity=s.CreateObject("Uniform field");gravity.gravity=SceneGravityComponent{};gravity.gravity->kind=SceneGravityKind::Uniform;gravity.gravity->regionShape=SceneRegionShape::Box;gravity.gravity->regionHalfExtents={30,30,30};
 SceneTransform placement;placement.position={-3,.6f,0};SceneObjectId root;if(!InstantiatePrefab(s,source,asset.id,placement,root,error))return false;
 auto& ghost=s.CreateObject("Blue visible ghost");ghost.tags=1;ghost.transform.position={0,1,0};ghost.render=SceneRenderComponent{};ghost.render->halfExtents={.8f,1,.6f};ghost.render->color={.05f,.3f,1};ghost.body=SceneBodyComponent{};ghost.body->halfExtents=ghost.render->halfExtents;ghost.body->collisionLayer=1;ghost.body->collisionMask=0;
 auto& query=s.CreateObject("Green query target");query.tags=2;query.transform.position={3,1,0};query.render=SceneRenderComponent{};query.render->color={.05f,1,.2f};query.body=SceneBodyComponent{};query.body->collisionLayer=2;query.body->collisionMask=0;
 auto& main=s.CreateObject("Yellow main-only marker");main.transform.position={-2,2,-4};main.render=SceneRenderComponent{};main.render->color={1,1,.05f};main.renderLayer=1;
 auto& remote=s.CreateObject("Magenta camera-only marker");remote.transform.position={2,2,-4};remote.render=SceneRenderComponent{};remote.render->color={1,.05f,1};remote.renderLayer=2;
 auto& camera=s.CreateObject("Layer camera");camera.transform.position={0,3,7};camera.renderCamera=SceneRenderCameraComponent{};camera.renderCamera->renderMask=CategoryBit(0)|CategoryBit(2);const auto cameraId=camera.id;
 auto& screen=s.CreateObject("Camera display");screen.transform.position={5,2,3};screen.render=SceneRenderComponent{};screen.render->halfExtents={2,1.2f,.08f};screen.render->color={1,1,1};screen.render->textureCamera=cameraId;
 return SaveSceneToFile(s,project.StartupScenePath(),error)&&project.Save(error);
}
}
int main(int argc,char** argv){
 std::string error;if(argc==3&&std::string(argv[1])=="--demo"){bool ok=Demo(argv[2],error);std::printf("%s\n",error.c_str());return ok?0:1;}
 const fs::path out=argc==3&&std::string(argv[1])=="--output"?argv[2]:"build/m39-focused";fs::create_directories(out);
 auto c=Categories();auto original=c.Serialize();ProjectClassification round;
 Check(ProjectClassification::Parse(original,round,error)&&round.Serialize()==original,"registry names and IDs roundtrip");
 unsigned id=0;round.tags.Remove(0);Check(round.tags.Add("new",id)&&id==2&&round.tags.Rename(1,"renamed")&&round.tags.Find("renamed")==1,"delete retires ID and rename preserves identity");
 Check(!round.collision.Remove(0,true)&&!ProjectClassification::Parse("JudasClassification 9",round,error),"reserved default and unsupported registry version rejected");
 Project project;auto directory=out/"project";fs::remove_all(directory);Check(Project::CreateNew(directory.string(),"Test",project,error),"create normal project");project.Settings().classification=c;Check(project.Save(error),"save project categories");Project reloaded;Check(reloaded.Load(project.ProjectFile(),error)&&reloaded.Settings().classification.Serialize()==original,"project reload preserves registry IDs and names");
 Scene scene=Source();scene.Settings().mainCameraRenderMask=3;auto& camera=scene.CreateObject("Camera");camera.renderCamera=SceneRenderCameraComponent{};camera.renderCamera->renderMask=5;auto& start=scene.CreateObject("Player");start.characterMotor=CharacterMotorSettings{};start.characterMotor->collisionLayer=2;start.characterMotor->collisionMask=CategoryBit(0);
 auto& door=scene.CreateObject("Door");door.render=SceneRenderComponent{};door.body=SceneBodyComponent{};door.body->collisionLayer=1;door.body->collisionMask=0;
 std::string text;SaveSceneToString(scene,text);Scene loaded;Check(LoadSceneFromString(text,loaded,error)&&ScenesEqual(scene,loaded),"scene tags/body/player/door/render/camera configuration roundtrip");
 std::string a,b;ComputeSceneFingerprint(scene,a,error);loaded.Find(1)->tags=1;ComputeSceneFingerprint(loaded,b,error);Check(a!=b,"classification changes authored fingerprint");
 RuntimeWorld world;Check(world.Build(scene,nullptr,error,&c),"real runtime constructs assigned categories");
 Check(world.QueryEntities(3)==std::vector<EntityId>{1}&&world.QueryEntities(2).size()==2,"all-tag queries include static/body-free entities");
 std::vector<EntityId> subset{2};Check(world.QueryEntities(2,0,&subset)==subset&&world.QueryEntities(2,1)==subset,"candidate intersection and excluded tag query");
 Check(world.RemoveTag(1,0)&&!world.HasTag(1,0)&&world.AddTag(2,0)&&world.HasTag(2,0),"generic runtime add/remove/has tag");
 const auto before=world.QueryEntities(0).size();auto invalid=scene;invalid.Find(1)->renderLayer=63;Check(!world.Build(invalid,nullptr,error,&c)&&world.QueryEntities(0).size()==before,"unregistered assignment rejected before live world mutation");
 auto createdDef=scene.Objects()[0];createdDef.id=0;createdDef.parent=0;const auto created=world.CreateEntity(createdDef,nullptr,&error);Check(created&&world.HasTag(created,1)&&world.DestroyEntity(created,&error)&&!world.HasTag(created,1),"runtime creation/destruction respects classification");
 auto invalidCreated=createdDef;invalidCreated.tags=CategoryBit(63);Check(world.CreateEntity(invalidCreated,nullptr,&error)==0,"unregistered tags rejected for auto-ID runtime creation");
 Scene prefab=Source();Check(SaveSceneToFile(prefab,(directory/"Assets/assembly.judasprefab").string(),error),"save ordinary prefab source");AssetDatabase assets;assets.Scan(directory.string(),project.AssetsDir());AssetRecord asset;Check(assets.Track((directory/"Assets/assembly.judasprefab").string(),asset,error),"prefab normal asset identity");if(failures){std::printf("ERROR %s\n",error.c_str());return 1;}Scene instances;SceneTransform placement;SceneObjectId root;Check(InstantiatePrefab(instances,prefab,asset.id,placement,root,error),"instantiate tagged/layered prefab");const auto child=instances.Find(root)->prefabIds.at(2);
 auto old=instances;instances.Find(child)->renderLayer=1;CapturePrefabEdits(old,instances);prefab.Find(2)->tags=3;prefab.Find(2)->renderLayer=0;SaveSceneToFile(prefab,asset.path,error);Scene resolved;Check(ResolvePrefabs(instances,&assets,resolved,error)&&resolved.Find(child)->renderLayer==1&&resolved.Find(child)->tags==3&&resolved.Find(root)->body->collisionMask==0,"generic override wins while non-overridden tag/layer values propagate");
 ResourceManager resources(nullptr,&assets);RuntimeWorld prefabWorld;Check(prefabWorld.Build(resolved,&resources,error,&c),"linked prefab actual runtime path");const auto savedIdentity=prefabWorld.BaselineFingerprint();prefab.Find(1)->body->collisionMask=kAllCategories;SaveSceneToFile(prefab,asset.path,error);RuntimeWorld changed;Check(changed.Build(resolved,&resources,error,&c)&&changed.BaselineFingerprint()!=savedIdentity,"prefab source filtering changes reject incompatible save");
 Scene legacy;auto& legacyBody=legacy.CreateObject("Legacy");legacyBody.body=SceneBodyComponent{};legacyBody.body->motion=SceneBodyMotion::Dynamic;SaveSceneToString(legacy,text);Check(text.find("collision-mask")==std::string::npos&&text.find("render-layer")==std::string::npos&&world.Build(legacy,nullptr,error),"legacy scene defaults unchanged and needs no registry setup");
 PhysicsWorld physics;Check(physics.Init(),"real physics initialized");auto wall=physics.CreateStaticBox({0,0,0},glm::quat(1,0,0,0),{.5f,.5f,.5f},0,0);auto body=physics.CreateDynamicSphere({-2,0,0},.5f,1,0,0);physics.SetLinearVelocity(body,{10,0,0});physics.SetCollisionFilter(wall,1,0);physics.SetBodyTags(wall,3);physics.Step(.2f);
 Check(physics.GetTransform(body).position.x>-.5f&&physics.LastStepStats().contactPoints==0&&physics.LastStepStats().layerRejectedPairs>0,"bilateral mask rejects real separated impact before narrowphase");std::printf("COUNTS rejected_pairs=%zu candidates=%zu contacts=%zu\n",physics.LastStepStats().layerRejectedPairs,physics.LastStepStats().candidatePairs,physics.LastStepStats().contactPoints);
 physics.SetCollisionFilter(wall,0,kAllCategories);physics.ResetBody(body,{-2,0,0},glm::quat(1,0,0,0));physics.SetLinearVelocity(body,{10,0,0});physics.Step(.2f);Check(physics.GetTransform(body).position.x<-.9f&&physics.LastStepStats().impactEvents>0,"default physical collision still stops body");
 physics.SetCollisionFilter(wall,1,0);PhysicsQueryFilter filter;filter.includeLayers=CategoryBit(1);filter.requiredTags=3;Check(physics.QueryBodiesInAabb({-5,-5,-5},{5,5,5},filter).size()==1,"explicit query includes physically noncolliding tagged layer");filter.excludeLayers=CategoryBit(1);Check(physics.QueryBodiesInAabb({-5,-5,-5},{5,5,5},filter).empty(),"query excludes layer independently");filter.excludeLayers=0;filter.ignoredBodies={wall};Check(physics.QueryBodiesInAabb({-5,-5,-5},{5,5,5},filter).empty(),"query ignores generation-safe body handle");
 physics.CreatePlayerShape(.2f,.3f);auto sweep=physics.SweepPlayerShape({0,0,3},glm::quat(1,0,0,0),{0,0,-6});Check(!sweep.hit,"default player physical sweep passes mask-zero ghost");filter.ignoredBodies.clear();sweep=physics.SweepPlayerShape({0,0,3},glm::quat(1,0,0,0),{0,0,-6},false,0,1,&filter);Check(sweep.hit&&sweep.hitBody.id==wall.id,"explicit filtered capsule sweep hits query-only geometry");
 const auto stale=wall;physics.DestroyBody(wall);wall=physics.CreateStaticBox({0,0,0},glm::quat(1,0,0,0),{.5f,.5f,.5f},0,0);unsigned layer;CategoryMask mask;Check(!physics.SetCollisionFilter(stale,3,0)&&physics.GetCollisionFilter(wall,layer,mask)&&layer==0&&mask==kAllCategories,"slot reuse resets category state and rejects stale handle");
 EditorDocument doc;doc.GetScene()=instances;doc.BeginEdit();doc.GetScene().Find(child)->tags=1;doc.CommitEdit();Check(doc.GetScene().Find(child)->prefabOverrides.count("tags"),"ordinary inspector edit captures tag override");doc.Undo();Check(doc.GetScene().Find(child)->tags==2,"classification edit undo restores source-derived tags");
 EngineHost host;Check(host.Init("M39 classification",256,256,false,error),"real engine GL context");if(failures){std::printf("ERROR %s\n",error.c_str());return 1;}auto& renderer=host.GetRenderer();Scene visual;visual.Settings().ambientColor={1,1,1};visual.Settings().mainCameraRenderMask=CategoryBit(0);
 for(unsigned i=0;i<2;++i){auto& object=visual.CreateObject("Layer prop");object.renderLayer=i;object.transform.position={0,0,-5};object.render=SceneRenderComponent{};object.render->color=i?glm::vec3(0,1,0):glm::vec3(1,0,0);auto& cam=visual.CreateObject("Layer view");cam.renderCamera=SceneRenderCameraComponent{};cam.renderCamera->width=128;cam.renderCamera->height=128;cam.renderCamera->renderMask=CategoryBit(i);}
 auto& emitter=visual.CreateObject("Layer particles");emitter.renderLayer=1;emitter.particleEmitter=ParticleEmitterSettings{};emitter.particleEmitter->rate=0;emitter.particleEmitter->burst=3;emitter.transform.position={2,0,-5};RuntimeWorld renderWorld;Check(renderWorld.Build(visual,&host.Resources(),error,&c),"render world resolves project layers");renderWorld.UpdateVisualParticles(.1f);
 const auto proj=glm::perspective(glm::radians(60.f),1.f,.1f,100.f);renderer.ResetStats();RenderWorldFrame(renderer,256,256,renderWorld,glm::mat4(1),proj,{0,0,0},1);
 TextureData first,second;const auto& cameras=renderWorld.PresentationCameras();Check(renderer.ReadTextureForDiagnostics(renderer.RenderTargetTexture(cameras[0].target),first)&&renderer.ReadTextureForDiagnostics(renderer.RenderTargetTexture(cameras[1].target),second)&&first.pixels!=second.pixels,"real M33 camera masks produce different target pixels");
 Check(renderer.Stats().layerRejectedDraws==3&&renderer.Stats().layerRejectedEmitters==2&&renderer.Stats().particlesSubmitted==3,"main/M33 meshes and particles use independent mask filtering");
 const auto particlesBefore=renderWorld.VisualEmitters()[0].pool.Particles()[0].age;renderWorld.UpdateVisualParticles(.1f);Check(renderWorld.VisualEmitters()[0].pool.Particles()[0].age>particlesBefore,"render filtering does not stop particle simulation");
 renderer.SetRenderMask(1);auto target=renderer.CreateRenderTarget(32,32,error);renderer.BeginRenderTarget(target);renderer.SetRenderMask(2);renderer.EndRenderTarget();Check(renderer.AllowsLayer(0)&&!renderer.AllowsLayer(1),"target restores previous render-mask state");renderer.DestroyRenderTarget(target);
 for(bool allowed:{true,false}){renderer.SetRenderMask(allowed?1:2);renderer.ResetStats();auto begin=std::chrono::steady_clock::now();for(int i=0;i<1000;++i){renderer.SetRenderLayer(0);renderer.DrawBox({0,0,-5},glm::quat(1,0,0,0),{.5f,.5f,.5f},{1,0,0});}std::printf("PERFORMANCE permitted=%d submissions=%u rejected=%u cpu_ms=%.6f\n",allowed,renderer.Stats().drawCalls,renderer.Stats().layerRejectedDraws,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());}
 renderWorld.Destroy();host.Shutdown();resources.Shutdown();std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
