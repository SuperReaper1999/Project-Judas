#include "RuntimeWorld.h"
#include "WorldPersistence.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "Prefab.h"
#include "SaveArchive.h"
#include "SaveStorage.h"
#include <cstdio>
#include <cmath>
#include <map>

namespace {
unsigned checks=0,failures=0;
void Check(bool value,const char* label){++checks;failures+=!value;std::printf("%s %s\n",value?"PASS":"FAIL",label);}
Scene Fixture(){
    Scene scene;scene.Settings().name="Wheel lifecycle fixture";
    auto& floor=scene.CreateObject("Ground");floor.body=SceneBodyComponent{};floor.body->halfExtents={20,.25f,20};floor.transform.position={0,-.25f,0};
    const auto chassisId=scene.CreateObject("Chassis").id;
    auto* chassis=scene.Find(chassisId);chassis->body=SceneBodyComponent{};chassis->body->motion=SceneBodyMotion::Dynamic;chassis->body->mass=150;chassis->body->managed=true;chassis->body->halfExtents={.5f,.2f,1};chassis->transform.position={0,.55f,0};
    for(unsigned n=0;n<4;++n){auto& wheel=scene.CreateObject("Independent wheel");wheel.parent=chassisId;wheel.wheel=WheelSettings{};wheel.wheel->chassis=chassisId;wheel.wheel->mount={n<2?-.7f:.7f,0,n%2?-.8f:.8f};wheel.transform.position=wheel.wheel->mount+glm::vec3(0,-wheel.wheel->restLength,0);}
    return scene;
}
void Tick(RuntimeWorld& world,unsigned frames){
    for(unsigned i=0;i<frames;++i){world.Physics().ApplyLinearAcceleration(world.RuntimeBody(2),{0,-9.81f,0},1.f/60);world.UpdateWheels(1.f/60);world.Physics().Step(1.f/60);world.PublishWheels();}
}
bool Near(float a,float b,float epsilon=1e-5f){return std::abs(a-b)<=epsilon;}
void ManagedRegionPublication(){
    Scene root;root.Settings().fidelityPolicy=SceneFidelityPolicy::Distance;
    root.Settings().fidelityFullRadius=10;root.Settings().fidelityCoarseRadius=25;
    RuntimeWorld world;world.legacyGameplay=false;std::string error;
    Check(world.Build(root,nullptr,error),"distance-policy region publication fixture builds");
    const auto source=Fixture();SceneObject chassis=*source.Find(2),wheel=*source.Find(3);
    chassis.id=100;chassis.transform.position={1000,.55f,0};
    wheel.id=101;wheel.parent=100;wheel.wheel->chassis=100;
    auto localWheel=wheel;wheel.transform.position=chassis.transform.position+localWheel.transform.position;
    Check(world.StageRegionObject(chassis,chassis,error)&&world.FindEntity(100)->fidelity==SimulationFidelity::Dormant,
          "private distant managed chassis initially follows the ordinary distance policy");
    Check(world.StageRegionObject(wheel,localWheel,error),"wheel is staged after its reduced chassis");
    bool privateBodies=true;for(auto body:world.Physics().AliveBodies())privateBodies&=!world.Physics().IsBodyEnabled(body);
    Check(privateBodies&&!world.RuntimeDefinition(100)&&!world.RuntimeDefinition(101),
          "reconstruction keeps staged bodies disabled and definitions unpublished");
    world.PublishRegion({100,101});auto* runtime=world.RuntimeWheel(101);
    Check(world.FindEntity(100)->fidelity==SimulationFidelity::Full&&world.RuntimeBody(100).IsValid()&&runtime&&runtime->state.available,
          "published wheel assembly reconstructs its distant managed chassis at full fidelity");
    Check(!world.SetEntityFidelity(100,SimulationFidelity::Coarse,&error),"published wheel dependency remains full");
    world.RemoveRegionObject(101);
    Check(world.SetEntityFidelity(100,SimulationFidelity::Coarse,&error),"removing streamed wheel releases its derived retention");

    chassis.id=200;wheel.id=201;wheel.parent=200;wheel.wheel->chassis=200;localWheel=wheel;
    localWheel.transform.position=source.Find(3)->transform.position;
    Check(world.StageRegionObject(wheel,localWheel,error)&&world.StageRegionObject(chassis,chassis,error),
          "the opposite wheel-before-chassis staging order is accepted");
    world.PublishRegion({201,200});runtime=world.RuntimeWheel(201);
    Check(world.FindEntity(200)->fidelity==SimulationFidelity::Full&&world.RuntimeBody(200).IsValid()&&runtime&&runtime->state.available,
          "wheel-before-chassis publication also retains the real physical body");

    chassis.id=300;
    Check(world.StageRegionObject(chassis,chassis,error),"unreferenced distant ordinary body is staged");
    world.PublishRegion({300});
    Check(world.FindEntity(300)->fidelity==SimulationFidelity::Dormant&&!world.RuntimeBody(300).IsValid(),
          "no-wheel region keeps its ordinary reduced-fidelity policy");
}
}
int main(){
    std::string error,text,base,modified;Scene scene=Fixture(),round;
    SaveSceneToString(scene,text);
    Check(LoadSceneFromString(text,round,error)&&ScenesEqual(scene,round),"wheel settings survive ordinary scene serialization");
    Check(ComputeSceneFingerprint(scene,base,error),"wheel scene has an authored physical fingerprint");
    round.Find(3)->wheel->friction=.3f;Check(ComputeSceneFingerprint(round,modified,error)&&modified!=base,"tyre setting changes authored fingerprint");
    auto malformed=scene;malformed.Find(3)->wheel->chassis=999;
    Check(!ComputeSceneFingerprint(malformed,modified,error),"missing chassis reference is rejected by authored validation");
    malformed=scene;malformed.Find(3)->body=SceneBodyComponent{};RuntimeWorld invalid;invalid.legacyGameplay=false;
    Check(!invalid.Build(malformed,nullptr,error),"wheel and rigid body cannot compete for owner motion");
    malformed=scene;malformed.Find(3)->transform.scale={2,1,1};
    Check(!invalid.Build(malformed,nullptr,error),"wheel owner non-unit scale is rejected rather than changing physical radius");
    Scene prefab;Check(CreatePrefab(scene,2,prefab,error),"ordinary hierarchy can become a wheel assembly prefab");
    Scene instances;SceneTransform pose;SceneObjectId first=0,second=0;
    Check(InstantiatePrefab(instances,prefab,"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",pose,first,error),"first normal wheel prefab instance");pose.position={8,0,0};
    Check(InstantiatePrefab(instances,prefab,"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",pose,second,error),"second normal wheel prefab instance");
    bool mapped=first!=second;for(const auto& object:instances.Objects())if(object.wheel)mapped&=object.wheel->chassis==object.prefabRoot;
    Check(mapped,"wheel chassis references remap independently for each prefab instance");
    RuntimeWorld world;world.legacyGameplay=false;Check(world.Build(scene,nullptr,error),"headless generic wheel world builds without player or vehicle policy");
    auto* one=world.RuntimeWheel(3);auto* two=world.RuntimeWheel(4);Check(one&&two&&one!=two,"wheel runtime state is independently owned");
    one->command={18,2,.2f};Tick(world,4);Check(one->state.available&&one->state.contact,"wheel queries authoritative geometry in the normal world");
    Check(two->command.driveTorque==0&&two->command.steeringAngle==0,"commanding one wheel does not alter another wheel");
    const auto presented0=world.PresentedTransform(3,{},0),presented1=world.PresentedTransform(3,{},1),presentedHalf=world.PresentedTransform(3,{},.5f);
    Check(glm::length(presentedHalf.position-glm::mix(presented0.position,presented1.position,.5f))<1e-5f,"visual wheel interpolation uses published fixed poses");
    Check(!world.SetEntityFidelity(2,SimulationFidelity::Coarse,&error),"live wheel reference retains a full physical chassis");
    Check(!world.SetTransientEntity(2,true,error),"required chassis cannot be excluded from a durable save");
    auto bad=one->settings;bad.radius=0;Check(!world.SetWheelSettings(3,bad)&&one->settings.radius==scene.Find(3)->wheel->radius,"invalid wheel configuration has no partial runtime mutation");
    auto configured=one->settings;configured.friction=.42f;Check(world.SetWheelSettings(3,configured),"valid wheel configuration uses normal component setter");
    std::map<std::string,SaveChunk> chunks;Check(WorldPersistence::Capture(world,chunks,error)&&chunks.count("wheels")&&chunks.at("wheels").version==1,"durable save adds only its optional wheel participant");
    Scene saved;RuntimeWorld restored;restored.legacyGameplay=false;
    Check(WorldPersistence::SceneFrom(chunks,saved,error)&&restored.Build(saved,nullptr,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(restored,error)&&WorldPersistence::Restore(restored,chunks,error),"fresh world restores ordinary physical state and wheel participant");
    auto* recovered=restored.RuntimeWheel(3);
    Check(recovered&&recovered->command.driveTorque==one->command.driveTorque&&recovered->command.brakeTorque==one->command.brakeTorque&&recovered->command.steeringAngle==one->command.steeringAngle&&Near(recovered->state.angularVelocity,one->state.angularVelocity)&&Near(recovered->state.angle,one->state.angle)&&Near(recovered->state.length,one->state.length),"new process wheel continuation restores exact command, spin, phase and travel");
    Check(recovered&&Near(recovered->settings.friction,.42f)&&glm::length(recovered->state.center-one->state.center)<1e-5f,"restored configuration and physical presentation have no zero-pose flash");
    auto incomplete=chunks;incomplete.erase("wheels");Check(!WorldPersistence::Restore(restored,incomplete,error),"missing wheel participant fails explicitly instead of resetting spin");
    GameSnapshot envelope;envelope.project="wheel-project";envelope.scene="scene";envelope.content=base;envelope.metadata="{}";envelope.gameVersion=1;
    for(unsigned i=0;i<18;++i)envelope.participants["participant-"+std::to_string(i)]={1,""};
    GameSnapshot decoded;
    Check(DecodeGameSnapshot(EncodeGameSnapshot(envelope),decoded,error)&&decoded.participants.size()==18,"save envelope admits all existing optional participants plus wheels");
    auto* reset=world.RuntimeWheel(3);reset->command={50,4,.1f};reset->state.angularVelocity=10;
    world.RestoreAuthoredState();reset=world.RuntimeWheel(3);
    Check(reset&&reset->command.driveTorque==0&&reset->command.brakeTorque==0&&reset->state.angularVelocity==0&&reset->settings.friction==scene.Find(3)->wheel->friction,"authored reset discards spin, controls and runtime wheel configuration");
    for(EntityId id:{3,4,5,6})Check(world.DestroyEntity(id,&error),"single wheel owner can be destroyed independently");
    Check(world.SetEntityFidelity(2,SimulationFidelity::Coarse,&error),"last wheel removal releases only its derived fidelity requirement");
    Check(world.SetTransientEntity(2,true,error),"last wheel removal releases the required-save dependency");
    RuntimeWorld changed;changed.legacyGameplay=false;Check(changed.Build(scene,nullptr,error),"reference replacement fixture builds");
    SceneObject replacement;replacement.body=SceneBodyComponent{};replacement.body->motion=SceneBodyMotion::Dynamic;replacement.body->managed=true;replacement.transform.position={8,.55f,0};
    const auto replacementId=changed.CreateEntity(replacement,nullptr,&error);bool reassigned=replacementId!=0;
    for(EntityId id:{3,4,5,6}){auto settings=changed.RuntimeWheel(id)->settings;settings.chassis=replacementId;reassigned&=changed.SetWheelSettings(id,settings);}
    Check(reassigned&&changed.SetEntityFidelity(2,SimulationFidelity::Coarse,&error)&&!changed.SetEntityFidelity(replacementId,SimulationFidelity::Coarse,&error),"changing chassis references releases the old body and retains the new body");
    RuntimeWorld gone;gone.legacyGameplay=false;Check(gone.Build(scene,nullptr,error),"chassis destruction fixture builds");const auto original=gone.RuntimeBody(2);Check(gone.DestroyEntity(2,&error),"native one-entity destruction removes chassis only");
    const auto newOwner=gone.CreateEntity(replacement,nullptr,&error);gone.UpdateWheels(1.f/60);
    Check(newOwner&&gone.RuntimeBody(newOwner).id!=original.id&&gone.RuntimeWheel(3)&&!gone.RuntimeWheel(3)->state.available,"destroyed chassis reference never aliases a reused body slot");
    Check(!WorldPersistence::Capture(gone,chunks,error)&&error.find("wheel chassis")!=std::string::npos,"orphaned wheel save fails with its missing required chassis");
    RuntimeWorld hierarchy;hierarchy.legacyGameplay=false;Check(hierarchy.Build(scene,nullptr,error)&&hierarchy.DestroyHierarchy(2,error)&&!hierarchy.RuntimeWheel(3),"explicit hierarchy destruction retires every wheel owner");
    hierarchy.Destroy();Check(!hierarchy.RuntimeWheel(3),"world stop discards wheel state and safe access");
    ManagedRegionPublication();
    std::printf("SUMMARY %u checks %u failures\n",checks,failures);return failures?1:0;
}
