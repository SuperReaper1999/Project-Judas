#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "WorldStreaming.h"
#include "WorldPersistence.h"
#include "SceneSerialization.h"
#include "SaveArchive.h"
#include <filesystem>
#include <thread>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
unsigned checks=0;void Check(bool ok,const std::string& text){++checks;if(!ok)throw std::runtime_error(text);std::cout<<"PASS "<<text<<'\n';}
bool Pump(WorldStreaming& stream,const std::function<bool()>& done){for(int i=0;i<10000;++i){stream.Advance(false);if(done())return true;std::this_thread::sleep_for(std::chrono::milliseconds(1));}return false;}
int main(int argc,char** argv){try{auto root=fs::absolute(argc>1?argv[1]:"/tmp/m97-stream");fs::create_directories(root);std::string error;Project p;Check(Project::CreateNew(root.string(),"retention",p,error),"ordinary project");
 Scene region;auto& terrain=region.CreateObject("editable volume");terrain.body=SceneBodyComponent{};terrain.body->halfExtents={5,4,5};terrain.render=SceneRenderComponent{};
 auto& body=region.CreateObject("load bearing body");body.body=SceneBodyComponent{};body.body->motion=SceneBodyMotion::Dynamic;body.transform.position={6,2,0};body.render=SceneRenderComponent{};
 auto& joint=region.CreateObject("physical fixed support");joint.joint=SceneJointComponent{};joint.joint->bodyA=2;joint.joint->settings.anchorB={6,2,0};
 auto& disposable=region.CreateObject("destroyable stock");disposable.body=SceneBodyComponent{};disposable.transform.position={-7,0,0};
 Check(SaveSceneToFile(region,(root/"Scenes/region.judas").string(),error),"ordinary physical assembly authoring");AssetDatabase assets;assets.Scan(p.RootDir(),p.AssetsDir());JobSystem jobs(2);ResourceManager resources(nullptr,&assets,&jobs);
 Scene bootstrap;bootstrap.Settings().worldOrigin={6371000,0,0};RuntimeWorld world;world.legacyGameplay=false;Check(world.Build(bootstrap,&resources,error),"large absolute origin root");WorldManifest manifest;manifest.retainedBytes=8*1024*1024;WorldRegion definition;definition.id="quarry";definition.scene="Scenes/region.judas";definition.origin=bootstrap.Settings().worldOrigin;manifest.regions.emplace(definition.id,definition);WorldStreaming stream(world,resources,p,manifest);
 auto token=stream.Request("quarry",false,error);Check(Pump(stream,[&]{return stream.Status(token)->state=="active";}),"activate real region");auto rock=stream.Resolve("quarry",1);auto old=world.RuntimeBody(rock);
 auto volume=CreateVolumeTerrain({17,13,17},{10,8,10});Check(world.LiveTerrain().Create(rock,volume,false,0,error),"optional terrain on streamed static owner");for(int i=0;i<10000&&world.RuntimeTerrainBusy(rock);++i){world.AdvanceRuntimeTerrain();std::this_thread::sleep_for(std::chrono::milliseconds(1));}Check(world.HasRuntimeTerrain(rock),"streamed terrain ready");Check(stream.ArchiveVersion()==9,"first active terrain save chooses the new archive before any suspension");
 Check(world.LiveTerrain().Brush(rock,{0,0,0},{VolumeBrushKind::Remove,2,1},error),"persistent enclosed chamber edit");for(int i=0;i<10000&&world.RuntimeTerrainBusy(rock);++i){world.AdvanceRuntimeTerrain();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 std::string before;Check(world.CaptureRuntimeTerrain(rock,before,error),"capture edited field before suspension");
 // A runtime-created primitive becomes part of the same durable region.
 SceneObject block;block.name="runtime construction";block.body=SceneBodyComponent{};block.body->motion=SceneBodyMotion::Dynamic;block.transform.position={7,3,0};block.render=SceneRenderComponent{};auto spawned=world.CreateEntity(block,nullptr,&error);Check(spawned&&stream.Adopt(spawned,"quarry",error),"runtime construction adopted without permanent pin: "+error);
 auto state=EntityPhysicalState{};world.GetEntityState(spawned,state);state.linearVelocity={1,2,3};state.angularVelocity={.1f,.2f,.3f};world.SetEntityState(spawned,state);
 Check(world.DestroyEntity(stream.Resolve("quarry",4),&error),"destroy physical load-bearing member");
 Check(stream.Release(token)&&Pump(stream,[&]{return stream.Regions()[0].state=="unloaded";}),"edited region with adopted construction really unloads");
 Check(!world.Physics().IsBodyEnabled(old)&&world.Physics().AliveBodyCount()==0,"no suspended region body remains simulated");Check(stream.ArchiveVersion()==9,"retained volume uses versioned optional archive");
 SaveArchive archive;stream.Persist(archive,stream.ArchiveVersion());std::ofstream(root/"region.bin",std::ios::binary)<<archive.bytes;
 Check(world.RequestOrigin({6371512,0,0},error)&&world.CommitOrigin(),"live chart shift while edited region is suspended: "+error);
 token=stream.Request("quarry",false,error);bool returnedReady=Pump(stream,[&]{return stream.Status(token)->state=="active";});if(!returnedReady)for(auto& row:stream.Regions()){std::cout<<"REGION "<<row.id<<" "<<row.state<<" "<<row.error<<std::endl;for(auto& pin:row.pins)std::cout<<"PIN "<<pin<<std::endl;}Check(returnedReady,"return reconstructs edited terrain and construction");rock=stream.Resolve("quarry",1);std::string after;Check(world.CaptureRuntimeTerrain(rock,after,error)&&before==after,"streaming scalar bytes exact across origin transition");Check(!world.Physics().IsBodyEnabled(old),"old collider handle never aliases replacement");Check(!stream.Resolve("quarry",4),"destroyed member stays destroyed");
 auto returned=stream.Resolve("quarry",5);Check(returned&&world.GetEntityState(returned,state)&&state.linearVelocity==glm::vec3(1,2,3)&&state.angularVelocity==glm::vec3(.1f,.2f,.3f),"constructed body restores physical momentum");
 // Negative: a root-owned joint requires the region to remain resident.
 auto rootJoint=world.CreateEntity(SceneObject{},nullptr,&error);SceneJointComponent external;external.bodyA=returned;external.settings.anchorB=state.position;Check(world.SetRuntimeJoint(rootJoint,external,false,error),"create external physical dependency");
 std::cout<<"PRE DEP owners "<<stream.Owner(returned)<<" "<<stream.Owner(rootJoint)<<" ids "<<returned<<" "<<rootJoint<<" definitions "<<world.DefinitionIds().size()<<" version "<<world.EntityVersion()<<std::endl;stream.Release(token);for(int i=0;i<10;++i)stream.Advance(false);for(const auto& row:stream.Regions()){std::cout<<"DEPENDENCY "<<row.state<<" "<<row.error<<std::endl;for(auto& pin:row.pins)std::cout<<"PIN "<<pin<<std::endl;}std::cout<<"OWNERS body="<<stream.Owner(returned)<<" rootjoint="<<stream.Owner(rootJoint)<<std::endl;Check(stream.Regions()[0].state=="active"&&!stream.Regions()[0].pins.empty(),"external joint pins instead of unsafe suspension");world.DestroyEntity(rootJoint,&error);Check(Pump(stream,[&]{return stream.Regions()[0].state=="unloaded";}),"removing dependency permits suspension");
 for(unsigned cycle=0;cycle<3;++cycle){token=stream.Request("quarry",false,error);Check(Pump(stream,[&]{return stream.Status(token)->state=="active";}),"repeated return "+std::to_string(cycle));rock=stream.Resolve("quarry",1);std::string repeated;Check(world.CaptureRuntimeTerrain(rock,repeated,error)&&repeated==before&&world.LiveTerrain().Owners().size()==1,"repeated scalar identity and bounded live ownership");JointState jointState;Check(world.Physics().GetJoint(world.RuntimeJoint(stream.Resolve("quarry",3)),jointState),"load-bearing constraint genuinely reconstructed");stream.Release(token);Check(Pump(stream,[&]{return stream.Regions()[0].state=="unloaded";})&&world.Physics().AliveBodyCount()==0&&world.LiveTerrain().Owners().empty(),"repeated suspension retires physical and terrain resources");}
 std::cout<<"SUMMARY "<<checks<<" checks 0 failures\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
