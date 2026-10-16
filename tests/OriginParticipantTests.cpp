#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "GameSession.h"
#include "Simulation.h"
#include "SceneSerialization.h"
#include "WorldPersistence.h"
#include "UniformGravity.h"
#include <filesystem>
#include <cstdio>

namespace {unsigned checks=0,failures=0;void Check(bool ok,const char* label){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",label);}}
int main(){
 std::string error;EngineHost host;Check(host.Init("M90 participant continuity",480,320,false,error),"native hidden GL/resource host starts");if(failures){std::puts(error.c_str());return 1;}
 host.OpenProjectAssets(std::filesystem::absolute("projects/ragdoll_demo").string(),std::filesystem::absolute("projects/ragdoll_demo/Assets").string());auto& resources=host.Resources();resources.GetMesh("46464646464646464646464646464601",error);resources.WaitForAll();
 Scene scene;Check(LoadSceneFromFile("projects/ragdoll_demo/Scenes/main.judas",scene,error),"original ordinary skeletal fixture loads");if(failures)return 1;
 for(auto& o:scene.Objects()){o.scripts.clear();o.ui.reset();}
 auto& socket=scene.CreateObject("visual socket");const auto socketId=socket.id;socket.socket=SceneSocketComponent{11,"Root/Elbow/Tip",{},true};socket.render=SceneRenderComponent{};
 RuntimeWorld world;world.legacyGameplay=false;Check(world.Build(scene,&resources,error),"skeletal world builds with script-owned execution");if(failures){std::puts(error.c_str());return 1;}world.UpdateAnimations(0);world.UpdateSockets();
 Check(world.EnterRagdoll(10,error),"real mapped passive articulation activates");if(failures){std::puts(error.c_str());return 1;}
 GameSession session;Check(session.Begin(world,error),"ordinary fixed-step path starts");for(unsigned i=0;i<45;++i)StepPlayedWorld(session,host.GetWindow(),1.f/60);
 auto bone=world.RuntimeBody(world.RagdollBody(10,"Root"));const auto p=world.Physics().GetTransform(bone).position,v=world.Physics().GetLinearVelocity(bone);const auto originalOrigin=world.Settings().worldOrigin;const auto beforeSocket=world.PresentedTransform(socketId,world.RuntimeDefinition(socketId)->transform,.35f);const auto beforeSkin=*world.AnimationSkin(10,.35f);const auto bodyCount=world.Physics().AliveBodyCount();
 const glm::dvec3 delta(512,-512,512);
 Check(world.RequestOrigin(originalOrigin+delta,error)&&world.CommitOrigin(),"active ragdoll and socket accept native chart translation");
 Check(world.RuntimeBody(world.RagdollBody(10,"Root")).id==bone.id&&world.Physics().AliveBodyCount()==bodyCount,"articulation body identity/count remains unchanged");
 Check(glm::length(glm::dvec3(world.Physics().GetTransform(bone).position)+delta-glm::dvec3(p))<.0002&&world.Physics().GetLinearVelocity(bone)==v,"physical bone endpoint translates with no velocity injection");
 world.UpdateSockets();auto afterSocket=world.PresentedTransform(socketId,world.RuntimeDefinition(socketId)->transform,.35f);Check(glm::length(glm::dvec3(afterSocket.position)+delta-glm::dvec3(beforeSocket.position))<.001,"resolved socket position remains continuous");
 const auto afterSkin=*world.AnimationSkin(10,.35f);double skinError=0;for(size_t i=0;i<beforeSkin.size();++i)for(unsigned c=0;c<4;++c)skinError=std::max(skinError,double(glm::length(beforeSkin[i][c]-afterSkin[i][c])));Check(skinError<.001,"interpolated skin remains model-local across chart publication");
 for(unsigned i=0;i<45;++i){StepPlayedWorld(session,host.GetWindow(),1.f/60);}
 Check(world.RagdollActive(10)&&std::isfinite(world.Physics().GetTransform(bone).position.x),"articulation continues real constraints/contact after shift");
 std::map<std::string,SaveChunk> chunks;Check(WorldPersistence::Capture(world,chunks,error),"active shifted articulation captures full continuation");Scene saved;Check(WorldPersistence::SceneFrom(chunks,saved,error),"saved articulation reconstructs shifted scene");RuntimeWorld cold;cold.legacyGameplay=false;bool restored=cold.Build(saved,&resources,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(cold,error)&&WorldPersistence::Restore(cold,chunks,error);Check(restored,"fresh world restores shifted articulation");if(!restored)std::puts(error.c_str());
 if(restored){auto restoredBone=cold.RuntimeBody(cold.RagdollBody(10,"Root"));Check(restoredBone.IsValid()&&glm::length(cold.Physics().GetTransform(restoredBone).position-world.Physics().GetTransform(bone).position)<.0001,"cold continuation checks actual restored bone endpoint");}
 Check(world.LeaveRagdoll(10,.25f,error)&&!world.Physics().IsBodyEnabled(bone),"return after rebasing retires old physical generation");for(unsigned i=0;i<20;++i)StepPlayedWorld(session,host.GetWindow(),1.f/60);Check(world.RuntimeAnimation(10)->external.count("ragdollReturn")==0,"pose return completes without a stale producer");
 // World-space IK intent moves with the chart; model-space intent must not.
 auto* animation=world.RuntimeAnimation(10);FullBodyIKSettings ik;ik.bodyRoot="Root";ik.chains.push_back({"arm",{"Root","Root/Elbow","Root/Elbow/Tip"}});
 FullBodyIKTarget worldTarget;worldTarget.id="world";worldTarget.chain="arm";worldTarget.space=IKTargetSpace::World;worldTarget.position={1,3,0};
 auto modelTarget=worldTarget;modelTarget.id="model";modelTarget.space=IKTargetSpace::Model;modelTarget.position={.2,1.5,0};ik.targets={worldTarget,modelTarget};
 Check(world.ConfigureFullBodyIK(10,ik,error),"shared IK accepts explicit world and model targets");
 world.PrepareAnimationReferences(1.f/60);const auto targetBefore=animation->fullBodyIK?animation->fullBodyIK->settings.targets[0].position:glm::dvec3(0);
 Check(world.RequestOrigin(originalOrigin,error)&&world.CommitOrigin(),"resolved shared IK participates in return chart translation");
 if(animation->fullBodyIK){Check(glm::length(animation->fullBodyIK->settings.targets[0].position-delta-targetBefore)<1e-9&&animation->fullBodyIK->settings.targets[1].position==modelTarget.position,"only world target positions translate; model targets and frames remain unchanged");Check(animation->fullBodyIK->sampledTargets[0].position==animation->fullBodyIK->settings.targets[0].position,"fixed sampled target and authored runtime target share the new chart");}
 else std::puts(error.c_str());
 Check(world.ConfigureFullBodyIK(10,std::nullopt,error),"IK relinquishes its own pose contribution normally");
 PhysicalAnimationSettings physical;Check(world.ConfigurePhysicalAnimation(10,physical,error),"optional physical animation setup remains ordinary");PhysicalAnimationRequest passive;passive.mode=PhysicalAnimationMode::Passive;
 Check(world.RequestPhysicalAnimation(10,passive,error),"physical pose authority transition requests normally");for(unsigned i=0;i<3;++i)StepPlayedWorld(session,host.GetWindow(),1.f/60);
 const auto reference=world.PhysicalAnimationSnapshot(10);Check(world.RequestOrigin(originalOrigin+delta,error)&&world.CommitOrigin(),"physical animation authority and reference histories translate");const auto shiftedReference=world.PhysicalAnimationSnapshot(10);
 bool same=reference.mode==shiftedReference.mode&&reference.recentReferenceWorld.size()==shiftedReference.recentReferenceWorld.size();for(size_t i=0;i<reference.recentReferenceWorld.size();++i)same&=glm::length(glm::dvec3(shiftedReference.recentReferenceWorld[i][3])+delta-glm::dvec3(reference.recentReferenceWorld[i][3]))<.001;
 Check(same&&world.RagdollActive(10),"physical mode and actual articulation survive without authority handoff or reset");
 // Local emitters already inherit the shifted emitter pose; applying a second
 // translation to their particle coordinates would visibly displace them twice.
 for(bool local:{false,true}){ParticleEmitterSettings settings;settings.localSpace=local;settings.rate=0;VisualParticlePool pool(settings);pool.Burst(3,{2,3,4},{1,0,0,0},{1,1,1});const auto before=pool.Particles();pool.ShiftOrigin(delta);bool valid=before.size()==pool.Particles().size();for(size_t i=0;i<before.size();++i)valid&=glm::length(glm::dvec3(pool.Particles()[i].position)+(local?glm::dvec3(0):delta)-glm::dvec3(before[i].position))<.0001&&pool.Particles()[i].velocity==before[i].velocity&&pool.Particles()[i].age==before[i].age;Check(valid,local?"local particles retain emitter-local coordinates and time":"world particles translate exactly once while retaining velocity and age");}
 // Direct matched deformable solve: retain strained shape, history and sleep,
 // rather than merely moving its render mesh or resetting to the rest asset.
 auto asset=std::make_shared<DeformableAsset>(MakeDeformableSheet(3,3,1,1));DeformableSettings settings;settings.asset="fixture";settings.selfContact=false;DeformableInstance a,b;a.Initialize(asset,settings,glm::dmat4(1),1);b.Initialize(asset,settings,glm::dmat4(1),2);PhysicsWorld pa,pb;pa.Init();pb.Init();UniformGravity gravity({0,-9.81f,0});
 a.velocities.back()={.4,.1,.2};b.velocities.back()={.4,.1,.2};for(unsigned i=0;i<8;++i){a.Step(1./60,pa,gravity,{});b.Step(1./60,pb,gravity,{});}const auto initial=b.positions;b.ShiftOrigin(delta);Check(glm::length(b.positions[0]+delta-initial[0])<1e-10,"deformable actual nodes and previous state translate");
 for(unsigned i=0;i<20;++i){a.Step(1./60,pa,gravity,{});b.Step(1./60,pb,gravity,{});}double nodeError=0,velocityError=0;for(size_t i=0;i<a.positions.size();++i){nodeError=std::max(nodeError,glm::length(b.positions[i]+delta-a.positions[i]));velocityError=std::max(velocityError,glm::length(a.velocities[i]-b.velocities[i]));}Check(nodeError<1e-6&&velocityError<1e-5,"deformed cloth continuation matches unshifted solver without a reset");
 std::printf("MEASURE skin_error=%.9g cloth_position_error_m=%.9g cloth_velocity_error_mps=%.9g\n",skinError,nodeError,velocityError);std::printf("SUMMARY %u checks %u failures\n",checks,failures);return failures?1:0;
}
