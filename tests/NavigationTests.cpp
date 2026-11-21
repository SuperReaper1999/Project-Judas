#include <algorithm>
#include "NavigationSystem.h"
#include "RuntimeWorld.h"
#include "Project.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "ResourceManager.h"
#include "Prefab.h"
#include "ScriptSystem.h"
#include <fstream>
#include <glm/gtc/quaternion.hpp>
#include <cstdio>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <locale>
#include <sstream>
namespace {int failures=0,checks=0;void check(bool b,const char* n){++checks;if(!b){++failures;std::printf("FAIL %s\n",n);}}Scene fixture(){Scene s;s.SetNextId(10);SceneObject floor;floor.id=1;floor.body=SceneBodyComponent{};floor.body->halfExtents={8,.5f,8};floor.transform.position={0,-.5f,0};s.Objects().push_back(floor);SceneObject surface;surface.id=2;surface.navigationSurface=NavigationSurfaceSettings{};surface.navigationSurface->halfExtents={10,4,10};surface.navigationSurface->minRegion=1;s.Objects().push_back(surface);return s;}}
int main(){std::string error;auto scene=fixture();auto* source=scene.Find(2);ProjectNavigation config;NavigationData data;check(BakeNavigation(scene,*source,config,data,error),"bake authoritative box floor");if(failures){std::puts(error.c_str());return 1;}check(data.layers.size()>1,"tiled bake");NavigationSystem system;check(system.LoadSurface(2,source->transform,std::make_shared<NavigationData>(data),error),"load tiled cache");auto loc=system.Sample({0,.2f,0},1);check(loc&&glm::distance(loc->position,glm::vec3(0,.1f,0))<.2f,"nearest nav surface");auto route=system.FindPath({-5,0,0},{5,0,0});check(route.status==NavigationPath::Status::Complete&&route.distance>9.8f,"complete route");check(!system.Sample({30,0,0},1),"sample outside fails");check(system.FindPath({30,0,0},{5,0,0}).status==NavigationPath::Status::Failed,"unreachable start fails");NavigationFilter excluded;excluded.exclude=CategoryBit(0);check(!system.Sample({0,0,0},1,excluded),"navigation area mask");NavigationFilter wrong;wrong.profile=1;check(!system.Sample({0,0,0},1,wrong),"incompatible profile rejected");auto ray=system.Raycast({0,0,0},{20,0,0});check(ray&&ray->position.x<8,"navigation ray stops at boundary");
 auto path=std::filesystem::path("/tmp/judas-m53-tests.judasnav");check(SaveNavigation(path.string(),data,error),"asset write");NavigationData loaded;check(LoadNavigation(path.string(),loaded,error)&&loaded.layers==data.layers&&loaded.fingerprint==data.fingerprint,"asset roundtrip");std::string text;check(SaveSceneToString(scene,text),"serialize navigation component");Scene roundtrip;check(LoadSceneFromString(text,roundtrip,error)&&NavigationProperties(*roundtrip.Find(2))==NavigationProperties(*source),"component roundtrip");auto registry=config.Serialize();ProjectNavigation parsed;check(ProjectNavigation::Parse(registry,parsed,error)&&parsed.Serialize()==registry,"project profile/area roundtrip");unsigned area;config.areas.Add("Costly",area);config.areas.Rename(area,"Slow");auto stable=area;config.areas.Remove(area,true);config.areas.Add("Next",area);check(area!=stable,"area deleted IDs not reused");
 auto rotated=fixture();auto rotation=glm::angleAxis(.9f,glm::normalize(glm::vec3(1,2,3)));glm::vec3 offset(40,20,-5);for(auto& o:rotated.Objects()){o.transform.position=offset+rotation*o.transform.position;o.transform.rotation=rotation;}NavigationData turned;check(BakeNavigation(rotated,*rotated.Find(2),ProjectNavigation{},turned,error),"arbitrarily oriented surface bake");NavigationSystem rotationSystem;check(rotationSystem.LoadSurface(2,rotated.Find(2)->transform,std::make_shared<NavigationData>(turned),error),"arbitrary frame load");auto p=rotationSystem.FindPath(offset+rotation*glm::vec3(-5,0,0),offset+rotation*glm::vec3(5,0,0));check(p.status==NavigationPath::Status::Complete&&std::abs(p.distance-route.distance)<.02f,"rotated path equivalent");check(rotationSystem.Sample(offset+rotation*glm::vec3(0,.2f,0),1).has_value(),"rotated sample");
 NavigationGeometry original,changed;CollectNavigationGeometry(scene,*source,ProjectNavigation{},original,error);auto altered=scene;altered.Find(1)->body->halfExtents.x=7;CollectNavigationGeometry(altered,*altered.Find(2),ProjectNavigation{},changed,error);check(original.fingerprint!=changed.fingerprint,"collision geometry change invalidates bake");altered=scene;altered.Find(1)->render=SceneRenderComponent{};CollectNavigationGeometry(altered,*altered.Find(2),ProjectNavigation{},changed,error);check(original.fingerprint==changed.fingerprint,"visual-only mesh does not affect navigation");
 auto fractional=fixture();
 fractional.Find(1)->transform.position={.000000125f,-.12345678f,1.2345678f};
 fractional.Find(1)->navigationModifier=NavigationModifierSettings{};
 fractional.Find(1)->navigationModifier->halfExtents={.5f,1.125f,2.25f};
 NavigationGeometry fractionalGeometry;
 const ProjectNavigation defaults;
 const auto& fractionalSurface=*fractional.Find(2);
 check(CollectNavigationGeometry(fractional,fractionalSurface,defaults,fractionalGeometry,error),
       "collect fractional geometry with a navigation modifier");
 std::ostringstream legacy;
 legacy.imbue(std::locale::classic());
 legacy<<std::setprecision(9)<<"JudasNavBake1 "<<defaults.Serialize()<<' ';
 auto descriptor=fractionalSurface;descriptor.navigationSurface->asset.clear();
 for(const auto& [key,value]:NavigationProperties(descriptor))legacy<<key<<' '<<std::quoted(value)<<' ';
 for(float v:fractionalGeometry.vertices)legacy<<(v==0?0:v)<<' ';
 for(int i:fractionalGeometry.triangles)legacy<<i<<' ';
 for(const auto& volume:fractionalGeometry.modifiers){
  legacy<<volume.area<<' '<<volume.blocked<<' '<<volume.bottom<<' '<<volume.top<<' ';
  for(float v:volume.polygon)legacy<<v<<' ';
 }
 check(fractionalGeometry.fingerprint==SceneFingerprintSha256(legacy.str()),
       "numeric formatting preserves the legacy baked navigation fingerprint");
 {
  Scene scenery;
  for(EntityId id=1;id<=256;++id){SceneObject object;object.id=id;scenery.InsertObject(object);}
  RuntimeWorld indexed;
  check(indexed.Build(scenery,nullptr,error),"scenery-only world builds for navigation membership checks");
  SceneObject agent;agent.id=300;agent.navigationAgent=NavigationAgentSettings{};
  check(indexed.StageRegionObject(agent,agent,error),"stage a navigation agent privately");
  indexed.UpdateNavigation(.02f);
  check(!indexed.Navigation().Agent(agent.id),"staged navigation owner remains private");
  indexed.PublishRegion({agent.id});
  const auto before=indexed.MetadataStats().lookups;
  indexed.UpdateNavigation(.02f);
  check(indexed.Navigation().Agent(agent.id)&&indexed.MetadataStats().lookups-before<16,
        "published agent updates without walking inert scenery");
  check(indexed.SetNavigationEnabled(agent.id,"agent",false),"disable a published agent");
  indexed.UpdateNavigation(.02f);
  check(!indexed.Navigation().Agent(agent.id),"disabled agent retires from navigation");
  check(indexed.SetNavigationEnabled(agent.id,"agent",true),"enable the same navigation owner");
  indexed.UpdateNavigation(.02f);
  check(indexed.Navigation().Agent(agent.id)!=nullptr,"re-enabled agent registers again");
  indexed.HideRegion({agent.id});
  indexed.UpdateNavigation(.02f);
  check(!indexed.Navigation().Agent(agent.id),"hidden agent retires from navigation");
  indexed.PublishRegion({agent.id});
  indexed.UpdateNavigation(.02f);
  check(indexed.Navigation().Agent(agent.id)!=nullptr,"republished agent registers again");
  indexed.RemoveRegionObject(agent.id);
  indexed.UpdateNavigation(.02f);
  check(indexed.NavigationOwners().empty()&&!indexed.Navigation().Agent(agent.id),"regional removal clears navigation ownership");
 }
 auto disabledModifier=fixture();disabledModifier.Find(1)->navigationModifier=NavigationModifierSettings{};disabledModifier.Find(1)->navigationModifier->enabled=false;disabledModifier.Find(1)->navigationModifier->excludeSource=true;NavigationGeometry disabledGeometry;check(CollectNavigationGeometry(disabledModifier,*disabledModifier.Find(2),ProjectNavigation{},disabledGeometry,error)&&disabledGeometry.fingerprint==original.fingerprint,"disabled modifier cannot exclude physical source");
 auto blocked=fixture();SceneObject wall;wall.id=3;wall.navigationModifier=NavigationModifierSettings{};wall.navigationModifier->halfExtents={.5f,2,3};wall.navigationModifier->blocked=true;blocked.Objects().push_back(wall);NavigationData block;check(BakeNavigation(blocked,*blocked.Find(2),ProjectNavigation{},block,error),"blocked modifier bake");NavigationSystem detour;detour.LoadSurface(2,{},std::make_shared<NavigationData>(block),error);auto around=detour.FindPath({-5,0,0},{5,0,0});check(around.status==NavigationPath::Status::Complete&&around.distance>10.5f,"route around nonwalkable volume");check(!detour.Sample({0,0,0},.2f),"blocked region not sampleable");
 auto costScene=fixture();ProjectNavigation costConfig;costConfig.areas.Add("Expensive",area);wall.navigationModifier->blocked=false;wall.navigationModifier->area=area;costScene.Objects().push_back(wall);NavigationData costs;check(BakeNavigation(costScene,*costScene.Find(2),costConfig,costs,error),"area modifier bake");NavigationSystem costsSystem(costConfig);costsSystem.LoadSurface(2,{},std::make_shared<NavigationData>(costs),error);auto cheap=costsSystem.FindPath({-5,0,0},{5,0,0});NavigationFilter expensive;expensive.costs[area]=30;auto avoid=costsSystem.FindPath({-5,0,0},{5,0,0},expensive);check(cheap.status==NavigationPath::Status::Complete&&avoid.distance>cheap.distance+.2f,"per-area cost chooses alternate route");expensive.exclude=CategoryBit(area);check(costsSystem.FindPath({-5,0,0},{5,0,0},expensive).status==NavigationPath::Status::Complete,"area exclusion still finds alternate");
 Project project;std::filesystem::remove_all("/tmp/judas-m53-project");check(Project::CreateNew("/tmp/judas-m53-project","Navigation",project,error),"create navigation project");project.Settings().navigation=costConfig;check(project.Save(error),"save project navigation registries");Project reloaded;check(reloaded.Load(project.ProjectFile(),error)&&reloaded.Settings().navigation.Serialize()==costConfig.Serialize(),"project reload retains profile and area IDs");AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());auto navPath=std::filesystem::path(project.AssetsDir())/"test.judasnav";SaveNavigation(navPath.string(),data,error);AssetRecord record;check(assets.Track(navPath.string(),record,error),"normal nav asset tracking");ResourceManager resources(nullptr,&assets);auto runtimeScene=fixture();runtimeScene.Find(2)->navigationSurface->asset=record.id;RuntimeWorld world;check(world.Build(runtimeScene,&resources,error),"ordinary world builds");if(!world.IsBuilt()){std::puts(error.c_str());return 1;}resources.WaitForAll();world.UpdateNavigation(.02f);check(world.Navigation().Statistics().surfaces==1&&world.Navigation().Errors().empty(),"async CPU navigation resource installs in world");auto invalid=runtimeScene;invalid.Find(2)->navigationSurface->asset="missing";RuntimeWorld missing;check(!missing.Build(invalid,&resources,error)&&!missing.IsBuilt(),"missing surface asset fails safely");auto staleScene=runtimeScene;staleScene.Find(1)->body->halfExtents.x=7;RuntimeWorld stale;check(stale.Build(staleScene,&resources,error),"stale asset world reports query-time error");resources.WaitForAll();stale.UpdateNavigation(.02f);check(!stale.Navigation().Errors().empty()&&!stale.Navigation().Sample({0,0,0},1),"stale source rejected instead of silently used");stale.Destroy();
 {
  RuntimeWorld warm;
  check(warm.Build(runtimeScene,&resources,error),"world for startup navigation builds");
  resources.WaitForAll();
  const auto revision=warm.Navigation().Revision();
  warm.Navigation().PrepareStartup(warm);
  check(warm.Navigation().Statistics().surfaces==0&&warm.Navigation().Revision()==revision&&
        !warm.Navigation().Sample({0,0,0},1),"startup preparation leaves navigation unpublished");
  warm.UpdateNavigation(.02f);
  const auto live=warm.Navigation().FindPath({-5,0,0},{5,0,0});
  const auto normal=world.Navigation().FindPath({-5,0,0},{5,0,0});
  check(warm.Navigation().Statistics().surfaces==1&&warm.Navigation().Errors().empty()&&
        live.status==NavigationPath::Status::Complete&&live.distance==normal.distance,
        "prepared and ordinary first-step publication give the same path");
  const auto copies=warm.MetadataStats().definitionsCopied;
  warm.UpdateNavigation(.02f);
  check(warm.MetadataStats().definitionsCopied==copies&&warm.Navigation().Statistics().surfaces==1,
        "routine navigation update reads live definitions without copying the bake source scene");
 }
 {
  RuntimeWorld changed;
  check(changed.Build(runtimeScene,&resources,error),"world for startup navigation invalidation builds");
  resources.WaitForAll();
  changed.Navigation().PrepareStartup(changed);
  auto transform=changed.RuntimeDefinition(1)->transform;
  transform.position.y+=2;
  check(changed.SetRuntimeTransform(1,transform),"collision geometry can change before first navigation tick");
  changed.UpdateNavigation(.02f);
  const auto& errors=changed.Navigation().Errors();
  check(changed.Navigation().Statistics().surfaces==0&&errors.count(2)&&
        errors.at(2).find("source geometry differs")!=std::string::npos,
        "first-step change discards prepared geometry and reports the stale bake");
 }
 {
  const auto editedPath=std::filesystem::path(project.AssetsDir())/"startup-source.judasnav";
  AssetRecord edited;
  check(SaveNavigation(editedPath.string(),data,error)&&assets.Track(editedPath.string(),edited,error),
        "register a startup dependency file");
  std::string digest;
  check(SceneFingerprintSha256File(edited.path,digest,error),"fingerprint original dependency");
  NavigationSourceFiles files;
  files.files.emplace(edited.id,NavigationSourceFiles::File{edited.path,digest,edited.type});
  check(files.Matches(assets),"unchanged dependency can be reused");
  const auto time=std::filesystem::last_write_time(editedPath);
  {
   std::fstream editedStream(editedPath,std::ios::in|std::ios::out|std::ios::binary);
   editedStream.put('X');
  }
  std::filesystem::last_write_time(editedPath,time);
  check(!files.Matches(assets),"same-length external edit with restored mtime invalidates preparation");
 }
 auto editPath=std::filesystem::path(project.AssetsDir())/"nav-edit.js";
 std::ofstream(editPath)<<R"JS(export default class {
  constructor({entity}) {this.entity=entity;this.state={};}
  start() {
   const e=this.entity;
   if(e.navigationObstacle) {
    this.state.applied=e.setNavigationObstacle({halfExtents:{x:1,y:1,z:9}});
    try {e.setNavigationObstacle({enabled:false,radius:-1});}
    catch {this.state.rejected=true;}
    try {e.setNavigationObstacle({halfExtents:{x:20,y:1,z:20}});}
    catch(error) {this.state.layerLimit=error.message.includes('eight tile layers');}
   } else {
    this.state.applied=e.setNavigationLink({bidirectional:true,localEnd:{x:6,y:0,z:1}});
    try {e.setNavigationLink({enabled:false,area:61});}
    catch {this.state.rejected=true;}
   }
  }
 })JS";
 AssetRecord editScript;
 check(assets.Track(editPath.string(),editScript,error),"register runtime navigation edit script");
 SceneObject obstacle;obstacle.id=0;obstacle.navigationObstacle=NavigationObstacleSettings{};obstacle.navigationObstacle->halfExtents={1,1,3};obstacle.scripts.push_back({1,editScript.id,true,"{}"});auto oid=world.CreateEntity(obstacle,nullptr,&error);check(oid!=0,"runtime obstacle spawn");for(int i=0;i<20;++i)world.UpdateNavigation(.02f);check(!world.Navigation().Sample({0,0,0},.2f),"tile-cache obstacle carves");auto obstaclePose=world.RuntimeDefinition(oid)->transform;obstaclePose.position.x=4;world.SetRuntimeTransform(oid,obstaclePose);for(int i=0;i<30;++i)world.UpdateNavigation(.02f);check(world.Navigation().Sample({0,0,0},.2f)&&!world.Navigation().Sample({4,0,0},.2f),"moved obstacle restores old footprint and carves new one");SceneObject repathProbe;
 repathProbe.transform.position={-5,0,0};
 repathProbe.navigationAgent=NavigationAgentSettings{};
 repathProbe.navigationAgent->repathSeconds=60;
 auto repathId=world.CreateEntity(repathProbe,nullptr,&error);
 world.UpdateNavigation(.02f);
 world.Navigation().SetDestination(repathId,{6,0,0});
 world.UpdateNavigation(.02f);
 auto* repathState=world.Navigation().Agent(repathId);
 check(repathState&&repathState->path.status==NavigationPath::Status::Complete,"short obstacle leaves an existing route around it");
 world.FixedScripts(nullptr,.02f);
 auto obstacleState=world.Scripts()->Capture();
 check(obstacleState.size()==1&&obstacleState[0].json.find("\"applied\":true")!=std::string::npos&&
       obstacleState[0].json.find("\"rejected\":true")!=std::string::npos&&
       world.RuntimeDefinition(oid)->navigationObstacle->enabled,"valid scripted obstacle resize and atomic invalid patch");
 check(obstacleState.size()==1&&obstacleState[0].json.find("\"layerLimit\":true")!=std::string::npos&&
       world.RuntimeDefinition(oid)->navigationObstacle->halfExtents==glm::vec3(1,1,9),
       "oversized edit reports the eight-layer limit and preserves the accepted footprint");
 for(int i=0;i<30;++i)world.UpdateNavigation(.02f);
 check(world.Navigation().FindPath({-5,0,0},{6,0,0}).status!=NavigationPath::Status::Complete&&
       repathState&&repathState->path.status!=NavigationPath::Status::Complete,
       "resized obstacle blocks a real route and repaths the active agent before its 60-second interval");
 auto oversized=*world.RuntimeDefinition(oid)->navigationObstacle;
 oversized.halfExtents={20,1,20};
 const auto beforeRejectedEdit=world.Navigation().Revision();
 std::string oversizedError;
 check(!world.SetNavigationObstacleSettings(oid,oversized,oversizedError)&&
       oversizedError.find("eight tile layers")!=std::string::npos,
       "native edit also rejects an oversized replacement of a live carve");
 world.UpdateNavigation(.02f);
 check(world.Navigation().Revision()==beforeRejectedEdit&&!world.Navigation().Sample({4,0,0},.2f),
       "rejected replacement keeps the existing carve and navigation revision");
 check(world.Navigation().Sample({-5,0,0},.2f).has_value(),
       "oversized rejected footprint does not carve the walkable start");
 world.DestroyEntity(repathId,&error);
 auto before=world.Navigation().Revision();check(world.DestroyEntity(oid,&error),"obstacle destruction");for(int i=0;i<20;++i)world.UpdateNavigation(.02f);check(world.Navigation().Sample({0,0,0},.2f).has_value()&&world.Navigation().Revision()>before,"obstacle removal restores walkability");
 SceneObject agent;agent.navigationAgent=NavigationAgentSettings{};agent.characterMotor=CharacterMotorSettings{};agent.transform.position={-5,.9f,0};auto aid=world.CreateEntity(agent,nullptr,&error);world.UpdateNavigation(.02f);check(world.Navigation().SetDestination(aid,{5,0,0}),"destination accepted");auto position=world.RuntimeCharacter(aid)->position;world.UpdateNavigation(.02f);auto state=world.Navigation().Agent(aid);check(state&&state->steering.x>0&&state->remaining>8,"agent guidance and distance");check(world.RuntimeCharacter(aid)->position==position,"navigation does not move authoritative motor");auto second=world.CreateEntity(agent,nullptr,&error);world.UpdateNavigation(.02f);check(second!=aid&&world.Navigation().Agent(second)!=state,"independent agent instances");world.Navigation().ClearDestination(aid);check(!world.Navigation().Agent(aid)->hasDestination,"clear route");world.DestroyEntity(aid,&error);world.UpdateNavigation(.02f);check(!world.Navigation().Agent(aid),"destroy clears stale agent");auto replacement=world.CreateEntity(agent,nullptr,&error);world.UpdateNavigation(.02f);check(replacement!=aid&&!world.Navigation().Agent(aid),"runtime identity reuse cannot alias destroyed agent");world.SetNavigationEnabled(replacement,"agent",false);world.UpdateNavigation(.02f);check(!world.Navigation().Agent(replacement),"disable unregisters navigation state");world.SetNavigationEnabled(replacement,"agent",true);world.UpdateNavigation(.02f);world.Navigation().SetDestination(replacement,world.RuntimeCharacter(replacement)->position-glm::vec3(0,.9f,0));world.UpdateNavigation(.02f);check(world.Navigation().Agent(replacement)->path.status==NavigationPath::Status::Complete&&world.Navigation().Agent(replacement)->remaining<.3f,"already reached destination reports short resolved route");auto offsetMotor=world.RuntimeCharacter(replacement);auto offsetSettings=offsetMotor->settings;offsetSettings.offset={0,2,0};world.SetCharacterSettings(replacement,offsetSettings);offsetMotor->position.y=-1.1f;world.Navigation().SetDestination(replacement,{5,0,0});world.UpdateNavigation(.02f);check(world.Navigation().Agent(replacement)->path.status==NavigationPath::Status::Complete,"motor shape offset contributes to physical capsule foot");SceneObject mover;mover.navigationAgent=NavigationAgentSettings{};mover.transform.position={-5,.9f,0};auto pointAgent=world.CreateEntity(mover,nullptr,&error);world.UpdateNavigation(.02f);world.Navigation().SetDestination(pointAgent,{5,0,0});world.UpdateNavigation(.02f);check(world.Navigation().Agent(pointAgent)->steering.x>0&&world.RuntimeDefinition(pointAgent)->transform.position==mover.transform.position,"non-motor root projection yields guidance without moving entity");world.Destroy();check(!world.IsBuilt(),"world cleanup");
 // Genuine disconnected islands connected only through an explicit link.
 auto islands=fixture();islands.Find(1)->body->halfExtents={3,.5f,3};islands.Find(1)->transform.position.x=-8;islands.Find(2)->transform.position.x=-8;islands.Find(2)->navigationSurface->halfExtents={4,4,4};SceneObject floor2=*islands.Find(1);floor2.id=4;floor2.transform.position.x=8;islands.Objects().push_back(floor2);SceneObject surface2=*islands.Find(2);surface2.id=5;surface2.transform.position.x=8;islands.Objects().push_back(surface2);NavigationData left,right;check(BakeNavigation(islands,*islands.Find(2),ProjectNavigation{},left,error)&&BakeNavigation(islands,*islands.Find(5),ProjectNavigation{},right,error),"two independent surface assets");auto leftFile=std::filesystem::path(project.AssetsDir())/"left.judasnav",rightFile=std::filesystem::path(project.AssetsDir())/"right.judasnav";SaveNavigation(leftFile.string(),left,error);SaveNavigation(rightFile.string(),right,error);AssetRecord leftAsset,rightAsset;assets.Track(leftFile.string(),leftAsset,error);assets.Track(rightFile.string(),rightAsset,error);islands.Find(2)->navigationSurface->asset=leftAsset.id;islands.Find(5)->navigationSurface->asset=rightAsset.id;RuntimeWorld linked;check(linked.Build(islands,&resources,error),"multiple-region fixture world");resources.WaitForAll();linked.UpdateNavigation(.02f);check(linked.Navigation().FindPath({-8,0,0},{8,0,0}).status!=NavigationPath::Status::Complete,"islands disconnected without link");
 SceneObject connection;connection.navigationLink=NavigationLinkSettings{};connection.navigationLink->start={-6,0,0};connection.navigationLink->end={6,0,0};connection.navigationLink->bidirectional=false;connection.scripts.push_back({1,editScript.id,true,"{}"});auto lid=linked.CreateEntity(connection,nullptr,&error);linked.UpdateNavigation(.02f);auto crossing=linked.Navigation().FindPath({-8,0,0},{8,0,0});check(crossing.status==NavigationPath::Status::Complete,"explicit cross-surface link connects islands");bool marker=false;for(auto c:crossing.corners)marker|=c.link==lid;check(marker,"path exposes traversal link identity");check(linked.Navigation().FindPath({8,0,0},{-8,0,0}).status!=NavigationPath::Status::Complete,"one-way link direction enforced");SceneObject linkFollower;
 linkFollower.transform.position={-8,0,0};
 linkFollower.navigationAgent=NavigationAgentSettings{};
 linkFollower.navigationAgent->repathSeconds=60;
 auto linkFollowerId=linked.CreateEntity(linkFollower,nullptr,&error);
 linked.UpdateNavigation(.02f);
 linked.Navigation().SetDestination(linkFollowerId,{8,0,0});
 linked.UpdateNavigation(.02f);
 auto* linkFollowerState=linked.Navigation().Agent(linkFollowerId);
 check(linkFollowerState&&linkFollowerState->path.status==NavigationPath::Status::Complete&&!linkFollowerState->onLink,
       "link follower has an existing route before reaching the traversal point");
 linked.FixedScripts(nullptr,.02f);
 auto linkScriptState=linked.Scripts()->Capture();
 check(linkScriptState.size()==1&&linkScriptState[0].json.find("\"applied\":true")!=std::string::npos&&
       linkScriptState[0].json.find("\"rejected\":true")!=std::string::npos&&
       linked.RuntimeDefinition(lid)->navigationLink->enabled,"valid scripted link edit and atomic invalid area");
 linked.UpdateNavigation(.02f);
 auto editedCrossing=linked.Navigation().FindPath({-8,0,0},{8,0,0});
 check(editedCrossing.status==NavigationPath::Status::Complete&&
       std::any_of(editedCrossing.corners.begin(),editedCrossing.corners.end(),[&](const auto& corner){
           return corner.link==lid&&corner.linkEnd==glm::vec3(6,0,1);
       })&&linked.Navigation().FindPath({8,0,0},{-8,0,0}).status==NavigationPath::Status::Complete,
       "routes publish the new endpoint and reverse direction");
 check(linkFollowerState&&std::any_of(linkFollowerState->path.corners.begin(),
       linkFollowerState->path.corners.end(),[&](const auto& corner){
           return corner.link==lid&&corner.linkEnd==glm::vec3(6,0,1);
       }),"link edit refreshes active route before its 60-second interval");
 linked.DestroyEntity(linkFollowerId,&error);
 check(linked.SetNavigationLinkSettings(lid,*connection.navigationLink),"restore original link for traversal checks");
 linked.UpdateNavigation(.02f);
 linked.SetNavigationEnabled(lid,"link",false);linked.UpdateNavigation(.02f);check(linked.Navigation().FindPath({-8,0,0},{8,0,0}).status!=NavigationPath::Status::Complete,"disabled link unavailable");linked.SetNavigationEnabled(lid,"link",true);linked.UpdateNavigation(.02f);auto walker=linked.CreateEntity(agent,nullptr,&error);auto walkerPose=linked.RuntimeDefinition(walker)->transform;walkerPose.position={-6,.9f,0};linked.SetRuntimeTransform(walker,walkerPose);linked.UpdateNavigation(.02f);linked.Navigation().SetDestination(walker,{8,0,0});linked.UpdateNavigation(.02f);check(linked.Navigation().Agent(walker)->onLink&&glm::length(linked.Navigation().Agent(walker)->steering)==0,"special link suspends guidance for explicit traversal");auto* activeTraversal=linked.Navigation().Agent(walker);
 const auto originalEnd=activeTraversal&&activeTraversal->corner<activeTraversal->path.corners.size()
     ?activeTraversal->path.corners[activeTraversal->corner].linkEnd:glm::vec3(0);
 auto changedTraversal=*connection.navigationLink;
 changedTraversal.end.z=1;
 check(linked.SetNavigationLinkSettings(lid,changedTraversal),"edit link during existing handoff");
 linked.UpdateNavigation(.02f);
 check(activeTraversal&&activeTraversal->onLink&&activeTraversal->corner<activeTraversal->path.corners.size()&&
       activeTraversal->path.corners[activeTraversal->corner].linkEnd==originalEnd,
       "active traversal keeps its original endpoint across the edit");
 check(linked.Navigation().CompleteLink(walker),"script traversal acknowledgement advances link");linked.DestroyEntity(lid,&error);connection.navigationLink->bidirectional=true;linked.CreateEntity(connection,nullptr,&error);linked.UpdateNavigation(.02f);check(linked.Navigation().FindPath({8,0,0},{-8,0,0}).status==NavigationPath::Status::Complete,"bidirectional link reverse route");linked.Destroy();
 // Separate geometric expectations: disconnected barrier, clearance and climb.
 auto split=fixture();wall.navigationModifier->area=0;wall.navigationModifier->blocked=true;wall.navigationModifier->halfExtents={.7f,2,10};split.Objects().push_back(wall);NavigationData splitData;BakeNavigation(split,*split.Find(2),ProjectNavigation{},splitData,error);NavigationSystem splitSystem;splitSystem.LoadSurface(2,{},std::make_shared<NavigationData>(splitData),error);check(splitSystem.FindPath({-5,0,0},{5,0,0}).status==NavigationPath::Status::Partial,"disconnected destination returns partial rather than complete");
 ProjectNavigation wide;wide.profiles[0].radius=1.4f;NavigationData wideData;BakeNavigation(scene,*source,wide,wideData,error);NavigationSystem wideSystem(wide);wideSystem.LoadSurface(2,{},std::make_shared<NavigationData>(wideData),error);check(system.Sample({7.2f,.1f,0},.15f)&&!wideSystem.Sample({7.2f,.1f,0},.15f),"larger profile erosion removes unsafe edge clearance");
 auto raised=fixture();SceneObject step;step.id=6;step.body=SceneBodyComponent{};step.body->halfExtents={2,.5f,2};step.transform.position={3,.5f,0};raised.Objects().push_back(step);NavigationData highStep;check(BakeNavigation(raised,*raised.Find(2),ProjectNavigation{},highStep,error),"raised physical geometry bake");NavigationSystem stepSystem;stepSystem.LoadSurface(2,{},std::make_shared<NavigationData>(highStep),error);check(stepSystem.FindPath({0,0,0},{3,1,0}).status!=NavigationPath::Status::Complete,"climb threshold excludes one metre step");
 raised.Find(6)->body->halfExtents.y=.1f;raised.Find(6)->transform.position.y=.1f;NavigationData lowStep;BakeNavigation(raised,*raised.Find(2),ProjectNavigation{},lowStep,error);NavigationSystem lowSystem;lowSystem.LoadSurface(2,{},std::make_shared<NavigationData>(lowStep),error);check(lowSystem.FindPath({0,0,0},{3,.2f,0}).status==NavigationPath::Status::Complete,"configured climb permits low step");
 auto far=fixture();far.Settings().worldOrigin={1e12,-2e12,3e12};NavigationData farData;check(BakeNavigation(far,*far.Find(2),ProjectNavigation{},farData,error)&&farData.fingerprint==data.fingerprint,"large absolute fixed origin preserves local bake precision");
 auto sphere=fixture();sphere.Find(1)->body->shape=SceneShape::Sphere;sphere.Find(1)->body->radius=3;sphere.Find(1)->transform.position={0,-3,0};NavigationData spherical;check(BakeNavigation(sphere,*sphere.Find(2),ProjectNavigation{},spherical,error),"real sphere collision surface tessellation");NavigationSystem sphereSystem;sphereSystem.LoadSurface(2,{},std::make_shared<NavigationData>(spherical),error);check(sphereSystem.Sample({0,0,0},.5f).has_value(),"sphere upper cap walkable without global gravity assumption");
 auto bytes=std::vector<unsigned char>{1,2,3};check(!DecodeNavigation(bytes,loaded,error),"malformed nav data fails safely");NavigationData repeat;BakeNavigation(scene,*source,ProjectNavigation{},repeat,error);check(repeat.layers==data.layers&&repeat.fingerprint==data.fingerprint,"identical bake is repeatable");
 Scene prefab;auto authored=fixture();authored.Find(1)->navigationAgent=NavigationAgentSettings{};check(CreatePrefab(authored,1,prefab,error)&&prefab.Find(1)->navigationAgent.has_value(),"generic prefab retains navigation component");std::string f1,f2;ComputeSceneFingerprint(authored,f1,error);authored.Find(1)->navigationAgent->speed=6;ComputeSceneFingerprint(authored,f2,error);check(f1!=f2,"authored navigation configuration contributes to baseline");
 {
  RuntimeWorld capacity;
  check(capacity.Build(runtimeScene,&resources,error),"build navigation obstacle capacity fixture");
  resources.WaitForAll();
  capacity.UpdateNavigation(.02f);
  SceneObject outside;
  outside.navigationObstacle=NavigationObstacleSettings{};
  outside.transform.position={100.f,0.f,100.f};
  const auto outsideId=capacity.CreateEntity(outside,nullptr,&error);
  capacity.UpdateNavigation(.02f);
  check(outsideId&&capacity.Navigation().Statistics().obstacles==0,"off-mesh obstacle uses no Detour slot");
  auto placement=outside.transform;
  placement.position={0.f,.3f,0.f};
  check(capacity.SetRuntimeTransform(outsideId,placement),"move off-mesh obstacle onto the surface");
  capacity.UpdateNavigation(.02f);
  check(capacity.Navigation().Statistics().obstacles==1,"previously off-mesh obstacle registers when it reaches tiles");
  check(capacity.SetRuntimeTransform(outsideId,outside.transform),"move the carving obstacle off the surface again");
  capacity.UpdateNavigation(.02f);
  check(capacity.Navigation().Statistics().obstacles==0,"leaving the surface retires the previous carving");
  std::vector<EntityId> ids;
  for(int i=0;i<257;++i){
   SceneObject obstacle;
   obstacle.navigationObstacle=NavigationObstacleSettings{};
   obstacle.transform=placement;
   ids.push_back(capacity.CreateEntity(obstacle,nullptr,&error));
  }
  for(int i=0;i<48;++i)capacity.UpdateNavigation(.02f);
  check(ids.front()&&ids.back()&&capacity.Navigation().Statistics().obstacles==256&&
        capacity.Navigation().Errors().count(ids.back()),"overflow stays pending at the Detour pool limit");
  check(capacity.SetNavigationEnabled(ids.front(),"obstacle",false),"retire an obstacle to release its Detour slot");
  capacity.UpdateNavigation(.02f);
  check(capacity.Navigation().Statistics().obstacles==255,"disabled obstacle frees a slot after tile-cache update");
  for(int i=0;i<12;++i)capacity.UpdateNavigation(.02f);
  check(capacity.Navigation().Statistics().obstacles==256,"unchanged pending footprint retries after a slot becomes available");
 }
 for(int n:{1,10,100}){RuntimeWorld measure;measure.Build(runtimeScene,&resources,error);resources.WaitForAll();for(int i=0;i<n;++i){SceneObject e;e.navigationAgent=NavigationAgentSettings{};e.characterMotor=CharacterMotorSettings{};e.transform.position={-5.5f+(i%10)*1.1f,.9f,-5.5f+(i/10)*1.1f};auto id=measure.CreateEntity(e,nullptr,&error);measure.UpdateNavigation(.02f);measure.Navigation().SetDestination(id,{5,0,5});}std::vector<double> samples;for(int i=0;i<150;++i){auto t=std::chrono::steady_clock::now();measure.UpdateNavigation(.02f);samples.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count());}std::sort(samples.begin(),samples.end());double sum=0;for(auto t:samples)sum+=t;std::printf("NAV %d agents: mean %.4f ms p95 %.4f ms max %.4f ms\n",n,sum/samples.size(),samples[samples.size()*95/100],samples.back());measure.Destroy();}
 auto begin=std::chrono::steady_clock::now();for(int i=0;i<1000;++i)system.FindPath({-5,0,0},{5,0,0});std::printf("1000 paths: %.3f ms\n",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());std::printf("M53 focused %d checks, %d failures\n",checks,failures);
 return failures?1:0;
}
