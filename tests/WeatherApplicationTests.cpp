#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "SaveService.h"
#include "WorldPersistence.h"
#include "ScreenshotWriter.h"
#include "../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace {
using J=nlohmann::json;using V=glm::dvec3;
unsigned checks=0,failures=0;
void Check(bool value,const char* message){++checks;failures+=!value;std::printf("%s %s\n",value?"PASS":"FAIL",message);std::fflush(stdout);}
double Percentile(std::vector<double> v,double q){if(v.empty())return 0;std::sort(v.begin(),v.end());return v[size_t((v.size()-1)*q)];}
J State(RuntimeWorld& world){if(world.Scripts())for(const auto& r:world.Scripts()->Capture())if(r.entity==10)return J::parse(r.json);return J::object();}
EntityId Actor(const J& state){if(!state.contains("actor")||!state.at("actor").is_string())return 0;return std::stoull(state.at("actor").get<std::string>());}
struct Saved {
    J state;std::string weather;Scene scene;EntityId actor=0;
    bool Read(SaveService& saves,const std::string& slot,std::string& error){
        GameSnapshot snapshot;bool recovered=false;SaveStorage storage(saves.Root());
        if(!storage.Read(slot,snapshot,recovered,error)||!WorldPersistence::SceneFrom(snapshot.participants,scene,error))return false;
        auto w=snapshot.participants.find("weather"),s=snapshot.participants.find("scripts");if(w==snapshot.participants.end()||s==snapshot.participants.end()){error="weather/scripts participant absent";return false;}
        SaveArchive wa(w->second.data);uint32_t count=0;wa(count);if(count>16)return false;
        for(uint32_t i=0;i<count;++i){EntityId id=0;std::string record;wa(id,record);if(id==20)weather=record;}wa.Finish();
        SaveArchive sa(s->second.data);sa(count);if(count>4096)return false;
        for(uint32_t i=0;i<count;++i){ScriptStateRecord r;sa(r.entity,r.slot,r.json);if(r.entity==10)state=J::parse(r.json);}sa.Finish();
        actor=Actor(state);return !weather.empty()&&!state.empty()&&actor&&scene.Find(actor);
    }
};
}

int main(int argc,char** argv){
    if(argc<3)return 2;
    const bool orbitOnly=argc>3&&std::string(argv[3])=="orbit";
    const auto project=std::filesystem::absolute(argv[1]),output=std::filesystem::absolute(argv[2]);std::filesystem::create_directories(output);
    const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
    const std::string flatSlot="m86-app-flat-"+std::to_string(stamp),planetSlot="m86-app-planet-"+std::to_string(stamp);
    ApplicationControl control;control.hidden=true;control.frameSeconds=[](float){return 1.f/60;};
    Window* window=nullptr;unsigned frame=0,phase=0,age=0;bool done=false,awaitRestore=false,restoreObserved=false;
    std::map<std::string,unsigned> releases;std::string error;uint64_t request=0,thunderBefore=0,sequenceBefore=0;double strikeTime=0,pauseTime=0;
    RuntimeWorld* priorWorld=nullptr;Saved saved;V panelStart(0);unsigned orbitRevision=0;
    std::vector<double> fixed,render,cpu,period;std::chrono::steady_clock::time_point frameStart,previousFrameStart;
    auto phaseTo=[&](unsigned next){phase=next;age=0;std::printf("PHASE %u frame=%u\n",phase,frame);std::fflush(stdout);};
    auto tap=[&](const std::string& physical){window->QueueTestPhysical(physical,1);releases[physical]=frame+2;};
    auto replace=[&](RuntimeWorld& w,const std::function<void(J&)>& change){auto records=w.Scripts()->Capture();for(auto& r:records)if(r.entity==10){auto state=J::parse(r.json);change(state);r.json=state.dump();}Check(w.Scripts()->Restore(records,error),"explicit public script-state restoration selects an accelerated diagnostic view");};
    auto saveRequest=[&](EngineHost& h,RuntimeWorld& w,const std::string& slot){request=w.SceneControl()->Saves(h.Resources())->Request("save",slot,"M86 application check","{}",error);Check(request!=0,"ordinary save service accepts the played weather scene");};
    auto saveFinished=[&](EngineHost& h,RuntimeWorld& w,const std::string& slot){auto* service=w.SceneControl()->Saves(h.Resources());auto* status=service->Status(request);if(!status||status->state=="queued"||status->state=="working"||status->state=="capturing"||status->state=="restoring")return false;if(status->state!="completed"&&status->state!="failed")return false;Check(status->state=="completed","durable weather save completes");if(status->state!="completed"){std::printf("SAVE %s\n",status->error.c_str());done=true;w.UI().RequestQuit();return false;}Check(saved.Read(*service,slot,error),"read actual archived weather, script and physical endpoint participants");if(!error.empty())std::printf("ARCHIVE %s\n",error.c_str());priorWorld=&w;awaitRestore=true;restoreObserved=false;request=service->Request("load",slot,"","{}",error);Check(request!=0,"ordinary cold-load request accepted");return true;};
    control.hostReady=[&](EngineHost& host){window=&host.GetWindow();window->SetTestInputMode(true);};
    control.beforeFrame=[&](EngineHost&,RuntimeWorld& w,InteractivePlay&){
        frameStart=std::chrono::steady_clock::now();
        if(previousFrameStart.time_since_epoch().count())period.push_back(std::chrono::duration<double,std::milli>(frameStart-previousFrameStart).count());
        previousFrameStart=frameStart;
        for(auto it=releases.begin();it!=releases.end();)if(frame>=it->second){window->QueueTestPhysical(it->first,0);it=releases.erase(it);}else ++it;
        if(awaitRestore&&&w!=priorWorld){
            std::string current;Check(w.CaptureWeather(20,current,error)&&current==saved.weather,"fresh published world restores the exact native weather before its first resumed step");
            const auto beforeConstruction=State(w);
            if(beforeConstruction!=saved.state) {
                std::printf("RESTORE script state before normal construction boundary: expected=%s observed=%s live_instances=%zu\n",saved.state.dump().c_str(),beforeConstruction.dump().c_str(),w.Scripts()->Capture().size());
            }
            // Load publishes native state plus pending VM records. Normal
            // Synchronize constructs instances and installs those records before
            // start/restore/update/fixedUpdate. Capture enumerates live instances,
            // so querying it before this boundary is not an observation of a
            // restored script object. Construct through that public boundary;
            // do not run a callback or advance the native/weather clock here.
            const auto* restoredWeather=w.RuntimeWeather(20);
            const auto clockBefore=restoredWeather?restoredWeather->State().time:-1;
            w.Scripts()->Synchronize(w.ScriptSlots());
            const auto afterConstruction=State(w);
            if(afterConstruction!=saved.state)std::printf("RESTORE constructed script state: expected=%s observed=%s\n",saved.state.dump().c_str(),afterConstruction.dump().c_str());
            Check(afterConstruction==saved.state&&restoredWeather&&restoredWeather->State().time==clockBefore,"normal script construction restores exact archived state before any callback or clock advancement");
            auto* motor=w.RuntimeCharacter(saved.actor);const auto* expected=saved.scene.Find(saved.actor);
            Check(motor&&expected&&glm::length(motor->position-expected->transform.position)<.0002f,"cold save restores the actual CharacterMotor physical endpoint");
            if(auto* expectedPanel=saved.scene.Find(115)){EntityPhysicalState state;Check(w.GetEntityState(115,state)&&glm::length(state.position-expectedPanel->transform.position)<.0002f,"cold save restores the actual wind-driven rigid body endpoint");}
            awaitRestore=false;restoreObserved=true;
        }
    };
    control.afterFrame=[&](EngineHost& host,RuntimeWorld& w,InteractivePlay& play){
        ++frame;++age;fixed.push_back(play.LastFixedStepMilliseconds());render.push_back(play.LastSceneMilliseconds());cpu.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frameStart).count());
        if(frame>5000){Check(false,"bounded weather application deadline");done=true;w.UI().RequestQuit();return;}
        if(!w.Scripts())return;
        if(!w.Scripts()->Diagnostics().empty()){for(const auto& d:w.Scripts()->Diagnostics())std::printf("SCRIPT %s: %s\n",d.callback.c_str(),d.message.c_str());Check(false,"weather lab scripts remain healthy");done=true;w.UI().RequestQuit();return;}
        const auto state=State(w);if(state.empty())return;const auto mode=state.value("mode","");auto* field=w.RuntimeWeather(20);if(!field)return;
        const auto graphics=host.GetRenderer().WeatherStats();if(!graphics.error.empty()){std::printf("WEATHER_RENDER %s\n",graphics.error.c_str());Check(false,"weather renderer reports no shader/framebuffer error");done=true;w.UI().RequestQuit();return;}
        auto shot=[&](const char* name){std::vector<unsigned char> pixels;host.GetRenderer().CaptureFrame(window->Width(),window->Height(),pixels);Check(WriteRgbPng((output/name).string(),window->Width(),window->Height(),pixels),"actual application weather screenshot captured");};
        auto selected=[&](unsigned index){return state.value("selected",999u)==index&&field->State().transitionElapsed>=field->State().transitionDuration;};
        if(orbitOnly&&phase==0&&mode=="ground"&&age>30){tap("key:F9");priorWorld=&w;phaseTo(16);return;}
        switch(phase){
        case 0:if(mode=="ground"&&age>100){auto* motor=w.RuntimeCharacter(Actor(state));Check(motor&&motor->result.supported,"normal project JS spawns a supported flat-world CharacterMotor");Check(graphics.enabled&&graphics.ready&&graphics.cloudWidth>0&&graphics.cloudWidth<=960&&graphics.cloudHeight<=540,"actual cloud targets initialize within bounded dimensions");panelStart=V(w.Physics().GetTransform(w.RuntimeBody(115)).position);shot("sunny.png");tap("key:2");phaseTo(1);}break;
        case 1:if(age>190&&selected(1)){Check(field->Current().cloudCoverage>.7,"logical profile input produces actual gathering clouds");shot("cloudy.png");tap("key:3");phaseTo(2);}break;
        case 2:if(age>190&&selected(2)){Check(field->Current().rain>.79&&graphics.particles>0,"rain profile drives bounded GPU precipitation");Check(w.WeatherStats().shelterQueries>=256,"actual application populates the staggered shelter atlas through physics queries");shot("rain.png");window->QueueTestPhysical("key:W",1);phaseTo(3);}break;
        case 3:if(age>=275){window->QueueTestPhysical("key:W",0);auto* motor=w.RuntimeCharacter(Actor(state));Check(motor&&motor->result.supported&&motor->position.z<-8&&motor->position.z>-15,"normal input walks the real motor below the pavilion");if(motor){auto hit=w.Physics().Raycast(motor->position+glm::vec3(0,1.4f,0),{0,1,0},8);Check(hit.hit&&w.EntityIdOfBody(hit.body)==105,"shelter is ordinary roof collision above the character");}shot("shelter.png");window->QueueTestPhysical("key:S",1);phaseTo(4);}break;
        case 4:if(age>=275){window->QueueTestPhysical("key:S",0);Check(w.view.has_value(),"script-owned presentation camera follows the returning traveller");thunderBefore=w.WeatherStats().thunderPlayed;sequenceBefore=field->State().lightning.sequence;strikeTime=field->State().time;const V eye=w.Settings().worldOrigin+V(w.view->pose.position);field->TriggerLightning(eye+V(343.3,0,0));Check(field->State().lightning.sequence==sequenceBefore+1,"explicit public weather strike creates a real source event");std::printf("THUNDER fixture_distance_m=343.3 expected_delay_s=1.0 source_time=%.9f\n",strikeTime);phaseTo(5);}break;
        case 5:if(age==3)shot("lightning.png");if(age==35)Check(w.WeatherStats().thunderPlayed==thunderBefore,"thunder is not played before the actual propagation delay");if(age>90){Check(w.WeatherStats().thunderPlayed>thunderBefore,"existing audio mixer receives thunder after distance propagation");Check(glm::length(V(w.Physics().GetTransform(w.RuntimeBody(115)).position)-panelStart)>.1&&w.WeatherStats().forces>0,"opted-in wind moves an actual ordinary physical prop");tap("key:4");phaseTo(6);}break;
        case 6:if(age>190&&selected(3)){Check(field->Current().rain>.99&&field->Current().lightningRate>0,"thunderstorm parameters coordinate rain, clouds, wind and source rate");shot("thunderstorm.png");tap("key:5");phaseTo(7);}break;
        case 7:if(age>190&&selected(4)){Check(field->Current().tornado>.99&&glm::length(field->Sample(V(0)).tornadoPosition)>1,"tornado profile has a moving three-dimensional wind source");shot("tornado.png");tap("key:6");phaseTo(8);}break;
        case 8:if(age>190&&selected(5)){Check(field->Current().snow>.79&&graphics.particles>0,"snow profile drives actual bounded precipitation");shot("snow.png");tap("key:7");phaseTo(9);}break;
        case 9:if(age>190&&selected(6)){Check(field->Current().snow>.99&&field->Current().fog>.7&&glm::length(field->Current().wind)>20,"blizzard combines snow, visibility and coherent strong wind");shot("blizzard.png");tap("key:8");phaseTo(10);}break;
        case 10:if(age>190&&selected(7)){Check(field->Current().fog>.8,"fog profile is independently usable");shot("fog.png");tap("key:3");phaseTo(11);}break;
        case 11:if(age==65){Check(field->State().transitionElapsed>0&&field->State().transitionElapsed<field->State().transitionDuration,"flat save is requested during a live transition");saveRequest(host,w,flatSlot);phaseTo(12);}break;
        case 12:if(saveFinished(host,w,flatSlot))phaseTo(13);break;
        case 13:if(restoreObserved&&age>30){auto* motor=w.RuntimeCharacter(Actor(state));Check(motor&&motor->result.supported,"flat cold load resumes real supported movement");tap("key:Escape");phaseTo(14);}break;
        case 14:if(age==4)pauseTime=field->State().time;if(age==20){Check(field->State().time==pauseTime,"pause UI stops the authoritative weather clock");tap("key:Escape");phaseTo(15);}break;
        case 15:if(age==20){Check(field->State().time>pauseTime,"resume restarts weather without replacing it");tap("key:F9");priorWorld=&w;phaseTo(16);}break;
        case 16:if(&w!=priorWorld&&w.Planet(1)&&mode=="ground"&&age>80){Check(w.RuntimeCharacter(Actor(state))&&w.RuntimeCharacter(Actor(state))->result.supported,"registered F9 transition lands on actual Earth-scale physical terrain");const auto sample=w.SampleWeather(V(0,6371002,0));Check(sample.active&&sample.frame.radial&&std::abs(sample.altitude-2)<1e-6&&glm::length(sample.up-V(0,1,0))<1e-8,"planet weather samples radial altitude independently of support");shot("planet-ground.png");tap("key:2");phaseTo(17);}break;
        case 17:if(age>190&&selected(1)){orbitRevision=unsigned(w.OriginRevision());tap("key:F1");phaseTo(18);}break;
        case 18:if(age==150){Check(mode=="ascending"&&state.value("altitude",0.)>100,"actual F1 input continuously ascends from the ground");std::printf("VIEW controlled sampling: subsequent cloud/orbit captures use public script-state restore to shorten a naturally longer trip\n");replace(w,[](J& s){s["altitude"]=1500;s["mode"]="flight";});phaseTo(19);}break;
        case 19:if(age==40){Check(w.OriginRevision()>orbitRevision&&glm::length(field->Settings().frame.center)<1e-12,"M85 chart changes keep the radial weather field at its absolute planet anchor");Check(w.view&&w.SampleWeather(w.Settings().worldOrigin+V(w.view->pose.position)).altitude>1400,"actual renderer visits the three-dimensional planetary cloud layer");shot("planet-cloud-layer.png");replace(w,[](J& s){s["altitude"]=4000000;s["mode"]="flight";});phaseTo(20);}break;
        case 20:if(age==30)w.weatherQuality=0;if(age==31)shot("planet-orbit-weather-off.png");if(age==32)w.weatherQuality=1;if(age==60){const V eye=w.Settings().worldOrigin+V(w.view->pose.position);const auto air=w.SampleWeather(eye);Check(air.active&&air.altitude>3e6&&air.airDensity==0&&glm::length(air.wind)==0,"orbit retains cloud-shell presentation while physical air fades to zero");Check(graphics.ready&&graphics.enabled,"same bounded renderer remains valid in the orbital chart");shot("planet-orbit.png");if(orbitOnly){done=true;w.UI().RequestQuit();phaseTo(27);break;}tap("key:F2");phaseTo(21);}break;
        case 21:if(age==20){Check(mode=="descending"&&state.value("altitude",4e6)<4e6,"actual F2 input begins a continuous descent");replace(w,[](J& s){s["altitude"]=3.;s["mode"]="descending";});}if(age>110&&mode=="ground"){auto* motor=w.RuntimeCharacter(Actor(state));Check(motor&&motor->result.supported,"accelerated return finishes with real terrain collision and motor support");tap("key:3");phaseTo(22);}break;
        case 22:if(age==65){saveRequest(host,w,planetSlot);phaseTo(23);}break;
        case 23:if(saveFinished(host,w,planetSlot))phaseTo(24);break;
        case 24:if(restoreObserved&&age>30){auto* motor=w.RuntimeCharacter(Actor(state));Check(motor&&motor->result.supported&&field->Settings().frame.radial,"planet cold load resumes actual supported physical and weather state");tap("key:F9");priorWorld=&w;phaseTo(25);}break;
        case 25:if(&w!=priorWorld&&!w.Planet(1)&&mode=="ground"&&age>80){Check(w.RuntimeWeather(20)&&!field->Settings().frame.radial&&field->State().time<5,"whole-scene replacement cleans old planet weather and creates the flat source");tap("key:R");priorWorld=&w;phaseTo(26);}break;
        case 26:if(&w!=priorWorld&&mode=="ground"&&age>70){Check(state.value("selected",99u)==0&&field->State().time<3&&w.WeatherStats().thunderPlayed==0,"authored reload resets profile, clock, script state and old thunder work");shot("reloaded.png");done=true;w.UI().RequestQuit();phaseTo(27);}break;
        default:break;
        }
    };
    Application application;std::string filename=project.string();char executable[]="judas";char* args[]={executable,filename.data()};
    Check(application.Run(2,args,&control)==0&&done&&phase==27,"ordinary application startup, transitions, cold saves, reload and shutdown complete");
    std::printf("MEASURE frames=%u fixed_p95_ms=%.6f fixed_max_ms=%.6f render_p95_ms=%.6f cpu_frame_p50_ms=%.6f cpu_frame_p95_ms=%.6f cpu_frame_max_ms=%.6f\n",frame,Percentile(fixed,.95),Percentile(fixed,1),Percentile(render,.95),Percentile(cpu,.5),Percentile(cpu,.95),Percentile(cpu,1));
    std::printf("MEASURE actual_frame_period_p50_ms=%.6f actual_frame_period_p95_ms=%.6f actual_frame_period_max_ms=%.6f periods_over_50ms=%zu (includes deliberate screenshots, saves, reload and cold planetary preparation)\n",Percentile(period,.5),Percentile(period,.95),Percentile(period,1),size_t(std::count_if(period.begin(),period.end(),[](double v){return v>50;})));
    std::printf("SUMMARY %u checks %u failures\n",checks,failures);return failures?1:0;
}
