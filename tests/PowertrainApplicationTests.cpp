#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "WorldStreaming.h"
#include "WorldPersistence.h"
#include "SaveService.h"
#include "SaveArchive.h"
#include "SceneSerialization.h"
#include "ScreenshotWriter.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <thread>

namespace {
unsigned checks=0,failures=0;
void Check(bool value,const char* message){++checks;failures+=!value;std::printf("%s %s\n",value?"PASS":"FAIL",message);std::fflush(stdout);}
std::string TrainBytes(PowertrainInstance* train){SaveArchive archive;if(train)train->Persist(archive);return archive.bytes;}
void DumpTrain(const std::filesystem::path& path,const PowertrainInstance* train){
 std::ofstream out(path);out<<std::setprecision(17);if(!train){out<<"missing powertrain\n";return;}
 for(size_t i=0;i<train->settings.shafts.size();++i)out<<"shaft "<<i<<" "<<train->settings.shafts[i].name<<" absolute="<<train->absoluteSpin[i]<<" omega="<<train->state.shafts[i].angularVelocity<<" angle="<<train->state.shafts[i].angle<<" initialized="<<train->internalInitialized[i]<<'\n';
 for(size_t i=0;i<train->command.sources.size();++i){const auto& c=train->command.sources[i];out<<"source "<<i<<" input="<<c.input<<" custom="<<c.customTorque<<" starter="<<c.starter<<'\n';}
 for(size_t i=0;i<train->command.links.size();++i){const auto& c=train->command.links[i];out<<"link "<<i<<" enabled="<<c.enabled<<" override="<<c.overrideRatio<<" ratio="<<c.ratio<<" engagement="<<c.engagement<<'\n';}
 for(size_t i=0;i<train->command.brakes.size();++i)out<<"brake "<<i<<" engagement="<<train->command.brakes[i].engagement<<'\n';
}
void DumpBytes(const std::filesystem::path& path,const std::string& bytes){std::ofstream out(path,std::ios::binary);out.write(bytes.data(),bytes.size());}
bool NegativeSnapshot(GameSnapshot& snapshot,const std::string& mode,std::string& error){
 try{
  if(mode=="missing"){snapshot.participants.erase("powertrains");return true;}
  auto& entities=snapshot.participants.at("entities");SaveArchive input(entities.data);std::string text;input(text);
  Scene scene;if(!LoadSceneFromString(text,scene,error))return false;
  for(auto& object:scene.Objects())object.powertrain.reset();
  if(!SaveSceneToString(scene,text)){error="cannot serialize phantom-participant fixture";return false;}
  SaveArchive replacement;replacement(text);replacement.bytes+=entities.data.substr(input.position);entities.data=std::move(replacement.bytes);return true;
 }catch(const std::exception& e){error=e.what();return false;}
}
std::vector<EntityId> Wheels(RuntimeWorld& world,EntityId chassis){std::vector<EntityId> result;for(auto id:world.DefinitionIds())if(const auto* d=world.RuntimeDefinition(id);d&&d->wheel&&d->wheel->chassis==chassis)result.push_back(id);return result;}
double Spin(const PowertrainInstance* train,size_t index){return train&&index<train->state.shafts.size()?train->state.shafts[index].angularVelocity:0;}
}

int main(int argc,char** argv){
 if(argc!=4)return 2;
 const std::string project=argv[1],mode=argv[2];const bool negativeMode=mode=="missing"||mode=="phantom";const auto output=std::filesystem::absolute(argv[3]);
 std::filesystem::create_directories(output);std::string error;
 unsigned frame=0,ready=0,phase=0;bool frozen=false,completed=false;
 Window* window=nullptr;RuntimeWorld* original=nullptr;GameSnapshot snapshot;
 glm::vec3 initial{0},forward{0},electricStart{0};double initialCrank=0,freeCrank=0,machineEarly=0,machineFree=0;
 float drivenSpeed=0;uint64_t region=0,negativeRequest=0;EntityId streamed=0,spawned=0;std::string retained;
 std::vector<double> fixedTimes,frameTimes;auto started=std::chrono::steady_clock::now();
 auto key=[&](SDL_Scancode code,bool down){window->QueueTestPhysical(std::string("key:")+SDL_GetScancodeName(code),down?1.f:0.f);};
 ApplicationControl control;control.hidden=true;control.frameSeconds=[&](float){return frozen?0.f:1.f/60;};
 control.hostReady=[&](EngineHost& host){window=&host.GetWindow();window->SetTestInputMode(true);SDL_GL_SetSwapInterval(0);std::string ignored;host.Audio().Init(ignored,true);};
 control.worldReady=[&](EngineHost&,RuntimeWorld& world,InteractivePlay&){original=&world;};
 control.beforeFrame=[&](EngineHost&,RuntimeWorld& world,InteractivePlay&){
  started=std::chrono::steady_clock::now();
  if(mode=="read"&&phase==1){key(SDL_SCANCODE_F8,true);phase=2;}
  else if(mode=="read"&&phase==2)key(SDL_SCANCODE_F8,false);
  if(mode=="read"&&phase==2&&&world!=original){frozen=true;phase=3;}
 };
 control.afterFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){
  ++frame;auto* train=world.RuntimePowertrain(100);const auto chassis=world.RuntimeBody(100);
  const bool healthy=world.Scripts()&&world.Scripts()->Diagnostics().empty();
  if(!healthy&&world.Scripts())for(const auto& d:world.Scripts()->Diagnostics())std::printf("SCRIPT %s %s\n",d.callback.c_str(),d.message.c_str());
  if(!ready&&train&&train->state.available&&healthy){
   ready=frame;const auto transform=world.Physics().GetTransform(chassis);initial=transform.position;forward=transform.rotation*glm::vec3(0,0,-1);
   Check(Wheels(world,100).size()==4&&Wheels(world,200).size()==6,"four-wheel combustion and six-wheel electric assemblies use ordinary M81 wheels");
   Check(world.RuntimePowertrain(300)&&world.RuntimeBody(301).IsValid(),"nonvehicle machine terminates in a real hinge-constrained rigid body");
   Check(train->settings.sources[0].kind==PowertrainSourceKind::Combustion&&train->settings.links.size()==3,"combustion fixture contains engine, gearbox, clutch and open differential");
   if(mode=="read"||negativeMode){
    auto* service=world.SceneControl()->Saves(host.Resources());SaveStorage storage(service->Root());bool recovered=false;
    const bool read=storage.Read("powertrain-current",snapshot,recovered,error);Check(read,"fresh process reads committed powertrain save bytes");phase=1;
    if(negativeMode){
     auto invalid=snapshot;const auto slot="powertrain-"+mode;const bool constructed=read&&NegativeSnapshot(invalid,mode,error);
     Check(constructed,"negative fixture changes only participant presence or authored powertrain component presence");
     const bool written=constructed&&storage.Write(slot,invalid,error);Check(written,"negative snapshot is committed through ordinary SaveStorage");
     if(written){DumpBytes(output/(mode+".save"),EncodeGameSnapshot(invalid));negativeRequest=service->Request("load",slot,"","{}",error);}
     Check(negativeRequest!=0,"negative snapshot enters the real asynchronous SaveService load path");phase=30;
     if(!negativeRequest){completed=true;world.UI().RequestQuit();}
    }
   }
  }
  if(mode=="contracts"&&world.Scripts())for(const auto& record:world.Scripts()->Capture())if(record.entity==11&&record.json.find("\"finished\":true")!=std::string::npos){
   for(const auto* expected:{"valid","startMutationBlocked","detachedSettings","invalidInputBlocked","unknownSourceBlocked","invalidRatioBlocked","unknownFieldBlocked","positiveCommand","disabled","measuredState","staleInvalid","staleReadBlocked","staleWriteBlocked"}){
    const std::string signature=std::string("\"")+expected+"\":true";Check(record.json.find(signature)!=std::string::npos,expected);
   }
   Check(healthy,"public graph API negative cases isolate errors without disabling scripts");completed=true;world.UI().RequestQuit();
  }
  if(ready&&mode!="read"&&mode!="contracts"&&!negativeMode){
   const unsigned tick=frame-ready;
   if(tick>20){fixedTimes.push_back(play.LastFixedStepMilliseconds());frameTimes.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count());}
   if(tick==100){
    unsigned contacts=0;for(auto id:Wheels(world,100))contacts+=world.RuntimeWheel(id)->state.contact;
    initialCrank=Spin(train,0);
    Check(contacts>=3&&world.view.has_value()&&healthy,"query tyres support actual chassis; JS input, camera, UI and measured graph telemetry execute");
    const auto gravity=world.Gravity().Sample(world.Physics().GetTransform(chassis).position);
    if(mode=="oblique")Check(std::abs(gravity.x)>5&&std::abs(gravity.y)>5,"powertrain chassis operates in arbitrarily oriented ordinary gravity");
    if(mode=="radial")Check(std::abs(gravity.z)>.5&&glm::length(world.Gravity().Sample({20,0,0})-world.Gravity().Sample({-20,0,0}))>4,"powertrain scene uses real spatial radial gravity and spherical collision geometry");
    if(mode=="lifecycle"){key(SDL_SCANCODE_P,true);region=world.SceneControl()->Streaming(world,error)->Request("garage",false,error);}
    else{key(SDL_SCANCODE_F,true);key(SDL_SCANCODE_W,true);key(SDL_SCANCODE_SPACE,true);}
   }
   if(tick==101){key(SDL_SCANCODE_P,false);key(SDL_SCANCODE_F,false);}
   if(tick==140&&mode!="lifecycle"){
    freeCrank=Spin(train,0);std::printf("FREE crank_radps=%.9g before_radps=%.9g wheel_radps=%.9g\n",freeCrank,initialCrank,Spin(train,3));
    Check(freeCrank>initialCrank+20&&std::abs(Spin(train,3))<2,"open clutch lets finite-inertia engine rev without driving wheel terminals");
    key(SDL_SCANCODE_F,true);key(SDL_SCANCODE_SPACE,false);
   }
   if(tick==141)key(SDL_SCANCODE_F,false);
   if(tick==142&&mode!="lifecycle"){
    Check(Spin(train,0)<freeCrank*.99,"clutch take-up changes actual engine RPM through reflected mechanical load");
   }
   if(tick==230&&mode!="lifecycle"){
    const auto delta=world.Physics().GetTransform(chassis).position-initial;drivenSpeed=glm::length(world.Physics().GetLinearVelocity(chassis));
    std::printf("DRIVE forward_m=%.9g speed_mps=%.9g crank_radps=%.9g wheel_radps=%.9g residual=%.9g iterations=%u\n",glm::dot(delta,forward),drivenSpeed,Spin(train,0),Spin(train,3),train->state.residual,train->state.iterations);
    Check(glm::dot(delta,forward)>1&&drivenSpeed>.5f&&std::abs(Spin(train,3))>1,"engine torque reaches chassis displacement through gear, clutch, differential and tyre loads");
    const double differentialRatio=train->command.links[2].overrideRatio?train->command.links[2].ratio:train->settings.links[2].ratio;
    Check(train->state.converged&&std::abs(Spin(train,3)+Spin(train,4)-2*differentialRatio*Spin(train,2))<.05,"open differential satisfies mean-output speed relation within physical publication tolerance");
    key(SDL_SCANCODE_E,true);
   }
   if(tick==231)key(SDL_SCANCODE_E,false);
   if(tick==250&&mode!="lifecycle"){
    Check(train->command.links[0].overrideRatio&&std::abs(train->command.links[0].ratio-1./3)<1e-9&&std::abs(Spin(train,1)-Spin(train,0)/3)<.05,"logical gear selection changes real shaft speed ratio rather than a displayed gear number");
   }
   if(tick==270&&mode!="lifecycle"){key(SDL_SCANCODE_W,false);key(SDL_SCANCODE_SPACE,true);}
   if(tick==340&&mode!="lifecycle"){
    const auto speed=glm::length(world.Physics().GetLinearVelocity(chassis));std::printf("BRAKE speed_mps=%.9g driven_mps=%.9g\n",speed,drivenSpeed);
    Check(speed<drivenSpeed*.7f+.1f,"mechanical and ordinary wheel brakes reduce actual chassis speed without reversing past rest");
    key(SDL_SCANCODE_SPACE,false);key(SDL_SCANCODE_TAB,true);
    electricStart=world.Physics().GetTransform(world.RuntimeBody(200)).position;
   }
   if(tick==341){key(SDL_SCANCODE_TAB,false);key(SDL_SCANCODE_W,true);}
   if(tick==450&&mode!="lifecycle"){
    auto* electric=world.RuntimePowertrain(200);double sum=0;for(auto torque:electric->state.sourceTorque)sum+=std::abs(torque);
    Check(electric->settings.sources.size()==6&&electric->settings.links.empty()&&sum>50&&glm::length(world.Physics().GetTransform(world.RuntimeBody(200)).position-electricStart)>1,"six independent electric sources drive a different actual chassis configuration");
    key(SDL_SCANCODE_TAB,true);
   }
   if(tick==451)key(SDL_SCANCODE_TAB,false);
   if(tick==525)machineEarly=Spin(world.RuntimePowertrain(300),1);
   if(tick==550&&mode!="lifecycle"){
    auto* machine=world.RuntimePowertrain(300);machineFree=Spin(machine,1);
    const auto axis=world.Physics().GetTransform(world.RuntimeBody(300)).rotation*glm::vec3(1,0,0);
    Check(machineFree>.8&&std::abs(machineFree-glm::dot(world.Physics().GetAngularVelocity(world.RuntimeBody(301)),axis))<1e-4,"nonvehicle shaft endpoint matches actual M45-hinged body's angular motion");
    key(SDL_SCANCODE_B,true);
   }
   if(tick==551)key(SDL_SCANCODE_B,false);
   if(tick==575&&mode!="lifecycle"){
    auto* machine=world.RuntimePowertrain(300);const double loaded=Spin(machine,1);
    std::printf("MACHINE early_radps=%.9g free_radps=%.9g loaded_radps=%.9g load_torque=%.9g\n",machineEarly,machineFree,loaded,machine->state.brakeTorque[0]);
    Check(std::abs(machine->state.brakeTorque[0])>20&&loaded-machineFree<(machineFree-machineEarly)*.8,"authored mechanical brake load changes flywheel acceleration and source response");
    key(SDL_SCANCODE_W,false);key(SDL_SCANCODE_SPACE,true);
   }
   if(tick==660&&mode!="lifecycle"){
    Check(std::abs(Spin(world.RuntimePowertrain(300),1))<machineFree*.5,"machine brake dissipates real physical rotor motion");key(SDL_SCANCODE_SPACE,false);
    if(mode=="write"){key(SDL_SCANCODE_F5,true);phase=8;}
    else{std::vector<unsigned char> pixels;host.GetRenderer().CaptureFrame(window->Width(),window->Height(),pixels);WriteRgbPng((output/"lab.png").string(),window->Width(),window->Height(),pixels);completed=true;world.UI().RequestQuit();}
   }
   if(tick==661)key(SDL_SCANCODE_F5,false);
   if(mode=="write"&&phase==8){
    auto* service=world.SceneControl()->Saves(host.Resources());for(uint64_t id=1;id<8;++id)if(auto* status=service->Status(id)){
     if(status->state=="completed"){Check(true,"public save transaction captures graph rotor state with ordinary physical endpoints");completed=true;world.UI().RequestQuit();phase=9;}
     if(status->state=="failed"){std::printf("SAVE %s\n",status->error.c_str());Check(false,"powertrain save transaction succeeds");completed=true;world.UI().RequestQuit();phase=9;}
    }
   }
   if(mode=="lifecycle"&&tick>120){
    auto* streaming=world.SceneControl()->Streaming(world,error);const auto status=streaming->Regions().front();
    if(!spawned)for(auto id:world.DefinitionIds())if(id!=100&&id!=200&&id!=300&&world.RuntimePowertrain(id)&&streaming->Owner(id)=="root"){
     spawned=id;auto* instance=world.RuntimePowertrain(id);const auto children=Wheels(world,id);
     Check(children.size()==4&&instance->settings.shafts[0].bearing==id&&std::find(children.begin(),children.end(),instance->settings.shafts[3].target)!=children.end(),"complete runtime prefab remaps chassis bearings and terminal-wheel identities");break;
    }
    if(phase==0&&status.state=="active"){
     streamed=streaming->Resolve("garage",1);auto* instance=world.RuntimePowertrain(streamed);
     Check(streamed&&instance&&Wheels(world,streamed).size()==4,"registered streamed region publishes a complete mechanical graph and endpoints");
     if(instance){instance->command.sources[0].input=.37;instance->absoluteSpin[0]=113.75;retained=TrainBytes(instance);DumpTrain(output/"retained-state.txt",instance);DumpBytes(output/"retained-powertrain.bin",retained);}streaming->Release(region);frozen=true;phase=1;
    }
    if(phase==1&&status.state=="unloaded"){Check(!world.RuntimeDefinition(streamed)&&status.retained>0,"stream suspension retires old graph identities and retains durable state");region=streaming->Request("garage",false,error);phase=2;}
    if(phase==2&&status.state=="active"){
     const auto restored=streaming->Resolve("garage",1);auto* instance=world.RuntimePowertrain(restored);
     const auto restoredBytes=TrainBytes(instance);
     if(restoredBytes!=retained){DumpTrain(output/"restored-state.txt",instance);DumpBytes(output/"restored-powertrain.bin",restoredBytes);std::printf("STREAM old_id=%llu new_id=%llu retained_bytes=%zu restored_bytes=%zu\n",static_cast<unsigned long long>(streamed),static_cast<unsigned long long>(restored),retained.size(),restoredBytes.size());}
     Check(restored&&restored!=streamed&&instance&&restoredBytes==retained,"stream revisit restores exact graph commands and rotor momentum with remapped identities");
     if(spawned){world.DestroyHierarchy(spawned,error);Check(!world.RuntimePowertrain(spawned),"prefab root destruction safely retires graph and endpoints");}frozen=false;world.SceneControl()->Reload(error);phase=4;
    }
    if(phase==4&&play.FixedStepsSinceReset()<4){Check(healthy&&world.RuntimePowertrain(100)&&Wheels(world,100).size()==4,"reload restores authored graph without stale runtime ownership");completed=true;world.UI().RequestQuit();}
   }
  }
  if(mode=="read"&&phase==3){
   std::map<std::string,SaveChunk> chunks;
   if(WorldPersistence::Capture(world,chunks,error)){
    Check(play.FixedStepsSinceReset()==0,"cold-save observation precedes resumed physics");
    Check(chunks.count("powertrains")&&snapshot.participants.count("powertrains")&&chunks.at("powertrains").data==snapshot.participants.at("powertrains").data,"cold restore exactly preserves graph settings, commands and finite rotor momentum");
    for(const char* name:{"saved","restored"}){std::ofstream stream(output/(std::string(name)+"-powertrains.bin"),std::ios::binary);const auto& data=std::string(name)=="saved"?snapshot.participants.at("powertrains").data:chunks.at("powertrains").data;stream.write(data.data(),data.size());}
    initial=world.Physics().GetTransform(world.RuntimeBody(301)).position;machineFree=glm::length(world.Physics().GetAngularVelocity(world.RuntimeBody(301)));frozen=false;key(SDL_SCANCODE_W,true);ready=frame;phase=4;
   }else if(error!="loading"&&error.find("required script instance not initialized")==std::string::npos){std::printf("RESTORE %s\n",error.c_str());Check(false,"restored scripts initialize normally");completed=true;world.UI().RequestQuit();}
  }
  if(mode=="read"&&phase==4&&frame>ready+45){
   Check(glm::length(world.Physics().GetAngularVelocity(world.RuntimeBody(301)))>machineFree+.2&&healthy,"cold-restored physical endpoint responds to new ordinary input, beyond cached graph observations");completed=true;world.UI().RequestQuit();
  }
  if(negativeMode&&phase==30&&negativeRequest){
   auto* service=world.SceneControl()->Saves(host.Resources());if(const auto* status=service->Status(negativeRequest);status&&(status->state=="failed"||status->state=="completed"||status->state=="cancelled")){
    std::printf("NEGATIVE mode=%s state=%s error=%s\n",mode.c_str(),status->state.c_str(),status->error.c_str());
    Check(status->state=="failed"&&status->error=="powertrain participant does not match snapshot components","missing/phantom powertrain participant fails before private publication with its exact diagnostic");
    Check(&world==original&&healthy&&world.RuntimePowertrain(100)&&world.RuntimeBody(301).IsValid(),"rejected load leaves the original world, scripts and actual mechanical endpoints published and healthy");
    completed=true;world.UI().RequestQuit();phase=31;
   }
  }
  if(frame>2200){Check(false,"bounded application deadline");world.UI().RequestQuit();}
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
 };
 Application application;std::string projectArg=project;
 if(mode=="oblique"||mode=="radial"||mode=="contracts")projectArg=(std::filesystem::absolute(project).parent_path()/"Scenes"/(mode+".judas")).string();
 char name[]="judas";char* args[]={name,projectArg.data()};Check(application.Run(2,args,&control)==0&&completed,"normal application startup and shutdown");
 if(!fixedTimes.empty()){std::sort(fixedTimes.begin(),fixedTimes.end());std::sort(frameTimes.begin(),frameTimes.end());std::printf("PERF mode=%s samples=%zu fixed_median_ms=%.6f p95_ms=%.6f max_ms=%.6f frame_median_ms=%.6f frame_p95_ms=%.6f\n",mode.c_str(),fixedTimes.size(),fixedTimes[fixedTimes.size()/2],fixedTimes[size_t(.95*(fixedTimes.size()-1))],fixedTimes.back(),frameTimes[frameTimes.size()/2],frameTimes[size_t(.95*(frameTimes.size()-1))]);}
 std::printf("M82 application %s checks=%u failures=%u\n",mode.c_str(),checks,failures);return failures?1:0;
}
