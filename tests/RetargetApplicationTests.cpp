// M74 ordinary-runtime proof. No authoring retarget library links into this app.
#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "WorldStreaming.h"
#include "WorldPersistence.h"
#include "SaveService.h"
#include "TestInput.h"
#include "PerformanceProfiler.h"
#include "ScreenshotWriter.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <thread>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <map>
namespace fs=std::filesystem;using Json=nlohmann::json;
namespace {
int checks=0,failures=0;
void Check(bool b,const char* s){++checks;failures+=!b;printf("%s %s\n",b?"PASS":"FAIL",s);fflush(stdout);}
void Quit(){SDL_Event e{};e.type=SDL_QUIT;SDL_PushEvent(&e);}
std::vector<std::string> LeftArmMask(const Skeleton& skeleton){
 std::vector<std::string> mask;
 // This deliberately incompatible lab target has opaque names, distinct from
 // the source's semantic labels. These are its authored left-arm fixture nodes.
 for(const char* name:{"Q07","Q08","Q09"}){
  auto found=std::find(skeleton.names.begin(),skeleton.names.end(),name);
  if(found==skeleton.names.end())return {};
  mask.push_back(SkeletonJointKey(skeleton,int(found-skeleton.names.begin())));
 }
 return mask;
}
Json Distribution(std::vector<double> values){
 if(values.empty())return Json{{"frames",0}};
 double sum=0;for(double value:values)sum+=value;
 std::sort(values.begin(),values.end());
 return Json{{"frames",values.size()},{"mean_ms",sum/values.size()},{"median_ms",values[values.size()/2]},
             {"p95_ms",values[std::min(values.size()-1,size_t(std::ceil(values.size()*.95)-1))]},{"max_ms",values.back()}};
}
// Named M56 inclusive scopes overlap. Summarize each independently rather than
// pretending their sum is the outer frame cost. History is bounded to 120 frames.
Json ProfileMetrics(bool includeStartup=false){
 auto frames=PerformanceProfiler::Get().History();if(includeStartup){auto startup=PerformanceProfiler::Get().Startup();if(startup.id)frames.insert(frames.begin(),std::move(startup));}std::vector<double> elapsed,interval;
 std::map<std::string,std::vector<double>> values;
 for(const char* name:{"Animation instances","Animation clip sampling","Pose clip mixing","Final pose resolution","Pose skin matrices",
                      "Resource pump","Resource completion and budget","Resource installation","Resource worker job","Mesh geometry upload","Texture driver texel upload"})values[name]={};
 for(const auto& frame:frames){
  elapsed.push_back(double(frame.end-frame.start)/1e6);interval.push_back(double(frame.interval)/1e6);
  std::map<std::string,double> totals;
  for(const auto& scope:frame.scopes){std::string name(scope.name.c_str());if(values.count(name))totals[name]+=double(scope.end-scope.start)/1e6;}
  for(auto& [name,samples]:values)samples.push_back(totals[name]);
 }
 Json scopes=Json::object();for(auto& [name,samples]:values)scopes[name]=Distribution(std::move(samples));
 return Json{{"frame_elapsed",Distribution(std::move(elapsed))},{"frame_interval",Distribution(std::move(interval))},{"inclusive_scopes",std::move(scopes)},{"startup_included",includeStartup},
             {"scope_note","Inclusive named scopes overlap; no sum across scopes. Uncapped hidden Release application, fixed simulation dt 1/60, no test sleep during steady measurement."}};
}
}
int main(int argc,char** argv){
 if(argc<4){puts("usage: project.judasproj probe|write|read|performance|performance-native|performance-baked output-dir");return 2;}
 std::string project=argv[1],mode=argv[2],error,suspended;fs::path out=argv[3];fs::create_directories(out);
 const bool performance=mode=="performance"||mode=="performance-native"||mode=="performance-baked";
 bool freeze=false,ready=false,done=false;unsigned frame=0,at=0,phase=0,prefabAt=0;uint64_t request=0,demand=0,coldStart=0,steadyUploads=0;EntityId old=0,prefab=0;RuntimeWorld* before=nullptr;Json expected,performanceRecord;
 ApplicationControl c;c.hidden=true;c.hostReady=[&](EngineHost& h){coldStart=PerformanceProfiler::Now();SDL_GL_SetSwapInterval(0);h.GetWindow().SetTestInputMode(true);std::string e;h.Audio().Init(e,true);PerformanceProfiler::Get().Enable(true);};c.frameSeconds=[&](float){return freeze?0.f:1.f/60;};
 auto fingerprint=[&](RuntimeWorld& w){auto* a=w.RuntimeAnimation(11);Json layers=Json::array();for(const auto& layer:a->layers)layers.push_back(Json{{"id",layer.settings.id},{"clip",layer.settings.clip},{"mask",layer.settings.mask},{"weight",layer.settings.weight},{"enabled",layer.settings.enabled},{"additive",layer.settings.additive}});return Json{{"clip",a->playback.clip},{"time",a->playback.time},{"fade",a->mixer.elapsed},{"layers",std::move(layers)},{"otherTime",w.RuntimeAnimation(13)->playback.time}};};
 c.beforeFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay&){
  if(mode=="probe"&&ready&&phase==0){unsigned n=frame-at;for(auto [f,key]:{std::pair<unsigned,SDL_Scancode>{1,SDL_SCANCODE_G},{5,SDL_SCANCODE_L},{9,SDL_SCANCODE_I},{29,SDL_SCANCODE_I},{35,SDL_SCANCODE_P},{80,SDL_SCANCODE_P},{95,SDL_SCANCODE_1},{100,SDL_SCANCODE_M},{115,SDL_SCANCODE_SPACE},{120,SDL_SCANCODE_RIGHT}})if(n==f||n==f+1)QueueTestKey(h.GetWindow(),key,n==f);}
  if(mode=="read"&&phase==1&&before!=&w){freeze=true;auto now=fingerprint(w);Check(now.at("clip")==expected.at("clip")&&std::abs(now.at("time").get<float>()-expected.at("time").get<float>())<1e-5,"fresh process restores ordinary baked clip and exact clock");Check(now.at("layers")==expected.at("layers")&&now.at("fade")==expected.at("fade"),"fresh process restores mixer transition, stable joint mask and authored weight");phase=2;}
 };
 c.afterFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay&){
  ++frame;if(done)return;
  if(w.Scripts()&&!w.Scripts()->Diagnostics().empty()){for(auto& d:w.Scripts()->Diagnostics())printf("FAULT %s %s\n",d.callback.c_str(),d.message.c_str());Check(false,"public JS lab lifecycle healthy");Quit();return;}
  auto* a=w.RuntimeAnimation(11);
  if(!ready&&a&&a->asset&&w.RuntimeAnimation(10)&&w.RuntimeAnimation(10)->asset&&w.RuntimeAnimation(12)&&w.RuntimeAnimation(12)->asset&&w.RuntimeAnimation(13)&&w.RuntimeAnimation(13)->asset&&w.BaselineFingerprint().size()==64){
   ready=true;at=frame;Check(a->asset==w.RuntimeAnimation(13)->asset&&a!=w.RuntimeAnimation(13),"two instances share immutable target but independent mixer");Check(a->asset->clips.size()==6&&w.RuntimeAnimation(12)->asset->clips.size()==6,"normal resources publish native and five transferred named clips");Check(a->asset->skeleton.names.size()!=w.RuntimeAnimation(12)->asset->skeleton.names.size(),"real runtime uses unequal target helper counts");
   if(performance){
    // Only the broad target11 changes clip across the paired runs. The exact
    // scene, immutable assets, all four skeletons, other clips, clocks, speed,
    // loop, layer/IK/physics settings and camera remain identical.
    const std::string selected=mode=="performance-native"?"Native":"Wave";
    for(EntityId id:{10,11,12,13}){auto* instance=w.RuntimeAnimation(id);instance->playback.clip=id==11?selected:"Wave";instance->playback.time=0;instance->playback.speed=1;instance->playback.loop=true;instance->playback.playing=true;instance->playback.stopped=false;instance->mixer.Clear();instance->mixer.paused=false;}
    auto clip=std::find_if(a->asset->clips.begin(),a->asset->clips.end(),[&](const auto& value){return value.name==selected;});
    Check(clip!=a->asset->clips.end(),"paired mode selects an ordinary clip in the same target asset");
    if(clip==a->asset->clips.end()){Quit();return;}
    size_t keys=0;for(const auto& track:clip->tracks)keys+=track.times.size();
    performanceRecord={{"mode",mode},{"project",project},{"baseline_fingerprint",w.BaselineFingerprint()},{"controlled_entity",11},{"selected_clip",selected},{"skeleton_nodes",a->asset->skeleton.names.size()},{"palette_entries",a->asset->skeleton.skinNodes.size()},{"selected_clip_tracks",clip->tracks.size()},{"selected_clip_keys",keys},{"selected_clip_duration_seconds",clip->duration},{"animated_instances",4},{"other_entities_clips","10=Wave,12=Wave,13=Wave"},{"cold_ready_ms",double(PerformanceProfiler::Now()-coldStart)/1e6},{"cold_ready_frame",frame},{"cold_uploads",h.Resources().Stats().uploads},{"cold_resource_misses",h.Resources().Stats().misses},{"cold_peak_resource_bytes",h.Resources().Stats().peakBytesResident},{"comparison_limit","Same scene/assets/settings; only entity11 Native versus Wave. Track density and pose differ, so timings demonstrate the ordinary runtime path, not an isolated universal zero-overhead claim."}};
   }
  }
  if(!ready){std::this_thread::sleep_for(std::chrono::milliseconds(2));if(frame>1500){Check(false,"bounded cold readiness");Quit();}return;}
  if(performance){
   if(frame-at==60){
    Check(h.Resources().Stats().loading==0,"steady measurement begins after normal async loading finishes");steadyUploads=h.Resources().Stats().uploads;
    // The ready frame is closed before exporting; all uploads now appear in
    // M56. This capture includes startup/loading and completed warmup frames.
    Check(PerformanceProfiler::Get().Export((out/"cold-profile.json").string(),error),"completed loading and GPU publication recorded separately from steady playback");
    performanceRecord["loading_and_warmup_metrics"]=ProfileMetrics(true);performanceRecord["cold_profile_note"]="Completed startup/loading scopes and pre-measurement warmup frames; cold_ready_ms is measured separately at actual animation readiness.";
    PerformanceProfiler::Get().Clear();
   }
   if(frame-at==360){
    Check(w.Scripts()->Diagnostics().empty()&&a->playback.playing&&a->playback.clip==performanceRecord.at("selected_clip"),"steady ordinary playback retains selected native or baked clip");
    Check(h.Resources().Stats().uploads==steadyUploads,"steady playback requires no new mesh or texture uploads");
    Check(PerformanceProfiler::Get().Export((out/"profile.json").string(),error),"paired same-scene four-instance steady M56 profile");
    performanceRecord["steady_metrics"]=ProfileMetrics();performanceRecord["steady_upload_delta"]=h.Resources().Stats().uploads-steadyUploads;performanceRecord["warmup_frames"]=60;performanceRecord["measurement_frames"]=300;performanceRecord["retained_steady_frames"]=PerformanceProfiler::Get().History().size();performanceRecord["renderer_draw_calls_accumulated"]=h.GetRenderer().Stats().drawCalls;performanceRecord["renderer_triangles_accumulated"]=h.GetRenderer().Stats().triangles;
    std::ofstream record(out/"metrics.json");record<<performanceRecord.dump(2)<<'\n';Check(bool(record),"paired performance conditions and independent named-scope metrics written");done=true;Quit();
   }
  }else if(mode=="probe"){
   if(phase==0&&frame-at>135){Check(a->playback.clip=="TravelExtract"&&!a->playback.playing,"logical JS selects extracted clip, pauses and seeks");Check(a->layers.size()==1&&a->layers[0].settings.mask.size()==3,"normal JS composes masked arm layer");Check(w.RuntimeAnimation(13)->playback.clip=="Wave"&&w.RuntimeAnimation(13)->playback.time!=a->playback.time,"independent instance retains different clip/time");Check(!w.RagdollActive(11),"shared physical contribution returns to animation without alternate skinning");auto* socket=w.RuntimeDefinition(30);SceneTransform palm;Check(socket&&socket->socket&&w.JointPose(11,socket->socket->joint,"world",1,palm)&&glm::length(socket->transform.position-palm.position)<.02f,"ordinary resolved skeleton drives stable palm socket");bool finite=true;for(auto& m:a->skin)for(int i=0;i<4;++i)for(int j=0;j<4;++j)finite&=std::isfinite(m[i][j]);Check(finite&&!a->skin.empty()&&h.GetRenderer().Stats().triangles>0,"real GL consumes finite target skinning palettes");std::vector<unsigned char> pixels;h.GetRenderer().CaptureFrame(h.GetWindow().Width(),h.GetWindow().Height(),pixels);Check(WriteRgbPng((out/"runtime.png").string(),h.GetWindow().Width(),h.GetWindow().Height(),pixels),"actual rendered source/two-target comparison captured");
    SceneTransform placement;placement.position={4,0,0};prefab=w.SpawnPrefab("1c71edcd7b765c9ad62cfec4511fd3d3",placement,error);
    Check(prefab!=0,"normal registered animated prefab spawns through RuntimeWorld");if(!prefab){printf("PREFAB ERROR %s\n",error.c_str());Quit();return;}prefabAt=frame;phase=5;
   }
   auto* stream=w.SceneControl()->Streaming(w,error);
   if(phase==5){
    auto* spawned=w.RuntimeAnimation(prefab);
    if(spawned&&spawned->asset){
     Check(spawned->asset==a->asset&&spawned!=a,"prefab uses normal shared immutable target and independent animation state");
     Check(spawned->playback.clip=="Wave"&&spawned->playback.playing&&!spawned->skin.empty(),"prefab authored baked clip becomes animation-ready through normal resource path");
     Check(a->playback.clip=="TravelExtract"&&!a->playback.playing&&a->layers.size()==1&&spawned->layers.empty(),"prefab does not inherit existing instance clock or mixer layer");
     prefabAt=frame;phase=6;
    }else if(frame-prefabAt>300){Check(false,"bounded normal prefab animation readiness");Quit();return;}
   }
   if(phase==6&&frame-prefabAt>=15){
    auto* spawned=w.RuntimeAnimation(prefab);bool finite=spawned&&spawned->asset;
    if(finite)for(const auto& matrix:spawned->skin)for(int i=0;i<4;++i)for(int j=0;j<4;++j)finite&=std::isfinite(matrix[i][j]);
    Check(finite&&spawned->playback.time>.05f&&h.GetRenderer().Stats().triangles>0,"runtime prefab baked playback advances with finite ordinary GPU skin matrices");
    Check(w.DestroyHierarchy(prefab,error),"normal prefab hierarchy destruction succeeds");
    Check(!w.RuntimeDefinition(prefab)&&!w.RuntimeAnimation(prefab),"destroyed prefab animation handle cannot resolve a live instance");
    Check(w.RuntimeAnimation(11)&&w.RuntimeAnimation(13)&&w.RuntimeAnimation(11)->asset==w.RuntimeAnimation(13)->asset,"destroying shared-asset prefab preserves original animated instances");
    Check(stream!=nullptr,"ordinary world manifest opens");if(!stream){Quit();return;}demand=stream->Request("rig",false,error);Check(demand!=0,"normal animated region request");phase=1;
   }
   if(phase==1&&(old=stream->Resolve("rig",1))){auto* r=w.RuntimeAnimation(old);if(!r||!r->asset)return;AnimationLayerSettings layer;layer.id="retained";layer.clip="Bob";layer.weight=.3f;Check(w.SetAnimationLayer(old,layer,false,error),"stream instance accepts ordinary baked layer");r->playback.clip="TravelInPlace";r->playback.time=.37f;r->playback.speed=0;w.ResolveAnimationPose(*r,0);freeze=true;Check(WorldPersistence::CaptureAnimation(w,old,suspended,error),"capture baked region clip/mixer snapshot");Check(stream->Release(demand)&&stream->Unload("rig"),"retire normal animated region");phase=2;}
   if(phase==2&&!w.RuntimeDefinition(old)){Check(!w.RuntimeAnimation(old),"retired handle cannot alias live animation");demand=stream->Request("rig",false,error);phase=3;}
   if(phase==3){auto id=stream->Resolve("rig",1);if(id&&w.RuntimeAnimation(id)&&w.RuntimeAnimation(id)->asset){std::string resumed;Check(id!=old&&WorldPersistence::CaptureAnimation(w,id,resumed,error)&&resumed==suspended,"revisit fresh handle restores exact baked clip/mixer payload");freeze=false;before=&w;Check(w.SceneControl()->Reload(error),"ordinary reload queued at outer boundary");phase=4;}}
   if(phase==4&&before!=&w&&w.RuntimeAnimation(11)&&w.RuntimeAnimation(11)->asset){Check(w.RuntimeAnimation(11)->layers.empty()&&!w.RagdollActive(11),"reload reconstructs authored mixer and physical state");Check(w.Physics().AliveBodyCount()==1,"reload leaves no physical articulation bodies");Check(PerformanceProfiler::Get().Export((out/"profile.json").string(),error),"bounded M56 runtime profile captured");done=true;Quit();}
  }else{
   auto* svc=w.SceneControl()->Saves(h.Resources());
   if(phase==0&&frame-at>10){freeze=true;if(mode=="write"){AnimationLayerSettings l;l.id="saved-left";l.clip="Wave";l.weight=.3f;l.mask=LeftArmMask(a->asset->skeleton);Check(l.mask.size()==3,"save fixture uses three real stable left-arm hierarchy keys");Check(w.SetAnimationLayer(11,l,false,error),"normal saved layer references baked clip with authored mask and weight");a->playback.clip="TravelInPlace";a->playback.time=.35f;a->mixer.CrossFade(a->playback,"Native",2);w.ResolveAnimationPose(*a,.3f);expected=fingerprint(w);std::ofstream(out/"expected.json")<<expected.dump(2)<<'\n';request=svc->Request("save","m74","Borrowed ordinary clips","{}",error);}else{std::ifstream(out/"expected.json")>>expected;before=&w;request=svc->Request("load","m74","","{}",error);}Check(request!=0,error.empty()?"modern save/load request":error.c_str());phase=1;}
   if(request){auto* s=svc->Status(request);if(s&&s->state=="failed"){Check(false,s->error.c_str());Quit();return;}if(s&&s->state=="completed"&&(mode=="write"||phase==2)){Check(true,"modern slot transaction completed");done=true;Quit();}}
  }
  if(!performance)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  if(frame>2200){Check(false,"bounded application completion");Quit();}
 };
 Application app;char name[]="judas";char* args[]={name,project.data()};Check(app.Run(2,args,&c)==0&&done,"ordinary application startup/lifecycle/shutdown");printf("SUMMARY mode=%s checks=%d failures=%d frames=%u\n",mode.c_str(),checks,failures,frame);return failures?1:0;
}
