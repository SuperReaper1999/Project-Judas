// M87: exercise the actual ordinary lab through logical input and the normal
// application loop. This is a correctness/lifecycle fixture, not a performance
// benchmark. It neither substitutes a solver nor changes the authored physics.
#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SaveArchive.h"
#include "SaveService.h"
#include "SceneFingerprint.h"
#include "SceneSession.h"
#include "ScreenshotWriter.h"
#include "WorldPersistence.h"
#include "../third_party/nlohmann/json.hpp"
#include <SDL2/SDL.h>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace {
using J=nlohmann::json;
unsigned checks=0,failures=0;
void Check(bool value,const char* message){
    ++checks;failures+=!value;
    std::printf("%s %s\n",value?"PASS":"FAIL",message);std::fflush(stdout);
}
J State(RuntimeWorld& world){
    if(world.Scripts())for(const auto& record:world.Scripts()->Capture())
        if(record.entity==10)return J::parse(record.json);
    return J::object();
}
unsigned SleepingArticulated(RuntimeWorld& world){
    unsigned count=0;
    for(unsigned machine=0;machine<32;++machine)for(unsigned child:{4u,5u,6u})
        count+=world.Physics().IsSleeping(world.RuntimeBody(1000+20*machine+child));
    return count;
}
bool Motors(RuntimeWorld& world,bool enabled){
    for(unsigned machine=0;machine<32;++machine){
        JointState joint;
        if(!world.Physics().GetJoint(world.RuntimeJoint(1008+20*machine),joint)||
           joint.settings.motor!=enabled||joint.settings.maxForce!=100||
           !joint.settings.spring||!joint.settings.limits||
           (enabled&&std::abs(joint.settings.speed)<.27f))return false;
    }
    return true;
}
bool FinitePopulation(RuntimeWorld& world){
    for(const auto body:world.Physics().AliveBodies()){
        const auto pose=world.Physics().GetTransform(body);
        const auto velocity=world.Physics().GetLinearVelocity(body);
        for(float value:{pose.position.x,pose.position.y,pose.position.z,pose.rotation.w,
                         pose.rotation.x,pose.rotation.y,pose.rotation.z,velocity.x,velocity.y,velocity.z})
            if(!std::isfinite(value))return false;
    }
    return true;
}
bool JointGeometry(RuntimeWorld& world){
    for(unsigned machine=0;machine<32;++machine)for(unsigned child:{8u,9u,10u}){
        JointState joint;
        if(!world.Physics().GetJoint(world.RuntimeJoint(1000+20*machine+child),joint))return false;
        const auto& settings=joint.settings;
        if(joint.coordinate<settings.lower-.025f||joint.coordinate>settings.upper+.025f)return false;
        if(settings.type==JointType::Hinge){
            const auto a=world.Physics().GetTransform(settings.bodyA),b=world.Physics().GetTransform(settings.bodyB);
            if(glm::length(a.position+a.rotation*settings.anchorA-b.position-b.rotation*settings.anchorB)>.025f)return false;
        }
    }
    return true;
}
J SnapshotState(const GameSnapshot& snapshot){
    const auto found=snapshot.participants.find("scripts");if(found==snapshot.participants.end())return J::object();
    SaveArchive archive(found->second.data);uint32_t count=0;archive(count);archive.Require(count<=4096,"bounded script archive");
    J result=J::object();
    for(uint32_t i=0;i<count;++i){ScriptStateRecord record;archive(record.entity,record.slot,record.json);if(record.entity==10)result=J::parse(record.json);}
    archive.Finish();return result;
}
}

int main(int argc,char** argv){try{
    if(argc!=3)return 2;
    const auto original=std::filesystem::absolute(argv[1]),output=std::filesystem::absolute(argv[2]);
    std::filesystem::create_directories(output);
    // A private project copy changes ONLY save identity. Public F6/F7 must not
    // overwrite an operator's manually saved lab when this fixture is rerun.
    const auto copy=output/"project";
    std::filesystem::create_directories(copy);
    std::filesystem::copy(original.parent_path(),copy,std::filesystem::copy_options::recursive|std::filesystem::copy_options::overwrite_existing);
    const auto project=copy/original.filename();J definition;
    {std::ifstream in(project);in>>definition;}
    const auto nonce=std::chrono::steady_clock::now().time_since_epoch().count();
    definition["data"]["saveIdentity"]=SceneFingerprintSha256("judas-m87-application-"+output.string()+std::to_string(nonce));
    {std::ofstream out(project);out<<definition.dump(2)<<'\n';}
    std::printf("FIXTURE normal project copy; only saveIdentity differs from %s\n",original.string().c_str());

    ApplicationControl control;control.hidden=true;control.frameSeconds=[](float){return 1.f/60;};
    Window* window=nullptr;RuntimeWorld* prior=nullptr;unsigned frame=0,phase=0,age=0;
    bool completed=false,restored=false;std::map<std::string,unsigned> releases;
    glm::vec3 initialShelf(0),restoredShelf(0),beforeKick(0),tourQuiet(0);
    BodyHandle oldBody;JointHandle oldJoint;GameSnapshot saved;Scene savedScene;J savedScript;
    double pausedClock=0;unsigned kicksBeforeTour=0;std::string error;
    auto next=[&](unsigned value){phase=value;age=0;std::printf("PHASE %u frame=%u\n",phase,frame);std::fflush(stdout);};
    auto tap=[&](const std::string& key){window->QueueTestPhysical(key,1);releases[key]=frame+2;};
    auto finish=[&](RuntimeWorld& world){completed=true;world.UI().RequestQuit();};
    auto image=[&](EngineHost& host,const char* name){
        std::vector<unsigned char> pixels;host.GetRenderer().CaptureFrame(window->Width(),window->Height(),pixels);
        Check(WriteRgbPng((output/name).string(),window->Width(),window->Height(),pixels),"capture actual rendered lab frame");
    };
    control.hostReady=[&](EngineHost& host){
        window=&host.GetWindow();window->SetTestInputMode(true);SDL_GL_SetSwapInterval(0);
        std::string ignored;host.Audio().Init(ignored,true);
    };
    control.beforeFrame=[&](EngineHost&,RuntimeWorld& world,InteractivePlay&){
        for(auto it=releases.begin();it!=releases.end();)if(frame>=it->second){window->QueueTestPhysical(it->first,0);it=releases.erase(it);}else ++it;
        if(phase==4&&&world!=prior){
            // Normal world publication precedes resumed callbacks. Construct
            // pending script instances without advancing a physical interval.
            world.Scripts()->Synchronize(world.ScriptSlots());
            Check(State(world)==savedScript,"cold load restores the exact archived lab state before callbacks");
            std::map<std::string,SaveChunk> actual;
            const bool captured=WorldPersistence::Capture(world,actual,error);
            Check(captured,"recapture native continuation at the real load publication boundary");
            if(!captured)std::printf("RESTORE_CAPTURE %s\n",error.c_str());
            for(const char* participant:{"articulation","contacts","sleep"}){
                const auto expected=saved.participants.find(participant),observed=actual.find(participant);
                Check(captured&&expected!=saved.participants.end()&&observed!=actual.end()&&
                      expected->second.version==observed->second.version&&expected->second.data==observed->second.data,
                      (std::string("cold load preserves actual ")+participant+" continuation bytes").c_str());
            }
            unsigned poses=0;bool match=true;
            for(const auto& object:savedScene.Objects())if(object.body&&object.body->motion==SceneBodyMotion::Dynamic){
                const auto body=world.RuntimeBody(object.id);if(!body.IsValid()){match=false;continue;}
                const auto pose=world.Physics().GetTransform(body);++poses;
                match&=glm::length(pose.position-object.transform.position)<.00005f&&
                       std::abs(glm::dot(pose.rotation,object.transform.rotation))>1-.00001f;
            }
            Check(match&&poses==288,"cold load restores all 288 physical endpoints, including runtime prefabs");
            Shape ignoredShape;BodyTransform ignoredPose;JointState ignoredJoint;
            Check(!world.Physics().GetBodyShape(oldBody,ignoredShape,ignoredPose)&&
                  !world.Physics().GetJoint(oldJoint,ignoredJoint),"old body and joint handles cannot alias the fresh loaded world");
            restoredShelf=world.Physics().GetTransform(world.RuntimeBody(1004)).position;
            restored=true;next(5);
        }
    };
    control.afterFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){
        ++frame;++age;
        if(frame>7000){Check(false,"bounded ordinary lab application deadline");finish(world);return;}
        if(!world.Scripts())return;
        if(!world.Scripts()->Diagnostics().empty()){
            for(const auto& diagnostic:world.Scripts()->Diagnostics())std::printf("SCRIPT %s %s\n",diagnostic.callback.c_str(),diagnostic.message.c_str());
            Check(false,"ordinary public lab scripts remain healthy");finish(world);return;
        }
        const auto state=State(world);if(state.empty())return;
        auto shelf=[&](){return world.Physics().GetTransform(world.RuntimeBody(1004)).position;};
        switch(phase){
        case 0:
            if(age==15){
                Check(world.Physics().DynamicBodyCount()==256&&world.Physics().AliveBodyCount()==389,"normal startup constructs exact physical room population");
                Check(world.Scripts()->Capture().size()==1&&world.view.has_value(),"one project script owns lab intent and the presentation camera");
                Check(Motors(world,false),"initial mechanisms keep real springs and limits with drive power off");
            }
            if(age>90&&(SleepingArticulated(world)==96||age>=360)){
                Check(SleepingArticulated(world)==96,"all 96 unpowered articulated bodies sleep through normal physics");
                unsigned looseSleeping=0;for(unsigned i=0;i<128;++i)looseSleeping+=world.Physics().IsSleeping(world.RuntimeBody(5000+i));
                Check(looseSleeping==128,"the 128 separated grounded pieces naturally sleep without script intervention");
                initialShelf=shelf();image(host,"resting.png");tap("key:M");next(1);
            }
            break;
        case 1:
            if(age==50){
                Check(state.value("motors",false)&&Motors(world,true),"logical M input enables native finite-force slider motors");
                Check(glm::length(shelf()-initialShelf)>.025f,"motor commands move the actual articulated shelf");
                Check(SleepingArticulated(world)<96&&JointGeometry(world),"moving native joints wake bodies while preserving limits and anchors");
                image(host,"powered.png");tap("key:P");next(2);
            }
            break;
        case 2:
            if(age==45){
                Check(world.Physics().DynamicBodyCount()==288&&state.at("extra").size()==32,"logical P input spawns 32 ordinary persistent dynamic prefabs");
                unsigned moved=0;
                for(const auto id:world.DefinitionIds())if(id>=kRuntimeEntityIdBase&&world.Physics().IsDynamicBody(world.RuntimeBody(id)))
                    moved+=world.Physics().GetTransform(world.RuntimeBody(id)).position.y<3.8f;
                Check(moved==32,"spawned pieces actually fall under gravity rather than existing only in script inventory");
                tap("key:F6");next(3);
            }
            break;
        case 3:{
            auto* service=world.SceneControl()->Saves(host.Resources());
            const SaveRequestStatus* status=nullptr;
            for(uint64_t id=1;id<=256;++id)if(auto* found=service->Status(id);found&&found->operation=="save"&&found->slot=="physics-stress-lab")status=found;
            if(status&&status->state=="failed"){std::printf("SAVE %s\n",status->error.c_str());Check(false,"logical F6 save completes");finish(world);break;}
            if(status&&status->state=="completed"){
                Check(true,"logical F6 produces a completed ordinary durable save");
                SaveStorage storage(service->Root());bool recovered=false;
                Check(storage.Read("physics-stress-lab",saved,recovered,error)&&WorldPersistence::SceneFrom(saved.participants,savedScene,error),"read the actual played scene and native participant archive");
                savedScript=SnapshotState(saved);
                Check(savedScript.value("motors",false)&&savedScript.at("extra").size()==32,"save captured running motor policy and runtime entity references");
                prior=&world;oldBody=world.RuntimeBody(1004);oldJoint=world.RuntimeJoint(1008);tap("key:F7");next(4);
            }
            break;}
        case 4:break;
        case 5:
            if(age==40){
                Check(restored&&world.Physics().DynamicBodyCount()==288&&glm::length(shelf()-restoredShelf)>.015f&&Motors(world,true),"loaded physical machinery genuinely continues moving under restored control");
                Check(state.at("extra").size()==32&&world.Scripts()->Capture().size()==1,"load reconstructs prefab handles with one independent script instance");
                tap("key:M");next(6);
            }
            break;
        case 6:
            if(age>90&&(SleepingArticulated(world)==96||age>=420)){
                Check(Motors(world,false)&&SleepingArticulated(world)==96,"removing power after motion permits ordinary physical sleep again");
                beforeKick=shelf();tap("key:J");next(7);
            }
            break;
        case 7:
            if(age==2){
                bool awake=true,independent=true;
                for(unsigned child:{4u,5u,6u}){awake&=!world.Physics().IsSleeping(world.RuntimeBody(1000+child));independent&=world.Physics().IsSleeping(world.RuntimeBody(1020+child));}
                Check(awake&&independent,"a real impulse wakes every connected member while a different assembly stays asleep");
            }
            if(age==25){
                Check(glm::length(shelf()-beforeKick)>.008f&&JointGeometry(world),"the impulsed articulation physically moves while its joints remain connected");
                tap("key:Escape");next(8);
            }
            break;
        case 8:
            if(age==5){pausedClock=state.at("time").get<double>();tap("key:M");}
            if(age==25){
                Check(play.IsPaused()&&state.at("time").get<double>()==pausedClock&&Motors(world,false),"modal pause stops simulation and blocks motor input leaking through the menu");
                tap("key:Escape");next(9);
            }
            break;
        case 9:
            if(age==20){
                Check(!play.IsPaused()&&state.at("time").get<double>()>pausedClock&&Motors(world,false),"resume advances ordinary physics without replaying the paused motor input");
                kicksBeforeTour=state.value("kicks",0u);tap("key:F5");next(10);
            }
            break;
        case 10:
            if(age==20)Check(state.value("tour",false)&&Motors(world,true),"logical F5 starts the real shared-script watch tour");
            if(age==510)Check(state.value("tourStage",-1)==1&&Motors(world,false),"the timed watch tour removes native motor power without disabling bodies");
            if(age==1000){
                Check(SleepingArticulated(world)==96,"unattended tour reaches natural sleeping state before its impulse stage");tourQuiet=shelf();
            }
            if(age==1210){
                Check(state.value("tourStage",-1)==2&&state.value("kicks",0u)==kicksBeforeTour+1&&glm::length(shelf()-tourQuiet)>.005f,"tour impulse changes real physical state after the quiet interval");
                Check(FinitePopulation(world)&&JointGeometry(world),"combined drive, rest, impulse and save lifecycle retains finite connected physics");
                tap("key:2");prior=&world;next(11);
            }
            break;
        case 11:
            if(&world!=prior&&age>35){
                Check(world.Physics().DynamicBodyCount()==640&&world.Physics().AliveBodyCount()==773,"registered 512-piece scene changes real population and cleans the former world");
                Check(state.at("extra").empty()&&!state.value("tour",true)&&Motors(world,false),"scene transition resets gameplay state and old runtime additions");
                Check(FinitePopulation(world)&&world.Physics().LastStepStats().contactPoints>0,"larger ordinary scene produces actual finite physical contacts");
                tap("key:3");prior=&world;next(12);
            }
            break;
        case 12:
            if(&world!=prior&&age>35){
                Check(world.Physics().DynamicBodyCount()==1152&&world.Physics().AliveBodyCount()==1285,"registered 1024-piece scene builds its full physical population");
                Check(FinitePopulation(world)&&world.Physics().LastStepStats().contactPoints>0,"the largest lab population remains finite with real contact response");
                image(host,"1024-pieces.png");tap("key:4");prior=&world;next(13);
            }
            break;
        case 13:
            if(&world!=prior&&age>90&&(SleepingArticulated(world)==96||age>=360)){
                const glm::quat rotation=glm::angleAxis(.68f,glm::vec3(0,0,1));
                const auto gravity=world.Gravity().Sample(rotation*glm::vec3(30,4,0));
                Check(world.Physics().DynamicBodyCount()==256&&glm::length(gravity-rotation*glm::vec3(0,-9.81f,0))<.0001f,"rotated scene samples the authored non-world-Y gravity vector");
                const auto hit=world.Physics().Raycast(rotation*glm::vec3(30,4,0),glm::normalize(gravity),8);
                Check(hit.hit&&world.EntityIdOfBody(hit.body)==3&&glm::dot(hit.normal,rotation*glm::vec3(0,1,0))>.999f,"actual rotated floor collision supplies the corresponding support normal");
                Check(SleepingArticulated(world)==96&&JointGeometry(world),"rotated mechanisms retain native joint geometry and natural sleeping");
                Check(FinitePopulation(world),"arbitrary-up population remains finite");image(host,"rotated.png");
                oldBody=world.RuntimeBody(1004);prior=&world;tap("key:R");next(14);
            }
            break;
        case 14:
            if(&world!=prior&&age>20){
                Shape ignoredShape;BodyTransform ignoredPose;
                Check(state.value("time",100.)<2&&!state.value("motors",true)&&state.at("extra").empty()&&world.Physics().DynamicBodyCount()==256,"authored reload restores clean native and shared-script state");
                Check(!world.Physics().GetBodyShape(oldBody,ignoredShape,ignoredPose),"reloaded world rejects prior body handles");
                next(15);finish(world);
            }
            break;
        default:break;
        }
    };
    Application application;std::string filename=project.string();char executable[]="judas";char* arguments[]={executable,filename.data()};
    Check(application.Run(2,arguments,&control)==0&&completed&&phase==15,"ordinary lab application, physical lifecycle and shutdown finish successfully");
    std::ofstream(output/"results.json")<<J{{"checks",checks},{"failures",failures},{"frames",frame},{"fixtureSaveIdentity",definition["data"]["saveIdentity"]},{"performanceBenchmark",false}}.dump(2)<<'\n';
    std::printf("M87 application checks=%u failures=%u frames=%u\n",checks,failures,frame);
    return failures?1:0;
}catch(const std::exception& error){std::fprintf(stderr,"M87 application exception after %u checks: %s\n",checks,error.what());return 1;}}
