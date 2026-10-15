#include "WorldStreaming.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "ResourceManager.h"
#include "SceneSerialization.h"
#include "SaveArchive.h"
#include "../third_party/nlohmann/json.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>

namespace {
using V=glm::dvec3;using J=nlohmann::ordered_json;
unsigned checks=0,failures=0;
void Check(bool b,const char* label){++checks;failures+=!b;std::printf("%s %s\n",b?"PASS":"FAIL",label);}
bool Pump(WorldStreaming& stream,const std::function<bool()>& done,const std::function<void()>& observe={}){for(unsigned i=0;i<3000;++i){stream.Advance(false);if(observe)observe();if(done())return true;std::this_thread::sleep_for(std::chrono::milliseconds(1));}return false;}
// Rewrite exactly one serialized retained-weather field, preserving all the
// independent origin/body/ownership fixture bytes. This is adversarial archive
// input, not a modification to the production archive reader.
std::string CorruptWeather(const std::string& bytes,bool missing){SaveArchive read(bytes);std::string baseline,id;uint32_t count=0,records=0;bool active=false;std::map<SceneObjectId,EntityId> mapping;std::vector<EntityId> members;std::set<std::string> pins;uint64_t memory=0;read(baseline,count);if(count!=1)throw std::runtime_error("test expected one region");read(id,active,mapping,members,pins,memory,records);
 for(uint32_t i=0;i<records;++i){V origin{0};SceneObjectId local=0;bool destroyed=false;std::string text,deform,animation,kinematic,wheel,powertrain,weather;glm::vec3 position{0},linear{0},angular{0};glm::quat rotation{1,0,0,0};read(origin,local,destroyed,text,position,rotation,linear,angular,deform,animation,kinematic,wheel,powertrain);const auto begin=read.position;read(weather);const auto end=read.position;
  if(!weather.empty()){if(missing)weather.clear();else{auto outer=J::parse(weather);auto inner=J::parse(outer.at("runtime").get<std::string>());inner["version"]=2;outer["runtime"]=inner.dump();weather=outer.dump();}SaveArchive encoded;encoded(weather);return bytes.substr(0,begin)+encoded.bytes+bytes.substr(end);}
  uint32_t scripts=0;read(scripts);for(uint32_t s=0;s<scripts;++s){EntityId entity=0;uint64_t slot=0;std::string json;read(entity,slot,json);}
 }throw std::runtime_error("weather fixture snapshot absent");}
}
int main(){namespace fs=std::filesystem;const fs::path root="build/m86-weather-stream-project";fs::remove_all(root);std::string error;Project project;
 Check(Project::CreateNew(root.string(),"Weather stream lifecycle",project,error),"ordinary project created");project.Settings().legacyGameplay=false;
 Scene local;auto& owner=local.CreateObject("Local weather field");owner.weather=WeatherSettings{};owner.weather->seed=86;owner.weather->profile.wind={12,0,3};owner.weather->profile.gust=4;owner.weather->profile.lightningRate=120;owner.weather->profile.tornado=.7;owner.weather->profile.tornadoSpeed=2;
 auto& floor=local.CreateObject("Ordinary streamed collision");floor.transform.position={0,-.5f,0};floor.body=SceneBodyComponent{};floor.body->halfExtents={4,.5f,4};
 auto& marker=local.CreateObject("Publication marker");marker.render=SceneRenderComponent{};marker.transform.position={3,2,0};
 Check(SaveSceneToFile(local,(root/"Scenes/region.judas").string(),error),"weather is authored through normal scene serialization");
 const V absolute{1000000,2000000,-3000000};std::ofstream(root/"Assets/local.judasworld")<<"JudasWorld 1\nbudget 2 1 1 33554432 67108864 4194304 67108864\nregion \"weather\" \"Scenes/region.judas\" 1000000 2000000 -3000000 0 0 0 1 30 30 30 0 \"snapshot\" 65536 \"\"\n";
 AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());AssetRecord manifest;Check(assets.Track((root/"Assets/local.judasworld").string(),manifest,error),"ordinary world manifest registered");project.Settings().worldManifest=manifest.id;JobSystem jobs(1);ResourceManager resources(nullptr,&assets,&jobs);Scene rootScene;rootScene.Settings().worldOrigin=absolute;
 RuntimeWorld world;world.legacyGameplay=false;auto control=std::make_shared<SceneSession>(project,"Scenes/root.judas");world.SetSceneControl(control);Check(world.Build(rootScene,&resources,error),"ordinary runtime built");auto* stream=control->Streaming(world,error);Check(stream!=nullptr,"normal session creates streaming owner");if(!stream)return 1;
 std::atomic<bool> release{false};stream->SetPreparationGate([&](const JobContext& context){while(!release&&!context.CancelRequested())std::this_thread::sleep_for(std::chrono::milliseconds(1));return !context.CancelRequested();});
 auto token=stream->Request("weather",true,error);stream->Advance(false);Check(!world.SampleWeather(absolute+V(0,2,0)).active,"worker preparation cannot publish a weather field");release=true;
 Check(Pump(*stream,[&]{auto s=stream->Status(token);return s&&s->state=="prepared";}),"preload reaches prepared without activation");Check(!world.SampleWeather(absolute+V(0,2,0)).active&&stream->Resolve("weather",1)==0,"preloaded field remains inaccessible and contributes no wind");
 Check(stream->Activate(token),"explicit activation accepted");bool staged=false,privateCorrect=true;
 Check(Pump(*stream,[&]{auto s=stream->Status(token);return s&&s->state=="active";},[&]{auto s=stream->Status(token);if(s&&s->state=="installing"&&s->installed>0){staged=true;privateCorrect=privateCorrect&&!world.SampleWeather(absolute+V(0,2,0)).active;}}),"weather publishes through normal atomic activation");
 Check(staged&&privateCorrect,"partially registered field remains unpublished across bounded install frames");auto id=stream->Resolve("weather",1);Check(id&&world.RuntimeWeather(id)&&world.SampleWeather(absolute+V(0,2,0)).active,"active region contributes actual sampled wind");auto* weather=world.RuntimeWeather(id);WeatherProfile snow;WeatherPreset("snow",snow);weather->Transition(snow,6);for(unsigned i=0;i<10;++i)world.UpdateWeather(.25f);
 const auto point=absolute+V(5,3,-2);const auto before=world.SampleWeather(point);const auto capture=weather->Capture();const auto clock=weather->State().time;std::string outer;Check(world.CaptureWeather(id,outer,error),"actual transition and pending thunder continuation captured");
 stream->Release(token);Check(Pump(*stream,[&]{return stream->Regions()[0].state=="unloaded";}),"weather and its collider unload normally");Check(!world.RuntimeWeather(id)&&!world.SampleWeather(point).active,"retired weather owner cannot remain a global field");for(unsigned i=0;i<12;++i)world.UpdateWeather(.5f);
 Check(world.RequestOrigin(absolute+V(1024,0,0),error)&&world.CommitOrigin(),"unloaded weather continuation allows ordinary origin transaction");Check(stream->ArchiveVersion()==8,"retained weather selects its explicit archive version");SaveArchive archive;stream->Persist(archive,8);Check(archive.bytes.size()<131072,"retained profile/event state remains bounded");
 token=stream->Request("weather",false,error);Check(Pump(*stream,[&]{auto s=stream->Status(token);return s&&s->state=="active";}),"same-process region revisit activates");id=stream->Resolve("weather",1);weather=world.RuntimeWeather(id);const auto after=world.SampleWeather(point);
 Check(weather&&weather->State().time==clock&&weather->Capture()==capture,"nonresident weather clock/transition/events freeze and resume exactly");
 Check(glm::length(after.wind-before.wind)<1e-9&&glm::length(after.tornadoPosition-before.tornadoPosition)<1e-9,"origin-shifted revisit retains absolute wind and tornado reference");
 // A fresh-process equivalent reconstructs the retained owner through the same
 // region installation path, not through cached WeatherState observations.
 rootScene.Settings().worldOrigin=world.Settings().worldOrigin;RuntimeWorld cold;cold.legacyGameplay=false;auto coldControl=std::make_shared<SceneSession>(project,"Scenes/root.judas");cold.SetSceneControl(coldControl);Check(cold.Build(rootScene,&resources,error),"fresh world builds at translated origin");auto* resumed=coldControl->Streaming(cold,error);bool decoded=false;
 try{SaveArchive read(archive.bytes);resumed->Persist(read,8);read.Finish();decoded=true;}catch(const std::exception& e){std::printf("ARCHIVE %s\n",e.what());}Check(decoded,"weather retained archive decodes in fresh owner");
 auto next=resumed->Request("weather",false,error);Check(Pump(*resumed,[&]{auto s=resumed->Status(next);return s&&s->state=="active";}),"cold retained weather region activates");auto* restored=cold.RuntimeWeather(resumed->Resolve("weather",1));Check(restored&&restored->Capture()==capture&&glm::length(cold.SampleWeather(point).wind-before.wind)<1e-9,"cold reconstruction restores actual field, transition and deterministic event history");
 if(!restored)return 1;
 world.UpdateWeather(.25f);cold.UpdateWeather(.25f);Check(world.RuntimeWeather(id)->Capture()==restored->Capture(),"uninterrupted and cold resumed next fixed boundary match");
 RuntimeWorld missing;missing.legacyGameplay=false;auto missingControl=std::make_shared<SceneSession>(project,"Scenes/root.judas");missing.SetSceneControl(missingControl);Check(missing.Build(rootScene,&resources,error),"negative-control world builds");auto* missingStream=missingControl->Streaming(missing,error);bool rejected=false;try{SaveArchive read(CorruptWeather(archive.bytes,true));missingStream->Persist(read,8);read.Finish();}catch(const std::exception&){rejected=true;}Check(rejected&&!missing.SampleWeather(point).active,"missing retained weather record is rejected instead of resetting weather silently");
 RuntimeWorld broken;broken.legacyGameplay=false;auto brokenControl=std::make_shared<SceneSession>(project,"Scenes/root.judas");broken.SetSceneControl(brokenControl);Check(broken.Build(rootScene,&resources,error),"malformed-continuation world builds");auto* brokenStream=brokenControl->Streaming(broken,error);bool malformedDecoded=false;try{SaveArchive read(CorruptWeather(archive.bytes,false));brokenStream->Persist(read,8);read.Finish();malformedDecoded=true;}catch(const std::exception& e){std::printf("MALFORMED PRECHECK %s\n",e.what());}Check(malformedDecoded,"negative fixture preserves archive structure and reaches actual weather restore");
 auto badToken=brokenStream->Request("weather",false,error);bool failed=Pump(*brokenStream,[&]{auto s=brokenStream->Status(badToken);return s&&!s->error.empty()&&(s->state=="unloaded"||s->state=="failed");});Check(failed&&!broken.SampleWeather(point).active&&broken.Physics().AliveBodies().empty(),"invalid weather continuation rolls back partial region/collision publication");
 std::printf("Weather streaming %u checks %u failures\n",checks,failures);return failures?1:0;
}
