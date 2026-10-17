#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "WorldPersistence.h"
#include "WorldStreaming.h"
#include "SceneSerialization.h"
#include "SaveArchive.h"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include <iostream>
namespace fs=std::filesystem;
unsigned checks=0;
void Check(bool ok,const std::string& text){++checks;if(!ok)throw std::runtime_error(text);std::cout<<"PASS "<<text<<'\n';}
Scene Source(glm::quat rotation={1,0,0,0}){Scene s;s.SetNextId(20);SceneObject rock;rock.id=1;rock.name="editable";rock.render=SceneRenderComponent{};rock.body=SceneBodyComponent{};rock.body->halfExtents={5,4,5};rock.transform.rotation=rotation;s.Objects().push_back(rock);return s;}
void Wait(RuntimeWorld& w,uint64_t id){for(int i=0;i<10000;++i){w.AdvanceRuntimeTerrain();auto* status=w.LiveTerrain().Status(id);if(status&&!status->busy){Check(status->ready&&status->error.empty(),"atomic terrain publication: "+status->error);return;}std::this_thread::sleep_for(std::chrono::milliseconds(1));}throw std::runtime_error("terrain timeout");}
int main(int argc,char** argv){try{
 auto root=fs::absolute(argc>1?argv[1]:"/tmp/m97-unit");fs::create_directories(root);AssetDatabase assets;JobSystem jobs(2);ResourceManager resources(nullptr,&assets,&jobs);std::string error;
 auto scene=Source();RuntimeWorld w;Check(w.Build(scene,&resources,error,nullptr,nullptr,true),"build ordinary static world: "+error);
 auto source=CreateVolumeTerrain({21,17,21},{10,8,10});auto old=w.RuntimeBody(1);
 Check(w.LiveTerrain().Create(1,source,true,0,error),"create optional live volume");
 std::string captured;Check(!w.CaptureRuntimeTerrain(1,captured,error),"pending edit cannot be archived");
 Check(!w.LiveTerrain().Create(1,source,false,0,error),"duplicate create rejected");Wait(w,1);
 Check(w.RuntimeBody(1).id==old.id,"static body handle and classification preserved");
 auto hit=w.Physics().Raycast({0,0,6},{0,0,-1},12);Check(hit.hit,"solid blocks tunnel before edit");
 Check(!w.LiveTerrain().Brush(1,{0,0,0},{VolumeBrushKind::Remove,-1,1},error),"invalid brush rejected");
 // A single spherical excavation crosses the bounded solid at its centre.
 Check(w.LiveTerrain().Brush(1,{0,0,4.5f},{VolumeBrushKind::Remove,2.5f,1},error),"excavate entrance");
 Check(!w.LiveTerrain().Brush(1,{0,0,0},{},error),"bounded single-flight backpressure");
 Check(w.Physics().Raycast({0,0,6},{0,0,-1},12).hit,"pending result retains last-good collider");Wait(w,1);
 for(float z:{2.25f,0.f,-2.25f,-4.5f}){Check(w.LiveTerrain().Brush(1,{0,0,z},{VolumeBrushKind::Remove,2.5f,1},error),"extend tunnel");Wait(w,1);}
 Check(!w.Physics().Raycast({0,0,6},{0,0,-1},12).hit,"committed excavation opens actual collider");
 auto nav=w.Navigation().Sample({0,-3,0},2);Check(bool(nav),"excavated cavity floor receives navigation");
 w.Navigation().Update(w,0);Check(bool(w.Navigation().Sample({0,-3,0},2)),"normal navigation update retains live surface");
 Check(w.CaptureRuntimeTerrain(1,captured,error)&&!captured.empty(),"capture committed scalar field");
 std::ofstream(root/"terrain.bin",std::ios::binary)<<captured;
 auto status=*w.LiveTerrain().Status(1);std::cout<<"METRIC samples="<<status.samples<<" triangles="<<status.triangles<<" estimated_bytes="<<status.bytes<<" prepare_ms="<<status.prepareMs<<" publish_ms="<<status.publishMs<<'\n';
 RuntimeWorld fresh;Check(fresh.Build(scene,&resources,error,nullptr,nullptr,true),"fresh world");Check(fresh.LiveTerrain().Restore(1,captured,error),"restore exact committed field: "+error);Check(!fresh.Physics().Raycast({0,0,6},{0,0,-1},12).hit,"restored collision remains excavated");
 std::string recaptured;Check(fresh.CaptureRuntimeTerrain(1,recaptured,error)&&recaptured==captured,"scalar persistence is byte exact");
 Check(w.LiveTerrain().Brush(1,{0,0,0},{VolumeBrushKind::Add,1.5f,1},error),"construction adds physical material");Wait(w,1);Check(w.Physics().Raycast({0,0,6},{0,0,-1},12).hit,"added material blocks real ray");
 auto probe=w.Physics().CreateDynamicSphere({0,2.5f,0},.3f,1,.6f,0);bool touchedAdded=false;for(unsigned step=0;step<120;++step){w.Physics().ApplyLinearAcceleration(probe,{0,-9.81f,0},1.f/60);w.Physics().Step(1.f/60);for(const auto& contact:w.Physics().LastStepContacts())if(contact.point.y>1.0f)touchedAdded=true;}auto supported=w.Physics().GetTransform(probe).position;std::cout<<"FALLING_PROBE position="<<supported.x<<","<<supported.y<<","<<supported.z<<" velocity="<<w.Physics().GetLinearVelocity(probe).y<<std::endl;Check(touchedAdded,"newly added geometry generates actual falling-body contacts above the cave floor");w.Physics().DestroyBody(probe);
 Check(w.LiveTerrain().Brush(1,{0,0,0},{VolumeBrushKind::Smooth,2,0.5f},error),"smooth uses shared M84 field operation");Wait(w,1);
 std::map<std::string,SaveChunk> chunks;Check(WorldPersistence::Capture(w,chunks,error),"full physical save captures terrain: "+error);RuntimeWorld restored;Check(restored.Build(scene,&resources,error,nullptr,nullptr,true),"cold save destination");Check(WorldPersistence::Restore(restored,chunks,error),"full physical save restores terrain: "+error);Check(restored.HasRuntimeTerrain(1)&&restored.Physics().Raycast({0,0,6},{0,0,-1},12).hit,"full restore reconstructs added collision");
 auto q=glm::angleAxis(.8f,glm::normalize(glm::vec3(1,2,3)));RuntimeWorld rotated;Check(rotated.Build(Source(q),&resources,error,nullptr,nullptr,true),"rotated world");Check(rotated.LiveTerrain().Restore(1,captured,error),"rotated volume restoration");Check(!rotated.Physics().Raycast(q*glm::vec3(0,0,6),q*glm::vec3(0,0,-1),12).hit,"arbitrary rigid orientation preserves excavation");
 // Preparation failure must not publish empty terrain or erase previous geometry.
 auto prior=w.LiveTerrain().Status(1)->revision;Check(w.LiveTerrain().Brush(1,{0,0,0},{VolumeBrushKind::Remove,64,1},error),"queue deliberately empty product");for(int i=0;i<10000&&w.LiveTerrain().Status(1)->busy;++i){w.AdvanceRuntimeTerrain();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 Check(!w.LiveTerrain().Status(1)->error.empty()&&w.LiveTerrain().Status(1)->revision==prior,"failed empty product retains revision and collision");Check(w.Physics().Raycast({0,0,6},{0,0,-1},12).hit,"failure rollback preserves physical solid");
 Check(w.DestroyEntity(1,&error),"remove terrain owner");Check(!w.HasRuntimeTerrain(1),"destroy cleans terrain runtime state");
 // Long private preparation must not accrue hidden free fall. Collider-only
 // disabling still has its original gameplay semantics.
 PhysicsWorld staging;staging.Init();auto hidden=staging.CreateDynamicBox({0,3,0},{.5f,.5f,.5f},2,.6f,0);staging.SetLinearVelocity(hidden,{1,2,3});staging.SetAngularVelocity(hidden,{.2f,.3f,.4f});auto heldPose=staging.GetTransform(hidden);Check(!staging.SetBodySimulationSuspended(hidden,true),"staging freeze requires collision to be disabled first");staging.SetBodyEnabled(hidden,false);Check(staging.SetBodySimulationSuspended(hidden,true),"explicit private simulation suspension");for(unsigned step=0;step<600;++step){staging.ApplyLinearAcceleration(hidden,{0,-9.81f,0},1.f/60);staging.ApplyForce(hidden,{1,2,3});staging.Step(1.f/60);}Check(staging.GetTransform(hidden).position==heldPose.position&&staging.GetTransform(hidden).rotation==heldPose.rotation&&staging.GetLinearVelocity(hidden)==glm::vec3(1,2,3)&&staging.GetAngularVelocity(hidden)==glm::vec3(.2f,.3f,.4f),"ten seconds of private staging preserves exact pose and momentum");staging.SetBodySimulationSuspended(hidden,false);staging.SetBodyEnabled(hidden,true);staging.Step(1.f/60);Check(staging.GetTransform(hidden).position!=heldPose.position,"publication resumes real motion without a settling delay");auto ordinary=staging.CreateDynamicBox({10,3,0},{.5f,.5f,.5f},2,.6f,0);staging.SetBodyEnabled(ordinary,false);staging.ApplyLinearAcceleration(ordinary,{0,-9.81f,0},1.f/60);staging.Step(1.f/60);Check(staging.GetTransform(ordinary).position.y<3,"ordinary collider-disabled actors still move normally");staging.SetBodyEnabled(hidden,false);staging.SetBodySimulationSuspended(hidden,true);staging.DestroyBody(hidden);auto reused=staging.CreateDynamicBox({20,3,0},{.5f,.5f,.5f},2,.6f,0);staging.ApplyLinearAcceleration(reused,{0,-9.81f,0},1.f/60);staging.Step(1.f/60);Check(reused.id!=hidden.id&&staging.GetTransform(reused).position.y<3,"generation reuse does not inherit the private suspension flag");
 // Independent moving-owner joint fixture; native frames are physical state,
 // not reconstructed from a numerically irreversible scene-relative rotation.
 Scene assembly;auto& beam=assembly.CreateObject("rotating joint owner");beam.body=SceneBodyComponent{};beam.body->motion=SceneBodyMotion::Dynamic;beam.transform.rotation=glm::angleAxis(.37f,glm::normalize(glm::vec3(1,2,3)));beam.joint=SceneJointComponent{};beam.joint->bodyA=beam.id;beam.joint->settings.frameB=glm::angleAxis(.19f,glm::normalize(glm::vec3(3,1,2)));
 RuntimeWorld physical;Check(physical.Build(assembly,&resources,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(physical,error),"independent moving-owner joint fixture");for(unsigned step=0;step<20;++step){physical.Physics().ApplyLinearAcceleration(physical.RuntimeBody(1),{0,-9.81f,0},1.f/60);physical.Physics().Step(1.f/60);}
 std::map<std::string,SaveChunk> jointChunks;Check(WorldPersistence::Capture(physical,jointChunks,error)&&jointChunks.at("articulation").version==5,"native ordinary joint physical settings have an explicit new save version");Scene jointScene;Check(WorldPersistence::SceneFrom(jointChunks,jointScene,error),"physical joint snapshot scene");RuntimeWorld resumed;Check(resumed.Build(jointScene,&resources,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(resumed,error)&&WorldPersistence::Restore(resumed,jointChunks,error),"moving-owner joint cold restoration: "+error);JointState savedJoint,readJoint;physical.Physics().GetJoint(physical.RuntimeJoint(1),savedJoint);resumed.Physics().GetJoint(resumed.RuntimeJoint(1),readJoint);Check(JointSettingsEqual(savedJoint.settings,readJoint.settings),"native joint settings restore bit-exactly without weakening continuation validation");physical.Physics().Step(1.f/60);resumed.Physics().Step(1.f/60);Check(glm::distance(physical.Physics().GetTransform(physical.RuntimeBody(1)).position,resumed.Physics().GetTransform(resumed.RuntimeBody(1)).position)<1e-5f,"next physical endpoint agrees after exact settings restoration");
 std::cout<<"SUMMARY "<<checks<<" checks 0 failures\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
