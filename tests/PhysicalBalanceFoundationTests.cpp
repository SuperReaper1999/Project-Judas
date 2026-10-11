#include "SaveStorage.h"
#include "PhysicsWorld.h"
#include <algorithm>
#include <cstdio>
#include <limits>

namespace {
int checks=0,failures=0;
void Check(bool ok,const char* text){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",text);}
float Angle(glm::quat a,glm::quat b){return 2*std::acos(std::clamp(std::abs(glm::dot(glm::normalize(a),glm::normalize(b))),0.f,1.f));}
void Drives(){
    {PhysicsWorld uninitialized;JointAngularDrive request;Check(!uninitialized.SetJointAngularDrive({},request),"uninitialized physics rejects actuator request safely");}
    PhysicsWorld physics;physics.Init();physics.SetSleepingEnabled(false);
    auto a=physics.CreateShape(Shape::Sphere(.2f),{{-.2f,0,0},{1,0,0,0}},true,2,.6f,0);
    auto b=physics.CreateShape(Shape::Sphere(.2f),{{.2f,0,0},{1,0,0,0}},true,3,.6f,0);
    JointSettings settings;settings.type=JointType::Ball;settings.bodyA=a;settings.bodyB=b;
    settings.anchorA={.2f,0,0};settings.anchorB={-.2f,0,0};
    auto joint=physics.CreateJoint(settings);physics.SetPairCollisionEnabled(a,b,false);
    JointAngularDrive drive;drive.enabled=true;drive.target=glm::angleAxis(.6f,glm::vec3(1,0,0));
    drive.stiffness=30;drive.damping=4;drive.maxTorque=.4f;
    Check(physics.SetJointAngularDrive(joint,drive),"opt-in paired angular drive accepted");
    physics.Step(1.f/60);
    JointState state;Check(physics.GetJoint(joint,state),"ordinary joint state remains readable");
    Check(glm::length(state.angularDrive.impulses)<=drive.maxTorque/60+1e-6f,"three-axis drive respects magnitude effort cap");
    auto momentum=physics.GetInertiaWorld(a)*physics.GetAngularVelocity(a)+physics.GetInertiaWorld(b)*physics.GetAngularVelocity(b);
    Check(glm::length(momentum)<1e-5f,"drive creates no net external angular impulse about shared axis");
    Check(glm::length(physics.GetLinearVelocity(a)+physics.GetLinearVelocity(b))<1e-5f,"angular drive invents no linear motion on shared-axis fixture");
    for(int step=0;step<180;++step)physics.Step(1.f/60);
    auto relative=glm::inverse(physics.GetTransform(a).rotation)*physics.GetTransform(b).rotation;
    Check(Angle(relative,drive.target)<.025f,"bounded drive approaches relative orientation without root anchor");
    auto bad=drive;bad.target.w=std::numeric_limits<float>::quiet_NaN();
    Check(!physics.SetJointAngularDrive(joint,bad),"nonfinite target rejected");
    bad=drive;bad.maxTorque=-1;Check(!physics.SetJointAngularDrive(joint,bad),"negative effort rejected");
    physics.GetJoint(joint,state);Check(Angle(state.angularDrive.target,drive.target)<1e-6f,"invalid update leaves target unchanged");
    drive.target=-drive.target;Check(physics.SetJointAngularDrive(joint,drive),"quaternion antipode accepted as same target");
    drive.stiffness=0;drive.damping=4;drive.relativeAngularVelocity={1,0,0};
    Check(physics.SetJointAngularDrive(joint,drive),"relative target-rate reference accepted");
    for(int step=0;step<180;++step)physics.Step(1.f/60);
 std::printf("RATE measured=%.9g\n",(physics.GetAngularVelocity(b)-physics.GetAngularVelocity(a)).x);
    Check(std::abs((physics.GetAngularVelocity(b)-physics.GetAngularVelocity(a)).x-1)<.025f,"implicit damping follows target rate rather than treating it as static");
    bad=drive;bad.relativeAngularVelocity.x=std::numeric_limits<float>::infinity();
    Check(!physics.SetJointAngularDrive(joint,bad),"nonfinite reference rate rejected atomically");
    drive.enabled=false;Check(physics.SetJointAngularDrive(joint,drive),"explicit disable releases effort");
    physics.GetJoint(joint,state);Check(glm::length(state.angularDrive.impulses)==0,"disable clears only angular-drive warm history");
    physics.DestroyJoint(joint);Check(!physics.SetJointAngularDrive(joint,drive),"destroyed joint handle rejected");
    settings.bodyB={};auto worldJoint=physics.CreateJoint(settings);drive.enabled=true;
    Check(!physics.SetJointAngularDrive(worldJoint,drive),"physical controller drive refuses world-anchor actuator");
}
void Limits(){
    PhysicsWorld physics;physics.Init();physics.SetSleepingEnabled(false);
    auto a=physics.CreateShape(Shape::Sphere(.2f),{{0,0,0},{1,0,0,0}},true,2,.6f,0);
    auto b=physics.CreateShape(Shape::Sphere(.2f),{{0,0,0},{1,0,0,0}},true,3,.6f,0);
    physics.SetPairCollisionEnabled(a,b,false);
    JointSettings settings;settings.type=JointType::Hinge;settings.bodyA=a;settings.bodyB=b;
    settings.limits=true;settings.lower=-.3f;settings.upper=.3f;
    auto joint=physics.CreateJoint(settings);
    JointAngularDrive drive;drive.enabled=true;drive.target=glm::angleAxis(1.f,glm::vec3(1,0,0));
    drive.stiffness=30;drive.damping=4;drive.maxTorque=1;
    Check(physics.SetJointAngularDrive(joint,drive),"hinge shares existing limit and drive solve");
    for(int step=0;step<240;++step)physics.Step(1.f/60);
    JointState state;physics.GetJoint(joint,state);
    Check(state.coordinate<.31f&&state.coordinate>.28f,"actual hinge limit resists out-of-range drive reference");
}
void Contacts(){
    PhysicsWorld physics;physics.Init();
    auto ground=physics.CreateShape(Shape::Box({2,.2f,2}),{{0,-.2f,0},{1,0,0,0}},false,0,.6f,0);
    auto foot=physics.CreateShape(Shape::Box({.2f,.1f,.3f}),{{0,.1f,0},{1,0,0,0}},true,2,.6f,0);
    std::vector<PhysicsWorld::BodyContactSample> contacts;
    Check(physics.ReadBodyContactSample(foot,contacts)&&contacts.empty(),"nearby geometry alone is not observed support");
    for(int step=0;step<90;++step){physics.ApplyLinearAcceleration(foot,{0,-9.81f,0},1.f/60);physics.Step(1.f/60);}
    Check(physics.ReadBodyContactSample(foot,contacts)&&!contacts.empty(),"completed solver contacts provide actual support");
    bool valid=true;float impulse=0;for(const auto& c:contacts){valid&=c.other.id==ground.id&&c.normal.y>.99f&&std::abs(c.point.y)<.02f&&std::isfinite(c.separation);impulse+=c.normalImpulse;}
    Check(valid&&impulse>0,"contact point normal body identity and load originate in solver");
    Check(!physics.ReadBodyContactSample(foot,contacts,1)&&contacts.empty(),"contact overflow fails closed instead of presenting a truncated support patch");
    Check(!physics.ReadBodyContactSample(foot,contacts,0),"invalid observation budget rejected");
    Check(contacts.empty(),"invalid observation cannot leave old contacts in output");
    physics.ResetBody(foot,{0,2,0},{1,0,0,0});
    Check(physics.ReadBodyContactSample(foot,contacts)&&contacts.empty(),"teleport invalidates observations without changing ordinary solver cache policy");
    physics.SetPairCollisionEnabled(foot,ground,false);
    Check(physics.ReadBodyContactSample(foot,contacts)&&contacts.empty(),"suppressed pair cannot grant support from cached rows");
    physics.SetPairCollisionEnabled(foot,ground,true);physics.SetBodySensor(ground,true);
    Check(physics.ReadBodyContactSample(foot,contacts)&&contacts.empty(),"sensor cannot grant physical support");
    physics.SetBodySensor(ground,false);physics.SetBodyEnabled(foot,false);
    Check(!physics.ReadBodyContactSample(foot,contacts),"disabled observer rejects observations");
    physics.DestroyBody(foot);auto replacement=physics.CreateShape(Shape::Sphere(.2f),{{0,3,0},{1,0,0,0}},true,1,.6f,0);
    Check(!physics.ReadBodyContactSample(foot,contacts),"stale generation cannot alias reused body slot");
    Check(physics.ReadBodyContactSample(replacement,contacts)&&contacts.empty(),"replacement inherits no old support");
}
void SaveBounds(){
    // M77 adds one optional native participant to the existing maximal save.
 GameSnapshot bounded;bounded.project="m77-bound";bounded.scene="Scenes/standing.judas";bounded.content=std::string(64,'a');bounded.metadata="{}";
 for(int n=0;n<17;++n)bounded.participants["participant-"+std::to_string(n)]={1,"x"};
 GameSnapshot decoded;std::string decodeError;
 Check(DecodeGameSnapshot(EncodeGameSnapshot(bounded),decoded,decodeError)&&decoded.participants.size()==17,"maximal 17-participant save retains every native record");
 bounded.participants["overflow"]={1,"x"};bool rejected=false;try{EncodeGameSnapshot(bounded);}catch(const std::exception&){rejected=true;}
 Check(rejected,"eighteenth save participant still rejects instead of dropping state");
}

}
int main(){Drives();Limits();Contacts();SaveBounds();std::printf("Physical balance foundations: %d checks, %d failures\n",checks,failures);return failures?1:0;}
