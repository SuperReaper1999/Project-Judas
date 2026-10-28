#include "PerformanceProfiler.h"
#include "TestHarness.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "DynamicBody.h"
#include "ScriptSystem.h"
#include "SceneSession.h"
#include "Renderer.h"
#include "Window.h"
#include "ScreenshotWriter.h"
#include "SimulationTiming.h"
#include <SDL2/SDL.h>
#include <cmath>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <map>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
struct Command {std::string kind,name,path;int begin=0,end=0;float value=0,other=0,pressure=0;std::int64_t finger=0;int down=0;};
struct Script {int frames=600,logEvery=1;float seconds=SimulationTiming::kFixedTimestep;bool realtime=false;std::string entity;std::vector<Command> commands;};
void Require(bool ok,const std::string& error){if(!ok)throw std::runtime_error(error);}
Script Load(const std::string& path,const InputMap& map){
 std::ifstream f(path);Require(bool(f),"cannot open "+path);Script s;std::string line;unsigned index=0;
 while(std::getline(f,line)){++index;line=line.substr(0,line.find('#'));std::istringstream in(line);std::string op;in>>op;if(op.empty())continue;Command c;c.kind=op;
  if(op=="STEPS"||op=="FRAMES")in>>s.frames;
  else if(op=="LOG_EVERY")in>>s.logEvery;
  else if(op=="REALTIME"){in>>s.frames;s.realtime=true;}
  else if(op=="FRAME_SECONDS")in>>s.seconds;
  else if(op=="ENTITY")in>>s.entity;
  else if(op=="SCREENSHOT")in>>c.begin>>c.path;
  else if(op=="HOLD"){in>>c.name>>c.begin>>c.end;c.value=1;c.name="key:"+c.name;}
  else if(op=="TAP"){in>>c.name>>c.begin;c.end=c.begin+1;c.value=1;c.name="key:"+(c.name=="SPACE"?std::string("Space"):c.name);}
  else if(op=="ACTION"||op=="AXIS"){in>>c.name>>c.value>>c.begin>>c.end;auto* entry=map.Find(c.name);Require(entry&&!entry->bindings.empty(),"unknown/unbound input "+c.name);Require(entry->axis==(op=="AXIS"),"input kind mismatch "+c.name);auto b=entry->bindings.front();Require(b.scale!=0,"zero binding scale");if(op=="AXIS"&&b.deadzone>0){const float v=c.value/b.scale;c.value=v==0?0:std::copysign(b.deadzone+std::abs(v)*(1-b.deadzone),v);}else c.value/=b.scale;c.name=b.control;}
  else if(op=="CONTROL")in>>c.name>>c.value>>c.begin>>c.end;
  else if(op=="LOOK")in>>c.value>>c.other>>c.begin;
  else if(op=="POINTER"||op=="POINTER_DOWN"||op=="POINTER_UP")in>>c.value>>c.other>>c.begin;
  else if(op=="TOUCH_DOWN"||op=="TOUCH_MOVE")in>>c.finger>>c.value>>c.other>>c.pressure>>c.begin;
  else if(op=="TOUCH_UP")in>>c.finger>>c.value>>c.other>>c.begin;
  else if(op=="EXPECT_POINTER")in>>c.value>>c.other>>c.down>>c.begin;
  else if(op=="EXPECT_TOUCHES")in>>c.value>>c.begin;
  else if(op=="SAMPLE_GRAVITY"){glm::vec3 p;in>>p.x>>p.y>>p.z;c.path=std::to_string(p.x)+" "+std::to_string(p.y)+" "+std::to_string(p.z);}
  else if(op=="EXPECT_AXIS"||op=="EXPECT_HELD"){
   in>>c.name>>c.value>>c.begin;auto* entry=map.Find(c.name);
   Require(entry&&!entry->bindings.empty(),"unknown/unbound expectation input "+c.name);
   Require(entry->axis==(op=="EXPECT_AXIS"),"expectation input kind mismatch "+c.name);
  }
  else if(op=="EXPECT_SCENE")in>>c.name>>c.begin;
  else if(op=="WAIT_SERVICES")in>>c.name>>c.value>>c.begin;
  else if(op=="EXPECT_PAUSED")in>>c.value>>c.begin;
  else throw std::runtime_error("unknown directive "+op);
  Require(!in.fail(),"invalid arguments at line "+std::to_string(index));std::string extra;Require(!(in>>extra),"extra argument at line "+std::to_string(index));
  if(op=="EXPECT_HELD"||op=="EXPECT_PAUSED")Require(c.value==0||c.value==1,"boolean expectation must be 0 or 1");
  if(op=="SCREENSHOT")Require(!c.path.empty(),"empty screenshot path");
  s.commands.push_back(c);
 }
 Require(s.frames>0&&s.frames<=1000000&&s.logEvery>=0&&std::isfinite(s.seconds)&&s.seconds>=0&&s.seconds<=.25f,"invalid frame count/clock");
 for(const auto& c:s.commands)if(c.kind=="SCREENSHOT"||c.kind=="LOOK"||(c.kind.rfind("POINTER",0)==0)||(c.kind.rfind("TOUCH_",0)==0)||c.kind.rfind("EXPECT_",0)==0||c.kind=="WAIT_SERVICES")Require(c.begin>=0&&c.begin<s.frames&&std::isfinite(c.value)&&std::isfinite(c.other),"invalid command frame/value");
 for(const auto& c:s.commands)if(c.kind=="ACTION"||c.kind=="AXIS"||c.kind=="HOLD"||c.kind=="TAP"||c.kind=="CONTROL")Require(c.begin>=0&&c.end>c.begin&&c.end<=s.frames&&std::isfinite(c.value),"invalid input interval");
 for(const auto& c:s.commands){
  if(c.kind.rfind("POINTER",0)==0)Require(double(c.value)>=std::numeric_limits<int>::min()&&double(c.value)<=std::numeric_limits<int>::max()&&double(c.other)>=std::numeric_limits<int>::min()&&double(c.other)<=std::numeric_limits<int>::max(),"pointer pixels exceed integer range");
  if(c.kind=="EXPECT_POINTER")Require((c.down==0||c.down==1)&&c.value>=0&&c.value<=1&&c.other>=0&&c.other<=1,"invalid pointer expectation");
  if(c.kind=="EXPECT_TOUCHES")Require(c.value>=0&&c.value<=10&&std::floor(c.value)==c.value,"invalid touch count expectation");
  if(c.kind.rfind("TOUCH_",0)==0)Require(c.finger>=0&&c.value>=0&&c.value<=1&&c.other>=0&&c.other<=1&&std::isfinite(c.pressure)&&c.pressure>=0&&c.pressure<=1,"invalid normalized touch contact");
 }
 return s;
}
}
int RunTestHarness(Window& window,Renderer& renderer,InteractivePlay& play,const std::string& path,const std::function<void()>& beforeFrame,const std::function<void()>& afterFrame,const std::function<bool(const std::string&)>& servicesReady){
 try{
  const auto script=Load(path,window.Input().Map());window.SetTestInputMode(true);SDL_GL_SetSwapInterval(0);
  for(const auto& c:script.commands)if(c.kind=="SAMPLE_GRAVITY"){glm::vec3 p;std::istringstream(c.path)>>p.x>>p.y>>p.z;auto g=play.Session().World().Gravity().Sample(p);std::printf("sample,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f\n",p.x,p.y,p.z,g.x,g.y,g.z);}
  Uint64 last=SDL_GetPerformanceCounter();
  std::printf("step,time,posX,posY,posZ,upX,upY,upZ,grounded,velX,velY,velZ,gravX,gravY,gravZ,controlled,frame,steps,paused");
  for(size_t i=0;i<play.Session().World().DynamicBodies().size();++i){std::printf(",obj%zuPosX,obj%zuPosY,obj%zuPosZ,obj%zuVelX,obj%zuVelY,obj%zuVelZ",i,i,i,i,i,i);}
  std::printf("\n");
  for(int frame=0;frame<script.frames;++frame){
   ProfileFrame frameProfile("scripted application frame");
   if(beforeFrame)beforeFrame();
   // No PollEvents: real desktop devices cannot contaminate deterministic input.
   window.BeginTestFrame();
   for(const auto& c:script.commands){
    if(c.kind=="HOLD"||c.kind=="TAP"||c.kind=="ACTION"||c.kind=="AXIS"||c.kind=="CONTROL"){
     if(frame==c.begin)window.ApplyTestPhysical(c.name,c.value);
     if(frame==c.end)window.ApplyTestPhysical(c.name,0);
    }
    if(frame==c.begin&&c.kind=="LOOK"){window.Input().AddDelta("mouse:dx",c.value);window.Input().AddDelta("mouse:dy",c.other);window.QueueTestMouseDelta(int(c.value),int(c.other));}
    if(frame==c.begin&&c.kind.rfind("POINTER",0)==0){window.SetTestPointer(int(c.value),int(c.other));if(c.kind!="POINTER")window.ApplyTestPhysical("mouse:Left",c.kind=="POINTER_DOWN"?1.f:0.f);}
    if(frame==c.begin&&c.kind.rfind("TOUCH_",0)==0){const bool active=window.Input().TouchId(0,c.finger)!=0;Require(c.kind=="TOUCH_DOWN"?!active:active,"touch transition requires matching contact lifetime");window.SetTestTouch(c.finger,c.value,c.other,c.pressure,c.kind!="TOUCH_UP");}
    if(frame==c.begin&&c.kind=="TAP"){if(c.name=="key:Space")window.RequestTestJump();if(c.name=="key:R")window.RequestTestReset();if(c.name=="key:F")window.RequestTestControlToggle();}
    if(c.kind=="HOLD"){const std::map<std::string,Action> actions={{"key:W",Action::MoveForward},{"key:S",Action::MoveBackward},{"key:A",Action::StrafeLeft},{"key:D",Action::StrafeRight},{"key:Q",Action::MoveDown},{"key:E",Action::MoveUp},{"key:I",Action::PitchUp},{"key:K",Action::PitchDown},{"key:J",Action::YawLeft},{"key:L",Action::YawRight},{"key:U",Action::RollLeft},{"key:O",Action::RollRight}};auto it=actions.find(c.name);Require(it!=actions.end(),"unknown HOLD key "+c.name);window.SetTestActionState(it->second,frame>=c.begin&&frame<c.end);}
   }
   bool capture=false;for(const auto& c:script.commands)capture|=c.kind=="SCREENSHOT"&&c.begin==frame;
   float dt=script.seconds;if(script.realtime){auto now=SDL_GetPerformanceCounter();dt=float(now-last)/SDL_GetPerformanceFrequency();last=now;}
   play.Frame(window,renderer,dt,true,capture||script.realtime);
   if(auto* scripts=play.Session().World().Scripts();scripts&&!scripts->Diagnostics().empty())throw std::runtime_error("script callback failed: "+scripts->Diagnostics().front().message);
   for(const auto& c:script.commands)if(c.begin==frame){
    if(c.kind=="SCREENSHOT"){std::filesystem::path output(c.path);if(output.has_parent_path())std::filesystem::create_directories(output.parent_path());std::vector<unsigned char> pixels;renderer.CaptureFrame(window.Width(),window.Height(),pixels);Require(WriteRgbPng(c.path,window.Width(),window.Height(),pixels),"capture failed "+c.path);std::printf("[TestHarness] Wrote screenshot: %s\n",c.path.c_str());}
    if(c.kind=="EXPECT_POINTER"){const auto p=window.Input().Pointer();Require(std::abs(p.x-c.value)<1e-5f&&std::abs(p.y-c.other)<1e-5f&&p.down==(c.down!=0),"gameplay pointer expectation failed");}
    if(c.kind=="EXPECT_TOUCHES")Require(window.Input().Touches().size()==std::size_t(c.value),"gameplay touch count expectation failed");
    if(c.kind=="EXPECT_AXIS")Require(std::abs(window.Input().Axis(c.name)-c.value)<1e-5f,"axis expectation failed "+c.name);
    if(c.kind=="EXPECT_HELD")Require(window.Input().Action(c.name).held==(c.value!=0),"held expectation failed "+c.name);
    if(c.kind=="EXPECT_SCENE")Require(play.Session().World().SceneControl()&&play.Session().World().SceneControl()->Current()==c.name,"scene expectation failed "+c.name);
    if(c.kind=="EXPECT_PAUSED")Require(play.IsPaused()==(c.value!=0),"pause expectation failed");
   }
   if(script.realtime)window.SwapBuffers();
   if(afterFrame)afterFrame();
   // Zero-clock application frames advance the SAME outer services. They are
   // individually profiled; fixed simulation does not run while waiting.
   for(const auto& c:script.commands)if(c.kind=="WAIT_SERVICES"&&c.begin==frame){
    Require(bool(servicesReady)&&c.value>0&&c.value<=60000,"WAIT_SERVICES requires application services and a 1..60000ms bound");
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(int(c.value));unsigned waits=0;
    frameProfile.End();
    while(!servicesReady(c.name)){
     Require(std::chrono::steady_clock::now()<deadline,"service wait timed out: "+c.name+" at scripted frame "+std::to_string(frame));
     ProfileFrame waitProfile("scripted service wait");if(beforeFrame)beforeFrame();window.BeginTestFrame();
     play.Frame(window,renderer,0,true,false);if(afterFrame)afterFrame();++waits;SDL_Delay(1);
    }
    std::printf("[TestHarness] service %s ready after %u wait frames at frame %d\n",c.name.c_str(),waits,frame);
   }
   if(script.logEvery&&frame%script.logEvery==0){auto& world=play.Session().World();glm::vec3 p=play.Session().Player().GetPosition(),v=play.Session().Player().GetVelocity();bool supported=play.Session().Player().IsGrounded();glm::vec3 up=play.Session().Player().GetOrientation()*glm::vec3(0,1,0);
    if(!script.entity.empty()){EntityId id=std::stoull(script.entity);auto* d=world.RuntimeDefinition(id);Require(d,"selected entity is unavailable");p=d->transform.position;up=d->transform.rotation*glm::vec3(0,1,0);if(auto* m=world.RuntimeCharacter(id)){v=m->result.velocity;supported=m->result.supported;}else v=world.Physics().GetLinearVelocity(world.RuntimeBody(id));}
    const auto g=world.Gravity().Sample(p);
    std::printf("%d,%.5f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%d,%d,%d",frame,frame*script.seconds,p.x,p.y,p.z,up.x,up.y,up.z,supported,v.x,v.y,v.z,g.x,g.y,g.z,play.Session().VehicleControl().controlled,frame,play.LastFixedStepsThisFrame(),play.IsPaused());
    for(auto& b:world.DynamicBodies()){auto pos=b.GetPosition(),vel=world.Physics().GetLinearVelocity(b.Handle());std::printf(",%.6f,%.6f,%.6f,%.6f,%.6f,%.6f",pos.x,pos.y,pos.z,vel.x,vel.y,vel.z);}std::printf("\n");
   }
  }
  if(std::getenv("JUDAS_METADATA_TRACE")){auto work=play.Session().World().MetadataStats();auto json=ScriptSystem::MetadataStats();std::printf("[TestHarness] metadata lookups=%llu rebuilds=%llu copies=%llu JSON-contexts=%llu reference-inspections=%llu\n",(unsigned long long)work.lookups,(unsigned long long)work.indexRebuilds,(unsigned long long)work.definitionsCopied,(unsigned long long)json.parserConstructions,(unsigned long long)json.referenceInspections);}
  window.SetTestInputMode(false);return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"[TestHarness] %s\n",e.what());window.SetTestInputMode(false);return 1;}
}
