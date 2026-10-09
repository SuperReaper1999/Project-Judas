#include "PhysicsWorld.h"
#include "CharacterMotor.h"
#include "UniformGravity.h"
#include "WorldCoordinates.h"
#include "CollisionAsset.h"
#include "SaveArchive.h"
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace {
constexpr float dt=1.f/60;
int checks=0,failures=0;
void Check(bool pass,const char* label){++checks;failures+=!pass;std::printf("%s %s\n",pass?"PASS":"FAIL",label);}
bool Near(glm::vec3 a,glm::vec3 b,float tolerance=1e-4f){return glm::length(a-b)<=tolerance;}
bool Same(glm::quat a,glm::quat b){return std::memcmp(&a.w,&b.w,sizeof(float))==0&&std::memcmp(&a.x,&b.x,sizeof(float))==0&&std::memcmp(&a.y,&b.y,sizeof(float))==0&&std::memcmp(&a.z,&b.z,sizeof(float))==0;}
BodyHandle KinematicBox(PhysicsWorld& p,glm::vec3 at,glm::vec3 half,glm::quat q={1,0,0,0},float friction=.7f){
    auto h=p.CreateShape(Shape::Box(half),{at,q},false,1,friction,0);
    Check(p.SetMotionType(h,BodyMotionType::Kinematic),"static-to-kinematic keeps ordinary collider");return h;
}
void Pusher(bool commanded,bool filtered=false){
    PhysicsWorld p;p.Init();auto h=KinematicBox(p,{-2,0,0},{.25f,.25f,.25f});
    auto crate=p.CreateDynamicSphere({0,0,0},.15f,2,0,0);
    if(filtered)p.SetCollisionFilter(h,0,0);
    if(commanded)Check(p.MoveKinematic(h,{{2,0,0},{1,0,0,0}}),"pose request accepted before timeline publication");
    Check(Near(p.GetTransform(h).position,{-2,0,0}),"pose request does not teleport to endpoint");
    p.Step(dt);auto v=p.GetLinearVelocity(crate);auto at=p.GetTransform(crate).position;
    std::printf("PUSHER commanded=%d filtered=%d actual=%.9g crate=%.9g velocity=%.9g events=%zu contacts=%zu\n",commanded,filtered,p.GetTransform(h).position.x,at.x,v.x,p.LastStepStats().impactEvents,p.LastStepContactCount());
    Check(commanded?Near(p.GetTransform(h).position,{2,0,0}):Near(p.GetTransform(h).position,{-2,0,0}),"prescribed endpoint / stationary control");
    Check(commanded&&!filtered?v.x>100&&at.x>.2f:Near(v,{0,0,0})&&Near(at,{0,0,0}),"fast crossing drives real impulse, static/filter controls do not");
    if(commanded&&!filtered){
        Check(p.LastStepStats().impactEvents>0&&p.LastStepContactCount()>0,"crossing has continuous impact evidence");
        auto events=p.LastStepTouchEvents();Check(events.size()==1&&events[0].a.id!=events[0].b.id,"one owner contact event for TOI pair");
    }
}
void MotorSupport(glm::quat frame,glm::vec3 linear,glm::vec3 omega,bool walking){
    PhysicsWorld p;p.Init();UniformGravity zero(frame*glm::vec3(0,-9.81f,0));
    auto support=KinematicBox(p,frame*glm::vec3(0,-.25f,0),{3,.25f,3},frame);
    CharacterMotor rider;rider.settings.gravityScale=0;rider.Reset(frame*glm::vec3(1,.92f,0),frame);
    p.Step(dt);rider.Step(p,zero,dt);Check(rider.result.supported,"motor acquires ordinary prescribed support");
    const auto initial=rider.position;
    Check(p.SetKinematicVelocity(support,frame*linear,frame*omega),"support velocity command accepted");
    float drift=0;
    for(int step=0;step<30;++step){
        if(walking)rider.velocity=rider.result.supportVelocity+frame*glm::vec3(.4f,0,0);
        const auto before=p.GetTransform(support);const auto old=rider.position;
        p.Step(dt);const auto after=p.GetTransform(support);
        const auto carried=after.position+after.rotation*(glm::inverse(before.rotation)*(old-before.position));
        rider.Step(p,zero,dt);
        // Walking adds .4 m/s after carry; its moving-frame direction is
        // fixed in simulation coordinates, so no parenting can satisfy this.
        const auto expected=carried+(walking?frame*glm::vec3(.4f*dt,0,0):glm::vec3(0));
        drift=std::max(drift,glm::length(rider.position-expected));
    }
    auto inherited=rider.result.supportVelocity;
    auto pointExpected=p.GetPointVelocity(support,rider.position+rider.orientation*rider.settings.offset);
    std::printf("SUPPORT walking=%d drift_max=%.9g velocity_error=%.9g displacement=%.9g\n",walking,drift,glm::length(inherited-pointExpected),glm::length(rider.position-initial));
    Check(rider.result.supported&&drift<.025f,"translation/lift/rotation carry plus relative walk once");
    Check(Near(inherited,pointExpected,.005f),"support reports rotational material-point velocity");
    auto up=frame*glm::vec3(0,1,0);auto launch=inherited+up*3.f;
    rider.velocity=launch;const auto beforeJump=rider.position;
    p.Step(dt);rider.Step(p,zero,dt);
    std::printf("RELEASE velocity_error=%.9g displacement_error=%.9g supported=%d\n",glm::length(rider.velocity-launch),glm::length(rider.position-beforeJump-launch*dt),rider.result.supported);
    Check(!rider.result.supported&&Near(rider.velocity,launch,.005f)&&Near(rider.position-beforeJump,launch*dt,.005f),"jump inherits support once and leaves free movement");
    p.StopKinematic(support);p.Step(dt);auto old=rider.position;rider.Step(p,zero,dt);
    Check(Near(rider.position-old,launch*dt,.005f),"support stop after dismount does not glue departing rider");
}
}
int main(){
    Pusher(false);Pusher(true);Pusher(true,true);
    {
        PhysicsWorld p;p.Init();auto h=p.CreateStaticBox({-2,0,0},{.25f,.25f,.25f},0,0);
        auto crate=p.CreateDynamicSphere({0,0,0},.15f,1,0,0);p.SetLinearVelocity(h,{240,0,0});
        Check(!p.MoveKinematic(h,{{2,0,0},{1,0,0,0}}),"static authority cannot accept prescribed commands");p.Step(dt);
        Check(Near(p.GetTransform(h).position,{-2,0,0})&&Near(p.GetLinearVelocity(crate),{0,0,0}),"legacy static pose stays stationary when velocity data is written");
    }
    {
        MeshData source;for(int i=0;i<8;++i){MeshVertex vertex;vertex.position={i&1?.25f:-.25f,i&2?.25f:-.25f,i&4?.25f:-.25f};source.vertices.push_back(vertex);}
        source.indices={0,2,3,0,3,1,4,5,7,4,7,6,0,1,5,0,5,4,2,6,7,2,7,3,0,4,6,0,6,2,1,3,7,1,7,5};
        CollisionCookSettings settings;settings.convex=true;CollisionAsset hull;std::string error;
        Check(CookCollision(source,settings,hull,error),"ordinary convex fixture cooks");
        for(auto shape:{Shape::Sphere(.25f),Shape::Compound({CompoundBox{{0,0,0},{.25f,.25f,.25f}}}),Shape::Cooked(std::make_shared<CollisionAsset>(hull),"M71 original cube")}){
            PhysicsWorld p;p.Init();auto h=p.CreateShape(shape,{{-2,0,0},{1,0,0,0}},false,1,0,0);auto crate=p.CreateDynamicSphere({0,0,0},.15f,1,0,0);
            Check(p.SetMotionType(h,BodyMotionType::Kinematic)&&p.MoveKinematic(h,{{2,0,0},{1,0,0,0}}),"sphere/compound/convex supports prescribed authority");p.Step(dt);
            Check(p.GetLinearVelocity(crate).x>100&&Near(p.GetTransform(h).position,{2,0,0}),"supported moving shape crosses into normal dynamic response");
        }
        settings.convex=false;CollisionAsset mesh;Check(CookCollision(source,settings,mesh,error),"static concave fixture cooks");PhysicsWorld p;p.Init();
        auto concave=p.CreateShape(Shape::Cooked(std::make_shared<CollisionAsset>(mesh),"M71 static mesh"),{},false,1,0,0);auto capsule=p.CreateQueryCapsule(.25f,.6f,{});
        Check(!p.SetMotionType(concave,BodyMotionType::Kinematic)&&!p.SetMotionType(capsule,BodyMotionType::Kinematic),"concave/query-only unsupported transitions reject atomically");
    }
    {
        PhysicsWorld p;p.Init();auto a=glm::angleAxis(glm::radians(170.f),glm::vec3(0,1,0)),b=glm::angleAxis(glm::radians(-170.f),glm::vec3(0,1,0));
        auto h=KinematicBox(p,{0,0,0},{.25f,.25f,.25f},a);p.MoveKinematic(h,{{0,0,0},b},2*dt);p.Step(dt);
        Check(std::abs(glm::dot(p.GetTransform(h).rotation,glm::angleAxis(glm::pi<float>(),glm::vec3(0,1,0))))>1-1e-5f&&p.GetAngularVelocity(h).y>0,"pose target crosses quaternion wrap through shortest constant-angular arc");
        p.Step(dt);Check(std::abs(glm::dot(p.GetTransform(h).rotation,b))>1-1e-5f,"wrapped target reaches requested attitude");
    }
    {
        PhysicsWorld p;p.Init();auto h=KinematicBox(p,{0,0,0},{1,.06f,.06f});auto crate=p.CreateDynamicSphere({0,.65f,0},.12f,1,0,0);
        Check(p.SetKinematicVelocity(h,{0,0,0},{0,0,glm::two_pi<float>()/dt}),"full-turn spin commanded without quaternion wrapping");
        p.Step(dt);auto v=p.GetLinearVelocity(crate);
        std::printf("ROTATION full_turn speed=%.9g impact_events=%zu actual_angle=%.9g\n",glm::length(v),p.LastStepStats().impactEvents,glm::angle(p.GetTransform(h).rotation));
        Check(glm::length(v)>1&&p.LastStepStats().impactEvents>0,"rotation generates contact between equivalent endpoint attitudes");
        Check(std::abs(glm::dot(p.GetTransform(h).rotation,glm::quat(1,0,0,0)))>1-1e-5f,"continuous spin reaches represented full-turn endpoint");
    }
    {
        PhysicsWorld p;p.Init();auto floor=p.CreateStaticBox({0,-.5f,0},{10,.5f,10},.7f,0);
        auto crate=p.CreateDynamicBox({0,.25f,0},{.25f,.25f,.25f},1,.7f,0);
        auto h=KinematicBox(p,{-2,.25f,0},{.25f,.25f,.25f});
        for(int i=0;i<120;++i){p.ApplyLinearAcceleration(crate,{0,-9.81f,0},dt);p.Step(dt);}
        Check(p.IsSleeping(crate),"dynamic crate settles before prescribed impact");
        p.MoveKinematic(h,{{.5f,.25f,0},{1,0,0,0}});p.ApplyLinearAcceleration(crate,{0,-9.81f,0},dt);p.Step(dt);
        Check(!p.IsSleeping(crate)&&p.GetLinearVelocity(crate).x>1,"moving prescribed collider wakes sleeping dynamic neighbour");(void)floor;
    }
    {
        PhysicsWorld p;p.Init();auto h=KinematicBox(p,{0,0,0},{.25f,.25f,.25f});
        p.MoveKinematic(h,{{1,0,0},{1,0,0,0}});p.MoveKinematic(h,{{2,0,0},{1,0,0,0}});p.SetKinematicVelocity(h,{3,0,0},{0,0,0});p.Step(dt);
        Check(Near(p.GetTransform(h).position,{.05f,0,0}),"last complete write replaces target rather than composing commands");
        p.StopKinematic(h);p.Step(dt);Check(Near(p.GetTransform(h).position,{.05f,0,0})&&Near(p.GetLinearVelocity(h),{0,0,0}),"stop clears durable velocity");
        p.MoveKinematic(h,{{.2f,0,0},{1,0,0,0}});
        auto bad=BodyTransform{{std::numeric_limits<float>::infinity(),0,0},{1,0,0,0}};
        Check(!p.MoveKinematic(h,bad)&&!p.MoveKinematic(h,{{1,0,0},{0,0,0,0}})&&!p.MoveKinematic(h,{{1,0,0},{1,0,0,0}},61),"invalid full requests rejected");
        p.Step(dt);Check(Near(p.GetTransform(h).position,{.2f,0,0}),"invalid request retains coherent earlier command");
        p.Step(dt);Check(Near(p.GetTransform(h).position,{.2f,0,0})&&Near(p.GetLinearVelocity(h),{0,0,0}),"completed target without a new command holds");
        p.MoveKinematic(h,{{.8f,0,0},{1,0,0,0}},4*dt);p.Step(dt);p.Step(dt);
        Check(Near(p.GetTransform(h).position,{.5f,0,0},.0002f),"bounded multi-step target advances on fixed timeline");
        p.StopKinematic(h);p.Step(dt);Check(Near(p.GetTransform(h).position,{.5f,0,0},.0002f),"stop cancels remaining target interval");
        p.SetKinematicVelocity(h,{4,0,0},{0,0,0});p.ResetBody(h,{5,0,0},{1,0,0,0});p.Step(dt);
        Check(Near(p.GetTransform(h).position,{5,0,0})&&Near(p.GetPreviousTransform(h).position,{5,0,0}),"explicit placement retires motion rather than replaying displacement");
        auto id=h.id;p.SetKinematicVelocity(h,{2,0,0},{0,0,0});
        KinematicMotionState retained;
        Check(!p.SetMotionType(h,BodyMotionType::Dynamic,std::numeric_limits<float>::denorm_min())&&p.IsKinematicBody(h)&&p.GetKinematicMotion(h,retained)&&Near(retained.linearVelocity,{2,0,0}),"unusable transition mass rejects without changing authority or queued intent");
        Check(p.SetMotionType(h,BodyMotionType::Dynamic,2)&&h.id==id&&p.IsDynamicBody(h),"safe dynamic authority transition retains native identity");
        p.Step(dt);Check(Near(p.GetTransform(h).position,{5,0,0})&&Near(p.GetLinearVelocity(h),{0,0,0}),"default authority transition clears prescribed launch and commands");
        p.SetLinearVelocity(h,{1,2,3});Check(p.SetMotionType(h,BodyMotionType::Kinematic,2,true),"explicit preserve-velocity handoff accepted");p.Step(dt);
        Check(Near(p.GetTransform(h).position,glm::vec3(5,0,0)+glm::vec3(1,2,3)*dt)&&!p.IsDynamicBody(h),"preserved prescribed velocity advances with infinite response mass");
        p.ApplyLinearImpulse(h,{100,100,100});p.ApplyForce(h,{100,100,100});p.ApplyLinearAcceleration(h,{100,100,100},dt);p.Step(dt);
        Check(Near(p.GetLinearVelocity(h),{1,2,3}),"forces gravity and impulses do not own prescribed trajectory");
        p.DestroyBody(h);auto fresh=p.CreateDynamicSphere({0,0,0},.2f,1,0,0);
        Check(fresh.id!=h.id&&!p.MoveKinematic(h,{{10,0,0},{1,0,0,0}}),"removed slot cannot receive stale pending command");p.Step(dt);Check(Near(p.GetTransform(fresh).position,{0,0,0}),"slot reuse does not inherit old prescribed state");
    }
    {
        PhysicsWorld p;p.Init();auto q=glm::normalize(glm::quat(.6061418652534485f,.25942757725715637f,.005194163415580988f,.751839280128479f));
        auto h=KinematicBox(p,{0,0,0},{.5f,.5f,.5f},q);auto anchor=p.GetTransform(h).rotation;
        for(int i=0;i<24;++i){p.MoveKinematic(h,{{0,0,0},i%2?-anchor:anchor});p.Step(dt);}
        Check(Same(p.GetTransform(h).rotation,anchor)&&Near(p.GetAngularVelocity(h),{0,0,0}),"sign-equivalent unchanged target preserves represented rotation anchor");
        p.SetKinematicVelocity(h,{0,0,0},{1e-30f,-1e-30f,1e-30f});p.Step(dt);
        Check(Same(p.GetTransform(h).rotation,anchor),"sub-ULP angular motion does not alternate normalization states");
    }
    {
        PhysicsWorld p;p.Init();auto h=KinematicBox(p,{0,0,0},{.25f,.25f,.25f});p.SetKinematicVelocity(h,{1,0,0},{0,.5f,0});p.Step(dt);
        SaveArchive valid;p.PersistKinematic(valid,h);
        auto rejects=[&](bool tinyInterval){
            KinematicMotionState command;glm::vec3 actualLinear,actualAngular;SaveArchive read(valid.bytes);
            read(command.control,command.target.position,command.target.rotation,command.linearVelocity,command.angularVelocity,command.remainingSeconds,command.targetNextStep,actualLinear,actualAngular);read.Finish();
            if(tinyInterval){command.control=KinematicControl::Target;command.target.position={1,0,0};command.remainingSeconds=std::numeric_limits<float>::denorm_min();command.targetNextStep=false;}
            else command.linearVelocity={1e20f,0,0};
            SaveArchive malformed;malformed(command.control,command.target.position,command.target.rotation,command.linearVelocity,command.angularVelocity,command.remainingSeconds,command.targetNextStep,actualLinear,actualAngular);
            bool rejected=false;try{SaveArchive restore(malformed.bytes);p.PersistKinematic(restore,h);restore.Finish();}catch(const std::exception&){rejected=true;}
            SaveArchive after;p.PersistKinematic(after,h);
            Check(rejected&&after.bytes==valid.bytes,tinyInterval?"unusable tiny saved target interval rejects without live mutation":"unusable saved velocity rejects without live mutation");
        };rejects(false);rejects(true);
    }
    {
        PhysicsWorld p;p.Init();auto shape=Shape::Compound({CompoundBox{{2,0,0},{.5f,.5f,.5f}}});
        auto h=p.CreateShape(shape,{{0,0,0},{1,0,0,0}},true,2,.7f,0);p.SetMotionType(h,BodyMotionType::Kinematic,2);
        p.SetKinematicVelocity(h,{0,0,0},{0,0,2});
        Check(Near(p.GetPointVelocity(h,{2,1,0}),{0,0,0}),"queued velocity leaves previous authoritative point velocity intact");p.Step(dt);
        Check(Near(p.GetPointVelocity(h,{2,1,0}),{-2,0,0}),"point velocity measures about compound COM rather than authored pivot");
        auto q=glm::angleAxis(2*dt,glm::vec3(0,0,1));
        Check(Near(p.GetTransform(h).position,glm::vec3(2,0,0)-q*glm::vec3(2,0,0)),"world COM velocity and angular pose describe one consistent trajectory");
        const auto segments=p.GetBodyMotionSegments(h);Check(segments.size()==1,"prescribed motion exports normal value-owned motion ledger");
        if(!segments.empty()){
            const auto at=PhysicsWorld::EvaluateBodyMotionSegment(segments.front(),dt*.5);
            auto middle=glm::angleAxis(dt,glm::vec3(0,0,1));
            Check(Near(at.position,glm::vec3(2,0,0)-middle*glm::vec3(2,0,0)),"passive motion sampling retains COM offset and exact prescribed rotation");
        }
    }
    {
        PhysicsWorld p;p.Init();auto support=KinematicBox(p,{0,-.25f,0},{3,.25f,3});auto crate=p.CreateDynamicBox({0,.3f,0},{.3f,.3f,.3f},1,.9f,0);
        for(int i=0;i<60;++i){p.ApplyLinearAcceleration(crate,{0,-9.81f,0},dt);p.Step(dt);}
        p.SetKinematicVelocity(support,{.5f,0,0},{0,0,0});float impulse=0;
        for(int i=0;i<120;++i){p.ApplyLinearAcceleration(crate,{0,-9.81f,0},dt);p.Step(dt);for(auto& e:p.LastStepTouchEvents())impulse+=e.normalImpulse;}
        auto relative=p.GetTransform(crate).position-p.GetTransform(support).position;
        std::printf("DYNAMIC_RIDER drift=%.9g velocity=%.9g support_impulse=%.9g\n",relative.x,p.GetLinearVelocity(crate).x,impulse);
        Check(std::abs(relative.x)<.15f&&p.GetLinearVelocity(crate).x>.4f&&impulse>0,"dynamic rider travels through normal contact/friction");
    }
    MotorSupport({1,0,0,0},{1,.3f,0},{0,0,0},false);
    MotorSupport({1,0,0,0},{0,0,0},{0,.6f,0},false);
    MotorSupport({1,0,0,0},{.5f,.2f,0},{0,.4f,0},true);
    auto rotated=glm::angleAxis(.83f,glm::normalize(glm::vec3(1,2,3)));
    MotorSupport(rotated,{.5f,.2f,0},{0,.4f,0},true);
    WorldCoordinates far({1e12,-2e12,3e12});auto local=glm::vec3(.125f,-.25f,.5f);
    Check(Near(far.ToLocal(far.ToGlobal(local)),local),"large absolute origin retains small local motion coordinates");
    std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
