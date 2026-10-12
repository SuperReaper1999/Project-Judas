#include "RuntimeWorld.h"
#include "WorldPersistence.h"
#include "ResourceManager.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "SaveArchive.h"
#include "../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs=std::filesystem;
using Json=nlohmann::json;
using Chunks=std::map<std::string,SaveChunk>;
unsigned checks=0,failures=0;
Json results=Json::array();
struct CheckedFailure:std::runtime_error {using std::runtime_error::runtime_error;};
void Check(bool ok,const std::string& label,Json detail=Json::object()){
 ++checks;failures+=!ok;results.push_back({{"name",label},{"passed",ok},{"details",std::move(detail)}});
 std::printf("%s %s\n",ok?"PASS":"FAIL",label.c_str());
}
void Require(bool ok,const std::string& label,const std::string& error={}){
 Check(ok,label,{{"error",error}});if(!ok)throw CheckedFailure(label+": "+error);
}
void Case(const std::string& label,const std::function<void()>& run){
 try{run();}catch(const CheckedFailure& e){std::fprintf(stderr,"%s stopped: %s\n",label.c_str(),e.what());}
 catch(const std::exception& e){Check(false,label+" unexpected exception",{{"error",e.what()}});}
}
void Write(const fs::path& path,const std::string& bytes){
 std::ofstream file(path,std::ios::binary);if(!file)throw std::runtime_error("cannot write "+path.string());
 file.write(bytes.data(),std::streamsize(bytes.size()));if(!file)throw std::runtime_error("short write "+path.string());
}
void Preserve(const fs::path& out,const Chunks& chunks){
 fs::create_directories(out);Json manifest=Json::object();
 for(const auto& [name,chunk]:chunks){Write(out/(name+".participant"),chunk.data);manifest[name]={{"version",chunk.version},{"bytes",chunk.data.size()},{"sha256",SceneFingerprintSha256(chunk.data)}};}
 Write(out/"PARTICIPANTS.json",manifest.dump(2)+"\n");
}
std::set<EntityId> SleepIds(const SaveChunk& chunk){
 if(chunk.version!=1)throw std::runtime_error("fixture expects existing sleep format 1");
 SaveArchive a(chunk.data);uint32_t n=0;a(n);a.Require(n<=8192,"fixture sleep count bound");
 std::set<EntityId> ids;
 for(uint32_t i=0;i<n;++i){EntityId id=0;bool sleeping=false,deferred=false;float quiet=0;glm::vec3 acceleration{0};
  // This is the existing PhysicsWorld::PersistSleep format, not fabricated
  // private sleep flags. Inspecting bytes does not alter the source world.
  a(id,sleeping,quiet,acceleration,deferred);a.Require(ids.insert(id).second,"duplicate captured sleep identity");}
 a.Finish();return ids;
}
bool Restore(RuntimeWorld& destination,const Chunks& chunks,ResourceManager* resources,std::string& error){
 error.clear();Scene saved;
 return WorldPersistence::SceneFrom(chunks,saved,error)&&destination.Build(saved,resources,error,nullptr,nullptr,true)&&
        WorldPersistence::Prepare(destination,error)&&WorldPersistence::Restore(destination,chunks,error);
}
Scene SimpleScene(EntityId& kept){
 Scene scene;kept=scene.CreateObject("durable simple body").id;auto* body=scene.Find(kept);
 body->body=SceneBodyComponent{};body->body->motion=SceneBodyMotion::Dynamic;body->body->managed=true;
 body->transform.position={0,3,0};return scene;
}
void Malformed(RuntimeWorld& source,EntityId kept,const Chunks& good,const fs::path& out){
 auto reject=[&](const std::string& label,const Chunks& bad,const std::string& diagnostic){
  Preserve(out/label,bad);RuntimeWorld privateWorld;std::string error;Scene saved;
  Require(WorldPersistence::SceneFrom(bad,saved,error),label+" valid entity participant",error);
  Require(privateWorld.Build(saved,nullptr,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(privateWorld,error),label+" normal private construction",error);
  const auto before=source.Physics().GetTransform(source.RuntimeBody(kept));const auto velocity=source.Physics().GetLinearVelocity(source.RuntimeBody(kept));
  const bool loaded=WorldPersistence::Restore(privateWorld,bad,error);
  Check(!loaded&&error.find(diagnostic)!=std::string::npos,label+" strict rejection remains",{{"loaded",loaded},{"error",error}});
  Check(source.Physics().GetTransform(source.RuntimeBody(kept)).position==before.position&&source.Physics().GetLinearVelocity(source.RuntimeBody(kept))==velocity,label+" failed private restore leaves source unchanged");
 };
 // Encode normal public PersistSleep fields with an invalid durable identity;
 // restore must reject it instead of silently skipping an unavailable owner.
 auto orphan=good;SaveArchive orphanSleep;uint32_t one=1;orphanSleep(one);auto absent=source.NextRuntimeEntityId()+64;
 orphanSleep(absent);source.Physics().PersistSleep(orphanSleep,source.RuntimeBody(kept));orphan["sleep"]={1,std::move(orphanSleep.bytes)};
 reject("orphan",orphan,"saved sleep body unavailable/duplicate");
 auto duplicate=good;SaveArchive duplicateSleep;uint32_t two=2;duplicateSleep(two);
 for(unsigned i=0;i<two;++i){auto id=kept;duplicateSleep(id);source.Physics().PersistSleep(duplicateSleep,source.RuntimeBody(kept));}
 duplicate["sleep"]={1,std::move(duplicateSleep.bytes)};reject("duplicate",duplicate,"saved sleep body unavailable/duplicate");
 auto truncated=good;truncated.at("sleep").data.pop_back();reject("truncated",truncated,"truncated save field");
}
void Simple(const fs::path& out,bool exclude,bool reinclude,bool malformed){
 const std::string label=reinclude?"reincluded":exclude?"excluded":"retained";
 EntityId kept=0;auto scene=SimpleScene(kept);RuntimeWorld source,destination;std::string error;
 Require(source.Build(scene,nullptr,error),label+" normal source construction",error);
 SceneObject actor;actor.name="ordinary runtime actor";actor.body=SceneBodyComponent{};
 actor.body->motion=SceneBodyMotion::Dynamic;actor.body->managed=true;actor.transform.position={5,3,0};
 const auto runtime=source.CreateEntity(actor,nullptr,&error);
 Require(runtime&&source.Physics().IsDynamicBody(source.RuntimeBody(runtime)),label+" ordinary runtime dynamic actor",error);
 source.Physics().SetLinearVelocity(source.RuntimeBody(kept),{1,2,3});
 // Normal fixed physics updates, with no applied gravity or contacts, make
 // the zero-motion actor sleep. No private state or forced sleep is authored.
 for(unsigned i=0;i<60;++i)source.Physics().Step(1.f/60);
 Require(source.Physics().IsSleeping(source.RuntimeBody(runtime)),label+" actor reaches real ordinary sleep");
 if(exclude||reinclude)Require(source.SetTransientEntity(runtime,true,error),label+" public simple-actor exclusion accepted",error);
 if(reinclude)Require(source.SetTransientEntity(runtime,false,error),label+" public reinclusion accepted",error);
 const bool expected=!exclude||reinclude;
 Check(source.IsTransientEntity(runtime)==!expected,label+" source ownership matches requested policy");
 Chunks chunks;Require(WorldPersistence::Capture(source,chunks,error),label+" ordinary full capture",error);Preserve(out,chunks);
 Scene saved;Require(WorldPersistence::SceneFrom(chunks,saved,error),label+" scene participant decode",error);
 Check(saved.Find(kept)&&bool(saved.Find(runtime))==expected,label+" scene contains exactly the retained owners");
 const auto sleeping=SleepIds(chunks.at("sleep"));
 Check(sleeping.count(kept)&&bool(sleeping.count(runtime))==expected&&sleeping.size()==(expected?2u:1u),label+" sleep records contain exactly the retained dynamic owners");
 const bool loaded=Restore(destination,chunks,nullptr,error);
 Require(loaded,label+" full private world restore",error);
 Check(destination.Physics().GetLinearVelocity(destination.RuntimeBody(kept))==source.Physics().GetLinearVelocity(source.RuntimeBody(kept)),label+" retained real velocity survives");
 Check(bool(destination.RuntimeDefinition(runtime))==expected,label+" restored actor presence matches ownership");
 if(expected){Check(destination.Physics().IsSleeping(destination.RuntimeBody(runtime)),label+" actual sleeping actor restores asleep");
  Check(!destination.IsTransientEntity(runtime),label+" ordinary actor restores retained ownership");}
 if(malformed)Malformed(source,kept,chunks,out/"malformed");
}
void Articulated(const fs::path& out,const fs::path& repository){
 const auto project=repository/"projects/ragdoll_demo";const AssetId asset="46464646464646464646464646464601";
 AssetDatabase assets;assets.Scan(project.string(),(project/"Assets").string());
 Require(assets.Find(asset)!=nullptr,"articulation original project asset registered");
 ResourceManager resources(nullptr,&assets);resources.SetHeadlessResidency(true);
 resources.RequestMesh(asset);resources.WaitForAll();
 Require(resources.StateOf(asset)==ResourceState::Ready&&resources.TryGetSkeletal(asset)!=nullptr,"articulation real skeleton decode without GPU substitute",resources.ErrorOf(asset));
 Scene authored;std::string error;
 Require(LoadSceneFromFile((project/"Scenes/main.judas").string(),authored,error),"articulation ordinary original scene loads",error);
 const auto* original=authored.Find(10);Require(original&&original->ragdoll&&original->animation,"articulation original three-bone definition exists");
 Scene scene;scene.Settings()=authored.Settings();auto rig=*original;rig.scripts.clear();
 Require(scene.InsertObject(rig),"articulation ordinary owner inserted with normal scene identity");
 RuntimeWorld source,destination;source.legacyGameplay=false;destination.legacyGameplay=false;
 Require(source.Build(scene,&resources,error),"articulation normal source world",error);source.UpdateAnimations(0);
 Require(source.EnterRagdoll(rig.id,error),"articulation real public physical activation",error);
 Check(!source.SetTransientEntity(rig.id,true,error)&&error.find("required component cannot be transient")!=std::string::npos&&!source.IsTransientEntity(rig.id),"articulation required owner cannot be publicly excluded",{{"error",error}});error.clear();
 const std::vector<std::string> keys={"Root","Elbow","Tip"};std::map<std::string,EntityId> mapped;
 for(const auto& key:keys){const auto id=source.RagdollBody(rig.id,key);mapped[key]=id;
  Require(id&&source.IsTransientEntity(id)&&source.Physics().IsDynamicBody(source.RuntimeBody(id))&&source.ArticulatedIdentity(id).first==rig.id,"articulation "+key+" is a genuine mapped transient dynamic body");}
 Chunks chunks;Require(WorldPersistence::Capture(source,chunks,error),"articulation full capture",error);Preserve(out,chunks);
 Scene saved;Require(WorldPersistence::SceneFrom(chunks,saved,error),"articulation scene participant decode",error);
 const auto sleeping=SleepIds(chunks.at("sleep"));
 for(const auto& [key,id]:mapped)Check(saved.Find(id)&&sleeping.count(id),"articulation "+key+" engine-transient body remains in scene AND sleep");
 Check(sleeping.size()==mapped.size()&&chunks.at("articulation").version==4&&chunks.at("contacts").version==2,"articulation retains exact existing continuation ownership/schema");
 Require(Restore(destination,chunks,&resources,error),"articulation complete strict continuation restore",error);
 for(const auto& [key,id]:mapped){auto from=source.RuntimeBody(id),to=destination.RuntimeBody(id);
  Check(destination.RagdollBody(rig.id,key)==id&&destination.IsTransientEntity(id)&&destination.ArticulatedIdentity(id).first==rig.id,"articulation "+key+" durable owner mapping restores");
  Check(source.Physics().GetTransform(from).position==destination.Physics().GetTransform(to).position&&source.Physics().GetLinearVelocity(from)==destination.Physics().GetLinearVelocity(to)&&source.Physics().IsSleeping(from)==destination.Physics().IsSleeping(to),"articulation "+key+" physical and sleep state survive");}
 Check(source.Physics().AliveBodyCount()==destination.Physics().AliveBodyCount(),"articulation restore creates no extra body ownership");
}
}
int main(int argc,char** argv){
 if(argc<2||argc>3){std::fprintf(stderr,"usage: judas_save_exclusion_tests FRESH-output [repository-root]\n");return 2;}
 const auto out=fs::absolute(fs::u8path(argv[1])).lexically_normal();const auto repository=argc==3?fs::absolute(fs::u8path(argv[2])):fs::current_path();
 try{if(fs::exists(out))throw std::runtime_error("output already exists; choose a fresh owned directory");fs::create_directories(out);}
 catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 2;}
 Case("retained",[&]{Simple(out/"retained",false,false,true);});
 Case("excluded",[&]{Simple(out/"excluded",true,false,false);});
 Case("reincluded",[&]{Simple(out/"reincluded",true,true,false);});
 Case("articulated",[&]{Articulated(out/"articulated",repository);});
 Write(out/"RESULT.json",Json{{"contract","LIF-002 saved sleep ownership follows scene/contact ownership; required articulation bodies remain retained and malformed records reject"},{"checks",results},{"failures",failures}}.dump(2)+"\n");
 std::printf("SUMMARY %u checks %u failures\n",checks,failures);return failures?1:0;
}
