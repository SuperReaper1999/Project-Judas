#include "PhysicsWorld.h"
#include "Deformable.h"
#include "SaveArchive.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "AssetDatabase.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cstdio>
#include <limits>
int checks=0,failures=0;void check(bool p,const char* s){++checks;failures+=!p;std::printf("%s %s\n",p?"PASS":"FAIL",s);}
int main(){
 for(auto q:{glm::quat(1,0,0,0),glm::angleAxis(.7f,glm::normalize(glm::vec3(1,2,3)))}){
 PhysicsWorld w;w.Init();w.SetSleepingEnabled(false);BodyTransform pose;pose.rotation=q;auto b=w.CreateShape(Shape::Box({.5,.5,.5}),pose,true,100,0,0);JointSettings s;s.bodyA=b;s.frameB=q;auto j=w.CreateJoint(s);JointMaterial m;m.enabled=true;m.stiffness=10000;m.angularStiffness=10000;m.damping=600;m.angularDamping=600;m.yieldForce=1000;m.yieldTorque=1000;m.maximumTranslation=.5;m.maximumAngle=1;
 check(w.ConfigureJointMaterial(j,m),"generic material configuration");
 for(int i=0;i<180;++i){w.ApplyForce(b,q*glm::vec3(500,0,0));w.Step(1.f/60);}auto position=glm::inverse(q)*w.GetTransform(b).position;JointState state;w.GetJoint(j,state);check(std::abs(position.x-.05)<.002&&glm::length(state.constitutive.translation)<1e-6,"elastic load has finite physical displacement and no plastic strain");
 for(int i=0;i<240;++i){w.Step(1.f/60);}
 check(glm::length(w.GetTransform(b).position)<.001,"elastic unloading returns physically");
 w.SetLinearVelocity(b,q*glm::vec3(4,0,0));for(int i=0;i<240;++i)w.Step(1.f/60);w.GetJoint(j,state);check(glm::length(state.constitutive.translation)>.05&&!state.constitutive.failed&&state.constitutive.plasticWork>0,"impact produces permanent strain and positive plastic work");check(glm::length(glm::inverse(q)*w.GetTransform(b).position+state.constitutive.translation)<.003,"unloaded physical endpoint follows plastic rest geometry");
 w.SetSleepingEnabled(true);for(unsigned step=0;step<240;++step)w.Step(1.f/60);check(w.IsSleeping(b),"plastically deformed material can sleep at its current equilibrium instead of requiring its original weld pose");w.SetSleepingEnabled(false);
 SaveArchive saved;w.PersistJointMaterial(saved,j);auto savedState=state.constitutive;w.ConfigureJointMaterial(j,m);SaveArchive read(saved.bytes);w.PersistJointMaterial(read,j);read.Finish();w.GetJoint(j,state);check(state.constitutive.translation==savedState.translation&&state.constitutive.plasticWork==savedState.plasticWork,"material history round trip exact");
 w.SetLinearVelocity(b,q*glm::vec3(20,0,0));for(int i=0;i<120;++i)w.Step(1.f/60);w.GetJoint(j,state);check(state.constitutive.failed&&glm::length(w.GetLinearVelocity(b))>1,"capacity failure releases real body motion");
 auto bad=m;bad.stiffness=std::numeric_limits<float>::quiet_NaN();check(!w.ConfigureJointMaterial(j,bad),"nonfinite material rejected");w.DestroyJoint(j);check(!w.ConfigureJointMaterial(j,m),"stale joint safely rejected");w.Shutdown();
 }
 {
 PhysicsWorld w;w.Init();w.SetSleepingEnabled(false);BodyTransform pose;
 auto b=w.CreateShape(Shape::Box({.5,.5,.5}),pose,true,100,0,0);JointSettings s;s.bodyA=b;auto j=w.CreateJoint(s);
 JointMaterial m;m.enabled=true;m.connected=true;m.stiffness=10000;m.angularStiffness=10000;m.damping=600;m.angularDamping=600;m.yieldForce=1000;m.yieldTorque=1000;m.maximumTranslation=.2f;m.maximumAngle=.4f;
 check(w.ConfigureJointMaterial(j,m),"connected hull accepts a finite physical material");
 w.SetLinearVelocity(b,{20,0,0});for(int i=0;i<240;++i)w.Step(1.f/60);
 JointState state;w.GetJoint(j,state);
 check(!state.constitutive.failed&&state.constitutive.plasticWork>0&&glm::length(state.constitutive.translationTravel)<=.20001f,"severe load retains connected hull with bounded permanent plastic flow");
 check(glm::length(w.GetTransform(b).position)<.205f,"hull stop reactions bound physical displacement without deleting its body");
 auto work=state.constitutive.plasticWork;w.SetLinearVelocity(b,{-20,0,0});for(int i=0;i<240;++i)w.Step(1.f/60);w.GetJoint(j,state);
 check(!state.constitutive.failed&&state.constitutive.plasticWork>=work&&glm::length(w.GetTransform(b).position)<.205f,"repeated opposite impacts retain bounded connected material and irreversible work");
 w.Shutdown();
 auto hull=MakeFractureBlock({1,1,3},{1,1,3},true);for(auto& bond:hull.fracture->bonds){bond.material.enabled=true;bond.material.connected=true;}
 hull.fracture->bonds.back().material.connected=false;
 auto bytes=EncodeDeformableAsset(hull);DeformableAsset loaded;std::string error;
 check(bytes.rfind("JudasDeformable7",0)==0&&DecodeDeformableAsset({bytes.begin(),bytes.end()},loaded,error)&&loaded.fracture->bonds.front().material.connected&&!loaded.fracture->bonds.back().material.connected,"cooked hull continuity and detachable component policies round trip independently");
 FractureState f;f.Initialize(*hull.fracture);
 check(!f.Request(*hull.fracture,0,f.revision)&&f.Request(*hull.fracture,1,f.revision),"scripted fracture cannot split a hull but can release designated components");
 check(!f.RequestRemoval(*hull.fracture,hull.fracture->bonds[0].a,f.revision),"piece removal cannot bypass connected hull ownership");
 f.Observe(*hull.fracture,0,{1e12,1e12,1e12},{1,0,0});f.Commit(*hull.fracture);
 check(!f.broken[0]&&f.broken[1],"physical load retains hull continuity while component fracture publishes normally");
 }
 {
 PhysicsWorld w;w.Init();w.SetSleepingEnabled(false);BodyTransform pa,pb;pa.position={-.5,0,0};pb.position={.5,0,0};
 auto a=w.CreateShape(Shape::Box({.4,.4,.4}),pa,true,100,0,0),b=w.CreateShape(Shape::Box({.4,.4,.4}),pb,true,100,0,0);w.SetPairCollisionEnabled(a,b,false);
 JointSettings settings;settings.bodyA=a;settings.bodyB=b;settings.anchorA={.5,0,0};settings.anchorB={-.5,0,0};auto joint=w.CreateJoint(settings);
 JointMaterial law;law.enabled=true;law.connected=true;law.stiffness=30000;law.angularStiffness=15000;law.yieldForce=1000;law.yieldTorque=500;law.maximumTranslation=.2f;law.maximumAngle=.4f;
 check(w.ConfigureJointMaterial(joint,law),"two dynamic hull sections use the authoritative rigid solver");
 w.SetLinearVelocity(a,{20,0,0});w.SetAngularVelocity(a,{0,0,8});
 const auto initialMomentum=w.GetLinearVelocity(a)*100.f+w.GetLinearVelocity(b)*100.f;
 for(unsigned i=0;i<300;++i)w.Step(1.f/60);
 const auto momentum=w.GetLinearVelocity(a)*100.f+w.GetLinearVelocity(b)*100.f;JointState state;w.GetJoint(joint,state);
 check(!state.constitutive.failed&&glm::length(momentum-initialMomentum)<.1f,"bounded hull reactions preserve equal/opposite dynamic linear momentum");
 check(glm::length(state.constitutive.rotationTravel)<=.40001f&&glm::length(state.constitutive.translationTravel)<=.20001f,"combined bending and crush retain finite plastic budgets");
 SaveArchive saved;w.PersistJointMaterial(saved,joint);auto history=state.constitutive;w.ConfigureJointMaterial(joint,law);SaveArchive reader(saved.bytes);w.PersistJointMaterial(reader,joint);reader.Finish();w.GetJoint(joint,state);
 check(state.constitutive.translation==history.translation&&state.constitutive.rotation==history.rotation&&state.constitutive.plasticWork==history.plasticWork&&!state.constitutive.failed,"connected damaged material cold participant restoration preserves physical history");
 w.Shutdown();
 }
 auto a=MakeFractureBlock({1,1,3},{1,1,3},true);for(auto& b:a.fracture->bonds)b.material.enabled=true;auto bytes=EncodeDeformableAsset(a);DeformableAsset restored;std::string error;check(bytes.rfind("JudasDeformable5",0)==0&&DecodeDeformableAsset({bytes.begin(),bytes.end()},restored,error)&&restored.fracture->HasMaterials(),"versioned physical partition material round trip");a.fracture->rigid=false;check(!PrepareDeformableAsset(a,error),"soft partition cannot silently use rigid material law");
 // This actual lazy-publication fixture would fail if the constructor used
 // the authored transform or reset parent-carried motion to zero.
 {namespace fs=std::filesystem;auto path=fs::temp_directory_path()/("judas-m83-frame-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(path/"Assets");auto fixture=MakeFractureBlock({1,1,2},{1,1,2},true);fixture.fracture->bonds[0].material.enabled=true;fixture.fracture->filterNeighbours=true;std::ofstream(path/"Assets/cells.judasdeform",std::ios::binary)<<EncodeDeformableAsset(fixture);AssetDatabase db;db.Scan(path.string(),(path/"Assets").string());AssetRecord asset;check(db.Track((path/"Assets/cells.judasdeform").string(),asset,error),"actual material resource registered");ResourceManager resources(nullptr,&db);resources.AddRef(asset.id);resources.RequestDeformable(asset.id);resources.WaitForAll();resources.Pump(100);Scene scene;auto& parent=scene.CreateObject("moving physical frame");parent.body=SceneBodyComponent{};parent.body->motion=SceneBodyMotion::Dynamic;parent.body->mass=100;parent.body->collisionMask=0;auto root=parent.id;auto& child=scene.CreateObject("lazy physical partition");child.parent=root;child.deformable=DeformableSettings{};child.deformable->asset=asset.id;child.deformable->collisionMask=0;auto owner=child.id;RuntimeWorld world;check(world.Build(scene,&resources,error),"lazy material scene builds");SceneTransform moved;moved.position={5,4,3};moved.rotation=glm::angleAxis(.6f,glm::normalize(glm::vec3(1,2,3)));check(world.SetRuntimeTransform(root,moved),"physical parent moves before material construction");auto body=world.RuntimeBody(root);world.Physics().SetLinearVelocity(body,{1,2,3});world.Physics().SetAngularVelocity(body,{.3f,.4f,.5f});auto* material=world.RuntimeDeformable(owner,error);bool pose=true,velocity=true;for(unsigned n=0;material&&n<fixture.nodes.size();++n){pose&=glm::length(material->positions[n]-glm::dvec3(moved.position+moved.rotation*glm::vec3(fixture.nodes[n])))<1e-5;velocity&=glm::length(material->velocities[n]-glm::dvec3(world.Physics().GetPointVelocity(body,glm::vec3(material->positions[n]))))<1e-6;}check(material&&pose&&velocity,"lazy material uses current arbitrary parent frame and inherited point motion");check(world.PrepareRigidFracture(owner,error),"current-frame physical children prepare");auto owned=world.FracturePartEntity(owner,0);check(world.FracturePartOwner(owned)==owner,"generated material body retains its owner identity");world.DestroyEntity(owner);check(!world.RuntimeDefinition(owned)&&!world.FracturePartOwner(owned),"destroying material retires physical children and ownership");world.Destroy();resources.Shutdown();}
 std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
