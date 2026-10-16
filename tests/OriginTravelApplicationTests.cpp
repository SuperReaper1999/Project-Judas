#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "WorldStreaming.h"
#include "SaveService.h"
#include "ScreenshotWriter.h"
#include "SaveArchive.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cstdio>

using J=nlohmann::json;
namespace {unsigned checks=0,failures=0;void Check(bool b,const char* label){++checks;failures+=!b;std::printf("%s %s\n",b?"PASS":"FAIL",label);std::fflush(stdout);}std::string Spin(PowertrainInstance* p){SaveArchive a;if(p)p->Persist(a);return a.bytes;}}
int main(int argc,char** argv){
 if(argc<3)return 2;
 const bool cameraOnly=argc>3&&std::string(argv[3])=="--camera-only";
 const auto out=std::filesystem::absolute(argv[2]);std::filesystem::create_directories(out);unsigned frame=0,phase=0,phaseFrame=0;bool frozen=false,done=false;Window* window=nullptr;RuntimeWorld* old=nullptr;uint64_t request=0;EntityId roverId=0;glm::dvec3 savedPosition;glm::vec3 savedVelocity;std::string savedSpin;std::vector<double> fixed,render,originTimes,frames;std::chrono::steady_clock::time_point frameStart;uint64_t revision=0,maximumShifts=0;double distance=0,beforeYaw=0,beforePitch=0;std::string error;
 auto key=[&](const char* name,bool down){window->QueueTestPhysical(std::string("key:")+name,down?1.f:0.f);};
 ApplicationControl control;control.hidden=true;control.frameSeconds=[&](float){return frozen?0.f:1.f/60;};control.hostReady=[&](EngineHost& h){window=&h.GetWindow();window->SetTestInputMode(true);SDL_GL_SetSwapInterval(0);h.Audio().Init(error,true);};
 control.beforeFrame=[&](EngineHost&,RuntimeWorld&,InteractivePlay&){frameStart=std::chrono::steady_clock::now();++frame;++phaseFrame;if(frame==20){window->QueueTestPhysical("mouse:dx",120);window->QueueTestPhysical("mouse:dy",-60);}if(frame==24){window->QueueTestPhysical("mouse:dx",-90);window->QueueTestPhysical("mouse:dy",30);}if(frame==50){window->QueueTestPhysical("mouse:dx",80);window->QueueTestPhysical("mouse:dy",25);}if(frame==70)key("Escape",true);if(frame==71)key("Escape",false);if(frame==74){window->QueueTestPhysical("mouse:dx",120);window->QueueTestPhysical("mouse:dy",120);}if(frame==76)key("Escape",true);if(frame==77)key("Escape",false);if(frame==80){window->QueueTestPhysical("mouse:dx",15);window->QueueTestPhysical("mouse:dy",-10);}if(frame==35)key("T",true);if(frame==37)key("T",false);if(phase==1)key("Space",true);};
 control.afterFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){
  if(frame>22000){Check(false,"bounded end-to-end travel finishes");done=true;world.UI().RequestQuit();return;}
  if(!world.Scripts())return;
  if(world.Scripts()&&!world.Scripts()->Diagnostics().empty()){for(auto& d:world.Scripts()->Diagnostics())std::printf("SCRIPT %s %s\n",d.callback.c_str(),d.message.c_str());Check(false,"project scripts remain healthy");done=true;world.UI().RequestQuit();return;}
  auto records=world.Scripts()->Capture();if(records.empty())return;J state=J::parse(records[0].json);if(!state.value("ready",false))return;
  roverId=std::stoull(state.at("rover").get<std::string>());auto* train=world.RuntimePowertrain(roverId);auto body=world.RuntimeBody(roverId);const auto pose=world.Physics().GetTransform(body);distance=state.value("distance",0.);maximumShifts=std::max(maximumShifts,world.OriginRevision());
  if(frame%600==0)std::printf("TRAVEL frame=%u phase=%u distance=%.3f speed=%.3f origin_revision=%llu local=%.3f error=%s\n",frame,phase,distance,glm::length(world.Physics().GetLinearVelocity(body)),(unsigned long long)world.OriginRevision(),glm::length(pose.position),world.OriginError().c_str());
  if(frame>200&&phase==0){frames.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frameStart).count());fixed.push_back(play.LastFixedStepMilliseconds());render.push_back(play.LastSceneMilliseconds());if(world.OriginRevision()!=revision){originTimes.push_back(play.LastFixedStepMilliseconds());revision=world.OriginRevision();}}
  if(phase==0&&frame==5400&&glm::length(world.Physics().GetLinearVelocity(body))<.2f){std::vector<unsigned char> pixels;host.GetRenderer().CaptureFrame(window->Width(),window->Height(),pixels);WriteRgbPng((out/"stalled.png").string(),window->Width(),window->Height(),pixels);std::printf("STALL pose %.6f %.6f %.6f rot %.6f %.6f %.6f %.6f\n",pose.position.x,pose.position.y,pose.position.z,pose.rotation.w,pose.rotation.x,pose.rotation.y,pose.rotation.z);for(auto id:world.DefinitionIds())if(auto* w=world.RuntimeWheel(id);w&&w->settings.chassis==roverId)std::printf("WHEEL %llu contact=%d compression=%f force=%f\n",(unsigned long long)id,w->state.contact,w->state.length,w->state.normalForce);Check(false,"watch drive progresses through waystation");done=true;world.UI().RequestQuit();return;}
  // Inject the physical controls emitted by Window's SDL motion path, never
  // the logical axis name: a broken project binding must fail this fixture.
  if(frame==19||frame==23||frame==49||frame==73||frame==79){beforeYaw=state.value("yaw",0.);beforePitch=state.value("pitch",0.);}
  if(frame==20||frame==24||frame==50||frame==74||frame==80)std::printf("LOOK frame=%u yaw=%.9f pitch=%.9f paused=%d\n",frame,state.value("yaw",0.),state.value("pitch",0.),world.UI().Paused());
  if(frame==20)Check(std::abs(state.value("yaw",0.)-beforeYaw-.24)<1e-6&&std::abs(state.value("pitch",0.)-beforePitch+.12)<1e-6,"canonical positive-X/negative-Y mouse deltas reach on-foot script look at full magnitude");
  if(frame==21)Check(std::abs(state.value("yaw",0.)-beforeYaw-.24)<1e-6,"relative mouse delta is consumed once rather than held into later frames");
  if(frame==24)Check(std::abs(state.value("yaw",0.)-beforeYaw+.18)<1e-6&&std::abs(state.value("pitch",0.)-beforePitch-.06)<1e-6,"opposite mouse directions reverse on-foot camera intent");
  if(frame==26){const auto actor=std::stoull(state.at("actor").get<std::string>());const auto up=glm::normalize(-world.SampleEntityGravity(actor,world.RuntimeCharacter(actor)->position));const auto base=glm::normalize(glm::vec3(0,0,-1)-up*glm::dot(glm::vec3(0,0,-1),up));const auto right=glm::cross(base,up);const auto forward=world.view?world.view->pose.rotation*glm::vec3(0,0,-1):base;const auto heading=std::atan2(glm::dot(forward,right),glm::dot(forward,base));std::printf("CAMERA measured_heading_rad=%.9f\n",heading);Check(world.view&&heading>.025f&&heading<.3f,"real third-person view orbits in the requested mouse direction, not merely due to settling");}
  if(frame==50)Check(state.value("driving",false)&&std::abs(state.value("yaw",0.)-beforeYaw-.16)<1e-6&&std::abs(state.value("pitch",0.)-beforePitch-.05)<1e-6,"physical mouse axes also orbit the same rover camera while driving");
  if(frame==74)Check(world.UI().Paused()&&state.value("yaw",0.)==beforeYaw&&state.value("pitch",0.)==beforePitch,"paused UI blocks mouse-look leakage into gameplay");
  if(frame==80)Check(!world.UI().Paused()&&std::abs(state.value("yaw",0.)-beforeYaw-.03)<1e-6&&std::abs(state.value("pitch",0.)-beforePitch+.02)<1e-6,"mouse look resumes through normal input after closing pause UI");
  if(frame==30){auto actor=std::stoull(state.at("actor").get<std::string>());const auto shown=world.PresentedTransform(actor,world.RuntimeDefinition(actor)->transform,1);Check(world.view&&world.RenderVisible(actor)&&glm::length(world.view->pose.position-shown.position)>1,"initial on-foot camera is outside the visible character model");}
  if(phase==0&&distance>(cameraOnly?20:2080)){

   if(!cameraOnly){Check(world.OriginRevision()>=3,"actual powered planetary drive crosses multiple origin charts");}
   Check(glm::length(pose.position)<600,"active physical coordinates remain local after kilometre-scale travel");unsigned contacts=0;for(auto id:world.DefinitionIds())if(auto* w=world.RuntimeWheel(id);w&&w->settings.chassis==roverId)contacts+=w->state.contact;
   Check(contacts>=3&&train&&train->state.available,"real wheel/ground contact and powertrain remain operational");
   auto* stream=world.SceneControl()->StreamingIfLoaded();unsigned retained=0;if(stream)for(auto& r:stream->Regions())retained+=r.state=="unloaded"&&r.retained>0;if(!cameraOnly)Check(retained>0,"departed waystation actually unloads with retained state");
   std::vector<unsigned char> pixels;host.GetRenderer().CaptureFrame(window->Width(),window->Height(),pixels);Check(WriteRgbPng((out/"drive.png").string(),window->Width(),window->Height(),pixels),"actual kilometre-scale drive frame captured");
   // Toggle ordinary project watch input; subsequent physical brakes stop it.
   key("T",true);phase=1;phaseFrame=0;
  }
  if(phase==1){if(phaseFrame==2)key("T",false);if(phaseFrame>40&&glm::length(world.Physics().GetLinearVelocity(body))<1.2f){key("Space",false);key("E",true);phase=2;phaseFrame=0;}}
  if(phase==2){if(phaseFrame==2)key("E",false);if(phaseFrame>90){auto actor=std::stoull(state.at("actor").get<std::string>());auto* motor=world.RuntimeCharacter(actor);Check(!state.at("driving").get<bool>()&&motor&&motor->result.supported,"leave moving assembly and stand on real planetary collision");const auto shown=world.PresentedTransform(actor,world.RuntimeDefinition(actor)->transform,1);const auto pivot=shown.position+glm::normalize(-world.SampleEntityGravity(actor,shown.position))*.5f;Check(world.view&&world.RenderVisible(actor)&&glm::length(world.view->pose.position-shown.position)>1,"dismounted third-person camera stays outside the visible character");Check(world.view&&glm::dot(world.view->pose.rotation*glm::vec3(0,0,-1),glm::normalize(pivot-world.view->pose.position))>.99f,"dismounted view aims at the actual presented character under local gravity");std::vector<unsigned char> footPixels;host.GetRenderer().CaptureFrame(window->Width(),window->Height(),footPixels);Check(WriteRgbPng((out/"on-foot.png").string(),window->Width(),window->Height(),footPixels),"actual dismounted third-person frame captured");frozen=true;savedPosition=world.Settings().worldOrigin+glm::dvec3(pose.position);savedVelocity=world.Physics().GetLinearVelocity(body);savedSpin=Spin(train);request=world.SceneControl()->Saves(host.Resources())->Request("save","m90-continuity","","{}",error);Check(request!=0,"ordinary save service accepts travelled assembly");phase=3;phaseFrame=0;}}
  if(phase==3){auto* status=world.SceneControl()->Saves(host.Resources())->Status(request);if(status&&(status->state=="completed"||status->state=="failed")){Check(status->state=="completed","travelled save commits successfully");if(status->state=="failed")std::puts(status->error.c_str());old=&world;request=world.SceneControl()->Saves(host.Resources())->Request("load","m90-continuity","","{}",error);phase=4;phaseFrame=0;}}
  if(phase==4&&&world!=old){Check(glm::length(world.Settings().worldOrigin+glm::dvec3(pose.position)-savedPosition)<.0001,"fresh world reconstruction preserves absolute chassis endpoint");Check(glm::length(world.Physics().GetLinearVelocity(body)-savedVelocity)<.0001f&&Spin(train)==savedSpin,"cold publication preserves actual momentum and mechanical continuation");frozen=false;phase=5;phaseFrame=0;}
  if(phase==5&&phaseFrame>60){auto actor=std::stoull(state.at("actor").get<std::string>());Check(world.RuntimeCharacter(actor)->result.supported,"restored character resumes against reconstructed collision");Check(world.SceneControl()->Reload(error),"normal scene reload requested");old=&world;phase=6;phaseFrame=0;}
  if(phase==6&&&world!=old&&phaseFrame>90){Check(state.value("distance",0.)<1&&world.Settings().worldOrigin.y==6371000,"reload rebuilds authored chart without old runtime state");done=true;world.UI().RequestQuit();}
  if(frame>22000){Check(false,"bounded end-to-end travel finishes");done=true;world.UI().RequestQuit();}
  if(frozen)std::this_thread::sleep_for(std::chrono::milliseconds(1));
 };
 Application app;char program[]="m90-travel";char* args[]={program,argv[1]};const auto result=app.Run(2,args,&control);
 Check(result==0&&done,"ordinary application lifecycle completes");
 auto stats=[](std::vector<double> values){std::sort(values.begin(),values.end());return values.empty()?std::vector<double>{0,0,0}:std::vector<double>{values[values.size()/2],values[values.size()*95/100],values.back()};};
 std::ofstream samples(out/"samples.csv");samples<<"frame_ms,fixed_ms,scene_ms\n";for(size_t i=0;i<fixed.size();++i)samples<<frames[i]<<','<<fixed[i]<<','<<render[i]<<'\n';std::ofstream transitions(out/"transitions.csv");transitions<<"fixed_ms\n";for(auto value:originTimes)transitions<<value<<'\n';
 J report={{"checks",checks},{"failures",failures},{"frames",frame},{"maximumOriginTransitions",maximumShifts},{"fixedMilliseconds",stats(fixed)},{"sceneMilliseconds",stats(render)},{"frameMilliseconds",stats(frames)},{"shiftStepMilliseconds",stats(originTimes)}};std::ofstream(out/"results.json")<<report.dump(2)<<'\n';std::printf("SUMMARY %u checks %u failures frames=%u\n",checks,failures,frame);return failures?1:result;
}
