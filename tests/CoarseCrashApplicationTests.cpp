#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "ScriptSystem.h"
#include "WorldPersistence.h"
#include "SaveService.h"
#include "WorldStreaming.h"
#include "SaveArchive.h"
#include "ScreenshotWriter.h"
#include "PerformanceProfiler.h"
#include <SDL2/SDL.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <map>
#include <algorithm>
#include <cstdio>
namespace fs=std::filesystem;
unsigned checks=0,failures=0;void check(bool p,const char* l){++checks;failures+=!p;std::printf("%s %s\n",p?"PASS":"FAIL",l);std::fflush(stdout);}
Window* window=nullptr;void key(SDL_Scancode k,bool on){window->QueueTestPhysical(std::string("key:")+SDL_GetScancodeName(k),on?1.f:0.f);}
std::string physical(RuntimeWorld& w,EntityId owner){SaveArchive a;std::string error;auto* m=w.RuntimeDeformable(owner+20,error);if(!m)return {};a(m->positions,m->velocities,m->fracture.plasticTranslation,m->fracture.plasticRotation,m->fracture.plasticWork,m->fracture.broken);auto body=[&](EntityId id){auto h=w.RuntimeBody(id);auto t=w.Physics().GetTransform(h);auto v=w.Physics().GetLinearVelocity(h),o=w.Physics().GetAngularVelocity(h);float mass=w.Physics().GetMass(h);a(t.position,t.rotation,v,o,mass);};body(owner);for(unsigned p=0;p<m->asset->fracture->parts.size();++p)body(w.FracturePartEntity(owner+20,p,true));for(unsigned i=1;i<=4;++i){auto* wheel=w.RuntimeWheel(owner+i);wheel->Persist(a);}auto* train=w.RuntimePowertrain(owner);train->Persist(a);return a.bytes;}
struct Metrics {std::vector<double> physics,frame,render,native;};
void report(const char* stage,Metrics& m){auto pct=[](std::vector<double> v,double p){std::sort(v.begin(),v.end());return v.empty()?0:v[size_t(p*(v.size()-1))];};std::printf("PERF stage=%s n=%zu physics_p95_ms=%.6f physics_max_ms=%.6f native_p95_ms=%.6f frame_p95_ms=%.6f frame_max_ms=%.6f render_p95_ms=%.6f\n",stage,m.physics.size(),pct(m.physics,.95),pct(m.physics,1),pct(m.native,.95),pct(m.frame,.95),pct(m.frame,1),pct(m.render,.95));}
int main(int argc,char** argv){if(argc<5){std::fprintf(stderr,"project case watch|write|read|lifecycle|stream output [expected physical file]\n");return 2;}
 auto project=fs::absolute(argv[1]);std::string scenario=argv[2],mode=argv[3],error;auto out=fs::absolute(argv[4]);fs::create_directories(out);
 const bool read=mode=="read",write=mode=="write";bool frozen=read,complete=false,started=false,healthy=true,press=false;unsigned frame=0,tick=0,phase=0,firstContact=0;RuntimeWorld* initial=nullptr;uint64_t request=0,region=0;EntityId annex=0;std::string expected;
 if(read&&argc>5){std::ifstream in(argv[5],std::ios::binary);expected.assign(std::istreambuf_iterator<char>(in),{});}
 auto scene=(project.parent_path()/"Scenes"/(scenario+".judas")).string();std::chrono::steady_clock::time_point begin;std::map<std::string,Metrics> metrics;
 std::vector<double> peaks(scenario.rfind("group",0)==0?4:scenario=="comparison"?3:1),before(peaks.size()),speed(peaks.size());std::vector<unsigned> broken(peaks.size());std::vector<double> left(peaks.size()),right(peaks.size());bool yieldedBeforeFailure=true;std::map<std::string,std::vector<double>> stages;
 ApplicationControl control;control.hidden=true;control.frameSeconds=[&](float){return frozen?0.f:1.f/60;};control.worldReady=[&](EngineHost&,RuntimeWorld& w,InteractivePlay&){initial=&w;};control.hostReady=[&](EngineHost& h){window=&h.GetWindow();window->SetTestInputMode(true);if(mode=="profile")PerformanceProfiler::Get().Enable(true);};
 control.beforeFrame=[&](EngineHost&,RuntimeWorld& w,InteractivePlay&){begin=std::chrono::steady_clock::now();if(started&&!firstContact)for(unsigned c=0;c<speed.size();++c){auto q=w.RuntimeDefinition(1)->transform.rotation;auto v=glm::inverse(q)*w.Physics().GetLinearVelocity(w.RuntimeBody(900+300*c));speed[c]=-v.z;}};
 control.afterFrame=[&](EngineHost& host,RuntimeWorld& w,InteractivePlay& play){++frame;
  auto shot=[&](const char* name){std::vector<unsigned char> pixels;host.GetRenderer().CaptureFrame(host.GetWindow().Width(),host.GetWindow().Height(),pixels);check(WriteRgbPng((out/name).string(),host.GetWindow().Width(),host.GetWindow().Height(),pixels),"rendered physical scene screenshot captured");};
  if(frame>4000){check(false,"bounded application deadline");w.UI().RequestQuit();complete=true;return;}
  if(w.Scripts()&&!w.Scripts()->Diagnostics().empty()){for(const auto& d:w.Scripts()->Diagnostics())std::printf("SCRIPT %s\n",d.message.c_str());healthy=false;check(false,"ordinary driver JS has no diagnostic");w.UI().RequestQuit();complete=true;return;}
  if(read&&phase==0&&frame==30){request=w.SceneControl()->Saves(host.Resources())->Request("load","physical-crash","","{}",error);check(request!=0,"fresh process requests ordinary saved game");phase=1;}
  if(read&&phase==1){auto* status=w.SceneControl()->Saves(host.Resources())->Status(request);if(status&&status->state=="failed"){std::printf("SAVE_ERROR %s\n",status->error.c_str());check(false,"cold load succeeds");w.UI().RequestQuit();complete=true;return;}
   if(status&&status->state=="completed"){check(&w!=initial&&play.FixedStepsSinceReset()==0,"fresh process restores before physical advancement");check(!expected.empty()&&physical(w,900)==expected,"exact physical poses, plastic history, wheels and shafts restore");frozen=false;phase=2;tick=0;started=true;shot("restored.png");}}
  if(scenario=="specimen"){
   auto* beam=w.RuntimeDeformable(520,error);auto body=w.RuntimeBody(w.FracturePartEntity(520,0));if(!beam||!body.IsValid()){std::this_thread::sleep_for(std::chrono::milliseconds(1));return;}
   static glm::vec3 initialPoint;static double elasticPeak=0,earlyPlastic=0,recovery=1;
   if(!started){started=true;initialPoint=w.Physics().GetTransform(body).position;shot("before.png");}
   ++tick;if(tick==90)key(SDL_SCANCODE_V,true);if(tick==91)key(SDL_SCANCODE_V,false);
   double t=(double(tick)-90)/60,plastic=0;for(auto v:beam->fracture.plasticRotation)plastic=std::max(plastic,double(glm::length(v)));for(auto v:beam->fracture.plasticTranslation)plastic=std::max(plastic,double(glm::length(v)));
   auto displacement=glm::length(w.Physics().GetTransform(body).position-initialPoint);if(t>=2&&t<4)elasticPeak=std::max(elasticPeak,double(displacement));if(t<8)earlyPlastic=std::max(earlyPlastic,plastic);if(t>6&&t<7)recovery=std::min(recovery,double(displacement));
   if(t>=22){auto broken=std::count_if(beam->fracture.broken.begin(),beam->fracture.broken.end(),[](auto b){return b!=0;});std::printf("SPECIMEN elastic_peak_m=%.9f unloaded_m=%.9f early_plastic=%.9f final_plastic=%.9f broken=%zu\n",elasticPeak,recovery,earlyPlastic,plastic,size_t(broken));check(elasticPeak>.001&&earlyPlastic<1e-5&&recovery<.005,"generic force-loaded beam bends elastically and returns without permanent strain");check(plastic>.02&&broken>0&&beam->error.empty(),"generic sustained overload retains physical plastic strain and eventually fails");shot("after.png");complete=true;w.UI().RequestQuit();}return;
  }
  auto* m=w.RuntimeDeformable(920,error);bool ready=m&&m->asset->fracture&&m->asset->fracture->HasMaterials()&&w.FracturePartEntity(920,3)&&w.RuntimeWheel(901)&&w.RuntimeWheel(901)->state.available;
  for(unsigned c=0;c<peaks.size();++c){auto owner=EntityId(900+300*c);ready&=w.FracturePartEntity(owner+20,3)!=0&&w.RuntimeWheel(owner+1)&&w.RuntimeWheel(owner+1)->state.available;}
  if(!ready){std::this_thread::sleep_for(std::chrono::milliseconds(1));return;}
  healthy&=m->error.empty()&&m->fracture.error.empty();
  if(!started&&!read){started=true;if(mode=="support-control"){for(unsigned p:{4u,5u})w.Physics().SetPairCollisionEnabled(w.RuntimeBody(900),w.RuntimeBody(w.FracturePartEntity(920,p)),false);}
   if(mode=="control"){std::vector<BodyHandle> group{w.RuntimeBody(900)};for(unsigned p=0;p<6;++p)group.push_back(w.RuntimeBody(w.FracturePartEntity(920,p)));for(unsigned a=0;a<group.size();++a)for(unsigned b=a+1;b<group.size();++b)w.Physics().SetPairCollisionEnabled(group[a],group[b],false);}check(w.MechanicalSubsteps()==2&&w.MechanicalIterations()==12,"current authored2 physical intervals and12 native rounds at60Hz");check(m->asset->fracture->parts.size()==4&&w.Physics().IsDynamicBody(w.RuntimeBody(w.WheelChassisEntity(w.RuntimeWheel(901)->settings))),"actual four-section geometry supplies the front suspension body");shot("before.png");}
  if(!started)return;
  ++tick;
  if(!read&&mode!="stream"&&mode!="rest"&&tick==90){key(SDL_SCANCODE_V,true);press=true;}if(press&&tick==91){key(SDL_SCANCODE_V,false);press=false;}
  if(read&&tick==120){check(healthy&&play.FixedStepsSinceReset()>0,"cold physical material and mechanical continuation stays healthy");complete=true;w.UI().RequestQuit();return;}
  if(mode=="stream"){
   auto* stream=w.SceneControl()->Streaming(w,error);if(!stream){std::printf("STREAM_ERROR %s\n",error.c_str());check(false,"ordinary world manifest loads");complete=true;w.UI().RequestQuit();return;}
   if(phase==0&&tick==90){region=stream->Request("annex",false,error);phase=1;}
   auto status=stream->Status(region);if(status&&status->state=="failed"){std::printf("STREAM_ERROR %s\n",status->error.c_str());check(false,"retained physical region restoration succeeds");complete=true;w.UI().RequestQuit();return;}
   if(phase==1&&status&&status->state=="active"){annex=stream->Resolve("annex",900);auto material=stream->Resolve("annex",920);std::string e;auto* d=w.RuntimeDeformable(material,e);auto wheel=stream->Resolve("annex",901);if(d&&w.FracturePartEntity(material,0)&&w.RuntimeWheel(wheel)->state.available){check(w.WheelChassisEntity(w.RuntimeWheel(wheel)->settings)==w.FracturePartEntity(material,0),"streamed part-mounted wheel resolves the remapped physical section");w.LoadFracturePart(material,0,{0,0,-400},true);phase=2;tick=0;}}
   if(phase==2&&tick==180){expected=physical(w,annex);frozen=true;stream->Release(region);check(stream->Unload("annex"),"complete physical section family can suspend after demand release");phase=3;}
   if(phase==3){auto list=stream->Regions();auto i=std::find_if(list.begin(),list.end(),[](const auto& s){return s.id=="annex";});if(i!=list.end()&&i->state=="unloaded"){check(!w.RuntimeDefinition(annex),"suspension destroys old physical family");region=stream->Request("annex",false,error);phase=4;}}
   if(phase==4&&status&&status->state=="active"){auto restored=stream->Resolve("annex",900);check(restored!=annex&&physical(w,restored)==expected,"retained geometry, plastic state and drivetrain restore under fresh identities");frozen=false;phase=5;tick=0;}
   if(phase==5&&tick==120){check(healthy,"re-published physical assembly continues normally");complete=true;w.UI().RequestQuit();}return;
  }
  if(mode=="lifecycle"){
   if(tick==150){key(SDL_SCANCODE_P,true);}if(tick==151)key(SDL_SCANCODE_P,false);
   if(tick==210){unsigned owners=0;for(const auto& o:w.ScriptObjects(false))owners+=bool(o.deformable);check(owners==2,"ordinary prefab runtime spawn independently publishes physical assembly");auto* front=w.RuntimeWheel(901);auto wrong=front->settings;wrong.part="absent";check(w.WheelChassisEntity(wrong)==0,"missing part does not alias another physical body");key(SDL_SCANCODE_R,true);phase=1;initial=&w;}
   if(phase==1&&&w!=initial){check(m->fracture.revision==1&&glm::length(m->fracture.plasticTranslation[0])==0,"normal reload removes old families and restores intact authored material");complete=true;w.UI().RequestQuit();}return;
  }
  bool hit=false;for(unsigned c=0;c<peaks.size();++c){auto owner=EntityId(900+300*c);auto* d=w.RuntimeDeformable(owner+20,error);if(!d)continue;double mass=w.Physics().GetMass(w.RuntimeBody(owner));for(unsigned p=0;p<d->asset->fracture->parts.size();++p){auto entity=w.FracturePartEntity(owner+20,p,true);auto body=w.RuntimeBody(entity);mass+=w.Physics().GetMass(body);std::vector<PhysicsWorld::BodyContactSample> contacts;w.Physics().ReadBodyContactSample(body,contacts);for(const auto& sample:contacts)if(sample.other.id==w.RuntimeBody(5000).id&&sample.normalImpulse>0&&scenario!="normal")hit=true;}
   healthy&=std::abs(mass-576)<.001&&d->error.empty();
   double plastic=0;for(unsigned b=0;b<d->fracture.broken.size();++b){plastic=std::max(plastic,double(glm::length(d->fracture.plasticTranslation[b])));if(d->fracture.broken[b]&&broken[c]==0)yieldedBeforeFailure&=d->fracture.plasticWork[b]>0&&(glm::length(d->fracture.plasticTranslation[b])+glm::length(d->fracture.plasticRotation[b])>0);}
   peaks[c]=std::max(peaks[c],plastic);broken[c]=std::count_if(d->fracture.broken.begin(),d->fracture.broken.end(),[](auto b){return b!=0;});
   // Approach speed was sampled before this exact physical macro boundary.
   for(unsigned b=0;b<d->asset->fracture->bonds.size();++b){const auto& bond=d->asset->fracture->bonds[b];if(bond.normal.z>.5){auto& value=bond.a%2?right[c]:left[c];value=std::max(value,double(glm::length(d->fracture.plasticTranslation[b])));}}
  }
  if(mode=="profile"&&firstContact&&tick==firstContact+20){std::string e;check(PerformanceProfiler::Get().Export((out/"impact-profile.json").string(),e),"bounded native profiler export");}
  if(hit&&!firstContact){firstContact=tick;std::printf("IMPACT tick=%u speed=%.6f\n",tick,speed[0]);shot("impact.png");}
  if((mode=="control"||mode=="rest"||mode=="support-control")&&tick==600){shot("diagnostic.png");complete=true;w.UI().RequestQuit();return;}
  if(tick==5||tick==20||tick==60||tick%120==0){const auto& stats=w.Physics().LastStepStats();auto p=w.Physics().GetTransform(w.RuntimeBody(900)).position;std::printf("STEP tick=%u position=%.3f,%.3f,%.3f speed=%.3f native_ms=%.3f broad_ms=%.3f narrow_ms=%.3f solver_ms=%.3f queries=%zu iterations=%zu events=%zu contacts=%zu pairs=%zu plastic=%.6f\n",tick,p.x,p.y,p.z,speed[0],stats.totalMilliseconds,stats.broadphaseMilliseconds,stats.narrowphaseMilliseconds,stats.solverMilliseconds,stats.impactQueries,stats.impactSearchIterations,stats.impactEvents,stats.contactPoints,stats.candidatePairs,peaks[0]);std::fflush(stdout);}
  if(tick>90){auto& costs=metrics[!firstContact?"approach":tick-firstContact<90?"impact":"damaged"];costs.physics.push_back(play.LastFixedStepMilliseconds());costs.native.push_back(w.Physics().LastStepStats().totalMilliseconds);costs.render.push_back(play.LastSceneMilliseconds());costs.frame.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());}
  const bool finish=scenario=="normal"?tick>=900:firstContact&&tick-firstContact>=360;
  if(finish&&phase==0){check(healthy,"all current physical owners remain finite and conserve the authored576kg per car");
   for(unsigned c=0;c<peaks.size();++c)std::printf("CAR index=%u speed_mps=%.6f plastic_translation_m=%.9f broken=%u left=%.9f right=%.9f\n",c,speed[c],peaks[c],broken[c],left[c],right[c]);
   if(scenario=="normal")check(peaks[0]<1e-5&&broken[0]==0,"normal drive and uneven-road loading do not permanently yield");
   else{double target=scenario=="low"?5:scenario.find("high")!=std::string::npos?25:15,tolerance=scenario=="low"?.5:scenario.find("high")!=std::string::npos?1:.75;check(firstContact>0&&std::abs(speed[0]-target)<=tolerance,"real torque-driven vehicle reaches the declared impact range and actual wall contact");if(scenario!="low")check(peaks[0]>.02,"actual crash leaves local permanent material strain");if(scenario.find("high")!=std::string::npos)check(broken[0]>0&&yieldedBeforeFailure,"high impact accumulates physical plastic work before capacity failure");if(scenario=="comparison")check(peaks[0]>peaks[2]||broken[0]>broken[2],"same geometry stronger material changes physical crash outcome");if(scenario=="offset")check(std::abs(left[0]-right[0])>.01,"offset contact produces asymmetric physical deformation");}
   if(scenario.find("high")!=std::string::npos){for(unsigned c=0;c<peaks.size();++c){auto owner=EntityId(900+300*c);auto* train=w.RuntimePowertrain(owner);check(train&&train->state.links.size()>=2&&(!train->state.links[0].connected||!train->state.links[1].connected),"actual crash topology disconnects a front driveline link");auto* wheel=w.RuntimeWheel(owner+1);check(wheel&&w.WheelChassisEntity(wheel->settings)==w.FracturePartEntity(owner+20,0),"damaged front wheel still mounts to its actual physical section");}}
   shot("after.png");if(write){frozen=true;expected=physical(w,900);std::ofstream(out/"expected-physical.bin",std::ios::binary).write(expected.data(),expected.size());request=w.SceneControl()->Saves(host.Resources())->Request("save","physical-crash","Physical material candidate","{}",error);phase=1;check(request!=0,"ordinary save request captures damaged assembly");}else{complete=true;w.UI().RequestQuit();}}
  if(write&&phase==1){auto* status=w.SceneControl()->Saves(host.Resources())->Status(request);if(status&&status->state=="failed"){std::printf("SAVE_ERROR %s\n",status->error.c_str());check(false,"save succeeds");complete=true;w.UI().RequestQuit();}if(status&&status->state=="completed"){check(true,"ordinary durable save completes");complete=true;w.UI().RequestQuit();}}
 };
 Application app;char name[]="judas";char* args[]={name,scene.data()};check(app.Run(2,args,&control)==0&&complete,"ordinary project startup and shutdown complete");if(mode=="profile"){std::string e;PerformanceProfiler::Get().Export((out/"post-profile.json").string(),e);}for(auto& [stage,m]:metrics){report(stage.c_str(),m);if(mode=="watch"&&m.physics.size()>=90){auto p=m.physics,f=m.frame;std::sort(p.begin(),p.end());std::sort(f.begin(),f.end());check(p[size_t(.95*(p.size()-1))]<=(peaks.size()>1?2.5*peaks.size():3),"declared native host p95 fixed-step budget");check(f[size_t(.95*(f.size()-1))]<=16.67,"declared native host p95 engine frame-work budget");}}std::printf("SUMMARY case=%s mode=%s checks=%u failures=%u\n",scenario.c_str(),mode.c_str(),checks,failures);return failures?1:0;
}
