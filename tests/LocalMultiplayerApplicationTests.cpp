#include "Application.h"
#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "InteractivePlay.h"
#include "ScriptSystem.h"
#include "SceneSession.h"
#include "SaveService.h"
#include "ScreenshotWriter.h"
#include "../third_party/nlohmann/json.hpp"
#include <cstdio>
#include <filesystem>
using J=nlohmann::json;
int checks=0,failures=0;void Check(bool ok,const char* label){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",label);fflush(stdout);}
J State(RuntimeWorld& w){if(w.Scripts())for(auto& s:w.Scripts()->Capture())if(s.entity==950)return J::parse(s.json);return J::object();}
int main(){std::filesystem::create_directories(".cache/m89/application");Application app;ApplicationControl c;c.hidden=true;c.frameSeconds=[](float){return 1.f/60;};unsigned f=0;uint64_t a=0,b=0,old=0,scored=0,recoveredPlayer=0,recoveredBody=0;float az=0,bz=0,stopA=0,moveB=0;RuntimeWorld* original=nullptr;bool reloaded=false,saved=false,restored=false,reloadRequested=false;uint64_t request=0;unsigned reloadFrame=0;std::vector<EntityId> savedBots;std::map<EntityId,glm::vec3> savedPositions;J savedState;std::string pad;
 c.hostReady=[](EngineHost& h){h.GetWindow().SetTestInputMode(true);SDL_GL_SetSwapInterval(0);std::string e;h.Audio().Init(e,true);};
 c.beforeFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay& play){++f;auto& window=h.GetWindow();auto* players=w.PlayersIfLoaded();auto key=[&](const char* name,float value){window.QueueTestDevice("keyboard",std::string("key:")+name,value);};
 if(f==8&&players)pad=players->devices->Connect("gamepad","M89 game policy fixture");
 if(f==10){Check(players&&!pad.empty()&&players->All().begin()->second.devices.front().device==pad,"new gamepad becomes an independent game seat");window.QueueTestPhysical("key:Escape",1);}
 if(f==11)window.QueueTestPhysical("key:Escape",0);
 if(f==13){auto* l=w.UI().LayoutOf(w.UI().Find("scramble"),"input0");Check(l&&l->visible,"shared lobby exposes device recovery control");if(l){auto center=l->rect.position+l->rect.size*.5f;window.SetTestPointer(int(center.x),int(center.y));window.QueueTestPhysical("mouse:Left",1);}}
 if(f==14)window.QueueTestPhysical("mouse:Left",0);
 if(f==16){const auto& routes=players->All().begin()->second.devices;Check(std::none_of(routes.begin(),routes.end(),[&](const auto& d){return d.device==pad;}),"explicit device release is not immediately stolen back by autojoin");window.QueueTestPhysical("key:Escape",1);}
 if(f==17)window.QueueTestPhysical("key:Escape",0);
 if(f==20){Check(players&&players->All().size()==2,"ordinary project creates two independent local identities");if(!players){w.UI().RequestQuit();return;}auto it=players->All().begin();a=it->first;b=(++it)->first;old=a;Check(players->Find(a)->entity!=players->Find(b)->entity,"identities bind distinct actual bodies");az=w.Physics().GetTransform(w.RuntimeBody(players->Find(a)->entity)).position.z;bz=w.Physics().GetTransform(w.RuntimeBody(players->Find(b)->entity)).position.z;Check(w.QueryEntities(CategoryBit(1)).size()==5,"five real physical core bodies");key("W",1);key("Up",1);}
 if(f==55){Check(w.Physics().GetTransform(w.RuntimeBody(players->Find(a)->entity)).position.z<az-.7f&&w.Physics().GetTransform(w.RuntimeBody(players->Find(b)->entity)).position.z<bz-.7f,"two keyboard partitions independently drive native rigid bodies");key("W",0);key("Up",0);}
 if(f==65){auto id=w.QueryEntities(CategoryBit(1)).front();const auto p=w.Physics().GetTransform(w.RuntimeBody(players->Find(a)->entity)).position;w.SetRuntimeTransform(id,{{p.x+1.8f,p.y,p.z},{1,0,0,0},{1,1,1}});w.Physics().SetLinearVelocity(w.RuntimeBody(id),{0,0,0});scored=id;key("Space",1);}
 if(f==66)key("Space",0);
 if(f==68)Check(w.Physics().GetLinearVelocity(w.RuntimeBody(scored)).x>1,"per-player action applies real physical shove impulse");
 if(f==72){w.SetRuntimeTransform(scored,{{-8,.7f,-8},{1,0,0,0},{1,1,1}});w.Physics().SetLinearVelocity(w.RuntimeBody(scored),{0,0,0});}
 if(f==76){Check(State(w).at("scores")[0]==1,"actual core entering goal changes project score");Check(!w.RuntimeBody(scored).IsValid()&&w.QueryEntities(CategoryBit(1)).size()==5,"scored core retires collision and a distinct core replaces it");key("Tab",1);}
 if(f==78)key("Tab",0);
 if(f==80){Check(w.UI().Paused(a)&&!play.IsPaused()&&!w.UI().Paused(b),"local pause owns one player's controls without pausing arena");stopA=w.Physics().GetTransform(w.RuntimeBody(players->Find(a)->entity)).position.z;moveB=w.Physics().GetTransform(w.RuntimeBody(players->Find(b)->entity)).position.z;key("W",1);key("Up",1);}
 if(f==110){const auto da=std::abs(w.Physics().GetTransform(w.RuntimeBody(players->Find(a)->entity)).position.z-stopA),db=std::abs(w.Physics().GetTransform(w.RuntimeBody(players->Find(b)->entity)).position.z-moveB);Check(da<.6f&&db>.8f,"local modal suppresses only owner gameplay; other player keeps moving");key("W",0);key("Up",0);key("Tab",1);}
 if(f==112)key("Tab",0);
 if(f==115)Check(!w.UI().Paused(a),"owner Back releases local focus without stuck input");
 if(f==120)window.QueueTestPhysical("key:V",1);
 if(f==122)window.QueueTestPhysical("key:V",0);
 if(f==130){Check(players->HasViews()&&players->All().size()==2&&players->Find(a)&&players->Find(b),"split view preserves exact player/body identities");Check(h.GetRenderer().RenderTargetDiagnostics().liveTargets>=2,"independent views own real render targets");}
 if(f==140)window.QueueTestPhysical("key:F4",1);
 if(f==142)window.QueueTestPhysical("key:F4",0);
 if(f==155){Check(players->All().size()==4&&w.QueryEntities(CategoryBit(0)).size()==4,"runtime joining expands to four real bodies");Check(State(w).at("scores")[0]==1,"camera/roster changes preserve score");}
 if(f==160)key("Tab",1);
 if(f==162)key("Tab",0);
 if(f==165)key("S",1);
 if(f==167)key("S",0);
 if(f==170)key("S",1);
 if(f==172)key("S",0);
 if(f==175)key("Space",1);
 if(f==177)key("Space",0);
 if(f==185){Check(!players->Find(old)&&players->All().size()==3,"actual owner UI navigation leaves safely and retires identity");Check(w.QueryEntities(CategoryBit(0)).size()==3,"project leave retires only that controlled body");window.QueueTestPhysical("key:F4",1);}
 if(f==187)window.QueueTestPhysical("key:F4",0);
 if(f==190){recoveredPlayer=players->All().begin()->first;recoveredBody=players->Find(recoveredPlayer)->entity;w.SetRuntimeTransform(recoveredBody,{{0,-8,0},{1,0,0,0},{1,1,1}});}
 if(f==195)Check(players->Find(recoveredPlayer)&&players->Find(recoveredPlayer)->entity!=recoveredBody&&!w.RuntimeBody(recoveredBody).IsValid(),"game respawn changes physical body while preserving player identity");
 if(f==200){Check(players->All().size()==4&&!players->Find(old),"rejoin cannot revive stale player token");Check(w.Scripts()->Diagnostics().empty(),"no faults through movement/UI/join/leave and device rerouting");original=&w;if(auto* doc=w.UI().Document(w.UI().Find("scramble")))doc->modal=true;savedState=State(w);savedBots=w.QueryEntities(CategoryBit(0));for(auto id:savedBots)savedPositions[id]=w.Physics().GetTransform(w.RuntimeBody(id)).position;std::string error;request=w.SceneControl()->Saves(*w.Resources())->Request("save","m89-round","M89 round","{}",error);Check(request!=0,"normal M61 save captures multiplayer consumer state");}
 if(f>200&&request&&!saved){SDL_Delay(1);const auto* status=w.SceneControl()->Saves(*w.Resources())->Status(request);if(status&&status->state=="completed"){saved=true;std::string error;request=w.SceneControl()->Saves(*w.Resources())->Request("load","m89-round","","{}",error);Check(request!=0,"normal M61 restore requested");}}
 if(saved&&!restored&&(&w)!=original){if(auto* doc=w.UI().Document(w.UI().Find("scramble")))doc->modal=true;if(players&&players->All().size()==4){restored=true;Check(State(w).at("scores")==savedState.at("scores")&&w.QueryEntities(CategoryBit(0))==savedBots,"new-world restore rebinds actual saved bodies and project score without duplicates");bool same=true;for(auto& pair:savedPositions)same&=glm::distance(pair.second,w.Physics().GetTransform(w.RuntimeBody(pair.first)).position)<.001f;Check(same,"restored physical endpoints preserved; identities do not respawn saved actors");Check(!players->Find(old),"restoration cannot revive old player handles");original=&w;window.QueueTestPhysical("key:Backspace",1);reloadRequested=true;reloadFrame=f;}}
 if(reloadRequested&&f==reloadFrame+2)window.QueueTestPhysical("key:Backspace",0);
 if(reloadRequested&&f>reloadFrame+3&&!reloaded&&(&w)!=original&&players){reloaded=true;Check(players->All().size()==2&&!players->Find(old),"scene reload builds new roster without stale identities");Check(w.QueryEntities(CategoryBit(0)).size()==2&&w.QueryEntities(CategoryBit(1)).size()==5,"scene reload cleans actual player/core body population");}

 if(f>300&&State(w).value("finished",false)){Check(reloaded&&w.UI().Paused(),"full native round reaches scored end screen after reload");Check(w.Scripts()->Diagnostics().empty(),"no script faults through completed round");w.UI().RequestQuit();}
 if(f>9000){Check(false,"bounded round completion");w.UI().RequestQuit();}
 };
 c.afterFrame=[&](EngineHost& h,RuntimeWorld&,InteractivePlay&){h.Audio().AdvanceWithoutDevice(800);
 if(f==135||f==158){std::vector<unsigned char> pixels;h.GetRenderer().CaptureFrame(h.GetWindow().Width(),h.GetWindow().Height(),pixels);Check(WriteRgbPng(f==135?".cache/m89/application/two.png":".cache/m89/application/four.png",h.GetWindow().Width(),h.GetWindow().Height(),pixels),"capture actual player view composition");}};
 char executable[]="judas",project[]="projects/core_scramble/core_scramble.judasproj";char* args[]={executable,project};Check(app.Run(2,args,&c)==0,"ordinary application shuts down safely");std::printf("%d checks %d failures\n",checks,failures);return failures?1:0;}
