#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "PhysicalAnimation.h"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <limits>

namespace {
int checks=0,failures=0;
void Check(bool ok,const char* text){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",text);}
float Angle(glm::quat a,glm::quat b){return 2*std::acos(std::clamp(std::abs(glm::dot(glm::normalize(a),glm::normalize(b))),0.f,1.f));}
std::shared_ptr<const SkeletalAsset> Asset(){
    auto asset=std::make_shared<SkeletalAsset>();auto& s=asset->skeleton;s.names={"Pivot","Branch","Leaf","UnmappedHidden"};s.parents={-1,0,1,0};s.order={0,1,2,3};s.rest.local.resize(4);s.rest.local[1].translation={0,1,0};s.rest.local[2].translation={0,1,0};s.rest.local[3].scale=glm::vec3(0);s.skinNodes={0,1,2};
    auto globals=ResolveJointMatrices(s,s.rest);for(int node:s.skinNodes)s.inverseBind.push_back(glm::inverse(globals[node]));
    AnimationClip clip;clip.name="Hold";clip.duration=1;asset->clips.push_back(clip);return asset;
}
Scene Fixture(){
    Scene scene;SceneObject field;field.id=1;field.gravity=SceneGravityComponent{};field.gravity->kind=SceneGravityKind::Uniform;field.gravity->magnitude=0;field.gravity->regionShape=SceneRegionShape::Box;field.gravity->regionHalfExtents=glm::vec3(1000);scene.InsertObject(field);
    SceneObject actor;actor.id=10;actor.name="Generic articulated fixture";actor.transform.position={0,3,0};actor.render=SceneRenderComponent{};actor.render->shape=SceneShape::Mesh;actor.render->meshAsset="00000000000000000000000000000070";actor.animation=SceneAnimationComponent{};actor.animation->clip="Hold";actor.animation->playOnStart=false;actor.characterMotor=CharacterMotorSettings{};actor.characterMotor->gravityScale=0;actor.characterMotor->reorientationDegreesPerSecond=0;actor.ragdoll=RagdollDefinition{};
    const char* names[]={"Pivot","Branch","Leaf"};for(int i=0;i<3;++i){RagdollBone bone;bone.joint=names[i];if(i)bone.parent=names[i-1];bone.offset=i?glm::vec3(0,.25f,0):glm::vec3(0);bone.halfExtents={.1f,.25f,.1f};bone.mass=i?1:3;bone.constraint.type=JointType::Ball;actor.ragdoll->bones.push_back(bone);}
    PhysicalAnimationSettings settings;PhysicalAnimationRegion region;region.id="selected";region.joints={"Branch","Leaf"};region.stiffness=12;region.damping=2;region.maxTorque=8;settings.regions.push_back(region);actor.ragdoll->physicalAnimation=settings;scene.InsertObject(actor);return scene;
}
bool Build(RuntimeWorld& world,const Scene& scene,std::shared_ptr<const SkeletalAsset> asset,std::string& error){
    if(!world.Build(scene,nullptr,error))return false;
    for(const auto& object:scene.Objects())if(object.animation){auto* animation=world.RuntimeAnimation(object.id);animation->asset=asset;world.ResolveAnimationPose(*animation,0);}
    return true;
}
void Advance(RuntimeWorld& world,int steps=1,int frames=0){
    constexpr float dt=1.f/60;for(int step=0;step<steps;++step){world.UpdateCharacters(dt,true,false);world.PrepareAnimationReferences(dt);world.PreparePhysicalAnimations(dt);world.Physics().Step(dt);world.UpdateCharacters(dt,false,true);world.UpdateAnimations(dt);world.UpdateRagdolls(dt);for(int i=0;i<frames;++i)if(auto* a=world.RuntimeAnimation(10))world.ResolveAnimationPose(*a,0);}
}
bool OneTouch(const PhysicsWorld& physics,BodyHandle observer,BodyHandle other,PhysicsWorld::TouchPhase phase){
    int matches=0;bool valid=true;
    for(const auto& event:physics.LastStepTouchEvents())if((event.a.id==observer.id&&event.b.id==other.id)||(event.a.id==other.id&&event.b.id==observer.id)){
        ++matches;valid&=event.phase==phase&&!event.sensor&&!event.impulseAvailable;
    }
    return valid&&matches==1;
}
void MotorContactChecks(std::shared_ptr<const SkeletalAsset> asset,std::string& error){
    auto scene=Fixture();scene.Find(10)->transform.position={0,.92f,0};
    SceneObject floor;floor.id=20;floor.body=SceneBodyComponent{};floor.body->halfExtents={5,.5f,5};floor.transform.position={0,-.5f,0};scene.InsertObject(floor);
    SceneObject wall;wall.id=21;wall.body=SceneBodyComponent{};wall.body->halfExtents={.1f,1,3};wall.transform.position={1.3f,1,0};scene.InsertObject(wall);
    RuntimeWorld world;const bool ready=Build(world,scene,asset,error);Check(ready,"M70 opt-in motor contact fixture builds through ordinary scene bodies");if(!ready)return;
    auto* motor=world.RuntimeCharacter(10);const auto observer=motor->observationBody,ground=world.RuntimeBody(20),side=world.RuntimeBody(21);
    Advance(world);Check(world.PhysicalAnimationSnapshot(10).mode==PhysicalAnimationMode::Animation&&!world.RagdollActive(10)&&OneTouch(world.Physics(),observer,ground,PhysicsWorld::TouchPhase::Enter),"pre-physics motor floor enter survives Step in animation authority");
    Advance(world);Check(OneTouch(world.Physics(),observer,ground,PhysicsWorld::TouchPhase::Stay),"pre-physics motor floor contact publishes one stay");
    auto pose=world.RuntimeDefinition(10)->transform;pose.position={0,3,0};world.SetRuntimeTransform(10,pose);Advance(world);Check(OneTouch(world.Physics(),observer,ground,PhysicsWorld::TouchPhase::Exit),"leaving floor publishes one exit across M70 phase ordering");
    Advance(world);Check(world.Physics().LastStepTouchEvents().empty(),"pre-physics floor exit does not repeat");
    pose.position={.85f,.92f,0};world.SetRuntimeTransform(10,pose);motor->velocity={3,0,0};Advance(world);Check(OneTouch(world.Physics(),observer,side,PhysicsWorld::TouchPhase::Enter),"pre-physics sweep wall enter survives Step");
    motor->velocity={3,0,0};Advance(world);Check(OneTouch(world.Physics(),observer,side,PhysicsWorld::TouchPhase::Stay),"continued wall intent publishes one wall stay");
    motor->velocity={-3,0,0};Advance(world);Check(OneTouch(world.Physics(),observer,side,PhysicsWorld::TouchPhase::Exit),"moving away publishes one wall exit");
    Advance(world);bool repeated=false;for(const auto& event:world.Physics().LastStepTouchEvents()){repeated|=event.a.id==side.id||event.b.id==side.id;}
    Check(!repeated,"wall exit does not repeat while floor contact continues");
}
float RotationDiagnostics(std::shared_ptr<const SkeletalAsset> asset,std::string& error){
    const auto rotation=glm::angleAxis(.8f,glm::normalize(glm::vec3(1,2,3)));const auto inverse=glm::inverse(rotation);
    RuntimeWorld world[2];BodyHandle arm[2];
    for(int trial=0;trial<2;++trial){auto scene=Fixture();if(trial){scene.Find(10)->transform.rotation=rotation;scene.Find(10)->transform.position=rotation*scene.Find(10)->transform.position;}
        if(!Build(world[trial],scene,asset,error))return std::numeric_limits<float>::infinity();
        world[trial].Physics().SetSleepingEnabled(false);PhysicalAnimationRequest partial;partial.mode=PhysicalAnimationMode::Partial;world[trial].RequestPhysicalAnimation(10,partial,error);Advance(world[trial]);arm[trial]=world[trial].RuntimeBody(world[trial].RagdollBody(10,"Branch"));
        const auto basis=trial?rotation:glm::quat(1,0,0,0);auto pose=world[trial].Physics().GetTransform(arm[trial]);world[trial].Physics().ApplyImpulseAtPoint(arm[trial],basis*glm::vec3(1,0,0),pose.position+basis*glm::vec3(0,.2f,0));}
    for(int step=1;step<=120;++step){Advance(world[0]);Advance(world[1]);if(step==1||step==3||step==10||step==30||step==60||step==120){
        auto a=world[0].Physics().GetTransform(arm[0]),b=world[1].Physics().GetTransform(arm[1]);auto sa=world[0].PhysicalAnimationSnapshot(10),sb=world[1].PhysicalAnimationSnapshot(10);
        std::printf("ROTATION_DRIVEN step=%d position_difference_m=%.9g angular_difference_rad_s=%.9g orientation_difference_rad=%.9g error_a=%.9g error_b=%.9g torque_a=%.9g torque_b=%.9g\n",step,glm::length(a.position-inverse*b.position),glm::length(world[0].Physics().GetAngularVelocity(arm[0])-inverse*world[1].Physics().GetAngularVelocity(arm[1])),Angle(a.rotation,inverse*b.rotation),sa.observations[0].angleError,sb.observations[0].angleError,sa.observations[0].torque,sb.observations[0].torque);}}
    // The same native M45 chain, with no M70 reference/drive code, localizes
    // any orientation discrepancy already present in the rigid solver.
    PhysicsWorld passive[2];BodyHandle bodies[2][3];
    for(int trial=0;trial<2;++trial){passive[trial].Init();passive[trial].SetSleepingEnabled(false);const auto basis=trial?rotation:glm::quat(1,0,0,0);
        for(int i=0;i<3;++i){glm::vec3 position(0,i?3.f+i+.25f:3.f,0);bodies[trial][i]=passive[trial].CreateShape(Shape::Box({.1f,.25f,.1f}),{basis*position,basis},i!=0,i?1.f:0.f,.6f,0);}
        for(int i=1;i<3;++i){JointSettings joint;joint.type=JointType::Ball;joint.bodyA=bodies[trial][i];joint.bodyB=bodies[trial][i-1];joint.anchorA={0,-.25f,0};joint.anchorB={0,i==1?1.f:.75f,0};passive[trial].CreateJoint(joint);passive[trial].SetPairCollisionEnabled(joint.bodyA,joint.bodyB,false);}
        auto pose=passive[trial].GetTransform(bodies[trial][1]);passive[trial].ApplyImpulseAtPoint(bodies[trial][1],basis*glm::vec3(1,0,0),pose.position+basis*glm::vec3(0,.2f,0));}
    for(int step=1;step<=120;++step){passive[0].Step(1.f/60);passive[1].Step(1.f/60);if(step==1||step==3||step==10||step==30||step==60||step==120){auto a=passive[0].GetTransform(bodies[0][1]),b=passive[1].GetTransform(bodies[1][1]);std::printf("ROTATION_PASSIVE step=%d position_difference_m=%.9g angular_difference_rad_s=%.9g orientation_difference_rad=%.9g\n",step,glm::length(a.position-inverse*b.position),glm::length(passive[0].GetAngularVelocity(bodies[0][1])-inverse*passive[1].GetAngularVelocity(bodies[1][1])),Angle(a.rotation,inverse*b.rotation));}}
    RuntimeWorld sleepProbe;if(!Build(sleepProbe,Fixture(),asset,error)){return std::numeric_limits<float>::infinity();}
    PhysicalAnimationRequest partial;partial.mode=PhysicalAnimationMode::Partial;sleepProbe.RequestPhysicalAnimation(10,partial,error);Advance(sleepProbe);auto probe=sleepProbe.RuntimeBody(sleepProbe.RagdollBody(10,"Branch"));auto pose=sleepProbe.Physics().GetTransform(probe);sleepProbe.Physics().ApplyImpulseAtPoint(probe,{1,0,0},pose.position+glm::vec3(0,.2f,0));Advance(sleepProbe,723);
    const auto sleeping=sleepProbe.PhysicalAnimationSnapshot(10);sleepProbe.Physics().SetSleepingEnabled(false);Advance(sleepProbe,720);const auto continuing=sleepProbe.PhysicalAnimationSnapshot(10);
    std::printf("DRIVE_SLEEP error_at_ordinary_sleep_rad=%.9g error_after_awake_continuation_rad=%.9g angular_speed_rad_s=%.9g\n",sleeping.observations[0].angleError,continuing.observations[0].angleError,glm::length(sleepProbe.Physics().GetAngularVelocity(probe)));
    return continuing.observations[0].angleError;
}
}
int main(){
    std::string error;PhysicalAnimationRegion region;region.id="test";region.joints={"arbitrary"};
    auto target=glm::angleAxis(.5f,glm::vec3(0,0,1));auto response=CalculatePhysicalTorque(target,{1,0,0,0},{1,0,0,0},{0,0,0},{0,0,0},glm::mat3(1),glm::mat3(0),region,1.f/60);
    const float expected=15/(1+4.f/60+30.f/3600);
    Check(std::abs(response.torque.z-expected)<1e-5f&&std::abs(response.angleError-.5f)<1e-5f,"independent bounded implicit-PD torque expectation");
    region.maxTorque=2;response=CalculatePhysicalTorque(target,{1,0,0,0},{1,0,0,0},{0,0,0},{0,0,0},glm::mat3(1),glm::mat3(0),region,1.f/60);
    Check(response.saturated&&std::abs(glm::length(response.torque)-2)<1e-5f,"physical effort saturates at declared N m limit");
    auto equivalent=CalculatePhysicalTorque(-target,{1,0,0,0},{1,0,0,0},{0,0,0},{0,0,0},glm::mat3(1),glm::mat3(0),region,1.f/60);
    Check(glm::length(equivalent.torque-response.torque)<1e-6f,"quaternion signs use identical shortest-path rotational error");
    region.effortWeight=0;response=CalculatePhysicalTorque(target,{1,0,0,0},{1,0,0,0},{0,0,1},{0,0,0},glm::mat3(1),glm::mat3(0),region,1.f/60);
    Check(response.torque==glm::vec3(0)&&std::abs(response.angleError-.5f)<1e-5f,"zero drive retains actual error and applies no restoring torque");
    {region.effortWeight=1;region.maxTorque=20;const auto rotation=glm::angleAxis(.8f,glm::normalize(glm::vec3(1,2,3))),parent=glm::angleAxis(.3f,glm::vec3(0,1,0)),child=parent*glm::angleAxis(-.2f,glm::vec3(1,0,0));const auto basis=glm::mat3_cast(rotation);glm::mat3 inertia(0),parentInverse(0);inertia[0][0]=.02f;inertia[1][1]=.03f;inertia[2][2]=.04f;parentInverse[0][0]=8;parentInverse[1][1]=15;parentInverse[2][2]=11;
    auto ordinary=CalculatePhysicalTorque(target,child,parent,{.1f,.2f,.3f},{-.2f,.1f,0},inertia,parentInverse,region,1.f/60),reoriented=CalculatePhysicalTorque(target,rotation*child,rotation*parent,rotation*glm::vec3(.1f,.2f,.3f),rotation*glm::vec3(-.2f,.1f,0),basis*inertia*glm::transpose(basis),basis*parentInverse*glm::transpose(basis),region,1.f/60);
    std::printf("ROTATION_TORQUE difference_Nm=%.9g error_difference_rad=%.9g\n",glm::length(rotation*ordinary.torque-reoriented.torque),std::abs(ordinary.angleError-reoriented.angleError));
    Check(glm::length(rotation*ordinary.torque-reoriented.torque)<1e-5f&&std::abs(ordinary.angleError-reoriented.angleError)<1e-6f,"independent anisotropic-inertia drive law is equivariant under world rotation");
    const auto axis=glm::normalize(glm::vec3(1,2,3));const glm::vec3 speed(.1f,.2f,.3f);auto known=CalculatePhysicalTorque({1,0,0,0},glm::angleAxis(-.4f,axis),{1,0,0,0},speed,{0,0,0},inertia,parentInverse,region,1.f/60);const auto responseMatrix=glm::mat3(1)+(region.damping/60+region.stiffness/3600)*(glm::inverse(inertia)+parentInverse);const auto demand=region.stiffness*.4f*axis-region.damping*speed;const auto residual=glm::length(responseMatrix*known.torque-demand);
    std::printf("TENSOR_RESPONSE linear_residual_Nm=%.9g\n",residual);Check(!known.saturated&&residual<1e-5f,"anisotropic implicit torque satisfies the independent complete linear system");}
    auto scene=Fixture();auto settings=*scene.Find(10)->ragdoll->physicalAnimation;auto text=SerializePhysicalAnimationSettings(settings);PhysicalAnimationSettings round;
    Check(ParsePhysicalAnimationSettings(text,round,error)&&PhysicalAnimationSettingsEqual(settings,round),"declarative region configuration round trip");
    auto invalid=settings;invalid.regions[0].damping=std::numeric_limits<float>::infinity();Check(!ValidPhysicalAnimationSettings(invalid,error),"nonfinite drive configuration rejected before publication");
    auto asset=Asset();MotorContactChecks(asset,error);const float awakeConvergence=RotationDiagnostics(asset,error);RuntimeWorld world;Check(Build(world,scene,asset,error),"headless ordinary entity and immutable skeleton ready");if(failures){std::puts(error.c_str());return 1;}
    world.RuntimeCharacter(10);auto baseline=world.Physics().AliveBodyCount();PhysicalAnimationRequest partial;partial.mode=PhysicalAnimationMode::Partial;
    Check(!world.EnterRagdoll(10,error)&&!world.RagdollActive(10),"legacy activation cannot bypass explicit motor authority handoff");
    Check(world.RequestPhysicalAnimation(10,partial,error)&&!world.RagdollActive(10),"authority request queues without immediate physics mutation");Advance(world);
    auto root=world.RuntimeBody(world.RagdollBody(10,"Pivot"));auto arm=world.RuntimeBody(world.RagdollBody(10,"Branch"));auto leaf=world.RuntimeBody(world.RagdollBody(10,"Leaf"));
    Check(world.RagdollActive(10)&&world.PhysicalAnimationSnapshot(10).mode==PhysicalAnimationMode::Partial&&!world.Physics().IsDynamicBody(root)&&world.Physics().IsDynamicBody(arm),"selected region dynamic with explicit static boundary anchor");
    auto* motor=world.RuntimeCharacter(10);Check(motor&&motor->settings.enabled,"partial physical response preserves CharacterMotor authority");
    PhysicsQueryFilter filter;filter.ignoredBodies={motor->observationBody,arm,leaf};auto rootCentre=world.Physics().GetTransform(root).position;
    Check(!world.Physics().Raycast(rootCentre+glm::vec3(0,0,2),{0,0,-1},4,filter).hit,"unselected boundary anchors do not become invisible query colliders");
    auto identity=world.ArticulatedIdentity(world.RagdollBody(10,"Branch"));Check(identity.first==10&&identity.second=="Pivot/Branch","ordinary mapped body has stable owner/joint attribution");
    Check(world.Physics().AliveBodyCount()==baseline+3,"one articulation without duplicate physical root population");
    auto before=world.Physics().GetTransform(arm);world.Physics().ApplyImpulseAtPoint(arm,{1,0,0},before.position+glm::vec3(0,.2f,0));Advance(world,3);
    auto disturbed=world.PhysicalAnimationSnapshot(10);float disturbance=disturbed.observations.empty()?0:disturbed.observations[0].angleError;
    std::printf("DRIVE fixture_gravity_m_s2=%.9g baseline_bodies=%zu active_bodies=%zu first_error_rad=%.9g first_angular_speed_rad_s=%.9g first_torque_Nm=%.9g\n",glm::length(world.Gravity().Sample(before.position)),baseline,world.Physics().AliveBodyCount(),disturbance,glm::length(world.Physics().GetAngularVelocity(arm)),disturbed.observations.empty()?0:disturbed.observations[0].torque);
    bool departed=world.Physics().GetTransform(arm).position!=before.position;
    for(int step=0;step<720;++step){Advance(world);auto sample=world.PhysicalAnimationSnapshot(10);if(!sample.observations.empty())disturbance=std::max(disturbance,sample.observations[0].angleError);
        if(step==176||step==416||step==719)std::printf("DRIVE step=%d error_rad=%.9g angular_speed_rad_s=%.9g torque_Nm=%.9g sleeping=%d\n",step+4,sample.observations.empty()?-1:sample.observations[0].angleError,glm::length(world.Physics().GetAngularVelocity(arm)),sample.observations.empty()?0:sample.observations[0].torque,world.Physics().IsSleeping(arm));}
    auto settled=world.PhysicalAnimationSnapshot(10);
    Check(!disturbed.observations.empty()&&disturbance>.01f&&departed,"external hit causes actual dynamic departure, not only rendered deformation");
    Check(std::isfinite(awakeConvergence)&&awakeConvergence<disturbance*.1f,"awake control retains strict impact-peak convergence independently of ordinary sleep policy");
    Check(world.Physics().IsSleeping(arm),"unchanged settled reference permits ordinary island sleeping");
    Check(!settled.observations.empty()&&settled.observations[0].angleError<=glm::radians(1.f),"ordinary quiet policy retains a measured sub-degree residual rather than exact servo convergence");
    world.Physics().ApplyLinearImpulse(arm,{.2f,0,0});Check(!world.Physics().IsSleeping(arm),"ordinary external impact wakes a driven region");
    {RuntimeWorld targetWake;Build(targetWake,scene,asset,error);targetWake.RequestPhysicalAnimation(10,partial,error);Advance(targetWake,120);auto wakeBody=targetWake.RuntimeBody(targetWake.RagdollBody(10,"Branch"));const bool wasSleeping=targetWake.Physics().IsSleeping(wakeBody);
    auto changedPose=asset->skeleton.rest;changedPose.local[1].rotation=glm::angleAxis(.2f,glm::vec3(0,0,1));PoseContribution changedReference;changedReference.pose=changedPose;targetWake.SetPoseContribution(10,"changedDriveReference",changedReference,error);targetWake.PrepareAnimationReferences(1.f/60);targetWake.PreparePhysicalAnimations(1.f/60);
    auto changedState=targetWake.PhysicalAnimationSnapshot(10);Check(wasSleeping&&!targetWake.Physics().IsSleeping(wakeBody)&&!changedState.observations.empty()&&changedState.observations[0].torque>0,"meaningful reference target change wakes the ordinarily sleeping articulated island");}
    bool bounded=true;for(const auto& observation:settled.observations)bounded&=observation.torque<=settings.regions[0].maxTorque+1e-5f;Check(bounded,"observed effort never exceeds authored cap");
    const auto motorStart=motor->position;motor->velocity={1,0,0};Advance(world,20);Check(motor->position.x>motorStart.x+.25f&&world.Physics().GetTransform(root).position.x>motorStart.x+.25f,"translating motor and boundary preserve independent root locomotion");
    PhysicalAnimationRequest active;active.mode=PhysicalAnimationMode::Active;
    Check(!world.RequestPhysicalAnimation(10,active,error)&&error.find("motorHandoff")!=std::string::npos,"full authority refuses an implicit competing motor root");active.motorHandoff=true;
    const auto handoffPosition=motor->position;
    Check(world.RequestPhysicalAnimation(10,active,error),"explicit motor-to-full articulation handoff accepted");Advance(world);
    auto fullRoot=world.RuntimeBody(world.RagdollBody(10,"Pivot"));Check(!motor->settings.enabled&&world.Physics().IsDynamicBody(fullRoot),"full physical root dynamic and motor explicitly disabled");
    Check(glm::length(world.Physics().GetTransform(fullRoot).position-handoffPosition)<.05f,"partial-to-full uses current root instead of a parked inactive anchor");
    auto oldRoot=world.PresentedTransform(10,world.RuntimeDefinition(10)->transform,1);world.Physics().ApplyLinearImpulse(fullRoot,{3,0,0});Advance(world);const auto fullVelocity=world.Physics().GetLinearVelocity(fullRoot);const auto fullArm=world.RuntimeBody(world.RagdollBody(10,"Branch"));
    auto currentRoot=world.PresentedTransform(10,world.RuntimeDefinition(10)->transform,1);auto firstRoot=world.PresentedTransform(10,world.RuntimeDefinition(10)->transform,0);auto middleRoot=world.PresentedTransform(10,world.RuntimeDefinition(10)->transform,.5f);
    Check(glm::length(firstRoot.position-oldRoot.position)<1e-6f&&glm::length(middleRoot.position-glm::mix(oldRoot.position,currentRoot.position,.5f))<1e-6f&&glm::length(currentRoot.position-oldRoot.position)>1e-4f,"full physical motor root preserves previous/mid/current presentation samples");
    SceneObject socket;socket.socket=SceneSocketComponent{};socket.socket->target=10;socket.socket->joint="Pivot";socket.socket->offset.position={.1f,0,0};auto socketId=world.CreateEntity(socket,nullptr,&error);bool coherent=true;
    for(float alpha:{0.f,.5f,1.f}){SceneTransform joint;coherent&=world.JointPose(10,"Pivot","world",alpha,joint);auto socketPose=world.PresentedTransform(socketId,world.RuntimeDefinition(socketId)->transform,alpha);coherent&=glm::length(socketPose.position-(joint.position+joint.rotation*(joint.scale*socket.socket->offset.position)))<1e-5f;}
    Check(socketId&&coherent,"socket and solved joint share identical interpolated physical root sample");
    PhysicalAnimationRequest passive;passive.mode=PhysicalAnimationMode::Passive;Check(world.RequestPhysicalAnimation(10,passive,error),"active to passive request accepted");world.PrepareAnimationReferences(1.f/60);world.PreparePhysicalAnimations(1.f/60);
    Check(world.RuntimeBody(world.RagdollBody(10,"Pivot")).id==fullRoot.id&&glm::length(world.Physics().GetLinearVelocity(fullRoot)-fullVelocity)<1e-6f,"passive release preserves body identity and velocity without another kick");
    auto passiveState=world.PhysicalAnimationSnapshot(10);bool noEffort=true;for(const auto& observation:passiveState.observations)noEffort&=observation.torque==0;Check(noEffort,"passive mode removes active effort without changing passive constraints");
    auto obstruction=world.Physics().CreateStaticBox({0,0,0},{2,2,2},.6f,0);PhysicalAnimationRequest resume;resume.resumeMotor=true;resume.placement=PhysicalAnimationPlacement{};
    Check(!world.RequestPhysicalAnimation(10,resume,error)&&world.PhysicalAnimationSnapshot(10).mode==PhysicalAnimationMode::Passive,"penetrating resume placement refuses and retains coherent physical authority");world.Physics().DestroyBody(obstruction);
    resume.placement->position={8,3,0};auto source=world.RuntimeDefinition(10)->transform.position;auto pathWall=world.Physics().CreateStaticBox((source+resume.placement->position)*.5f,{.05f,2,2},.6f,0);
    Check(!world.RequestPhysicalAnimation(10,resume,error)&&error.find("path is blocked")!=std::string::npos,"clear destination behind a wall does not authorize root tunnelling");world.Physics().DestroyBody(pathWall);
    resume.fade=.5f;Check(world.RequestPhysicalAnimation(10,resume,error),"safe explicit motor placement and visual return accepted");Advance(world);
    Check(!world.RagdollActive(10)&&motor->settings.enabled&&world.RuntimeAnimation(10)->external.count("ragdollReturn"),"return retires physics and preserves a captured transition pose");
    Check(!world.Physics().IsDynamicBody(fullArm)&&world.Physics().AliveBodyCount()==baseline,"old body generations cannot alias later articulation state");Advance(world,40);
    Check(!world.RuntimeAnimation(10)->external.count("ragdollReturn"),"visual transition retires through the existing M47 resolver");
    auto bad=settings;bad.regions[0].joints={"missing"};Check(!world.ConfigurePhysicalAnimation(10,bad,error)&&PhysicalAnimationSettingsEqual(*world.RuntimeDefinition(10)->ragdoll->physicalAnimation,settings),"invalid mapped batch does not partially replace authored settings");
    Check(world.RequestPhysicalAnimation(10,partial,error),"partial reactivation accepted");Advance(world);auto removed=world.RagdollBody(10,"Leaf");world.DestroyEntity(removed);world.UpdateRagdolls(0);
    Check(!world.RagdollActive(10)&&world.Physics().AliveBodyCount()==baseline,"mapped body removal tears down articulation safely");
    Check(world.RequestPhysicalAnimation(10,partial,error),"repeated activation has no stale collider ghosts");Advance(world);world.SetRagdollEnabled(10,false,error);world.PreparePhysicalAnimations(1.f/60);
    Check(!world.RagdollActive(10)&&world.PhysicalAnimationSnapshot(10).mode==PhysicalAnimationMode::Animation,"component disable retires mode and constraints");world.Destroy();
    std::vector<glm::vec3> endpoints;for(int trial=0;trial<2;++trial){RuntimeWorld w;Build(w,scene,asset,error);w.RequestPhysicalAnimation(10,partial,error);Advance(w);auto body=w.RuntimeBody(w.RagdollBody(10,"Branch"));auto center=w.Physics().GetTransform(body).position;w.Physics().ApplyImpulseAtPoint(body,{1,0,0},center+glm::vec3(0,.2f,0));Advance(w,120,trial?5:0);endpoints.push_back(w.Physics().GetTransform(body).position);}
    Check(glm::length(endpoints[0]-endpoints[1])<1e-6f,"additional presentation evaluations do not change fixed physical trajectory");
    auto sharedScene=scene;auto second=*sharedScene.Find(10);second.id=11;second.transform.position.x=5;sharedScene.InsertObject(second);RuntimeWorld independent;Build(independent,sharedScene,asset,error);independent.RequestPhysicalAnimation(10,partial,error);Advance(independent);
    Check(independent.RuntimeAnimation(10)->asset==independent.RuntimeAnimation(11)->asset&&!independent.RagdollActive(11)&&independent.PhysicalAnimationSnapshot(11).mode==PhysicalAnimationMode::Animation,"shared immutable rig retains independent authority and mixer state");
    auto retuned=settings;retuned.regions[0].id="retuned";retuned.regions[0].maxTorque=.25f;retuned.regions[0].poseWeight=.4f;const bool retunedAccepted=independent.ConfigurePhysicalAnimation(10,retuned,error);Advance(independent);auto retunedState=independent.PhysicalAnimationSnapshot(10);
    Check(retunedAccepted&&!retunedState.observations.empty()&&retunedState.observations[0].region=="retuned"&&retunedState.observations[0].torque<=.25f,"region setup cache refreshes when authored coefficients and visual weight change");
    retuned.regions[0].joints={"Leaf"};const auto priorMapped=independent.RuntimeBody(independent.RagdollBody(10,"Leaf"));const bool remapped=independent.ConfigurePhysicalAnimation(10,retuned,error);Advance(independent);auto remappedState=independent.PhysicalAnimationSnapshot(10);
    Check(remapped&&remappedState.observations.size()==1&&remappedState.observations[0].joint=="Pivot/Branch/Leaf"&&independent.RuntimeBody(independent.RagdollBody(10,"Leaf")).id!=priorMapped.id&&!independent.Physics().IsDynamicBody(independent.RuntimeBody(independent.RagdollBody(10,"Branch"))),"changed joint selection rebuilds the mapped region cache and physical subset atomically");
    independent.RuntimeAnimation(10)->asset=Asset();Advance(independent);const bool retiredOnReplacement=!independent.RagdollActive(10)&&independent.PhysicalAnimationSnapshot(10).mode==PhysicalAnimationMode::Animation;const bool restarted=independent.RequestPhysicalAnimation(10,partial,error);Advance(independent);
    Check(retiredOnReplacement&&restarted&&independent.PhysicalAnimationSnapshot(10).observations.size()==1&&independent.PhysicalAnimationSnapshot(10).observations[0].joint=="Pivot/Branch/Leaf","asset replacement retires old mappings and rebuilds disposable region setup for the new immutable asset");
    RuntimeWorld movingReturn;Build(movingReturn,scene,asset,error);movingReturn.RequestPhysicalAnimation(10,partial,error);Advance(movingReturn);auto movingArm=movingReturn.RuntimeBody(movingReturn.RagdollBody(10,"Branch"));auto actual=movingReturn.Physics().GetTransform(movingArm);movingReturn.Physics().ApplyImpulseAtPoint(movingArm,{1,0,0},actual.position+glm::vec3(0,.2f,0));Advance(movingReturn,5);actual=movingReturn.Physics().GetTransform(movingArm);auto actualJoint=actual.position-actual.rotation*scene.Find(10)->ragdoll->bones[1].offset;
    PhysicalAnimationRequest returnToAnimation;returnToAnimation.fade=.5f;movingReturn.RequestPhysicalAnimation(10,returnToAnimation,error);movingReturn.RuntimeCharacter(10)->velocity={2,0,0};movingReturn.UpdateCharacters(1.f/60,true,false);movingReturn.PrepareAnimationReferences(1.f/60);movingReturn.PreparePhysicalAnimations(1.f/60);SceneTransform returnedJoint;const bool capturedCurrent=movingReturn.JointPose(10,"Branch","world",1,returnedJoint);
    Check(capturedCurrent&&!movingReturn.RagdollActive(10)&&glm::length(returnedJoint.position-actualJoint)<1e-5f&&Angle(returnedJoint.rotation,actual.rotation)<1e-3f,"partial return captures actual bones in the current moving motor root frame");
    auto hiddenScene=scene;hiddenScene.Find(10)->ragdoll->physicalAnimation->regions[0].joints={"Branch"};RuntimeWorld hidden;Build(hidden,hiddenScene,asset,error);auto hiddenPose=asset->skeleton.rest;hiddenPose.local[2].scale=glm::vec3(0);PoseContribution headLayer;headLayer.pose=hiddenPose;headLayer.mask={2};hidden.SetPoseContribution(10,"existingHiddenLayer",headLayer,error);
    Check(hidden.RequestPhysicalAnimation(10,partial,error),"uninvolved hidden mapped branch requires no singular-transform inversion");Advance(hidden);auto freeBranch=hidden.RuntimeBody(hidden.RagdollBody(10,"Branch"));hidden.Physics().ApplyImpulseAtPoint(freeBranch,{1,0,0},hidden.Physics().GetTransform(freeBranch).position+glm::vec3(0,.2f,0));Advance(hidden,5);
    Check(hidden.RuntimeAnimation(10)->finalPose.local[2].scale==glm::vec3(0)&&hidden.PhysicalAnimationSnapshot(10).observations[0].angleError>.01f,"hidden unselected child stays hidden and cannot kinematically pin its dynamic parent");
    auto rotatedScene=scene;auto rotation=glm::angleAxis(.8f,glm::normalize(glm::vec3(1,2,3)));rotatedScene.Find(10)->transform.rotation=rotation;rotatedScene.Find(10)->transform.position=rotation*scene.Find(10)->transform.position;RuntimeWorld rotated;Build(rotated,rotatedScene,asset,error);rotated.RequestPhysicalAnimation(10,partial,error);Advance(rotated);auto rotatedArm=rotated.RuntimeBody(rotated.RagdollBody(10,"Branch"));
    auto rotatedCenter=rotated.Physics().GetTransform(rotatedArm).position;rotated.Physics().ApplyImpulseAtPoint(rotatedArm,rotation*glm::vec3(1,0,0),rotatedCenter+rotation*glm::vec3(0,.2f,0));Advance(rotated,120);
    const auto rotatedEndpoint=glm::inverse(rotation)*rotated.Physics().GetTransform(rotatedArm).position;
    std::printf("ROTATION original_endpoint=(%.9g,%.9g,%.9g) rotated_back=(%.9g,%.9g,%.9g) difference_m=%.9g gravity_m_s2=%.9g\n",endpoints[0].x,endpoints[0].y,endpoints[0].z,rotatedEndpoint.x,rotatedEndpoint.y,rotatedEndpoint.z,glm::length(rotatedEndpoint-endpoints[0]),glm::length(rotated.Gravity().Sample(rotatedCenter)));
    Check(glm::length(rotatedEndpoint-endpoints[0])<.003f,"rotating the whole articulation preserves bounded physical response");
    auto hingeScene=scene;for(auto& bone:hingeScene.Find(10)->ragdoll->bones){bone.constraint.type=JointType::Hinge;bone.constraint.limits=true;bone.constraint.lower=-.2f;bone.constraint.upper=.2f;}RuntimeWorld limited;Build(limited,hingeScene,asset,error);auto desired=asset->skeleton.rest;desired.local[1].rotation=glm::angleAxis(.9f,glm::vec3(1,0,0));PoseContribution requested;requested.pose=desired;limited.SetPoseContribution(10,"knownReference",requested,error);limited.RequestPhysicalAnimation(10,partial,error);Advance(limited,240);
    auto limitedState=limited.PhysicalAnimationSnapshot(10);bool limitReported=false;for(const auto& observation:limitedState.observations)limitReported|=observation.saturated;
    auto limitedRoot=limited.Physics().GetTransform(limited.RuntimeBody(limited.RagdollBody(10,"Pivot"))),limitedArm=limited.Physics().GetTransform(limited.RuntimeBody(limited.RagdollBody(10,"Branch")));
    Check(limitReported&&Angle(limitedRoot.rotation,limitedArm.rotation)<.25f,"unreachable reference saturates at ordinary authored hinge limit");
    auto freeScene=scene;freeScene.Find(10)->characterMotor.reset();RuntimeWorld free;Build(free,freeScene,asset,error);PhysicalAnimationRequest freeMode;freeMode.mode=PhysicalAnimationMode::Active;free.RequestPhysicalAnimation(10,freeMode,error);Advance(free);auto freeRoot=free.RuntimeBody(free.RagdollBody(10,"Pivot"));auto freePrevious=free.PresentedTransform(10,free.RuntimeDefinition(10)->transform,1);free.Physics().ApplyLinearImpulse(freeRoot,{1,0,0});Advance(free);
    auto freeCurrent=free.PresentedTransform(10,free.RuntimeDefinition(10)->transform,1),freeMiddle=free.PresentedTransform(10,free.RuntimeDefinition(10)->transform,.5f);
    Check(glm::length(freeCurrent.position-freePrevious.position)>1e-4f&&glm::length(freeMiddle.position-glm::mix(freePrevious.position,freeCurrent.position,.5f))<1e-6f,"collider-free non-motor physical root uses the same authoritative presentation seam");
    std::printf("OBSERVATIONS impact_error_rad=%.9g returned_error_rad=%.9g presentation_difference_m=%.9g\n",disturbance,settled.observations.empty()?-1:settled.observations[0].angleError,glm::length(endpoints[0]-endpoints[1]));
    std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
