#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "SaveService.h"
#include "ScriptSystem.h"
#include "ScreenshotWriter.h"
#include "PerformanceProfiler.h"
#include "modding/ModRuntime.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <cstdio>
#include <algorithm>
using JSON=JudasMods::JSON;namespace fs=std::filesystem;
unsigned checks=0,failures=0;
void check(bool b,const char* s){++checks;failures+=!b;printf("%s %s\n",b?"PASS":"FAIL",s);fflush(stdout);}
JSON state(RuntimeWorld& w){for(const auto& s:w.Scripts()->Capture())if(s.entity==10)return JSON::parse(s.json);return JSON::object();}
int main(int argc,char** argv){if(argc<4)return 2;auto scene=fs::absolute(argv[1]).string();auto out=fs::absolute(argv[2]);fs::create_directories(out);const std::string mode=argv[3];float shelfMin=100,shelfMax=-100;unsigned tick=0,frames=0;bool complete=false,release=false,verified=false;Window* window=nullptr;RuntimeWorld* first=nullptr;std::vector<double> fixed,work;std::chrono::steady_clock::time_point begin;std::ofstream csv(out/"steps.csv");csv<<"tick,balls,awake,pairs,contacts,events,queries,broad_ms,narrow_ms,solver_ms,physics_ms,fixed_ms,frame_work_ms\n";
ApplicationControl c;c.hidden=true;c.frameSeconds=[](float){return 1.f/60;};c.hostReady=[&](EngineHost& h){window=&h.GetWindow();window->SetTestInputMode(true);};c.beforeFrame=[&](EngineHost&,RuntimeWorld&,InteractivePlay&){begin=std::chrono::steady_clock::now();};
c.afterFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay& p){++frames;if(frames>2500){check(false,"bounded startup/game scenario");complete=true;w.UI().RequestQuit();return;}if(w.Scripts()&&!w.Scripts()->Diagnostics().empty()){for(auto& d:w.Scripts()->Diagnostics())printf("SCRIPT %s\n",d.message.c_str());check(false,"scripts remain healthy");complete=true;w.UI().RequestQuit();return;}auto s=w.Scripts()?state(w):JSON();if(s.empty()){std::this_thread::sleep_for(std::chrono::milliseconds(2));return;}++tick;{const auto z=w.Physics().GetTransform(w.RuntimeBody(6)).position.z;shelfMin=std::min(shelfMin,z);shelfMax=std::max(shelfMax,z);}if(!first)first=&w;if(release){window->QueueTestPhysical("mouse:Left",0);release=false;}
 auto click=[&](const char* id){auto doc=w.UI().Find("tuppence");w.UI().Layout(window->Width(),window->Height());auto* l=w.UI().LayoutOf(doc,id);check(l&&l->visible&&l->enabled,"authored button visible/enabled");if(l&&l->visible&&l->enabled){window->SetTestPointer(int(l->rect.position.x+l->rect.size.x*.5f),int(l->rect.position.y+l->rect.size.y*.5f));window->QueueTestPhysical("mouse:Left",1);release=true;}};
 auto shot=[&](const char* name){std::vector<unsigned char> pixels;h.GetRenderer().CaptureFrame(window->Width(),window->Height(),pixels);check(WriteRgbPng((out/name).string(),window->Width(),window->Height(),pixels),"actual gameplay screenshot");};
 if(tick==20){const float scale=mode=="coins"?.55f:1.f,x=mode=="coins"?1.2f:1.58f;auto low=w.Physics().Raycast({x,2.2f,3.43f*scale},{0,-1,0},1);auto high=w.Physics().Raycast({x,2.2f,3.67f*scale},{0,-1,0},1);check(low.hit&&high.hit&&low.body.id==w.RuntimeBody(26).id&&low.body.world==w.RuntimeBody(26).world&&high.body.id==w.RuntimeBody(26).id&&high.body.world==w.RuntimeBody(26).world&&high.point.y>low.point.y+.002f&&high.normal.z<-.015f,"payout lip has actual shallow upslope collision");if(mode=="coins")check(w.RuntimeDefinition(1000)->body->shape==SceneShape::ConvexHull,"2p pieces use physical flat convex coin geometry");check(h.Assets().Problems().empty(),"all project assets registered");check(s["balls"].size()==(mode=="stress"?104:mode=="coins"?24:56),"real initial physical population");check(w.RuntimeDefinition(6)->body->motion==SceneBodyMotion::Dynamic,"pusher uses the authored native body authority");}
 if(mode=="idle"){if(tick==600){check(s.value("collected",0)==0&&s["balls"].size()==56,"untouched ball bed remains in machine for ten seconds");shot("idle.png");complete=true;w.UI().RequestQuit();}return;}
 if(tick==40){shot("ready.png");if(mode=="stress"){check(!w.UI().Element(w.UI().Find("tuppence"),"drop")->enabled,"full physical tray refuses additional paid drops");window->QueueTestPhysical("key:Space",1);}else click("drop");}
 if(tick==43&&mode=="stress")window->QueueTestPhysical("key:Space",0);
 if(tick==50){check(s.value("dropped",0)==(mode=="stress"?0:1)&&s.value("bank",0)==60-s.value("dropped",0)+s.value("score",0)/(mode=="coins"?2:10),"drop spends one credit on success and none on refusal");if(mode!="single")click("auto");}
 if(tick==70&&mode!="single")check(s.value("autoplay",false),"UI autopilot runs ordinary drop policy");
 if(tick==100)click("pause_button");
 if(tick==110){check(w.UI().Paused(),"pause owns simulation");shot("pause.png");click("resume");}
 if(tick>=120&&p.LastFixedStepsThisFrame()){
  const auto& st=w.Physics().LastStepStats();double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();fixed.push_back(p.LastFixedStepMilliseconds());work.push_back(ms);csv<<tick<<','<<s["balls"].size()<<','<<st.awakeBodies<<','<<st.candidatePairs<<','<<st.contactPoints<<','<<st.impactEvents<<','<<st.impactQueries<<','<<st.broadphaseMilliseconds<<','<<st.narrowphaseMilliseconds<<','<<st.solverMilliseconds<<','<<st.totalMilliseconds<<','<<p.LastFixedStepMilliseconds()<<','<<ms<<'\n';if(tick%60==0)csv.flush();
 }
 if(mode=="single"&&tick==850){printf("SINGLE population=%zu collected=%d dropped=%d\n",s["balls"].size(),s.value("collected",0),s.value("dropped",0));check(s["balls"].size()>=42,"one inserted ball does not clear the starting bed");complete=true;w.UI().RequestQuit();return;}
 if(tick==900){printf("SHELF min=%.6f max=%.6f\n",shelfMin,shelfMax);check(shelfMax-shelfMin>.1f,"native driven shelf actually travels under load");shot("playing.png");check(s.value("collected",0)>0&&s.value("score",0)>0,"actual falling balls reach pockets and score");check(s["balls"].size()+s.value("collected",0)+s.value("lost",0)==size_t((mode=="stress"?104:mode=="coins"?24:56)+s.value("dropped",0)),"spawn/collection ledger accounts for every real ball");check(s["balls"].size()<=104,"paid spawning respects physical cabinet capacity");
  if(mode!="stress")click("pause_button");else{verified=true;complete=true;w.UI().RequestQuit();}}
 if(mode!="stress"&&tick==910)click("save");
 if(mode!="stress"&&tick==1020){auto* r=w.SceneControl()->Saves(h.Resources())->Status(1);check(r&&r->state=="completed","real machine save completes");click("pause_button");}
 if(mode!="stress"&&tick==1030)click("load");
 if(first!=&w&&!verified){verified=true;size_t realBalls=0;for(const auto& o:w.ScriptObjects(false))if((o.tags&1)&&w.RuntimeBody(o.id).IsValid())++realBalls;check(realBalls>0&&realBalls==s["balls"].size(),"load restores exact script-referenced physical population");check(s.value("score",0)>0,"load restores earned score");}
 if(mode!="stress"&&tick==1120&&verified)click("pause_button");
 if(mode!="stress"&&tick==1130)click(mode=="coins"?"mode_balls":"mode_coins");
 if(mode!="stress"&&tick==1200&&verified){check(w.UI().Element(w.UI().Find("tuppence"),"title")->text==(mode=="coins"?"TUPPENCE":"TUPPENCE / 2p"),"mode menu loads the other ordinary scene");check(s.value("dropped",-1)==0&&!s.value("autoplay",true),"new mode resets drops and autoplay independently of settling payouts");complete=true;w.UI().RequestQuit();}
};Application app;char bin[]="judas";char* args[]={bin,scene.data()};check(app.Run(2,args,&c)==0&&complete,"ordinary startup/lifecycle/shutdown");auto pct=[](std::vector<double> a,double p){std::sort(a.begin(),a.end());return a.empty()?0:a[size_t(p*(a.size()-1))];};printf("PERF fixed_p50_ms=%.4f fixed_p95_ms=%.4f fixed_max_ms=%.4f frame_work_p95_ms=%.4f samples=%zu\n",pct(fixed,.5),pct(fixed,.95),pct(fixed,1),pct(work,.95),fixed.size());printf("SUMMARY checks=%u failures=%u\n",checks,failures);return failures?1:0;}
