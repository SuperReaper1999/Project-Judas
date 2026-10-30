#include "PhysicsWorld.h"
#include "PlanarGeometry.h"
#include "Narrowphase.h"
#include "CollisionGeometry.h"
#include "SaveArchive.h"
#include "../tools/prototypes/planar2d_3d_control.h"
#include <glm/gtc/quaternion.hpp>
#include <cstdio>
#include <stdexcept>
#include <cmath>
namespace {
unsigned checks=0;void Check(bool v,const char* name){++checks;std::printf("%s %s\n",v?"PASS":"FAIL",name);if(!v)throw std::runtime_error(name);}
BodyHandle Body(PhysicsWorld& w,std::shared_ptr<const PlanarCollider> geometry,glm::vec3 p,bool dynamic,float mass=1,glm::quat q={1,0,0,0},float restitution=0){return w.CreateShape(Shape::Planar(std::move(geometry)),{p,q},dynamic,mass,.5f,restitution);}
void Geometry(){
    RigidBody compatible{{0,0,0},{1,0,0,0},{1,2,3},{0,0,0},.5f,glm::mat3(1),{0,0,0},{0,0,0}};
    Check(compatible.inverseMass==.5f&&!compatible.IsPlanar(),"legacy native RigidBody aggregate fields retain their order");
    PhysicsQueryFilter filter{kAllCategories,0,0,0,true,{}};
    Check(filter.includeSensors&&!filter.planar,"legacy native query-filter aggregate keeps spatial defaults");

    auto box=PlanarBox({1,2});auto mass=PlanarMassProperties(*box);
    Check(mass.area==8&&std::abs(mass.inertia.z/mass.area-20./12)<1e-12,"rectangle uses lamina rather than thin-box inertia");
    auto disk=PlanarMassProperties(*PlanarCircle(.5));Check(std::abs(disk.inertia.z/disk.area-.125)<1e-12,"circle has exact disk inertia");
    auto capsule=PlanarMassProperties(*PlanarCapsule(.5,0));Check(std::abs(capsule.inertia.z/capsule.area-.125)<1e-12,"zero core-length capsule has disk mass limit");
    ContactPose a{{0,0,0},{1,0,0,0},{0,0,0}},b{{0,2.4f,0},{1,0,0,0},{0,0,0}};
    auto contacts=PlanarContacts(Shape::Planar(box),a,Shape::Planar(PlanarCircle(.5)),b,0);
    Check(contacts.count==1&&std::abs(contacts.points[0].signedSeparation+.1)<1e-6&&contacts.points[0].normal.y<-.99,"native rounded-core geometry reports circle-box penetration and witnesses");
    b.position.y=2.6f;Check(PlanarContacts(Shape::Planar(box),a,Shape::Planar(PlanarCircle(.5)),b,0).count==0,"separated circle-box negative control");
    b.position={0,3.9f,0};contacts=PlanarContacts(Shape::Planar(box),a,Shape::Planar(box),b,0);
    Check(contacts.count==2,"parallel 2D faces produce two physical anchors");
    b.position.z=1;Check(PlanarContacts(Shape::Planar(box),a,Shape::Planar(box),b,0).count==0,"different planes do not invent extrusion collision");
    std::shared_ptr<const PlanarCollider> decoded;std::string error;
    Check(DecodePlanar(EncodePlanar(*PlanarCapsule(.3,.4,true)),decoded,error)&&decoded->kind==PlanarKind::Capsule&&decoded->lockRotation,"native descriptor roundtrip retains exact shape and rotation policy");
    const auto retained=decoded;
    Check(!DecodePlanar("{\"version\":900}",decoded,error)&&decoded==retained,"malformed descriptor fails without replacing published immutable geometry");
    bool rejected=false;try{PlanarConvex({{0,0},{2,0},{.5,.5},{2,2},{0,2}});}catch(const std::exception&){rejected=true;}Check(rejected,"concave geometry rejects instead of pretending convex");
}
void Dynamics(){
    PhysicsWorld w;w.Init();auto box=Body(w,PlanarBox({.5,.7}),{0,3,0},true,2);
    w.ApplyImpulseAtPoint(box,{3,2,100},{.5f,3,20});const auto v=w.GetLinearVelocity(box),spin=w.GetAngularVelocity(box);
    Check(v.z==0&&spin.x==0&&spin.y==0&&std::abs(v.x-1.5f)<1e-6,"actual point impulse uses reduced linear and angular mobility");
    bool invalid=false;try{w.SetLinearVelocity(box,{0,0,1});}catch(const std::exception&){invalid=true;}Check(invalid&&w.GetLinearVelocity(box)==v,"forbidden velocity command rejects atomically");
    Body(w,PlanarBox({20,.5}),{0,-.5,0},false);
    for(int step=0;step<300;++step){w.ApplyLinearAcceleration(box,{0,-9.81f,100},1.f/60);w.Step(1.f/60);}
    const auto p=w.GetTransform(box).position;const auto pose=w.GetTransform(box);
    const auto contour=PlanarBox({.5,.7});
    double lowest=1e9;for(auto point:contour->vertices)lowest=std::min(lowest,double((pose.rotation*glm::vec3(point,0)).y)+p.y);
    const auto velocity=w.GetLinearVelocity(box),angular=w.GetAngularVelocity(box);
    auto manifold=PlanarContacts(Shape::Planar(contour),{p,pose.rotation,{0,0,0}},Shape::Planar(PlanarBox({20,.5})),{{0,-.5f,0},{1,0,0,0},{0,0,0}},1);
    for(int k=0;k<manifold.count;++k)std::printf("CONTACT gap=%.12g normal=(%.9g,%.9g) point=(%.9g,%.9g)\n",manifold.points[k].signedSeparation,manifold.points[k].normal.x,manifold.points[k].normal.y,manifold.points[k].point.x,manifold.points[k].point.y);
    std::printf("DETAIL minY=%.9g spin=%.9g vy=%.9g q=(%.9g,%.9g)\n",lowest,angular.z,velocity.y,pose.rotation.w,pose.rotation.z);
    std::printf("MEASURE resting=(%.9g,%.9g,%.9g)\n",p.x,p.y,p.z);
    Check(p.y>.45f&&p.y<.9f&&p.z==0,"off-centre landing collides with real 2D floor and remains in plane");
    Check(lowest>-.002,"independent transformed-corner oracle bounds floor penetration below 2 mm");
    PhysicsWorld custom;custom.Init();const auto customBody=Body(custom,PlanarBox({.5,.5}),{0,0,0},true);
    glm::mat3 tensor(0);tensor[0][0]=1;tensor[1][1]=2;tensor[2][2]=3;tensor[0][2]=tensor[2][0]=.2f;
    Check(custom.SetMassDistribution(customBody,1,tensor),"native custom physical inertia is accepted before reduction");
    custom.ApplyAngularImpulse(customBody,{0,0,3});
    Check(std::abs(custom.GetAngularVelocity(customBody).z-1)<1e-6,"constrained spin uses reciprocal physical Iz, not the spatial inverse tensor's zz entry");
    auto circle=Body(w,PlanarCircle(.3,true),{4,3,0},true);
    w.ApplyAngularImpulse(circle,{0,0,10});Check(w.GetAngularVelocity(circle)==glm::vec3(0),"authored rotation lock absorbs torque through mobility, not damping");
    JointSettings unsupported;unsupported.bodyA=circle;unsupported.type=JointType::Hinge;
    Check(!w.CreateJoint(unsupported).IsValid(),"3D hinge coupling rejects for planar body");
    PhysicsWorld fast;fast.Init();Body(fast,PlanarBox({.05,10}),{0,0,0},false);
    auto bullet=Body(fast,PlanarBox({.1,.1}),{-1,0,0},true);
    fast.SetLinearVelocity(bullet,{150,0,0});fast.Step(1.f/60);
    std::printf("MEASURE fast x=%.9g vx=%.9g\n",fast.GetTransform(bullet).position.x,fast.GetLinearVelocity(bullet).x);
    Check(fast.GetTransform(bullet).position.x<.01f,"native MK2 CCD stops a 150 m/s planar box at a thin wall");
}
void QueriesAndChains(){
    PhysicsWorld w;w.Init();const auto floor=Body(w,PlanarChain({{-10,0},{0,0},{10,0}}),{0,0,0},false);
    const auto query=Shape::Planar(PlanarCapsule(.25,.4));
    auto hit=w.CastPlanar(query,{{0,2,0},{1,0,0,0}},{0,-1,0},5);
    Check(hit.hit&&hit.body.id==floor.id&&std::abs(hit.distance-1.35f)<2e-5&&hit.normal.y>.99f,"capsule cast uses actual planar chain geometry and distance");
    Check(!w.Raycast({0,2,0},{0,-1,0},5).hit,"ordinary spatial queries do not silently treat 2D lines as 3D boxes");
    PhysicsQueryFilter ignored;ignored.ignoredBodies={floor};
    Check(!w.CastPlanar(query,{{0,2,0},{1,0,0,0}},{0,-1,0},5,ignored).hit,"2D casts preserve ignored-body filtering");
    bool bad=false;try{w.CastPlanar(query,{{0,2,0},{1,0,0,0}},{0,0,1},5);}catch(const std::exception&){bad=true;}Check(bad,"off-plane query direction rejects clearly");
    const auto ball=Body(w,PlanarCircle(.3),{-2,.31f,0},true);
    float low=1e9;
    for(int step=0;step<180;++step){auto v=w.GetLinearVelocity(ball);v.x=2;w.SetLinearVelocity(ball,v);w.ApplyLinearAcceleration(ball,{0,-9.81f,0},1.f/60);w.Step(1.f/60);low=std::min(low,w.GetTransform(ball).position.y);}
    std::printf("MEASURE seam x=%.9g lowY=%.9g\n",w.GetTransform(ball).position.x,low);
    Check(w.GetTransform(ball).position.x>3&&low>.297f,"circle traverses a shared collinear chain vertex without snagging or sinking");
}
void MomentumAndFrames(){
    PhysicsWorld elastic;elastic.Init();
    auto a=Body(elastic,PlanarCircle(.3),{-1,0,0},true,2,{1,0,0,0},1);
    auto b=Body(elastic,PlanarCircle(.3),{1,0,0},true,2,{1,0,0,0},1);
    elastic.SetPhysicalMaterial(a,"",0,1);elastic.SetPhysicalMaterial(b,"",0,1);
    elastic.SetLinearVelocity(a,{5,0,0});elastic.SetLinearVelocity(b,{-5,0,0});
    for(int step=0;step<30;++step)elastic.Step(1.f/60);
    const auto va=elastic.GetLinearVelocity(a),vb=elastic.GetLinearVelocity(b);
    Check(glm::length(va+vb)<1e-5&&std::abs(glm::dot(va,va)+glm::dot(vb,vb)-50)<1e-3,"elastic 2D contacts conserve momentum and kinetic energy independently");
    Check(va.x<0&&vb.x>0,"elastic bodies actually collide and separate; free-flight is not a passing oracle");
    const auto q=glm::angleAxis(.63f,glm::normalize(glm::vec3(1,2,3)));const auto n=q*glm::vec3(0,0,1);
    PhysicsWorld rotated;rotated.Init();Body(rotated,PlanarBox({10,.5}),q*glm::vec3(0,-.5f,0),false,1,q);
    const auto body=Body(rotated,PlanarBox({.4,.5}),q*glm::vec3(0,2,0),true,1,q);
    rotated.ApplyImpulseAtPoint(body,n*100.f,q*glm::vec3(.3f,2,0));
    Check(glm::length(rotated.GetLinearVelocity(body))<2e-5&&glm::length(rotated.GetAngularVelocity(body))<2e-5,"rotated planar mobility rejects a purely normal impulse without gameplay repair");
    for(int step=0;step<300;++step){rotated.ApplyLinearAcceleration(body,q*glm::vec3(0,-9.81f,0),1.f/60);rotated.Step(1.f/60);}
    const auto local=glm::inverse(q)*rotated.GetTransform(body).position;
    std::printf("MEASURE rotated local=(%.9g,%.9g,%.9g)\n",local.x,local.y,local.z);
    Check(std::abs(local.z)<1e-4&&local.y>.497f&&local.y<.502f,"rotated frame has equivalent floor support and bounded normal drift");
    PhysicsWorld pin;pin.Init();const auto rotor=Body(pin,PlanarBox({1,.2}),{0,0,0},true);
    JointSettings joint;joint.type=JointType::PlanarPin;joint.bodyA=rotor;
    const auto handle=pin.CreateJoint(joint);Check(handle.IsValid(),"native planar pin is a two-row physical joint");
    pin.SetAngularVelocity(rotor,{0,0,3});for(int step=0;step<60;++step)pin.Step(1.f/60);
    Check(std::abs(pin.GetAngularVelocity(rotor).z-3)<1e-4&&glm::length(pin.GetTransform(rotor).position)<1e-4,"planar pin preserves its free spin and anchor");
    joint.type=JointType::Hinge;Check(!pin.SetJoint(handle,joint),"runtime joint reconfiguration cannot bypass the planar coupling guard");
}

void SensorsAndKinematics(){
    PhysicsWorld w;w.Init();auto sensor=Body(w,PlanarBox({1,1}),{0,0,0},false);
    w.SetBodySensor(sensor,true);auto ball=Body(w,PlanarCircle(.2),{0,0,0},true);
    w.Step(1.f/60);bool enter=false;for(const auto& e:w.LastStepTouchEvents())enter|=e.sensor&&e.phase==PhysicsWorld::TouchPhase::Enter;
    Check(enter&&glm::length(w.GetLinearVelocity(ball))==0,"unscripted planar sensor publishes enter without response");
    w.Step(1.f/60);bool stay=false;for(const auto& e:w.LastStepTouchEvents())stay|=e.sensor&&e.phase==PhysicsWorld::TouchPhase::Stay;
    Check(stay,"sensor stay is pair-bounded rather than manifold-point callbacks");
    w.SetCollisionFilter(ball,1,0);w.Step(1.f/60);bool exit=false;for(const auto& e:w.LastStepTouchEvents())exit|=e.sensor&&e.phase==PhysicsWorld::TouchPhase::Exit;
    Check(exit,"collision filter suppresses planar contact and publishes exit");
    PhysicsWorld support;support.Init();auto floor=Body(support,PlanarBox({5,.25}),{0,0,0},true,10);
    Check(support.SetMotionType(floor,BodyMotionType::Kinematic),"planar body supports kinematic authority");
    auto rider=Body(support,PlanarBox({.25,.25},true),{0,.501f,0},true);
    support.SetPhysicalMaterial(floor,"",1,0);support.SetPhysicalMaterial(rider,"",1,0);
    support.SetKinematicVelocity(floor,{1,0,0},{0,0,0});
    for(int i=0;i<90;++i){support.ApplyLinearAcceleration(rider,{0,-9.81f,0},1.f/60);support.Step(1.f/60);}
    const auto rp=support.GetTransform(rider).position,fp=support.GetTransform(floor).position;
    std::printf("MEASURE moving-support rider=(%.9g,%.9g) platform=%.9g\n",rp.x,rp.y,fp.x);
    Check(rp.x>1&&std::abs(rp.x-fp.x)<.15&&rp.y>.497,"actual friction carries a body on a translating kinematic platform");
    const bool refused=!support.SetKinematicVelocity(floor,{0,0,1},{0,0,0});
    Check(refused&&support.GetLinearVelocity(floor).z==0,"off-plane kinematic prescription rejects atomically");
    PhysicsWorld gaps;gaps.Init();Body(gaps,PlanarChain({{-2,0},{-1,0}}),{0,0,0},false);Body(gaps,PlanarChain({{1,0},{2,0}}),{0,0,0},false);
    auto hole=gaps.CastPlanar(Shape::Planar(PlanarCircle(.2)),{{0,2,0},{1,0,0,0}},{0,-1,0},4);
    Check(!hole.hit,"finite chains leave real gaps rather than becoming infinite planes");
    auto segment=gaps.CastPlanar(Shape::Planar(PlanarCircle(.2)),{{-1.5f,2,0},{1,0,0,0}},{0,-1,0},4);
    Check(segment.hit&&segment.childKey==1,"chain casts report stable segment child identity");
}
void JointControls(){
    PhysicsWorld w;w.Init();auto rotor=Body(w,PlanarBox({1,.2}),{0,0,0},true);
    JointSettings pin;pin.bodyA=rotor;pin.type=JointType::PlanarPin;pin.motor=true;pin.speed=2;pin.maxForce=10;
    auto handle=w.CreateJoint(pin);for(int i=0;i<60;++i)w.Step(1.f/60);
    JointState state;w.GetJoint(handle,state);
    std::printf("MEASURE pin motor w=%.9g coordinate=%.9g\n",w.GetAngularVelocity(rotor).z,state.coordinate);
    Check(handle.IsValid()&&std::abs(w.GetAngularVelocity(rotor).z+2)<.02,"planar pin motor controls B-minus-A angular speed physically");
    pin.motor=false;pin.limits=true;pin.lower=-.3f;pin.upper=.3f;w.SetJoint(handle,pin);
    for(int i=0;i<120;++i)w.Step(1.f/60);
    w.GetJoint(handle,state);
    Check(state.coordinate>=-.305&&state.coordinate<=.305,"planar pin limits stop real rotation at authored bounds");
    JointMaterial material;material.enabled=true;Check(!w.ConfigureJointMaterial(handle,material),"unsupported spatial constitutive joint law rejects for native pin");
    PhysicsWorld weld;weld.Init();auto a=Body(weld,PlanarBox({.5,.5}),{0,0,0},true);
    JointSettings fixed;fixed.bodyA=a;fixed.type=JointType::PlanarWeld;Check(weld.CreateJoint(fixed).IsValid(),"native weld creates three independent planar rows");
    weld.ApplyImpulseAtPoint(a,{10,5,0},{.3f,0,0});for(int i=0;i<60;++i)weld.Step(1.f/60);
    Check(glm::length(weld.GetTransform(a).position)<.001&&glm::length(weld.GetAngularVelocity(a))<.001,"weld resists translation and rotation without extra spatial alignment rows");
}

void SpinAndPersistence(){
    PhysicsWorld w;w.Init();auto a=Body(w,PlanarBox({.25,.5}),{0,0,0},true,2,{1,0,0,0},1);
    auto left=Body(w,PlanarBox({.05,5}),{-1,0,0},false,1,{1,0,0,0},1);
    auto right=Body(w,PlanarBox({.05,5}),{1,0,0},false,1,{1,0,0,0},1);
    for(auto h:{a,left,right})w.SetPhysicalMaterial(h,"",0,1);
    w.SetLinearVelocity(a,{5,0,0});w.SetAngularVelocity(a,{0,0,20});
    const double iz=2*(.25*.25+.5*.5)/3;
    const auto energy=[&](){auto v=w.GetLinearVelocity(a),spin=w.GetAngularVelocity(a);return glm::dot(v,v)+.5*iz*spin.z*spin.z;};
    const double initial=energy();double maximum=initial,extent=0;
    const auto contour=PlanarBox({.25,.5});
    for(int i=0;i<120;++i){w.Step(1.f/60);maximum=std::max(maximum,energy());auto pose=w.GetTransform(a);
        for(auto vertex:contour->vertices)extent=std::max(extent,std::abs(double((pose.position+pose.rotation*glm::vec3(vertex,0)).x)));}
    std::printf("MEASURE planar-spin initial=%.12g peak=%.12g final=%.12g max-wall-extent=%.9g\n",initial,maximum,energy(),extent);
    Check(maximum<=initial*1.00001,"fast rotating planar box has no artificial elastic energy gain");
    Check(extent<.953,"rotation-aware CCD preserves the actual finite wall boundary");
    SaveArchive archive;w.PersistBodyMass(archive,a);
    PhysicsWorld restored;restored.Init();const auto pose=w.GetTransform(a);auto b=Body(restored,PlanarBox({.25,.5}),pose.position,true,2,pose.rotation,1);
    SaveArchive reader(archive.bytes);restored.PersistBodyMass(reader,b);reader.Finish();
    Check(restored.GetPlanarNormal(b)==w.GetPlanarNormal(a),"native mass participant retains fixed planar frame");
    auto invalid=archive.bytes;invalid.back()=char(0x7f);const auto before=restored.GetMass(b);
    bool rejected=false;try{SaveArchive bad(invalid);restored.PersistBodyMass(bad,b);}catch(const std::exception&){rejected=true;}
    Check(rejected&&restored.GetMass(b)==before,"corrupt native plane record rejects before mass publication");
}

void FrictionCovariance(){
    const auto run=[](glm::quat frame){PhysicsWorld w;w.Init();
        Body(w,PlanarBox({20,.5}),frame*glm::vec3(0,-.5,0),false,1,frame);
        const auto box=Body(w,PlanarBox({.4,.5},true),frame*glm::vec3(0,.5f,0),true,1,frame);
        w.SetLinearVelocity(box,frame*glm::vec3(5,0,0));
        for(int i=0;i<30;++i){w.ApplyLinearAcceleration(box,frame*glm::vec3(0,-9.81f,0),1.f/60);w.Step(1.f/60);}
        const auto pose=glm::inverse(frame)*w.GetTransform(box).position;
        const auto velocity=glm::inverse(frame)*w.GetLinearVelocity(box);
        return std::pair<glm::vec3,glm::vec3>{pose,velocity};};
    const auto aligned=run({1,0,0,0});
    const auto rotated=run(glm::angleAxis(.63f,glm::normalize(glm::vec3(1,2,3))));
    std::printf("MEASURE friction aligned=(%.9g,%.9g) rotated=(%.9g,%.9g)\n",aligned.first.x,aligned.second.x,rotated.first.x,rotated.second.x);
    Check(glm::length(aligned.first-rotated.first)<1e-4&&glm::length(aligned.second-rotated.second)<1e-4,"native sliding friction is covariant under arbitrary fixed-frame rotation");
}

}
int main(){try{Check(PlanarStage0Trajectory3D()==0xf67106f8392d2826ULL,"accepted 3D physical trajectory remains bit-identical to Stage 0 control");Geometry();Dynamics();QueriesAndChains();MomentumAndFrames();SensorsAndKinematics();JointControls();SpinAndPersistence();FrictionCovariance();std::printf("SUMMARY %u checks 0 failures\n",checks);return 0;}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
