#include "AISensing.h"
#include "RuntimeWorld.h"
#include "Project.h"
#include "PlanarGeometry.h"
#include "../third_party/nlohmann/json.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <chrono>
#include <algorithm>

namespace {
int checks=0,failures=0;
void Check(bool condition,const char* message) {++checks;if(!condition){++failures;std::cerr<<"FAIL "<<message<<'\n';}}
template<class F>void Reject(F&& fn,const char* message){try{fn();Check(false,message);}catch(const std::invalid_argument&){Check(true,message);}}
SceneObjectId Body(Scene& scene,const char* name,glm::vec3 position,unsigned layer=0,CategoryMask tags=0,bool sensor=false) {
    auto& o=scene.CreateObject(name);o.transform.position=position;o.tags=tags;
    o.body=SceneBodyComponent{};o.body->motion=SceneBodyMotion::Kinematic;o.body->halfExtents={.2f,.2f,.2f};o.body->collisionLayer=layer;o.body->sensor=sensor;
    return o.id;
}
const AISensingTarget* Find(const AISensingResult& result,SceneObjectId id) {
    for(const auto& target:result.targets)if(target.entityId==id)return &target;
    return nullptr;
}
}
int Benchmark() {
    using Clock=std::chrono::steady_clock;
    std::cout<<"targets,cached,iterations,median_us,p95_us,worst_us,query_candidates,raycasts,truncated_samples\n";
    for(int count:{64,256}) {
        ProjectSettings project;unsigned tag=0;project.classification.tags.Add("target",tag);
        Scene scene;const auto owner=Body(scene,"observer",{0,0,0});
        for(int i=0;i<count;++i)Body(scene,"target",{float(i%16+1),0,float(i/16+1)},0,1);
        RuntimeWorld world;world.ConfigureProject(project);std::string error;
        if(!world.Build(scene,nullptr,error,&project.classification)){std::cerr<<error;return 2;}
        AISensingRequest query;query.radius=50;query.targets.requiredTags=1;query.visibility=true;
        for(bool cached:{false,true}) {
            AISensing sensor({cached?1.:0.,16,8,128});sensor.Sample(world,owner,query);
            std::vector<double> times;std::size_t candidates=0,rays=0,truncated=0;
            for(int i=0;i<100;++i) {
                const auto began=Clock::now();const auto result=sensor.Sample(world,owner,query);
                times.push_back(std::chrono::duration<double,std::micro>(Clock::now()-began).count());
                candidates+=result.queryCandidates;rays+=result.raycasts;truncated+=result.truncated;
                if(result.cached!=cached||result.targets.size()>16||result.raycasts>8)return 3;
            }
            std::sort(times.begin(),times.end());
            std::cout<<count<<','<<cached<<",100,"<<(times[49]+times[50])/2<<','<<times[94]<<','<<times.back()<<','<<candidates<<','<<rays<<','<<truncated<<'\n';
        }
    }
    return 0;
}
int main(int argc,char** argv) {
    if(argc==2&&std::string(argv[1])=="--benchmark")return Benchmark();
    ProjectSettings project;unsigned ignored=0;
    project.classification.tags.Add("target",ignored);project.classification.collision.Add("Target",ignored);
    Scene scene;
    const auto owner=Body(scene,"observer",{0,0,0});
    const auto a=Body(scene,"target A",{3,0,0},1,1);
    const auto b=Body(scene,"target B",{0,3,0},1,1);
    const auto incoming=Body(scene,"outside target",{9,0,0},1,1);
    const auto trigger=Body(scene,"trigger target",{-2,0,0},1,1,true);
    const auto wall=Body(scene,"occluder",{1.5f,0,0});
    scene.Find(wall)->body->halfExtents={.2f,.5f,.5f};
    RuntimeWorld world;world.ConfigureProject(project);std::string error;
    Check(world.Build(scene,nullptr,error,&project.classification),"normal published physics world builds");
    AISensingRequest request;request.origin={0,0,0};request.radius=6;request.targets.requiredTags=1;
    request.targets.includeLayers=CategoryBit(1);request.visibility=true;
    AISensing sensor;
    const auto first=sensor.Sample(world,owner,request);
    Check(first.targets.size()==2,"native spatial query filters own body, tags, layers and sensors");
    Check(first.targets.size()==2&&first.targets[0].entityId==a&&first.targets[1].entityId==b,"equidistant targets use stable entity identity order");
    Check(first.raycasts==2&&Find(first,a)&&Find(first,a)->visible==false&&Find(first,b)&&Find(first,b)->visible==true,
          "real MK2 rays detect wall independently of target tag/layer filters");
    const auto same=sensor.Sample(world,owner,request);
    Check(same.cached&&same.stale&&same.age==0&&same.raycasts==0&&same.queryCandidates==0,"same-frame cache does no extra queries and never claims a freshly verified observation");
    auto transform=world.RuntimeDefinition(wall)->transform;transform.position={0,1.5f,0};
    Check(world.SetRuntimeTransform(wall,transform),"move actual wall between observations");
    const auto changedSameTick=sensor.Sample(world,owner,request);
    Check(changedSameTick.cached&&changedSameTick.stale&&changedSameTick.age==0&&Find(changedSameTick,b)->visible==true,
          "same-tick occluder movement cannot make a reused snapshot claim current visibility");
    world.AdvanceSimulationTime(.1);
    const auto stale=sensor.Sample(world,owner,request);
    Check(stale.cached&&stale.stale&&std::abs(stale.age-.1)<1e-12&&Find(stale,b)->visible==true,
          "timed cached visibility is explicitly stale, not asserted current after an unseen occluder moves");
    world.AdvanceSimulationTime(.11);
    const auto refreshed=sensor.Sample(world,owner,request);
    Check(!refreshed.cached&&!refreshed.stale&&Find(refreshed,a)->visible==true&&Find(refreshed,b)->visible==false,
          "interval expiry recomputes both clear and obstructed visibility using actual moved wall");
    transform=world.RuntimeDefinition(a)->transform;transform.position={4,0,0};world.SetRuntimeTransform(a,transform);
    const auto moved=sensor.Sample(world,owner,request);
    Check(!moved.cached&&Find(moved,a)&&Find(moved,a)->distance==4,"retained target movement invalidates cache without waiting for cadence");
    transform=world.RuntimeDefinition(incoming)->transform;transform.position={1,0,0};world.SetRuntimeTransform(incoming,transform);
    const auto unseen=sensor.Sample(world,owner,request);
    Check(unseen.cached&&!Find(unseen,incoming),"new arrivals are not invented by a cached observation");
    world.AdvanceSimulationTime(.21);
    const auto arrival=sensor.Sample(world,owner,request);
    Check(!arrival.cached&&Find(arrival,incoming)&&arrival.targets[0].entityId==incoming,"new arrivals appear at next bounded observation");
    Check(world.DestroyEntity(a,&error),"retained target destruction succeeds");
    const auto removed=sensor.Sample(world,owner,request);
    Check(!removed.cached&&!Find(removed,a),"destroyed retained identity never survives cache");
    world.RemoveTag(b,0);
    const auto reclassified=sensor.Sample(world,owner,request);
    Check(!reclassified.cached&&!Find(reclassified,b),"runtime target-tag changes invalidate retained membership");
    auto sensors=request;sensors.targets.includeSensors=true;
    const auto triggerResult=sensor.Sample(world,owner,sensors);
    Check(!triggerResult.cached&&Find(triggerResult,trigger),"changed query options refresh and include authored sensors deliberately");
    AISensing limited({.2,2,1,128});
    const auto budget=limited.Sample(world,owner,sensors);
    unsigned observed=0;for(const auto& target:budget.targets)observed+=target.visible.has_value();
    Check(budget.targets.size()<=2&&budget.raycasts==1&&observed==1,"actual ray count is bounded; unchecked visibility is unknown");
    AISensing outputCap({.2,1,8,128});const auto shortened=outputCap.Sample(world,owner,sensors);
    Check(shortened.targets.size()==1&&shortened.truncated,"limited target list marks omitted results explicitly");
    AISensing noRays({.2,16,0,128});const auto unknown=noRays.Sample(world,owner,sensors);
    Check(!unknown.targets.empty()&&!unknown.targets[0].visible&&unknown.raycasts==0,"zero ray budget does not fabricate visibility");
    AISensing candidateCap({.2,1,0,1});
    const auto partial=candidateCap.Sample(world,owner,sensors);
    Check(partial.candidatesConsidered==1&&partial.queryCandidates>1&&partial.truncated,"post-query work cap and uncapped broadphase match count are distinguished");
    AISensing fresh({0,16,0,128});
    fresh.Sample(world,owner,sensors);Check(!fresh.Sample(world,owner,sensors).cached,"zero interval explicitly disables reuse");
    auto invalid=request;invalid.radius=-1;
    Reject([&]{sensor.Sample(world,owner,invalid);},"negative radius rejects");
    invalid=request;invalid.origin.x=std::numeric_limits<float>::quiet_NaN();
    Reject([&]{sensor.Sample(world,owner,invalid);},"nonfinite origin rejects");
    invalid=request;invalid.targets.ignoredBodies.push_back(world.RuntimeBody(owner));
    Reject([&]{sensor.Sample(world,owner,invalid);},"unsupported custom ignore vectors cannot escape cache identity");
    Reject([&]{AISensing bad({-1,16,8,128});},"negative cadence rejects");
    Reject([&]{AISensing bad({.2,0,8,128});},"zero result budget rejects");
    Reject([&]{AISensing bad({.2,16,129,128});},"oversized ray budget rejects");
    Reject([&]{AISensing bad({.2,16,8,8});},"candidate budget below result bound rejects");
    const auto before=sensor.Sample(world,owner,sensors);
    const auto originBefore=world.OriginRevision();
    const bool shifted=world.RequestOrigin({1024,0,0},error)&&world.CommitOrigin();
    Check(shifted&&world.OriginRevision()!=originBefore,"actual origin chart shift succeeds");
    sensors.origin=world.Physics().GetTransform(world.RuntimeBody(owner)).position;
    const auto after=sensor.Sample(world,owner,sensors);
    Check(!after.cached&&after.targets.size()==before.targets.size()&&after.targets[0].distance==before.targets[0].distance,
          "origin shift cannot reuse old chart positions and preserves physical distances");
    sensor.Invalidate();Check(!sensor.Sample(world,owner,sensors).cached,"explicit lifecycle invalidation drops cache");
    const auto encoded=nlohmann::json::parse(after.Json());
    Check(encoded.at("targets")[0].at("entityId").is_string()&&encoded.at("targets")[0].at("visible").is_boolean(),
          "public snapshots retain string identities and tested visibility values");
    Check(world.DestroyEntity(owner,&error),"requester destruction succeeds");Reject([&]{sensor.Sample(world,owner,sensors);},"destroyed requester cannot query through old sensor");

    // Reuse the same world and sensor object across normal scene rebuilds.
    Check(world.Build(scene,nullptr,error,&project.classification),"world rebuild with same native object succeeds");
    request.origin={0,0,0};
    const auto rebuilt=sensor.Sample(world,owner,request);
    Check(!rebuilt.cached&&Find(rebuilt,a)&&Find(rebuilt,a)->position==glm::vec3(3,0,0),"rebuilt world cannot inherit an old incarnation's geometry");
    world.Destroy();

    // Spatial sensing has no world-up or gravity selection policy.
    auto rotatedScene=scene;
    const auto q=glm::angleAxis(.63f,glm::normalize(glm::vec3(1,-2,3)));
    for(auto& object:rotatedScene.Objects()) {object.transform.position=q*object.transform.position;object.transform.rotation=q*object.transform.rotation;}
    RuntimeWorld rotated;rotated.ConfigureProject(project);
    Check(rotated.Build(rotatedScene,nullptr,error,&project.classification),"rotated physical scene builds");
    AISensing covariant;const auto turned=covariant.Sample(rotated,owner,request);
    Check(Find(turned,a)&&Find(turned,a)->visible==false&&Find(turned,b)&&Find(turned,b)->visible==true,
          "global rotation retains physical occlusion and clear-target answers");
    Check(Find(turned,a)&&std::abs(Find(turned,a)->distance-3)<1e-5,"global rotation retains physical centre distance");
    rotated.Destroy();

    // Normal CharacterMotor actors publish their query capsule as RuntimeBody.
    // Query before asking for a RuntimeCharacter getter: the sensor itself must
    // observe normal published geometry, not accidentally materialize a target.
    Scene motorScene;
    const auto motorObserver=Body(motorScene,"motor observer",{0,0,0});
    auto& character=motorScene.CreateObject("motor target");const auto motorId=character.id;
    character.transform.position={3,0,0};character.tags=1;
    character.characterMotor=CharacterMotorSettings{};
    character.characterMotor->collisionLayer=1;character.characterMotor->offset={.3f,.2f,0};
    auto disabled=character;disabled.id=motorScene.CreateObject("disabled motor").id;
    disabled.transform.position={0,3,0};disabled.characterMotor->enabled=false;
    *motorScene.Find(disabled.id)=disabled;
    RuntimeWorld motors;motors.ConfigureProject(project);
    Check(motors.Build(motorScene,nullptr,error,&project.classification),"ordinary CharacterMotor sensing world builds");
    // This is the normal pre-script publication boundary, with no simulated
    // time or motor integration and without calling a target motor getter.
    motors.FixedScripts(nullptr,0);
    AISensing motorSense;request.origin={0,0,0};request.targets.planar=false;
    const auto motorResult=motorSense.Sample(motors,motorObserver,request);
    const auto* motorTarget=Find(motorResult,motorId);
    Check(motorResult.targets.size()==1&&motorTarget&&motorTarget->body.id==motors.RuntimeBody(motorId).id&&
          motors.EntityIdOfBody(motorTarget->body)==motorId,
          "tagged published CharacterMotor capsule is attributed to its actual entity without a getter side effect");
    Check(motorTarget&&motorTarget->visible==true&&glm::distance(motorTarget->position,glm::vec3(3.3f,.2f,0))<1e-6,
          "CharacterMotor sensing reports actual offset capsule centre and real unobstructed LOS");
    Check(!Find(motorResult,disabled.id),"disabled CharacterMotor is not a sensed target");
    Check(motors.RemoveTag(motorId,0),"runtime CharacterMotor tag removal succeeds");
    const auto untaggedMotor=motorSense.Sample(motors,motorObserver,request);
    Check(!untaggedMotor.cached&&!Find(untaggedMotor,motorId),
          "cached CharacterMotor membership invalidates immediately after runtime untagging");
    Check(motors.AddTag(motorId,0),"runtime CharacterMotor tag restoration succeeds");
    motors.AdvanceSimulationTime(.21);
    Check(Find(motorSense.Sample(motors,motorObserver,request),motorId)!=nullptr,
          "next observation rediscovers a retagged CharacterMotor through physical query filtering");
    Check(motors.DestroyEntity(motorId,&error),"CharacterMotor target removal succeeds");
    Check(!Find(motorSense.Sample(motors,motorObserver,request),motorId),
          "removed CharacterMotor and its retired query capsule cannot survive a cached observation");
    motors.Destroy();

    // The same bounded helper uses native planar rays when requested.
    Scene plane;
    const auto observer=Body(plane,"planar observer",{0,0,0});
    const auto goal=Body(plane,"planar goal",{3,0,0},1,1);
    const auto block=Body(plane,"planar blocker",{1.5f,0,0});
    for(auto& object:plane.Objects()){object.body->shape=SceneShape::Planar;object.body->planar=PlanarBox({.2,.2});}
    RuntimeWorld planar;planar.ConfigureProject(project);
    Check(planar.Build(plane,nullptr,error,&project.classification),"normal planar physics scene builds");
    AISensing planarSense;request.targets.planar=true;
    const auto blocked=planarSense.Sample(planar,observer,request);
    Check(Find(blocked,goal)&&Find(blocked,goal)->visible==false,"2D visibility uses real native planar collision");
    Check(planar.DestroyEntity(block,&error),"planar blocker destruction succeeds");
    Check(Find(planarSense.Sample(planar,observer,request),goal)->visible==true,"2D removed blocker no longer occludes");
    std::cout<<"AISensing: "<<checks<<" checks, "<<failures<<" failures\n";
    return failures?1:0;
}
