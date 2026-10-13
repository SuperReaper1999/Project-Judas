#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "WorldStreaming.h"
#include "WorldPersistence.h"
#include "SaveService.h"
#include "SaveArchive.h"
#include "ScreenshotWriter.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>

namespace {
unsigned checks=0,failures=0;
void Check(bool value,const char* message){++checks;failures+=!value;std::printf("%s %s\n",value?"PASS":"FAIL",message);std::fflush(stdout);}
std::string SpinBytes(WheelInstance* wheel){SaveArchive archive;if(wheel)wheel->Persist(archive);return archive.bytes;}
std::vector<EntityId> Wheels(RuntimeWorld& world,EntityId chassis){
    std::vector<EntityId> result;
    for(auto id:world.DefinitionIds())if(const auto* definition=world.RuntimeDefinition(id);definition&&definition->wheel&&definition->wheel->chassis==chassis)result.push_back(id);
    return result;
}
}

int main(int argc,char** argv){
    if(argc!=4)return 2;
    const std::string project=argv[1],mode=argv[2];const auto output=std::filesystem::absolute(argv[3]);
    std::filesystem::create_directories(output);std::string error;
    unsigned frame=0,ready=0,phase=0;bool frozen=false,completed=false;
    Window* window=nullptr;RuntimeWorld* original=nullptr;GameSnapshot snapshot;
    glm::vec3 initial{0},forward{0};float calmLength=0,calmLoad=0,drivenSpeed=0;
    uint64_t region=0;EntityId streamed=0,spawned=0;std::string retained;
    std::vector<double> fixedTimes,frameTimes;auto started=std::chrono::steady_clock::now();
    auto key=[&](SDL_Scancode code,bool down){window->QueueTestPhysical(std::string("key:")+SDL_GetScancodeName(code),down?1.f:0.f);};
    ApplicationControl control;control.hidden=true;control.frameSeconds=[&](float){return frozen?0.f:1.f/60;};
    control.hostReady=[&](EngineHost& host){window=&host.GetWindow();window->SetTestInputMode(true);SDL_GL_SetSwapInterval(0);std::string ignored;host.Audio().Init(ignored,true);};
    control.worldReady=[&](EngineHost&,RuntimeWorld& world,InteractivePlay&){original=&world;};
    control.beforeFrame=[&](EngineHost&,RuntimeWorld& world,InteractivePlay&){
        started=std::chrono::steady_clock::now();
        if(mode=="read"&&phase==1){key(SDL_SCANCODE_F8,true);phase=2;}
        else if(mode=="read"&&phase==2)key(SDL_SCANCODE_F8,false);
        // Stop the next physical interval at the real publication boundary,
        // then observe after restored JS instances have initialized at dt=0.
        if(mode=="read"&&phase==2&&&world!=original){frozen=true;phase=3;}
    };
    control.afterFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){
        ++frame;auto* wheel=world.RuntimeWheel(101);auto chassis=world.RuntimeBody(100);
        bool healthy=world.Scripts()&&world.Scripts()->Diagnostics().empty();
        if(!healthy&&world.Scripts())for(const auto& diagnostic:world.Scripts()->Diagnostics())std::printf("SCRIPT %s %s\n",diagnostic.callback.c_str(),diagnostic.message.c_str());
        if(!ready&&wheel&&wheel->state.available&&healthy){
            ready=frame;const auto transform=world.Physics().GetTransform(chassis);initial=transform.position;forward=transform.rotation*glm::vec3(0,0,-1);
            Check(Wheels(world,100).size()==4&&Wheels(world,200).size()==6,"ordinary four-wheel and six-wheel assemblies use the same component");
            Check(!world.RuntimeBody(101).IsValid(),"visual tyre has no duplicate rigid-body mass or contact owner");
            if(mode=="read"){
                auto* service=world.SceneControl()->Saves(host.Resources());SaveStorage storage(service->Root());bool recovered=false;
                Check(storage.Read("wheel-current",snapshot,recovered,error),"fresh process reads committed wheel save bytes");phase=1;
            }
        }
        if(mode=="contracts"&&world.Scripts()){
            for(const auto& record:world.Scripts()->Capture())if(record.entity==11&&record.json.find("\"finished\":true")!=std::string::npos){
                for(const auto* expected:{"valid","startMutationBlocked","invalidConfigBlocked","invalidCommandBlocked","unknownFieldBlocked","positiveConfig","disabled","positiveCommand","contacts","staleInvalid","staleReadBlocked","staleWriteBlocked"}){
                    const std::string signature=std::string("\"")+expected+"\":true";Check(record.json.find(signature)!=std::string::npos,expected);
                }
                Check(healthy,"public binding negative cases isolate errors without disabling scripts");completed=true;world.UI().RequestQuit();
            }
        }
        if(ready&&mode!="read"&&mode!="contracts"){
            const unsigned tick=frame-ready;
            if(tick>20){fixedTimes.push_back(play.LastFixedStepMilliseconds());frameTimes.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count());}
            if(tick==100){
                const auto wheels=Wheels(world,100);unsigned contacts=0;calmLength=calmLoad=0;
                for(auto id:wheels){const auto* state=world.RuntimeWheel(id);contacts+=state->state.contact;calmLength+=state->state.length;calmLoad+=state->state.normalForce;}
                calmLength/=float(wheels.size());
                std::printf("CALM contacts=%u length_m=%.9g load_N=%.9g speed_mps=%.9g\n",contacts,calmLength,calmLoad,glm::length(world.Physics().GetLinearVelocity(chassis)));
                Check(contacts>=3&&calmLength>.1f&&calmLength<.4f,"actual query contacts compress suspension under chassis self-weight");
                Check(world.view.has_value()&&healthy,"public wheel JS, logical input, UI and presentation camera execute");
                const auto gravity=world.Gravity().Sample(world.Physics().GetTransform(chassis).position);
                if(mode=="oblique")Check(std::abs(gravity.x)>5&&std::abs(gravity.y)>5,"oblique ordinary gravity has no universal world-up");
                if(mode=="radial")Check(std::abs(gravity.z)>.5&&glm::length(world.Gravity().Sample({20,0,0})-world.Gravity().Sample({-20,0,0}))>4,"radial scene samples the real spatial field on spherical collision geometry");
                if(mode=="lifecycle"){
                    key(SDL_SCANCODE_P,true);region=world.SceneControl()->Streaming(world,error)->Request("garage",false,error);
                }else key(SDL_SCANCODE_W,true);
            }
            if(tick==101)key(SDL_SCANCODE_P,false);
            if(mode=="steer"&&tick==130)key(SDL_SCANCODE_A,true);
            if(mode=="steer"&&tick==180){
                const auto direction=world.Physics().GetTransform(chassis).rotation*glm::vec3(0,0,-1);
                const float angle=std::acos(std::clamp(glm::dot(glm::normalize(direction),glm::normalize(forward)),-1.f,1.f));
                std::printf("STEER heading_change_radians=%.9g command=%.9g\n",angle,wheel->command.steeringAngle);
                Check(wheel->command.steeringAngle>.3f&&angle>.03f,"logical steering rotates tyre contact direction and changes actual chassis heading");
                key(SDL_SCANCODE_A,false);
            }
            if(tick==210&&mode!="lifecycle"){
                const auto delta=world.Physics().GetTransform(chassis).position-initial;drivenSpeed=glm::length(world.Physics().GetLinearVelocity(chassis));
                std::printf("DRIVEN forward_m=%.9g speed_mps=%.9g spin_rps=%.9g\n",glm::dot(delta,forward),drivenSpeed,wheel->state.angularVelocity);
                Check(glm::dot(delta,forward)>2&&drivenSpeed>.5f&&std::abs(wheel->state.angularVelocity)>1,"logical W input drives real chassis displacement through rolling traction");
                Check(wheel->command.driveTorque>0,"project JS supplies direct torque rather than moving chassis transforms");
                key(SDL_SCANCODE_W,false);key(SDL_SCANCODE_SPACE,true);
            }
            if(tick==290&&mode!="lifecycle"){
                const float speed=glm::length(world.Physics().GetLinearVelocity(chassis));std::printf("BRAKED speed_mps=%.9g\n",speed);
                Check(speed<drivenSpeed*.65f+.1f,"ordinary wheel brake torque reduces actual vehicle speed");
                key(SDL_SCANCODE_SPACE,false);key(SDL_SCANCODE_B,true);
            }
            if(tick==291)key(SDL_SCANCODE_B,false);
            if(tick==365&&mode!="lifecycle"){
                float load=0,length=0;for(auto id:Wheels(world,100)){auto* current=world.RuntimeWheel(id);load+=current->state.normalForce;length+=current->state.length;}length/=4;
                std::printf("PAYLOAD length_m=%.9g load_N=%.9g\n",length,load);
                Check(load>calmLoad*1.12f&&length<calmLength-.006f,"real finite-mass payload changes suspension load and travel");
                key(SDL_SCANCODE_H,true);
            }
            if(tick==366)key(SDL_SCANCODE_H,false);
            if(tick==390&&mode!="lifecycle"){
                Check(std::abs(wheel->settings.friction-.02f)<1e-6f,"runtime tyre configuration uses the public fixed-update API");
                if(mode=="write"){key(SDL_SCANCODE_W,true);key(SDL_SCANCODE_F5,true);phase=8;}
                else{std::vector<unsigned char> pixels;host.GetRenderer().CaptureFrame(window->Width(),window->Height(),pixels);WriteRgbPng((output/"lab.png").string(),window->Width(),window->Height(),pixels);completed=true;world.UI().RequestQuit();}
            }
            if(tick==391)key(SDL_SCANCODE_F5,false);
            if(mode=="write"&&phase==8){
                auto* service=world.SceneControl()->Saves(host.Resources());for(uint64_t id=1;id<8;++id)if(auto* status=service->Status(id)){
                    if(status->state=="completed"){Check(true,"public JS saves wheel commands and spin with physical chassis state");completed=true;world.UI().RequestQuit();phase=9;}
                    if(status->state=="failed"){std::printf("SAVE %s\n",status->error.c_str());Check(false,"wheel save transaction succeeds");completed=true;world.UI().RequestQuit();phase=9;}
                }
            }
            if(mode=="lifecycle"&&tick>120){
                auto* streaming=world.SceneControl()->Streaming(world,error);const auto status=streaming->Regions().front();
                    if(!spawned)for(auto id:world.DefinitionIds())if(id!=100&&id!=200&&world.RuntimeDefinition(id)->body&&Wheels(world,id).size()==4&&streaming->Owner(id)=="root"){
                    spawned=id;Check(world.RuntimeWheel(Wheels(world,id).front())->settings.chassis==id,"runtime prefab remaps wheel/chassis references to independent safe entities");break;
                }
                if(phase==0&&status.state=="active"){
                    streamed=streaming->Resolve("garage",1);const auto children=Wheels(world,streamed);Check(streamed&&children.size()==4,"registered streamed region constructs a complete assembly");
                    if(!children.empty()){auto* current=world.RuntimeWheel(children.front());current->command={35,0,.1f};current->state.angularVelocity=3.7f;retained=SpinBytes(current);}
                    streaming->Release(region);frozen=true;phase=1;
                }
                if(phase==1&&status.state=="unloaded"){
                    Check(!world.RuntimeDefinition(streamed)&&status.retained>0,"stream suspension invalidates old entity identities and retains wheel state");
                    region=streaming->Request("garage",false,error);phase=2;
                }
                if(phase==2&&status.state=="active"){
                    const auto newChassis=streaming->Resolve("garage",1);const auto children=Wheels(world,newChassis);
                    Check(newChassis&&newChassis!=streamed&&!children.empty()&&SpinBytes(world.RuntimeWheel(children.front()))==retained,"stream revisit restores exact durable spin/command state with remapped chassis identity");
                    if(spawned){auto child=Wheels(world,spawned).front();world.DestroyHierarchy(spawned,error);Check(!world.RuntimeDefinition(child)&&!world.RuntimeWheel(child),"assembly root destruction releases wheel children safely");}
                    frozen=false;world.SceneControl()->Reload(error);phase=4;
                }
                if(phase==4&&play.FixedStepsSinceReset()<4){Check(healthy&&Wheels(world,100).size()==4&&std::abs(wheel->command.driveTorque)<1e-6f,"reload restores authored assembly with no stale wheel commands");completed=true;world.UI().RequestQuit();}
            }
        }
        if(mode=="read"&&phase==3){
            std::map<std::string,SaveChunk> chunks;
            if(WorldPersistence::Capture(world,chunks,error)){
                Check(play.FixedStepsSinceReset()==0,"cold-save observation precedes any resumed physical step");
                Check(chunks.count("wheels")&&snapshot.participants.count("wheels")&&chunks.at("wheels").data==snapshot.participants.at("wheels").data&&std::abs(wheel->settings.friction-.02f)<1e-6f,"cold restore exactly preserves runtime tyre settings, commands, spin and travel");
                for(const char* name:{"saved","restored"}){std::ofstream stream(output/(std::string(name)+"-wheels.bin"),std::ios::binary);const auto& data=std::string(name)=="saved"?snapshot.participants.at("wheels").data:chunks.at("wheels").data;stream.write(data.data(),data.size());}
                initial=world.Physics().GetTransform(chassis).position;frozen=false;key(SDL_SCANCODE_W,true);ready=frame;phase=4;
            }else if(error!="loading"&&error.find("required script instance not initialized")==std::string::npos){std::printf("RESTORE %s\n",error.c_str());Check(false,"restored world initializes normal script lifecycle");completed=true;world.UI().RequestQuit();}
        }
        if(mode=="read"&&phase==4&&frame>ready+45){
            Check(glm::length(world.Physics().GetTransform(chassis).position-initial)>.02f&&wheel->state.available&&healthy,"cold-restored physical endpoint genuinely moves under new wheel input");
            completed=true;world.UI().RequestQuit();
        }
        if(frame>1400){Check(false,"bounded application deadline");world.UI().RequestQuit();}
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    };
    Application application;std::string projectArg=project;
    if(mode=="oblique"||mode=="radial"||mode=="contracts")projectArg=(std::filesystem::absolute(project).parent_path()/"Scenes"/(mode+".judas")).string();
    char name[]="judas";char* args[]={name,projectArg.data()};
    Check(application.Run(2,args,&control)==0&&completed,"normal application startup and shutdown");
    if(!fixedTimes.empty()){
        std::sort(fixedTimes.begin(),fixedTimes.end());std::sort(frameTimes.begin(),frameTimes.end());
        std::printf("PERF mode=%s samples=%zu fixed_median_ms=%.6f p95_ms=%.6f max_ms=%.6f frame_median_ms=%.6f frame_p95_ms=%.6f\n",mode.c_str(),fixedTimes.size(),fixedTimes[fixedTimes.size()/2],fixedTimes[size_t(.95*(fixedTimes.size()-1))],fixedTimes.back(),frameTimes[frameTimes.size()/2],frameTimes[size_t(.95*(frameTimes.size()-1))]);
    }
    std::printf("M81 application %s checks=%u failures=%u\n",mode.c_str(),checks,failures);return failures?1:0;
}
