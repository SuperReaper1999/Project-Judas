#include "Application.h"
#include "TestInput.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "SaveService.h"
#include "WorldPersistence.h"
#include "WorldStreaming.h"
#include "ScreenshotWriter.h"
#include "CollisionAsset.h"
#include "CollisionGeometry.h"
#include "PerformanceProfiler.h"
#include "NavigationAsset.h"
#include "NavigationSystem.h"
#include <SDL2/SDL.h>
#include <filesystem>
#include <thread>
#include <cstdio>
namespace {unsigned checks=0,failures=0;void check(bool ok,const std::string& message){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",message.c_str());std::fflush(stdout);}}
int main(int argc,char** argv){if(argc!=4){std::fprintf(stderr,"usage: %s <project.judasproj> <probe|write|read|stream> <output-dir>\n",argv[0]);return 2;}std::string mode=argv[2],out=argv[3],error;std::filesystem::create_directories(out);unsigned frame=0;uint64_t request=0,region=0;unsigned streamPhase=0,baselineBodies=0;EntityId adopted=0;BodyHandle oldRegion;bool restored=false,frozen=false;GameSnapshot expected;RuntimeWorld* first=nullptr;glm::vec3 before{0};bool skateReady=false;unsigned skateFrames=0;double minSpeed=8,maxProgress=-5,maxPen=0,maxImpulse=0,initialEnergy=0,finalEnergy=0;
 setenv("JUDAS_PROFILE","1",1);ApplicationControl control;control.hidden=true;control.hostReady=[](EngineHost& host){host.GetWindow().SetTestInputMode(true);SDL_GL_SetSwapInterval(0);std::string e;host.Audio().Init(e,true);};control.frameSeconds=[&](float){return frozen?0.f:1.f/60;};control.worldReady=[&](EngineHost&,RuntimeWorld& w,InteractivePlay&){first=&w;};
 control.beforeFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay&){if(mode=="read"&&&w!=first&&!restored){restored=true;frozen=true;std::map<std::string,SaveChunk> chunks;auto captured=WorldPersistence::Capture(w,chunks,error);check(captured||error.find("script instance not initialized")!=std::string::npos,"restored lifecycle defers before script start");check(w.RuntimeDefinition(200)->body->collisionAsset==expected.metadata,"cold reload retains stable cooked identity");check(glm::length(w.Physics().GetLinearVelocity(w.RuntimeBody(200))-glm::vec3(1.25,0,.5))<1e-5,"cold reload exact hull velocity before resumed step");auto c=w.Physics().ClosestPoint(w.Physics().GetTransform(w.RuntimeBody(200)).position+glm::vec3(1,0,0),3);check(c.hit,"cold process cooked geometry participates in real query");}};
 control.afterFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay& play){++frame;for(auto& d:w.Scripts()->Diagnostics()){std::printf("SCRIPT %s: %s\n",d.callback.c_str(),d.message.c_str());check(false,"normal project JavaScript remains healthy");w.UI().RequestQuit();return;}
  if(mode.rfind("skate-",0)==0){
   const std::string scene=mode.find("boxes")!=std::string::npos?"Scenes/skate-boxes.judas":"Scenes/skate-mesh.judas";
   if(frame==1)check(w.SceneControl()->Request(scene,error),"ordinary registered skate scene requested");
   if(w.SceneControl()->Current()==scene&&w.RuntimeDefinition(200)&&w.RuntimeDefinition(200)->body->shape==SceneShape::Box){
    ++skateFrames;auto b=w.RuntimeBody(200);auto v=w.Physics().GetLinearVelocity(b),spin=w.Physics().GetAngularVelocity(b);auto t=w.Physics().GetTransform(b);
    const double energy=2*glm::dot(v,v)+.5*glm::dot(spin,w.Physics().GetInertiaWorld(b)*spin)+4*9.81*t.position.y;
    if(!skateReady){skateReady=true;initialEnergy=energy;if(mode.find("off")!=std::string::npos)QueueTestKey(h.GetWindow(),SDL_SCANCODE_V,true);}
    if(skateFrames==2)QueueTestKey(h.GetWindow(),SDL_SCANCODE_V,false);
    minSpeed=std::min(minSpeed,double(glm::length(v)));maxProgress=std::max(maxProgress,double(t.position.x));finalEnergy=energy;
    for(auto& contact:w.Physics().LastStepContacts())maxPen=std::max(maxPen,double(contact.penetration));
    for(auto& event:w.Physics().LastStepTouchEvents())if(event.impulseAvailable)maxImpulse=std::max(maxImpulse,double(event.normalImpulse));
    if(skateFrames==240){bool found=false;for(auto& state:w.Scripts()->Capture())if(state.entity==200){found=state.json.find(mode.find("off")!=std::string::npos?"\"assist\":false":"\"assist\":true")!=std::string::npos;}
     check(found,"comparison changes only JS seam-transport toggle");check(maxProgress>0,"ordinary four-ray skate chassis reaches curved approach");
     std::printf("SKATE mode=%s min_speed=%g max_progress=%g max_impulse=%g penetration=%g initial_energy=%g final_energy=%g\n",mode.c_str(),minSpeed,maxProgress,maxImpulse,maxPen,initialEnergy,finalEnergy);w.UI().RequestQuit();
    }
   }
   if(frame>=500){check(false,"skate scene startup timeout");w.UI().RequestQuit();}return;
  }
  if(frame==100&&mode=="probe"){
    const auto path=w.Navigation().FindPath({9,.1,1},{9,.1,-6});check(path.status==NavigationPath::Status::Complete&&path.distance<8,"navigation routes through physical doorway opening");
    const auto wall=w.Navigation().Raycast({6,.1,1},{6,.1,-6});check(wall&&wall->position.z>-4,"navigation is blocked by actual doorway material");
    bool open=false,blocked=false;for(auto& e:w.AudioEmitters()){if(e.id==461)open=e.voice.IsValid()&&e.obstruction==0;if(e.id==462)blocked=e.voice.IsValid()&&e.obstruction==1;}
    check(open&&blocked,"actual spatial audio obstruction distinguishes opening from wall");
    auto drape=w.RuntimeDeformable(400,error),soft=w.RuntimeDeformable(401,error),fragment=w.RuntimeDeformable(402,error);
    check(drape&&drape->error.empty()&&drape->Minimum().y>-.1,"current cloth drape remains healthy on curve");
    check(soft&&soft->error.empty()&&soft->Minimum().y>-.1,"current soft body remains healthy on curve");
    check(fragment&&fragment->error.empty()&&fragment->asset->fracture->rigid,"M63 ordinary rigid fracture cells remain healthy on mesh");
    for(auto& state:w.Scripts()->Capture())if(state.entity==11){check(state.json.find("\"inspectedHull\":true")!=std::string::npos,"JS collider snapshot exposes real hull identity");check(state.json.find("\"closest\":true")!=std::string::npos,"JS closestPoint returns safe physical feature data");check(state.json.find("\"fragmentReleased\":true")!=std::string::npos,"public JS releases an ordinary fracture interface on the mesh");}
  }
  if(frame==20){baselineBodies=w.Physics().DynamicBodyCount();Shape mesh,hull,compound;BodyTransform pose;check(w.Physics().GetBodyShape(w.RuntimeBody(100),mesh,pose)&&mesh.asset&&!mesh.asset->convex,"CPU collision asset published as real static mesh");check(w.Physics().GetBodyShape(w.RuntimeBody(200),hull,pose)&&hull.asset&&hull.asset->convex,"dynamic hull resource sharing / geometry");check(w.Physics().GetBodyShape(w.RuntimeBody(201),compound,pose)&&compound.boxes.size()==3&&compound.boxes[0].key==17,"normal compound independent rotations/types/keys");
   check(!w.Physics().Raycast({9,2,4},{0,0,-1},9).hit,"doorway opening stays open to ordinary audio/physics ray");auto wall=w.Physics().Raycast({6,2,-1},{0,0,-1},5);check(wall.hit&&wall.body.id==w.RuntimeBody(101).id,"doorway material obstructs ordinary audio/physics ray");
   if(mode=="write"){w.Physics().SetLinearVelocity(w.RuntimeBody(200),{1.25,0,.5});frozen=true;request=w.SceneControl()->Saves(h.Resources())->Request("save","collision-current","M64 cold fixture","{}",error);check(request!=0,"normal M61 save queued");}
   if(mode=="read"){SaveStorage store(w.SceneControl()->Saves(h.Resources())->Root());bool recovered=false;check(store.Read("collision-current",expected,recovered,error),"new process reads M61 storage");expected.metadata=w.RuntimeDefinition(200)->body->collisionAsset;request=w.SceneControl()->Saves(h.Resources())->Request("load","collision-current","","{}",error);check(request!=0,"normal M61 cold load queued");}
   if(mode=="stream"){region=w.SceneControl()->Streaming(w,error)->Request("annex",false,error);check(region!=0,"normal region prepare requested");}
   if(mode=="probe"){QueueTestKey(h.GetWindow(),SDL_SCANCODE_P,true);before=w.RuntimeDefinition(10)->transform.position;QueueTestKey(h.GetWindow(),SDL_SCANCODE_W,true);}
  }
  if(frame==21){QueueTestKey(h.GetWindow(),SDL_SCANCODE_P,false);}
  if(mode=="probe"&&frame==35)QueueTestKey(h.GetWindow(),SDL_SCANCODE_C,true);
  if(mode=="probe"&&frame==36)QueueTestKey(h.GetWindow(),SDL_SCANCODE_C,false);
  if(mode=="probe"&&frame==80){QueueTestKey(h.GetWindow(),SDL_SCANCODE_W,false);check(glm::length(w.RuntimeDefinition(10)->transform.position-before)>2,"JS + CharacterMotor moves in normal lab");check(w.Physics().DynamicBodyCount()>baselineBodies,"normal prefab spawns cooked dynamic body");QueueTestKey(h.GetWindow(),SDL_SCANCODE_ESCAPE,true);}
  if(mode=="probe"&&frame==81){QueueTestKey(h.GetWindow(),SDL_SCANCODE_ESCAPE,false);check(play.IsPaused(),"authored UI pause owns input/time");QueueTestKey(h.GetWindow(),SDL_SCANCODE_ESCAPE,true);}
  if(mode=="probe"&&frame==82){QueueTestKey(h.GetWindow(),SDL_SCANCODE_ESCAPE,false);check(!play.IsPaused(),"authored UI resumes");}
  if(mode=="stream"){auto* stream=w.SceneControl()->Streaming(w,error);if(region&&stream->Status(region)->state=="active"&&streamPhase==0){streamPhase=1;auto staticId=stream->Resolve("annex",1);adopted=stream->Resolve("annex",2);oldRegion=w.RuntimeBody(staticId);check(oldRegion.IsValid()&&adopted,"budgeted activation publishes actual mesh and compound");check(stream->Adopt(adopted,"root",error),"travelling compound adopts without double region transform");stream->Release(region);region=0;}
   if(streamPhase==1&&adopted&&!region&&stream->Regions().front().state=="unloaded"){check(!w.Physics().IsBodyEnabled(oldRegion)&&w.RuntimeBody(adopted).IsValid(),"unload retires mesh while adopted compound remains");region=stream->Request("annex",false,error);streamPhase=2;}
   if(frame>=220){check(stream->Regions().front().state=="active","region revisit recreates real geometry without ghost loss");w.UI().RequestQuit();}
  }
  if(mode=="write"&&request){auto status=w.SceneControl()->Saves(h.Resources())->Status(request);if(status&&status->state=="failed"){check(false,"save: "+status->error);w.UI().RequestQuit();}if(status&&status->state=="completed"){check(true,"M61 save completed in source process");w.UI().RequestQuit();}}
  if(mode=="read"&&restored&&frame>=60){frozen=false;if(frame>=65){check(w.Physics().GetTransform(w.RuntimeBody(200)).position.y>=-.7,"restored dynamic hull continues contacting ordinary floor");w.UI().RequestQuit();}}
  if(mode=="probe"&&frame==140){std::vector<unsigned char> pixels;h.GetRenderer().CaptureFrame(h.GetWindow().Width(),h.GetWindow().Height(),pixels);check(WriteRgbPng(out+"/lab.png",h.GetWindow().Width(),h.GetWindow().Height(),pixels),"actual GL collision lab screenshot");QueueTestKey(h.GetWindow(),SDL_SCANCODE_F9,true);}
  if(mode=="probe"&&frame==141)QueueTestKey(h.GetWindow(),SDL_SCANCODE_F9,false);
  if(mode=="probe"&&frame==180){check(w.Physics().DynamicBodyCount()==baselineBodies,"scene reload clears runtime hull prefab state");w.UI().RequestQuit();}
  if(frame>=500){check(false,"application check bounded timeout");w.UI().RequestQuit();}
  if(mode=="stream"||mode=="write"||mode=="read")std::this_thread::sleep_for(std::chrono::milliseconds(1));
 };
 control.beforeShutdown=[&](EngineHost&,RuntimeWorld& w,InteractivePlay&){check(w.Physics().AliveBodyCount()<100,"mesh is one body, not one body per triangle");check(PerformanceProfiler::Get().Export(out+"/profile.json",error),"normal M56 profiling export");};
 std::string executable="judas";char* args[]={executable.data(),argv[1]};Application app;int status=app.Run(2,args,&control);check(status==0,"ordinary application clean shutdown");std::printf("SUMMARY %u checks %u failures\n",checks,failures);return failures?1:status;
}
