// FTFT4A: first-step contact through ordinary scene/lifecycle/persistence paths.
// Original FTFT4A cases have no temporal-impact acceptance: all fixture surfaces are independently shown
// to touch exactly (canonical axes) or overlap slightly (represented oblique
// geometry). There is no positive-gap-as-touching expectation in this suite.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "ContactSolver.h"
#include "Contacts.h"
#include "GameSession.h"
#include "RigidBody.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "Simulation.h"
#include "SimulationTiming.h"
#include "Window.h"
#include "WorldState.h"

namespace {
int g_checks = 0;
int g_failures = 0;
int g_steps = 0;
void Check(bool pass, const std::string& label) {
    ++g_checks;
    if (!pass) ++g_failures;
    std::printf("  %s %s\n", pass ? "OK  " : "FAIL", label.c_str());
}

// Independent scalar R(q)/dot(q,q), evaluated on actual stored quaternion
// components. No GLM quaternion rotation or production collision predicate.
struct V {
    long double x, y, z;
};
V Column(const glm::quat& q, int column) {
    const long double w=q.w, x=q.x, y=q.y, z=q.z;
    const long double n=w*w+x*x+y*y+z*z;
    if (column == 0) return {(w*w+x*x-y*y-z*z)/n, 2*(x*y+w*z)/n, 2*(x*z-w*y)/n};
    if (column == 1) return {2*(x*y-w*z)/n, (w*w-x*x+y*y-z*z)/n, 2*(y*z+w*x)/n};
    return {2*(x*z+w*y)/n, 2*(y*z-w*x)/n, (w*w-x*x-y*y+z*z)/n};
}
long double Dot(V a, V b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
V Difference(glm::vec3 a, glm::vec3 b) {
    return {static_cast<long double>(a.x)-b.x, static_cast<long double>(a.y)-b.y,
            static_cast<long double>(a.z)-b.z};
}

struct Fixture {
    const char* name;
    glm::quat authoredRotation;
    glm::vec3 exactAxis;
    float gravity;
    bool oblique;
};
struct World {
    RuntimeWorld world;
    GameSession session;
    Window window;
    bool Begin(const Scene& scene, std::string& error) {
        window.SetTestInputMode(true);
        return world.Build(scene, nullptr, error) && session.Begin(world, error);
    }
    void Step() {
        StepPlayedWorld(session, window, SimulationTiming::kFixedTimestep);
        ++g_steps;
    }
};

Scene MakeScene(const Fixture& f, bool sphere, const glm::vec3& offset, EntityId& id) {
    Scene scene;
    scene.Settings().name = "FTFT4A first-step lifecycle";
    SceneObject& ground=scene.CreateObject("support");
    ground.transform.position=offset;
    ground.transform.rotation=f.authoredRotation;
    ground.body=SceneBodyComponent{};
    ground.body->motion=SceneBodyMotion::Static;
    ground.body->halfExtents=glm::vec3(5.0f, 0.5f, 5.0f);
    ground.body->friction=0.6f;
    ground.body->restitution=0.0f;
    ground.gravity=SceneGravityComponent{};
    ground.gravity->kind=SceneGravityKind::Uniform;
    ground.gravity->magnitude=f.gravity;
    ground.gravity->regionShape=SceneRegionShape::Sphere;
    ground.gravity->regionRadius=512.0f;
    SceneObject& body=scene.CreateObject("body");
    id=body.id;
    body.transform.rotation=f.authoredRotation;
    if (f.oblique) {
        // This deliberately authored 2-micrometre overlap is not called
        // exact touching: the actual binary32 shape geometry is checked below.
        // It avoids using an intended, pre-quantization contact as an oracle.
        const V normal=Column(f.authoredRotation, 1);
        body.transform.position=offset+glm::vec3(normal.x*0.999998L,
                                                 normal.y*0.999998L,
                                                 normal.z*0.999998L);
    } else {
        body.transform.position=offset+f.exactAxis;
    }
    body.body=SceneBodyComponent{};
    body.body->motion=SceneBodyMotion::Dynamic;
    body.body->shape=sphere ? SceneShape::Sphere : SceneShape::Box;
    body.body->halfExtents=glm::vec3(0.5f);
    body.body->radius=0.5f;
    body.body->mass=37.0f;
    body.body->friction=0.6f;
    body.body->restitution=0.0f;
    body.body->managed=false;
    SceneObject& player=scene.CreateObject("player start");
    player.transform.position=offset+glm::vec3(2000.0f);
    player.playerStart=ScenePlayerStartComponent{};
    return scene;
}

BodyHandle Handle(World& w, EntityId id) {
    const EntityRecord* entity=w.world.FindEntity(id);
    if (!entity || entity->fidelity != SimulationFidelity::Full) return {};
    return w.world.DynamicBodies()[entity->slot].Handle();
}

bool CheckGeometry(World& w, EntityId id, const Fixture& f, bool sphere) {
    Shape moving, ground;
    BodyTransform a, b;
    const BodyHandle dynamic=Handle(w,id);
    BodyHandle support;
    for (BodyHandle h:w.world.Physics().AliveBodies()) {
        if (!w.world.Physics().IsDynamicBody(h)) support=h;
    }
    const bool shapes=w.world.Physics().GetBodyShape(dynamic,moving,a) &&
                      w.world.Physics().GetBodyShape(support,ground,b);
    Check(shapes,"real runtime shapes available");
    if (!shapes) return false;
    const V normal=Column(b.rotation,1), relative=Difference(a.position,b.position);
    const long double radius=sphere ? moving.radius : moving.halfExtents.y;
    const long double gap=Dot(normal,relative)-ground.halfExtents.y-radius;
    const long double x=Dot(Column(b.rotation,0),relative);
    const long double z=Dot(Column(b.rotation,2),relative);
    const bool equalRotation=a.rotation==b.rotation;
    Check(equalRotation,"body/support share actual stored orientation");
    Check(std::abs(x)+0.5L<ground.halfExtents.x && std::abs(z)+0.5L<ground.halfExtents.z,
          "support contact is inside the face, away from edges");
    const bool geometry=f.oblique ? (gap<0 && gap>-1.0e-5L) : gap==0;
    Check(geometry,f.oblique ? "represented oblique geometry has known shallow overlap" :
                              "represented canonical geometry is exactly touching");
    const glm::vec3 gravity=w.world.Gravity().Sample(a.position);
    std::printf("  GEOMETRY gap=%.21Lg class=%s body=(%.9g,%.9g,%.9g) "
                "support=(%.9g,%.9g,%.9g) stored_q=(%.9g,%.9g,%.9g,%.9g) "
                "gravity=(%.9g,%.9g,%.9g)\n",gap,f.oblique?"penetrating":"touching",
                a.position.x,a.position.y,a.position.z,b.position.x,b.position.y,b.position.z,
                b.rotation.w,b.rotation.x,b.rotation.y,b.rotation.z,gravity.x,gravity.y,gravity.z);
    return geometry && equalRotation;
}

void InspectStep(World& w, EntityId id, const char* phase) {
    EntityPhysicalState before, after;
    if (!w.world.GetEntityState(id,before)) {
        Check(false,"entity before ordinary fixed step");
        return;
    }
    w.Step();
    Check(w.world.GetEntityState(id,after),"entity after ordinary fixed step");
    const std::size_t contacts=w.world.Physics().LastStepContactCount();
    const float speed=glm::length(after.linearVelocity);
    const float spin=glm::length(after.angularVelocity);
    const float displacement=glm::length(after.position-before.position);
    const WorldState delta=CaptureWorldState(w.world);
    std::printf("  STEP %s contacts=%zu position=(%.9g,%.9g,%.9g) velocity=(%.9g,%.9g,%.9g) "
                "angular=(%.9g,%.9g,%.9g) speed=%.9g displacement=%.9g delta_entities=%zu\n",
                phase,contacts,after.position.x,after.position.y,after.position.z,
                after.linearVelocity.x,after.linearVelocity.y,after.linearVelocity.z,
                after.angularVelocity.x,after.angularVelocity.y,after.angularVelocity.z,
                speed,displacement,delta.entities.size());
    Check(contacts>0,"supported surface has first-step contact");
    // Fixed before executing the new test: much stricter than persistence's
    // existing 0.02 m/s / 0.01 m policy, and far below the 0.1635 m/s gravity
    // spike this regression targets. These are engineering solver tolerances,
    // not a claimed universal floating-point error bound.
    Check(std::isfinite(speed) && speed<1.0e-4f,"no first-step gravity velocity spike (<1e-4 m/s)");
    Check(std::isfinite(spin) && spin<1.0e-4f,"no artificial first-step angular velocity (<1e-4 rad/s)");
    Check(std::isfinite(displacement) && displacement<1.0e-5f,"no first-step gravity position spike (<1e-5 m)");
    Check(delta.entities.empty(),"unchanged supported entity produces no moved delta under unmodified persistence policy");
}

// Public convenience queries omit sphere orientation because sphere geometry
// is rotation invariant. Their returned anchors must still be interpreted
// correctly when passed to the ordinary solver with a rotated rigid body.
// Exact touching and central normal impulses isolate this API/frame contract;
// these are not separated-impact timing tests or a second contact mechanism.
void TestSphereConvenience(const char* name, const glm::quat& rotation, bool spherePair) {
    RigidBody a, b;
    a.position=glm::vec3(0,1,0);
    a.orientation=rotation;
    a.inverseMass=0.5f; // 2 kg
    a.inverseInertiaLocal=SolidSphereInverseInertia(2.0f,0.5f);
    a.linearVelocity=glm::vec3(0,-1,0);
    b.position=glm::vec3(0);
    b.orientation=spherePair ? glm::conjugate(rotation) : glm::quat(1,0,0,0);
    if (spherePair) {
        b.inverseMass=1.0f/3.0f; // 3 kg
        b.inverseInertiaLocal=SolidSphereInverseInertia(3.0f,0.5f);
    }
    const Contact contact=spherePair ? SphereVsSphere(a.position,0.5f,b.position,0.5f) :
        SphereVsBox(a.position,0.5f,b.position,b.orientation,glm::vec3(5.0f,0.5f,5.0f));
    Check(contact.hit,"sphere convenience query retains exact touching");
    Check(contact.signedSeparation==0.0,"sphere convenience query reports exact zero separation");
    ContactSolver solver;
    solver.AddContact(a,b,contact,0.0f,0.0f); // no tangential impulse or restitution
    solver.Prepare(SimulationTiming::kFixedTimestep);
    solver.SolveVelocities();
    // Independent 1-D mechanics: J = 1 / (1/mA + 1/mB).
    // Static floor: J=2, vA'=0. Two spheres: J=6/5, vA'=vB'=-2/5.
    const double expectedImpulse=spherePair ? 1.2 : 2.0;
    const glm::vec3 expectedA(0,spherePair ? -0.4f : 0.0f,0);
    const glm::vec3 expectedB(0,spherePair ? -0.4f : 0.0f,0);
    const float impulse=solver.Constraints().empty() ? -1.0f : solver.Constraints().front().normalImpulse;
    std::printf("  CONVENIENCE %s %s impulse=%.9g expected=%.17g "
                "velocityA=(%.9g,%.9g,%.9g) velocityB=(%.9g,%.9g,%.9g) "
                "angularA=(%.9g,%.9g,%.9g) angularB=(%.9g,%.9g,%.9g)\n",
                name,spherePair?"sphere-sphere":"sphere-box",impulse,expectedImpulse,
                a.linearVelocity.x,a.linearVelocity.y,a.linearVelocity.z,b.linearVelocity.x,b.linearVelocity.y,b.linearVelocity.z,
                a.angularVelocity.x,a.angularVelocity.y,a.angularVelocity.z,b.angularVelocity.x,b.angularVelocity.y,b.angularVelocity.z);
    Check(std::abs(double(impulse)-expectedImpulse)<1.0e-6,"central normal impulse agrees with independent two-mass mechanics");
    Check(glm::length(a.linearVelocity-expectedA)<1.0e-6f && glm::length(b.linearVelocity-expectedB)<1.0e-6f,
          "sphere orientation does not change central post-impact linear velocities");
    Check(glm::length(a.angularVelocity)<1.0e-7f && glm::length(b.angularVelocity)<1.0e-7f,
          "central normal impulse creates no angular velocity in either body");
}

// M78: completed physical observations must not be solver warm-start storage.
// This ordinary box/floor fixture receives an actual incoming sphere TOI;
// no controller, animation, query-generated contacts or gain retuning is used.
struct SolvedSupport {
    PhysicsWorld physics;
    BodyHandle floor,body,ball,second;
    explicit SolvedSupport(bool compound=false,bool connected=false) {
        physics.Init();
        if(compound) {
            // One support handle, two distinct collision primitives. Both
            // pads are beneath the broad box, on opposite sides of its COM.
            floor=physics.CreateShape(Shape::Compound({
                CompoundBox({-.6f,0,0},{.3f,.5f,.5f}),
                CompoundBox({ .6f,0,0},{.3f,.5f,.5f})}),
                {{0,-.5f,0},{1,0,0,0}},false,0,.8f,0);
        }else floor=physics.CreateStaticBox({0,-.5f,0},{4,.5f,3},.8f,0);
        body=physics.CreateDynamicBox({0,.5f,0},
            compound?glm::vec3(.8f,.5f,.5f):glm::vec3(.5f),4,.8f,0);
        if(connected) {
            second=physics.CreateDynamicBox({2,.5f,0},{.5f,.5f,.5f},4,.8f,0);
            JointSettings link;link.type=JointType::Fixed;link.bodyA=body;link.bodyB=second;
            link.anchorA={1,0,0};link.anchorB={-1,0,0};physics.CreateJoint(link);
            physics.SetPairCollisionEnabled(body,second,false);
        }
        for(int i=0;i<4;++i)Step();
    }
    void Step() {
        for(auto handle:{body,ball,second})if(handle.IsValid())
            physics.ApplyLinearAcceleration(handle,{0,-9.81f,0},SimulationTiming::kFixedTimestep);
        physics.Step(SimulationTiming::kFixedTimestep);
    }
    void Impact() {
        ball=physics.CreateDynamicSphere({0,1.3f,0},.15f,2,.8f,0);
        physics.SetLinearVelocity(ball,{0,-20,0});Step();
    }
};
unsigned SupportRows(PhysicsWorld& physics,BodyHandle body,BodyHandle floor) {
    std::vector<PhysicsWorld::BodyContactSample> contacts;
    if(!physics.ReadBodyContactSample(body,contacts))return 0;
    unsigned rows=0;
    for(const auto& contact:contacts)if(contact.other.id==floor.id&&contact.normalImpulse>0)++rows;
    return rows;
}
void TestCompletedSupportObservations() {
    SolvedSupport fixture;
    auto& physics=fixture.physics;
    std::vector<PhysicsWorld::BodyContactSample> contacts;
    Check(SupportRows(physics,fixture.body,fixture.floor)>0,"completed start solve exposes real load-bearing floor rows");
    fixture.Impact();
    Check(physics.LastStepStats().impactEvents>0,"incoming sphere produces an actual bounded TOI solve");
    Check(physics.ReadBodyContactSample(fixture.body,contacts),"completed TOI observations remain available after warm-cache invalidation");
    unsigned floors=0;double load=0;bool finite=true;
    for(const auto& contact:contacts)if(contact.other.id==fixture.floor.id){
        ++floors;load+=contact.normalImpulse;
        finite&=std::isfinite(contact.normalImpulse)&&contact.normalImpulse>0&&
            std::isfinite(contact.point.x)&&std::isfinite(contact.point.y)&&std::isfinite(contact.point.z)&&
            std::isfinite(contact.normal.x)&&std::isfinite(contact.normal.y)&&std::isfinite(contact.normal.z);
    }
    Check(floors>0&&load>0&&finite,"actual ground reaction is observable without a scripted ground or balance controller");
    std::printf("  COMPLETED_TOI floor_rows=%u normal_impulse=%.9g events=%zu\n",floors,load,physics.LastStepStats().impactEvents);
    Check(!physics.ReadBodyContactSample(fixture.body,contacts,1)&&contacts.empty(),"completed-row contact overflow remains fail-closed");
    Check(!physics.ReadBodyContactSample(fixture.body,contacts,64,1)&&contacts.empty(),"completed-row candidate overflow remains fail-closed");
    Check(!physics.ReadBodyContactSample(fixture.body,contacts,0),"zero completed-row budget is invalid");

    physics.SetCollisionFilter(fixture.body,0,0);
    Check(physics.ReadBodyContactSample(fixture.body,contacts)&&contacts.empty(),"current collision masks exclude completed support observations");
    physics.SetCollisionFilter(fixture.body,0,kAllCategories);
    Check(physics.ReadBodyContactSample(fixture.body,contacts)&&contacts.empty(),"restoring a filter before another solve cannot revive old observations");
    fixture.Step();
    Check(SupportRows(physics,fixture.body,fixture.floor)>0,"next real solve reacquires support after filter invalidation");
    physics.SetPhysicalMaterial(fixture.body,"changed authored material",.6f,0);
    Check(physics.ReadBodyContactSample(fixture.body,contacts)&&contacts.empty(),"physical-material mutation invalidates completed observations with warm history");
    fixture.Step();
    Check(SupportRows(physics,fixture.body,fixture.floor)>0,"next real solve reacquires support after material invalidation");
    const auto pose=physics.GetTransform(fixture.body);
    physics.ResetBody(fixture.body,pose.position,pose.rotation);
    Check(physics.ReadBodyContactSample(fixture.body,contacts)&&contacts.empty(),"even same-pose reset invalidates completed observation revisions");
    fixture.Step();
    physics.SetBodyEnabled(fixture.body,false);
    Check(!physics.ReadBodyContactSample(fixture.body,contacts)&&contacts.empty(),"disabled body cannot expose completed observations");
    physics.SetBodyEnabled(fixture.body,true);
    Check(physics.ReadBodyContactSample(fixture.body,contacts)&&contacts.empty(),"re-enabling before another solve cannot revive disabled observations");
    fixture.Step();
    physics.SetBodySensor(fixture.body,true);
    Check(!physics.ReadBodyContactSample(fixture.body,contacts)&&contacts.empty(),"sensor cannot grant physical support from completed rows");
    physics.SetBodySensor(fixture.body,false);
    Check(physics.ReadBodyContactSample(fixture.body,contacts)&&contacts.empty(),"leaving sensor mode cannot revive earlier physical observations");
    fixture.Step();
    physics.SetPairCollisionEnabled(fixture.body,fixture.floor,false);
    Check(physics.ReadBodyContactSample(fixture.body,contacts)&&contacts.empty(),"suppressed pair cannot expose completed support");
    physics.SetPairCollisionEnabled(fixture.body,fixture.floor,true);
    Check(physics.ReadBodyContactSample(fixture.body,contacts)&&contacts.empty(),"unsuppressing before another solve cannot revive old support");
    fixture.Step();
    // A different collider can still have a legitimate completed reaction.
    // Slot reuse must reject the removed/replacement floor's observations,
    // without erasing real contacts belonging to that other collider.
    physics.ReadBodyContactSample(fixture.body,contacts);
    auto otherContacts=contacts;
    otherContacts.erase(std::remove_if(otherContacts.begin(),otherContacts.end(),
        [&](const auto& contact){return contact.other.id==fixture.floor.id;}),otherContacts.end());
    Check(!otherContacts.empty(),"slot-reuse fixture exercises another real load-bearing contact");
    const auto removedFloor=fixture.floor;physics.DestroyBody(removedFloor);
    fixture.floor=physics.CreateStaticBox({0,-.5f,0},{4,.5f,3},.8f,0);
    const bool readAfterReuse=physics.ReadBodyContactSample(fixture.body,contacts);
    const bool staleFloor=std::any_of(contacts.begin(),contacts.end(),[&](const auto& contact){
        return contact.other.id==removedFloor.id||contact.other.id==fixture.floor.id;});
    Check(fixture.floor.id!=removedFloor.id&&readAfterReuse&&!staleFloor,"reused support slot cannot alias completed contact generations");
    bool retainedOtherContacts=readAfterReuse&&contacts.size()==otherContacts.size();
    for(std::size_t i=0;retainedOtherContacts&&i<contacts.size();++i)
        retainedOtherContacts=contacts[i].other.id==otherContacts[i].other.id&&
            contacts[i].normalImpulse==otherContacts[i].normalImpulse&&
            contacts[i].point==otherContacts[i].point&&contacts[i].normal==otherContacts[i].normal;
    Check(retainedOtherContacts,"removing a neighbour preserves other genuine completed reactions");
    const auto removedBody=fixture.body;physics.DestroyBody(removedBody);
    fixture.body=physics.CreateDynamicBox({0,8,0},{.5f,.5f,.5f},4,.8f,0);
    Check(!physics.ReadBodyContactSample(removedBody,contacts)&&contacts.empty(),"destroyed observer handle fails safely after slot reuse");
    Check(physics.ReadBodyContactSample(fixture.body,contacts)&&contacts.empty(),"new body inherits no old completed observations");

    SolvedSupport island(false,true);
    Check(SupportRows(island.physics,island.second,island.floor)>0,"connected articulation has observed stance support before incoming impact");
    island.Impact();
    Check(island.physics.LastStepStats().impactEvents>0&&SupportRows(island.physics,island.second,island.floor)>0,
          "incoming connected-body impact cannot erase the other load-bearing stance observations");

    SolvedSupport compound(true,false);
    compound.Impact();
    bool left=false,right=false;
    Check(compound.physics.ReadBodyContactSample(compound.body,contacts),"compound primitive TOI rows retain valid observation mapping");
    for(const auto& contact:contacts)if(contact.other.id==compound.floor.id&&contact.normalImpulse>0){
        left|=contact.point.x<0;right|=contact.point.x>0;
    }
    Check(compound.physics.LastStepStats().impactEvents>0&&left&&right,"distinct compound support primitives retain independent actual solved rows");
}

void Run(const Fixture& f, bool sphere, const glm::vec3& offset) {
    std::printf("CASE %s %s offset=(%.9g,%.9g,%.9g)\n",f.name,sphere?"sphere":"box",offset.x,offset.y,offset.z);
    const int failuresBefore=g_failures;
    EntityId id;
    const Scene authored=MakeScene(f,sphere,offset,id);
    std::string authoredText,error;
    Scene scene;
    Check(SaveSceneToString(authored,authoredText),"ordinary authored scene serializes");
    if (!LoadSceneFromString(authoredText,scene,error)) {
        Check(false,"ordinary authored scene loads: "+error);
        return;
    }
    World w;
    if (!w.Begin(scene,error)) { Check(false,"ordinary runtime begins: "+error); return; }
    Check(w.world.Physics().AliveBodyCount()==2 && w.world.Entities().size()==1,
          "exactly one dynamic entity and static support, no other contact bodies");
    if (!CheckGeometry(w,id,f,sphere)) return;
    InspectStep(w,id,"initial");

    EntityPhysicalState retained,reconstructed;
    w.world.GetEntityState(id,retained);
    const BodyHandle old=Handle(w,id);
    Check(w.world.SetEntityFidelity(id,SimulationFidelity::Coarse,&error),"Full -> Coarse: "+error);
    Check(!w.world.Physics().IsDynamicBody(old),"released old body handle is invalid");
    Check(w.world.SetEntityFidelity(id,SimulationFidelity::Full,&error),"Coarse -> Full: "+error);
    const BodyHandle fresh=Handle(w,id);
    Check(fresh.IsValid() && fresh.id!=old.id,"reconstruction obtains a fresh body handle");
    Check(w.world.GetEntityState(id,reconstructed) && reconstructed.position==retained.position &&
          reconstructed.linearVelocity==retained.linearVelocity && reconstructed.angularVelocity==retained.angularVelocity &&
          reconstructed.rotation==retained.rotation,"reconstruction preserves exact retained pose and velocities");
    InspectStep(w,id,"reconstructed");

    // Capture and reload through the real FTFT1 compatibility/atomicity path.
    // No fabricated compatibility header or scene-name-only shortcut.
    const WorldState captured=CaptureWorldState(w.world);
    std::string saved;
    Check(SaveWorldStateToString(captured,saved),"valid baseline delta serializes");
    WorldState parsed;
    Check(LoadWorldStateFromString(saved,parsed,error),"valid baseline delta parses: "+error);
    World restored;
    if (!restored.Begin(scene,error)) { Check(false,"reload runtime begins: "+error); return; }
    Check(parsed.compatibility.baselineFingerprint==restored.world.BaselineFingerprint(),
          "saved delta carries the actual matching authored content fingerprint");
    Check(ApplyWorldState(restored.world,parsed,error),"real FTFT1 preflight accepts valid baseline: "+error);
    Check(CaptureWorldState(restored.world).entities.empty(),"reload introduces no artificial moved records");
    InspectStep(restored,id,"persisted-reload");
    std::printf("CASE_RESULT %s %s %s\n",f.name,sphere?"sphere":"box",g_failures==failuresBefore?"PASS":"FAIL");
}
}

int main() {
    const std::vector<Fixture> cases={
        {"zero",{1,0,0,0},{0,1,0},0,false},
        {"up_y",{1,0,0,0},{0,1,0},9.81f,false},
        {"down_y",{0,1,0,0},{0,-1,0},9.81f,false},
        {"up_x",{1,0,0,-1},{1,0,0},9.81f,false},
        {"down_x",{1,0,0,1},{-1,0,0},9.81f,false},
        {"up_z",{1,1,0,0},{0,0,1},9.81f,false},
        {"down_z",{1,-1,0,0},{0,0,-1},9.81f,false},
        {"oblique",{3,1,2,-1},{0,0,0},9.81f,true},
    };
    int fixtures=0;
    for (const Fixture& f:cases) for (bool sphere:{false,true}) {
        Run(f,sphere,glm::vec3(0));
        ++fixtures;
        if (!f.oblique) {
            Run(f,sphere,glm::vec3(137,-42,88));
            ++fixtures;
        }
    }
    const glm::quat quarter=glm::normalize(glm::quat(1,0,0,1));
    const glm::quat oblique=glm::normalize(glm::quat(3,1,2,-1));
    for (bool spherePair:{false,true}) {
        TestSphereConvenience("identity",glm::quat(1,0,0,0),spherePair);
        TestSphereConvenience("quarter_z",quarter,spherePair);
        TestSphereConvenience("oblique",oblique,spherePair);
    }
    TestCompletedSupportObservations();
    std::printf("FTFT4A contact lifecycle: %d fixtures, %d fixed steps, 6 convenience contacts, %d checks, %d failures\n",
                fixtures,g_steps,g_checks,g_failures);
    return g_failures==0?0:1;
}
