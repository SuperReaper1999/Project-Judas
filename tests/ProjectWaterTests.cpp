#include "ProjectFeaturePolicy.h"
#include "BuildIdentity.h"
#include "BasicWater.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "NamedAuthoring.h"
#include "Prefab.h"
#include "GameSession.h"
#include "Simulation.h"
#include "InputSystem.h"
#include "ProductionFluidCoupling.h"
#include "LiquidGeometry.h"
#include "WorldPersistence.h"
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

namespace {
int checks=0,failures=0;
void Check(bool value,const std::string& label){++checks;failures+=!value;std::cout<<(value?"PASS ":"FAIL ")<<label<<'\n';}
bool Near(float a,float b,float e=1e-4f){return std::abs(a-b)<e;}
Scene WaterScene(bool advancedComponents=false){
    Scene s;s.Settings().name="Basic and advanced policy";
    auto& water=s.CreateObject("Pool");water.basicWater=BasicWaterSettings{};water.basicWater->flow={.4f,0,0};
    auto& gravity=s.CreateObject("Gravity");gravity.gravity=SceneGravityComponent{};gravity.gravity->kind=SceneGravityKind::Uniform;gravity.gravity->magnitude=9.81f;gravity.gravity->regionRadius=10000;
    if(advancedComponents){auto& fluid=s.CreateObject("Legacy particle field");fluid.fluidVolume=SceneFluidVolumeComponent{};fluid.fluidVolume->countX=2;fluid.fluidVolume->countY=2;fluid.fluidVolume->countZ=2;fluid.fluidVolume->spacing=.1f;}
    return s;
}
}
int main(){
    std::string error;
    // The expected contract comes from the compiled channel, not the policy
    // function under test: accidentally unlocking Alpha must fail this suite.
    const bool alphaChannel = std::string(GetBuildIdentity().channel) == "Alpha";
    Check((AdvancedFluidDynamicsRestriction() != nullptr) == alphaChannel,
          "advanced-fluid restriction agrees with independently observed build channel");
    ProjectSettings defaults;defaults.name="Water policy";defaults.startupScene="Scenes/main.judas";defaults.legacyGameplay=false;
    Check(defaults.basicWater&&defaults.waterSwimming&&!defaults.advancedFluidDynamics,"basic water/swimming on, advanced off by default");
    auto text=Project::SerializeToString(defaults);ProjectSettings parsed;
    Check(Project::ParseFromString(text,parsed,error)&&parsed.basicWater&&parsed.waterSwimming&&!parsed.advancedFluidDynamics,"project policy round trip");
    auto enabled=defaults;enabled.advancedFluidDynamics=true;enabled.basicWater=false;enabled.waterSwimming=false;
    Check(Project::ParseFromString(Project::SerializeToString(enabled),parsed,error)&&parsed.advancedFluidDynamics&&!parsed.basicWater&&!parsed.waterSwimming,"all policy values persist independently");
    auto legacy=text;for(const auto* key:{"basic-water","water-swimming","advanced-fluid-dynamics"}){auto pos=legacy.find(std::string(key)+" ");auto end=legacy.find('\n',pos);legacy.erase(pos,end-pos+1);}
    Check(Project::ParseFromString(legacy,parsed,error)&&!parsed.advancedFluidDynamics&&parsed.basicWater,"missing old fields select safe defaults");
    for(const char* bad:{"yes","1","TRUE","false garbage"}){auto malformed=text;auto p=malformed.find("advanced-fluid-dynamics \"false\"");malformed.replace(p,std::string("advanced-fluid-dynamics \"false\"").size(),std::string("advanced-fluid-dynamics \"")+bad+"\"");auto sentinel=parsed;sentinel.name="sentinel";Check(!Project::ParseFromString(malformed,sentinel,error)&&sentinel.name=="sentinel","invalid project boolean rejected atomically");}
    std::string named;
    Check(LegacyToNamed(Project::SerializeToString(enabled),"project",named,error)&&Project::ParseFromString(named,parsed,error)&&parsed.advancedFluidDynamics&&!parsed.basicWater&&!parsed.waterSwimming,"named authoring preserves all project policy flags");
    BasicWaterSettings water;auto encoded=EncodeBasicWater(water);BasicWaterSettings decoded;
    Check(DecodeBasicWater(encoded,decoded,error)&&EncodeBasicWater(decoded)==encoded,"water component exact codec round trip");
    auto malformed=encoded;auto p=malformed.find("\"version\":1");malformed.replace(p,11,"\"version\":2");Check(!DecodeBasicWater(malformed,decoded,error),"unknown water schema rejected");
    Check(!DecodeBasicWater("{\"version\":1,\"density\":true}",decoded,error),"boolean is not a numeric density");
    Check(!DecodeBasicWater("{\"version\":1,\"invented\":3}",decoded,error),"unknown water field rejected");
    for(float bad:{-1.f,0.f,std::numeric_limits<float>::infinity()}){auto invalid=water;invalid.halfExtents.x=bad;Check(!ValidateBasicWater(invalid,error),"invalid water geometry rejected");}
    BasicWaterInstance instance;instance.settings=water;instance.settings.flow={2,0,0};
    Check(Near(SampleBasicWater({instance},{0,-1,0},{0,1,0},.3f,.2f).immersion,1),"fully immersed capsule samples wet without a fluid solver");
    Check(SampleBasicWater({instance},{0,1,0},{0,1,0},.3f,.2f).immersion==0,"dry capsule samples dry");
    auto edge=SampleBasicWater({instance},{0,0,0},{0,1,0},.3f,.2f);Check(edge.immersion>.4f&&edge.immersion<.6f,"surface partial immersion");
    Check(SampleBasicWater({instance},{50,-1,0},{0,1,0},.3f,.2f).immersion==0,"outside footprint is dry");
    Check(Near(SampleBasicWater({instance,instance},{0,-1,0},{0,1,0},.3f,.2f).immersion,1),"overlap never double counts immersion");
    instance.rotation=glm::angleAxis(1.17f,glm::normalize(glm::vec3(.3f,.6f,.2f)));instance.position={3,8,-5};
    auto rotated=SampleBasicWater({instance},instance.position+instance.rotation*glm::vec3(0,-1,0),instance.rotation*glm::vec3(0,1,0),.3f,.2f);
    Check(Near(rotated.immersion,1)&&glm::length(rotated.velocity-instance.rotation*glm::vec3(2,0,0))<1e-4f,"rotated water/swimming/current are covariant");
    auto view=glm::lookAt(glm::vec3(0,3,0),glm::vec3(0,-1,0),glm::vec3(0,0,-1));auto proj=glm::perspective(glm::radians(60.f),1.f,.1f,50.f);
    instance={};instance.settings=water;auto paths=BasicWaterOpticalPaths({instance},view,proj,1,1);
    Check(paths.size()==1&&Near(paths[0].x,2.9f,.001f)&&Near(paths[0].y,4.9f,.001f),"underwater optical interval uses real water depth");
    Check(BasicWaterOpticalPaths({},view,proj,1,1)[0]==glm::vec2(0),"no water produces no attenuation");
    auto scene=WaterScene(true);std::string bytes;Check(SaveSceneToString(scene,bytes),"water scene serializes");Scene restored;
    Check(LoadSceneFromString(bytes,restored,error)&&ScenesEqual(scene,restored),"water scene persistence preserves component");
    std::string before,after;Check(ComputeSceneFingerprint(scene,before,error),"water scene fingerprint");restored.Find(1)->basicWater->waveSpeed+=.5f;
    Check(ComputeSceneFingerprint(restored,after,error)&&before!=after,"wave parameters participate in content identity");
    RuntimeWorld world;world.ConfigureProject(defaults);Check(world.Build(scene,nullptr,error),"default world constructs authored basic and dormant advanced components: "+error);
    Check(!world.HasFluid()&&world.FluidVolumes().empty()&&world.BasicWaters().size()==1,"default world allocates no particle field or particle owners");
    bool noParticleStorage=false;try{world.Fluid();}catch(const std::logic_error&){noParticleStorage=true;}Check(noParticleStorage,"default world has no hidden FluidWorld allocation");
    Check(world.SampleWater({0,-1,0},{0,1,0},.3f,.2f).immersion==1,"runtime water query works with advanced disabled");
    SceneObject forbidden;forbidden.gravity=SceneGravityComponent{};Check(!world.CreateEntity(forbidden,nullptr,&error)&&error.find("scene-global")!=std::string::npos,"cold metadata repair does not permit runtime creation of scene-global gravity");
    GameSession game;Check(game.Begin(world,error),"default world session begins");InputSystem input;
    for(unsigned i=0;i<120;++i){input.BeginFixedStep();StepAuthoritativeWorld(game,input,1.f/60);}
    Check(world.Liquids().States().empty()&&world.Liquids().Parcels().empty()&&world.Liquids().StepTimes().total==0&&!world.HasFluid(),"120 real steps perform no advanced fluid work");
    auto denied=world.Liquids().AddBasin(1,{}, {},nullptr,error);Check(!denied.id&&error.find("disabled")!=std::string::npos,"disabled world rejects even native liquid-owner registration before surface allocation");
    Check(world.EmitFluidParticle()==false,"default emitter cannot activate a hidden particle solver");
    bool locked=false;try{world.ConfigureProject(enabled);}catch(const std::logic_error&){locked=true;}Check(locked,"live project policy cannot mutate under an existing world");
    std::map<std::string,SaveChunk> chunks;Check(WorldPersistence::Capture(world,chunks,error),"cold-save capture with dormant advanced authorship: "+error);
    Scene cold;Check(WorldPersistence::SceneFrom(chunks,cold,error),"cold save reconstructs basic water authoring");RuntimeWorld coldWorld;coldWorld.ConfigureProject(defaults);
    const bool coldBuilt=coldWorld.Build(cold,nullptr,error,nullptr,nullptr,true);
    for(const auto& o:cold.Objects()){EntityPhysicalState a,b;std::cout<<"Cold diagnostic entity "<<o.id<<" original state="<<world.GetEntityState(o.id,a)<<" restored state="<<coldWorld.GetEntityState(o.id,b)<<" name="<<o.name<<"\n";}
    const bool coldRestored=coldBuilt&&WorldPersistence::Restore(coldWorld,chunks,error);
    Check(coldRestored,"cold restore uses ordinary persistence with default policy: "+error);
    Check(coldWorld.BasicWaters().size()==1&&coldWorld.SampleWater({0,-1,0},{0,1,0},.3f,.2f).immersion==1&&!coldWorld.HasFluid(),"restored water remains usable without a fluid solver");
    game.End();
    SceneObject region;region.id=777;region.name="Streamed water";region.basicWater=BasicWaterSettings{};region.transform.position={20,0,0};
    Check(world.StageRegionObject(region,region,error)&&world.BasicWaters().size()==1,"staged regional water is private before publication");
    world.PublishRegion({777});Check(world.BasicWaters().size()==2&&world.SampleWater({20,-1,0},{0,1,0},.3f,.2f).immersion==1,"published region supplies ordinary water queries");
    world.HideRegion({777});Check(world.BasicWaters().size()==1,"hidden departing region cannot supply water");
    world.RemoveRegionObject(777);Check(world.BasicWaters().size()==1,"removed region retires its water owner");
    Scene prefab;Check(CreatePrefab(scene,1,prefab,error)&&prefab.Objects().front().basicWater.has_value(),"ordinary prefab authoring preserves basic water");
    Check(world.RequestOrigin({512,0,0},error)&&world.CommitOrigin(),"basic water supports existing live origin shifts");
    Check(world.SampleWater({-512,-1,0},{0,1,0},.3f,.2f).immersion==1,"water frame follows shifted world");
    world.Destroy();Check(world.Build(scene,nullptr,error)&&!world.HasFluid()&&world.BasicWaters().size()==1,"Play/Stop reconstruction retains immutable project policy");
    auto optin=defaults;optin.advancedFluidDynamics=true;RuntimeWorld advanced;advanced.ConfigureProject(optin);
    if (AdvancedFluidDynamicsRestriction()) {
        Check(!advanced.AdvancedFluidsEnabled(),"Alpha denies effective activation even before Build");
        Check(!advanced.Build(scene,nullptr,error)&&error.find("contact stability")!=std::string::npos,
              "Alpha rejects explicit opt-in before constructing the world");
        Check(!advanced.IsBuilt(),"rejected Alpha world is not published");
        bool noField=false;try{advanced.Fluid();}catch(const std::logic_error&){noField=true;}
        Check(noField,"rejected Alpha opt-in allocates no particle field");
        Check(advanced.Liquids().States().empty()&&advanced.Liquids().Parcels().empty(),
              "rejected Alpha opt-in creates no advanced liquid owners");
        Check(!advanced.Build(Scene{},nullptr,error),"even an empty scene cannot bypass Alpha project policy");
        Check(!advanced.Build(scene,nullptr,error,nullptr,nullptr,true),"cold snapshot construction cannot bypass Alpha policy");
        Check(!advanced.EmitFluidParticle(),"rejected Alpha project cannot emit particles");
    } else {
    Check(advanced.Build(scene,nullptr,error)&&advanced.HasFluid()&&advanced.Fluid().Particles().size()==8,"explicit opt-in creates existing PBF field");
    GameSession advancedGame;Check(advancedGame.Begin(advanced,error),"opt-in session begins");for(unsigned i=0;i<4;++i){input.BeginFixedStep();StepAuthoritativeWorld(advancedGame,input,1.f/60);}
    Check(advanced.FluidCoupling().Measurements().executedSteps>0&&advanced.HasFluid(),"opt-in existing particle path executes");
    advancedGame.End();
    }
    auto off=defaults;off.basicWater=false;RuntimeWorld noWater;noWater.ConfigureProject(off);Check(noWater.Build(WaterScene(),nullptr,error)&&noWater.BasicWaters().empty()&&noWater.SampleWater({0,-1,0},{0,1,0},.3f,.2f).immersion==0,"basic-water project switch controls rendering volumes and queries");
    auto noSwim=defaults;noSwim.waterSwimming=false;RuntimeWorld visualOnly;visualOnly.ConfigureProject(noSwim);Check(visualOnly.Build(WaterScene(),nullptr,error)&&visualOnly.BasicWaters().size()==1&&visualOnly.SampleWater({0,-1,0},{0,1,0},.3f,.2f).immersion==0,"swimming can be disabled independently of water surfaces");
    std::cout<<"ProjectWaterTests: "<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
