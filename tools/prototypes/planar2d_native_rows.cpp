// Real MK2 Stage 0 experiment. The private Stage0Planar enum exists ONLY in
// the runner's generated source copy. Shapes remain 3D test proxies, not 2D API.
#include "PhysicsWorld.h"
#include "RigidBody.h"
#include "SaveArchive.h"
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int checks = 0;
void Require(bool ok, const char* label) {
    ++checks;
    if (!ok) throw std::runtime_error(label);
}
constexpr float dt = 1.f / 60;
JointHandle Planar(PhysicsWorld& world, BodyHandle body, glm::quat frame = {1, 0, 0, 0}) {
#ifdef PLANAR_REDUCED
    return {world.Stage0ConfigurePlane(body,frame*glm::vec3(0,0,1))?1u:0u};
#else
    JointSettings settings;
    settings.bodyA = body;
    settings.type = JointType::Stage0Planar;
    settings.anchorB = world.GetTransform(body).position;
    settings.frameB = frame;
    return world.CreateJoint(settings);
#endif
}
double Energy(PhysicsWorld& world, BodyHandle body, double mass) {
    const glm::dvec3 v(world.GetLinearVelocity(body)), w(world.GetAngularVelocity(body));
    return .5 * mass * glm::dot(v, v) + .5 * glm::dot(w, glm::dmat3(world.GetInertiaWorld(body)) * w);
}

void Impulses() {
    PhysicsWorld world;
    world.Init();
    auto body = world.CreateDynamicBox({0, 0, 0}, {.5f, .7f, .1f}, 3, 0, 0);
    Require(Planar(world, body).IsValid(), "private native planar joint created");
    world.ApplyLinearImpulse(body, {3, 6, 9});
    world.ApplyAngularImpulse(body, {1, 2, 3});
    std::printf("direct_impulse_before_step forbidden_vz=%.9g forbidden_spin=%.9g\n",
                world.GetLinearVelocity(body).z, glm::length(glm::vec2(world.GetAngularVelocity(body))));
#ifdef PLANAR_REDUCED
    Require(world.GetLinearVelocity(body).z == 0 && glm::vec2(world.GetAngularVelocity(body)) == glm::vec2(0),
            "reduced motion is consistent immediately at public impulse boundary");
    bool rejected = false;
    try { world.SetLinearVelocity(body, {0, 0, 3}); }
    catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected, "incompatible 3D velocity command explicitly rejects");
    rejected = false;
    try { world.SetAngularVelocity(body, {2, 0, 0}); }
    catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected, "incompatible 3D angular command explicitly rejects");
#endif
    for (int i = 0; i < 120; ++i) world.Step(dt);
    const auto v = world.GetLinearVelocity(body), w = world.GetAngularVelocity(body);
    const auto p = world.GetTransform(body).position;
    std::printf("after_120_steps z=%.9g vz=%.9g spin_xy=%.9g x=%.9g y=%.9g\n",
                p.z, v.z, glm::length(glm::vec2(w)), p.x, p.y);
    Require(std::abs(v.x - 1) < 2e-4f && std::abs(v.y - 2) < 2e-4f, "native plane leaves tangential linear impulse free");
    Require(std::abs(v.z) < 1e-5f && glm::length(glm::vec2(w)) < 1e-4f, "native rows remove forbidden velocity by completed step");
    Require(std::abs(w.z - 3.f/.74f) < 2e-5f, "native rectangle agrees with analytic lamina Izz");
    // Do not reinterpret the pre-step observations as a pass: ordinary direct
    // impulse APIs currently publish forbidden state before rows are solved.
}

void WallAndCorner() {
    for (const float speed : {5.f, 150.f}) {
        PhysicsWorld world;
        world.Init();
        world.CreateStaticBox({2, 0, 0}, {.05f, 4, 2}, 0, 1);
        world.CreateStaticBox({0, 2, 0}, {4, .05f, 2}, 0, 1);
        auto body = world.CreateDynamicSphere({0, 0, 0}, .2f, 1, 0, 1);
        Require(Planar(world, body).IsValid(), "CCD fixture planar joint");
        world.SetLinearVelocity(body, {speed, speed, 0});
        const auto initial = Energy(world, body, 1);
        double maxEnergy = initial;
        float maxForbidden = 0, maxPlane = 0, maxPosition = 0;
        std::size_t responses = 0;
        for (int i = 0; i < 60; ++i) {
            world.Step(dt);
            const auto p = world.GetTransform(body).position;
            maxPosition = std::max(maxPosition, std::max(p.x, p.y));
            maxPlane = std::max(maxPlane, std::abs(p.z));
            maxForbidden = std::max(maxForbidden, std::abs(world.GetLinearVelocity(body).z));
            maxEnergy = std::max(maxEnergy, Energy(world, body, 1));
            responses += world.LastStepStats().impactResponsesApplied;
            if(speed==150&&i==0){
                const auto v=world.GetLinearVelocity(body);
                Require(glm::length(glm::vec2(p)-glm::vec2(1))<5e-5f,"CCD corner advances actual remaining flight time");
                Require(glm::length(glm::vec2(v)-glm::vec2(-150))<1e-4f,"CCD corner publishes both physical rebound velocities");
                Require(world.LastStepStats().impactSafetyFallback==0,"CCD corner pass does not rely on path holds");
            }
        }
        std::printf("wall_corner speed=%.9g max_xy=%.9g plane=%.9g forbidden=%.9g energy_ratio=%.9g CCD_responses=%zu\n",
                    speed, maxPosition, maxPlane, maxForbidden, maxEnergy / initial, responses);
        Require(maxPosition < 1.77f, "native CCD does not cross corner walls");
        Require(maxPlane < 1e-5f && maxForbidden < 1e-5f, "native symmetric corner remains planar");
        Require(maxEnergy <= initial * 1.0001, "native elastic corner creates no energy");
        if (speed == 150) Require(responses > 0, "fast fixture exercises actual CCD response");
    }
}

void SupportAndSleep() {
    for (const bool rotated : {false, true}) {
        const auto q = rotated ? glm::angleAxis(.7f, glm::normalize(glm::vec3(1, 2, 3))) : glm::quat(1, 0, 0, 0);
        const auto n = q * glm::vec3(0, 0, 1);
        PhysicsWorld world;
        world.Init();
        world.CreateStaticBox(q * glm::vec3(0, -.5f, 0), q, {8, .5f, 2}, .7f, 0);
        auto body = world.CreateDynamicBox(q * glm::vec3(0, 1, 0), {.3f, .3f, .1f}, 2, .7f, 0);
        world.ResetBody(body, q * glm::vec3(0, 1, 0), q);
        Require(Planar(world, body, q).IsValid(), "support native joint");
        float maximumPlane = 0, maximumTilt = 0;
        for (int i = 0; i < 600; ++i) {
            world.ApplyLinearAcceleration(body, q * glm::vec3(0, -9.81f, 0), dt);
            world.Step(dt);
            const auto pose = world.GetTransform(body);
            maximumPlane = std::max(maximumPlane, std::abs(glm::dot(n, pose.position)));
            maximumTilt = std::max(maximumTilt, glm::length(glm::cross(n, pose.rotation * glm::vec3(0, 0, 1))));
        }
        const auto local = glm::inverse(q) * world.GetTransform(body).position;
        const bool sleeping = world.IsSleeping(body);
        std::printf("support rotated=%d plane=%.9g tilt=%.9g height=%.9g asleep=%d\n", rotated, maximumPlane, maximumTilt, local.y, sleeping);
        Require(std::abs(local.y - .3f) < .015f, "native supported height");
        Require(maximumPlane < 1e-4f && maximumTilt < 1e-3f, "native support planar tolerances");
        Require(sleeping, "planar world-anchor does not prevent native sleeping");
        world.ApplyLinearImpulse(body, q * glm::vec3(0, 2, 0));
        Require(!world.IsSleeping(body), "ordinary impulse wakes planar body");
    }
}

void ContactAndForces() {
    PhysicsWorld world;
    world.Init();
    world.CreateStaticBox({0, -.5f, 0}, {8, .5f, 2}, .6f, 0);
    auto body = world.CreateDynamicBox({0, 2, 0}, {.5f, .3f, .1f}, 3, .6f, .3f);
    Require(Planar(world, body).IsValid(), "off-centre contact plane");
    world.ApplyImpulseAtPoint(body, {4, 0, 0}, {0, 2.3f, 0});
    float maxPlane = 0, maxTilt = 0;
    for (int i = 0; i < 360; ++i) {
        world.ApplyLinearAcceleration(body, {0, -9.81f, 0}, dt);
        world.Step(dt);
        const auto pose = world.GetTransform(body);
        maxPlane = std::max(maxPlane, std::abs(pose.position.z));
        maxTilt = std::max(maxTilt, glm::length(glm::cross(pose.rotation * glm::vec3(0, 0, 1), glm::vec3(0, 0, 1))));
    }
    std::printf("offcentre_floor plane=%.9g tilt=%.9g speed=%.9g\n", maxPlane, maxTilt, glm::length(world.GetLinearVelocity(body)));
    Require(maxPlane < 1e-4f && maxTilt < 1e-3f, "off-centre floor response stays planar");
    Require(glm::length(world.GetLinearVelocity(body)) < .05f, "physical friction brings spinning plate to rest");
    PhysicsWorld free;
    free.Init();
    auto driven = free.CreateDynamicBox({0, 0, 0}, {.5f, .3f, .1f}, 3, 0, 0);
    Require(Planar(free, driven).IsValid(), "force fixture plane");
    free.ApplyForce(driven, {3, 6, 30});
    free.ApplyTorque(driven, {3, 6, 9});
    free.Step(dt);
    const auto v = free.GetLinearVelocity(driven), w = free.GetAngularVelocity(driven);
    std::printf("force_velocity=(%.9g,%.9g,%.9g) spin=(%.9g,%.9g,%.9g)\n", v.x, v.y, v.z, w.x, w.y, w.z);
    Require(std::abs(v.x - dt) < 1e-6f && std::abs(v.y - 2 * dt) < 1e-6f, "planar force preserves analytic in-plane acceleration");
    Require(std::abs(v.z) < 1e-5f && glm::length(glm::vec2(w)) < 1e-4f, "normal force/torque do not create completed forbidden motion");
}

void PairAndHinge() {
    PhysicsWorld world;
    world.Init();
    auto a=world.CreateDynamicSphere({-1,0,0},.2f,2,0,1);
    auto b=world.CreateDynamicSphere({1,.1f,0},.2f,3,0,1);
    Require(Planar(world,a).IsValid()&&Planar(world,b).IsValid(),"two native planar participants");
    world.SetLinearVelocity(a,{5,0,0});
    auto moment=[&]{double result=0;for(auto h:{a,b}){
        const double mass=world.GetMass(h);
        const auto p=glm::dvec3(world.GetTransform(h).position),v=glm::dvec3(world.GetLinearVelocity(h)),w=glm::dvec3(world.GetAngularVelocity(h));
        result+=glm::cross(p,mass*v).z+(glm::dmat3(world.GetInertiaWorld(h))*w).z;
    }return result;};
    const auto initialMoment=moment();
    const auto initialEnergy=Energy(world,a,2)+Energy(world,b,3);
    double momentumError=0,angularError=0,energyError=0;
    for(int i=0;i<60;++i){world.Step(dt);
        momentumError=std::max(momentumError,glm::length(2.*glm::dvec3(world.GetLinearVelocity(a))+3.*glm::dvec3(world.GetLinearVelocity(b))-glm::dvec3(10,0,0)));
        angularError=std::max(angularError,std::abs(moment()-initialMoment));
        energyError=std::max(energyError,std::abs((Energy(world,a,2)+Energy(world,b,3))/initialEnergy-1));
    }
    std::printf("free_pair linear_error=%.12g angular_z_error=%.12g relative_energy_error=%.12g\n",momentumError,angularError,energyError);
    Require(momentumError<1e-4&&angularError<1e-4&&energyError<1e-4,"actual two-body momentum/angular/elastic energy");
    Require(world.GetLinearVelocity(b).x>1,"native collision transfers momentum to second body");

    PhysicsWorld hinges;
    hinges.Init();
    auto rotor=hinges.CreateDynamicBox({0,0,0},{.5f,.2f,.1f},2,0,0);
    Require(Planar(hinges,rotor).IsValid(),"hinge rotor plane");
    JointSettings joint;
    joint.type=JointType::Hinge;
    joint.bodyA=rotor;
    joint.frameA=joint.frameB=glm::angleAxis(-glm::half_pi<float>(),glm::vec3(0,1,0));
#ifdef PLANAR_REDUCED
    Require(!hinges.CreateJoint(joint).IsValid(),"unsupported approximate 3D hinge rejects on planar body");
#endif
    joint.type=JointType::Stage0Pin;
    joint.frameA=joint.frameB=glm::quat(1,0,0,0);
    const auto hinge=hinges.CreateJoint(joint);
    Require(hinge.IsValid(),"explicit planar pin shares native row solver");
    hinges.ApplyAngularImpulse(rotor,{0,0,1});
    const auto start=Energy(hinges,rotor,2);
    for(int i=0;i<120;++i)hinges.Step(dt);
    const auto w=hinges.GetAngularVelocity(rotor);
    Require(glm::length(hinges.GetTransform(rotor).position)<1e-5&&std::abs(w.z)>1&&glm::length(glm::vec2(w))<1e-4,"native hinge permits in-plane rotation only");
    Require(std::abs(Energy(hinges,rotor,2)/start-1)<1e-4,"native hinge free rotation energy");
}

void MobilityPersistence() {
#ifdef PLANAR_REDUCED
    PhysicsWorld world;
    world.Init();
    auto body=world.CreateDynamicBox({0,0,0},{.5f,.3f,.1f},3,0,0);
    Require(Planar(world,body).IsValid(),"persisted mobility source");
    SaveArchive saved;
    world.PersistBodyMass(saved,body);
    auto target=world.CreateDynamicBox({3,0,0},{.5f,.3f,.1f},3,0,0);
    SaveArchive restore(saved.bytes);
    world.PersistBodyMass(restore,target);
    restore.Finish();
    world.ApplyLinearImpulse(target,{3,0,30});
    Require(world.GetLinearVelocity(target)==glm::vec3(1,0,0),"saved mobility restores actual impulse response on unconfigured body");
    for(int failure=0;failure<3;++failure){
        SaveArchive invalid;
        float inverseMass=1.f/3;
        auto inverseInertia=SolidBoxInverseInertia(3,{.5f,.3f,.1f});
        uint32_t version=failure==0?99:1;
        float inverseIz=1;
        glm::vec3 normal=failure==1?glm::vec3(0):glm::vec3(0,0,1);
        invalid(inverseMass,inverseInertia,version,inverseIz,normal);
        if(failure==2)invalid.bytes.pop_back();
        auto before=world.GetLinearVelocity(target);
        bool rejected=false;
        try {SaveArchive input(invalid.bytes);world.PersistBodyMass(input,target);}
        catch(const std::exception&){rejected=true;}
        Require(rejected&&world.GetLinearVelocity(target)==before,"invalid/versioned/truncated mobility rejects without changing velocity");
        world.ApplyLinearImpulse(target,{0,0,3});
        Require(world.GetLinearVelocity(target)==before,"rejected mobility leaves prior physical response intact");
    }
    Require(!world.Stage0ConfigurePlane({},{0,0,1}),"stale planar body rejects");
    world.DestroyBody(target);
    auto reused=world.CreateDynamicBox({3,0,0},{.5f,.3f,.1f},3,0,0);
    world.ApplyLinearImpulse(reused,{0,0,3});
    Require(std::abs(world.GetLinearVelocity(reused).z-1)<1e-6,"slot reuse does not inherit planar mobility");
#endif
}

void LegacyHingeFailure() {
    PhysicsWorld world;world.Init();
    auto body=world.CreateDynamicBox({0,0,0},{.5f,.2f,.1f},2,0,0);
    Require(Planar(world,body).IsValid(),"legacy hinge counterexample plane");
    JointSettings settings;settings.bodyA=body;settings.type=JointType::Hinge;
    settings.frameA=settings.frameB=glm::angleAxis(-glm::half_pi<float>(),glm::vec3(0,1,0));
    Require(world.CreateJoint(settings).IsValid(),"legacy diagnostic bypasses ONLY unsupported-joint guard");
    world.ApplyAngularImpulse(body,{0,0,1});
    const auto initial=world.GetAngularVelocity(body);
    for(int i=0;i<120;++i)world.Step(dt);
    const auto final=world.GetAngularVelocity(body);
    std::printf("legacy_hinge spin_before=%.12g spin_after=%.12g\n",initial.z,final.z);
    Require(std::abs(final.z-initial.z)<1e-4f,"approximate 3D hinge must preserve planar free spin (expected failure)");
}

// Stage 0 native-physics participant continuation, not a RuntimeWorld game save
// or public format. Body identities are resolved anew in a genuinely fresh
// process. Captures native contacts/joints, not cached gameplay status.
struct Continuation {
    PhysicsWorld world;
    BodyHandle body, ground;
    JointHandle plane;
    explicit Continuation(bool constrained=true) {
        world.Init();
        ground = world.CreateStaticBox({0, -.5f, 0}, {8, .5f, 2}, .6f, 0);
        body = world.CreateDynamicBox({0, 1, 0}, {.4f, .3f, .1f}, 3, .6f, 0);
        plane = constrained?Planar(world, body):JointHandle{};
        if (constrained&&!plane.IsValid()) throw std::runtime_error("cold fixture plane construction");
        world.ApplyImpulseAtPoint(body, {1, 0, 0}, {0, 1.2f, 0});
    }
    void Step(int count) {
        for (int i = 0; i < count; ++i) {
            world.ApplyLinearAcceleration(body, {0, -9.81f, 0}, dt);
            world.Step(dt);
        }
    }
    void State(SaveArchive& archive) {
        auto pose = world.GetTransform(body);
        auto linear = world.GetLinearVelocity(body), angular = world.GetAngularVelocity(body);
        archive(pose.position, pose.rotation, linear, angular);
        if (archive.reading) {
            world.ResetBody(body, pose.position, pose.rotation);
            world.SetLinearVelocity(body, linear);
            world.SetAngularVelocity(body, angular);
        }
        world.PersistBodyMass(archive, body);
        world.PersistBodyForces(archive, body);
        world.PersistSleep(archive, body);
#ifndef PLANAR_REDUCED
        if(plane.IsValid())world.PersistJointContinuation(archive, plane);
#endif
        const auto identity = [&](BodyHandle h) -> uint64_t { return h.id == ground.id ? 1 : h.id == body.id ? 2 : 0; };
        const auto resolve = [&](uint64_t id) { return id == 1 ? ground : id == 2 ? body : BodyHandle{}; };
        world.PersistTouches(archive, identity, resolve);
        world.PersistCompletedContactObservations(archive, identity, resolve);
        archive.Finish();
    }
    std::string Endpoint() {
        SaveArchive archive;
        auto pose = world.GetTransform(body);
        auto v = world.GetLinearVelocity(body), w = world.GetAngularVelocity(body);
        auto asleep = world.IsSleeping(body);
        archive(pose.position, pose.rotation, v, w, asleep);
        return archive.bytes;
    }
};
void Cold(const std::string& mode, const std::string& directory, bool ablateWarm=false) {
    const std::string path = directory + "/";
    const bool unconstrained=mode.find("cold3d-")==0;
    Continuation fixture(!unconstrained);
    const auto action=unconstrained?mode.substr(7):mode.substr(5);
    if (action == "write") {
        fixture.Step(45);
        const auto checkpoint=fixture.Endpoint();
        std::ofstream(path + "checkpoint.bin",std::ios::binary).write(checkpoint.data(),checkpoint.size());
        SaveArchive saved;
        fixture.State(saved);
        std::ofstream(path + "snapshot.bin", std::ios::binary).write(saved.bytes.data(), saved.bytes.size());
        if(ablateWarm)fixture.world.Stage0DropContactWarmCache();
        std::ofstream trace(path+"baseline.trace",std::ios::binary);
        for(int i=0;i<120;++i){fixture.Step(1);const auto p=fixture.Endpoint();trace.write(p.data(),p.size());}
        auto endpoint = fixture.Endpoint();
        std::ofstream(path + "expected.bin", std::ios::binary).write(endpoint.data(), endpoint.size());
        std::printf("cold baseline endpoint bytes=%zu height=%.9g asleep=%d\n", endpoint.size(), fixture.world.GetTransform(fixture.body).position.y, fixture.world.IsSleeping(fixture.body));
    } else {
        std::ifstream input(path + "snapshot.bin", std::ios::binary);
        std::string bytes((std::istreambuf_iterator<char>(input)), {});
        Require(!bytes.empty(), "cold snapshot exists");
        SaveArchive saved(bytes);
        fixture.State(saved);
        const auto checkpoint=fixture.Endpoint();
        std::ifstream before(path+"checkpoint.bin",std::ios::binary);
        const std::string expectedBefore((std::istreambuf_iterator<char>(before)),{});
        Require(checkpoint==expectedBefore,"cold restored pose/velocities/sleep are identical BEFORE continuation");
        if(ablateWarm)fixture.world.Stage0DropContactWarmCache();
        std::ofstream trace(path+"restored.trace",std::ios::binary);
        for(int i=0;i<120;++i){fixture.Step(1);const auto p=fixture.Endpoint();trace.write(p.data(),p.size());}
        auto endpoint = fixture.Endpoint();
        std::ofstream(path + "actual.bin", std::ios::binary).write(endpoint.data(), endpoint.size());
        std::ifstream expected(path + "expected.bin", std::ios::binary);
        const std::string target((std::istreambuf_iterator<char>(expected)), {});
        if (action == "read") Require(target == endpoint, "fresh-process native physical endpoint matches uninterrupted bytes");
        else {
            // Separate, explicitly physical criterion; the earlier strict-byte
            // failure remains a failing diagnostic, never retrospectively PASS.
            SaveArchive targetState(target), actualState(endpoint);
            glm::vec3 tp, ap, tv, av, tw, aw;
            glm::quat tq, aq;
            bool ts = false, as = false;
            targetState(tp,tq,tv,tw,ts); actualState(ap,aq,av,aw,as);
            targetState.Finish(); actualState.Finish();
            const double poseError = glm::length(glm::dvec3(tp)-glm::dvec3(ap));
            const double angleError = 2*std::min(glm::length(glm::dquat(tq)-glm::dquat(aq)),glm::length(glm::dquat(tq)+glm::dquat(aq)));
            std::printf("cold physical position_error=%.12g angle_error=%.12g linear_error=%.12g angular_error=%.12g byte_identical=%d\n",
                        poseError,angleError,glm::length(tv-av),glm::length(tw-aw),target==endpoint);
            Require(poseError < 5e-6 && angleError < 5e-6 && glm::length(tv-av) < 5e-5f && glm::length(tw-aw) < 5e-5f && ts==as,
                    "fresh-process native endpoint within declared float physical tolerance");
        }
        Require(std::abs(fixture.world.GetTransform(fixture.body).position.y - .3f) < .015f, "cold physical endpoint rests on actual ground");
        std::printf("cold restored endpoint bytes=%zu height=%.9g asleep=%d\n", endpoint.size(), fixture.world.GetTransform(fixture.body).position.y, fixture.world.IsSleeping(fixture.body));
    }
}

void Digest3D() {
    PhysicsWorld world;
    world.Init();
    world.CreateStaticBox({0, -.5f, 0}, {8, .5f, 8}, .6f, .2f);
    std::vector<BodyHandle> bodies;
    for (int i = 0; i < 6; ++i) {
        auto h = world.CreateDynamicBox({float(i) - 2, 1.f + .5f * i, .1f * i}, {.3f, .4f, .2f}, 3, .6f, .2f);
        world.SetAngularVelocity(h, {.3f * i, -.4f, 2.f});
        bodies.push_back(h);
    }
    JointSettings hinge;
    hinge.bodyA = bodies[4];
    hinge.bodyB = bodies[5];
    hinge.type = JointType::Hinge;
    hinge.anchorA = {.5f, .25f, .05f};
    hinge.anchorB = {-.5f, -.25f, -.05f};
    Require(world.CreateJoint(hinge).IsValid(), "3D digest hinge");
    uint64_t hash = UINT64_C(14695981039346656037);
    for (int step = 0; step < 300; ++step) {
        for (auto h : bodies) world.ApplyLinearAcceleration(h, {0, -9.81f, 0}, dt);
        if (step == 130) world.ApplyImpulseAtPoint(bodies[1], {1, 3, 2}, world.GetTransform(bodies[1]).position + glm::vec3(.1f, .3f, .2f));
        world.Step(dt);
        SaveArchive samples;
        for (auto h : bodies) {
            auto p = world.GetTransform(h);
            auto v = world.GetLinearVelocity(h), w = world.GetAngularVelocity(h);
            samples(p.position, p.rotation, v, w);
        }
        for (const unsigned char byte : samples.bytes) { hash ^= byte; hash *= UINT64_C(1099511628211); }
    }
    std::printf("3D_PHYSICAL_TRAJECTORY_DIGEST=%016llx steps=300 bodies=6\n", static_cast<unsigned long long>(hash));
}

void Scaling() {
    for (int count : {32, 256, 1024}) {
        PhysicsWorld world;
        world.Init();
        std::vector<BodyHandle> bodies;
        for (int i = 0; i < count; ++i) {
            auto h = world.CreateDynamicSphere({float(i % 32) * 3, float(i / 32) * 3, 0}, .2f, 1, 0, 0);
            if(!Planar(world, h).IsValid())throw std::runtime_error("scaling planar joint creation");
            world.SetLinearVelocity(h, {.1f, .2f, 0});
            bodies.push_back(h);
        }
        Require(int(bodies.size())==count,"scaling fixture constructs intended count");
        std::vector<double> times;
        for (int i = 0; i < 100; ++i) {
            auto start = std::chrono::steady_clock::now();
            world.Step(dt);
            auto finish = std::chrono::steady_clock::now();
            if (i >= 10) times.push_back(std::chrono::duration<double, std::milli>(finish - start).count());
        }
        std::sort(times.begin(), times.end());
        std::printf("native_row_scaling count=%d median_ms=%.9g p95_ms=%.9g worst_ms=%.9g\n", count, times[45], times[85], times.back());
    }
}
}
int main(int argc, char** argv) {
    try {
        const std::string mode = argc > 1 ? argv[1] : "all";
        if (mode == "all" || mode == "impulse") Impulses();
        if (mode == "all" || mode == "ccd") WallAndCorner();
        if (mode == "all" || mode == "sleep") SupportAndSleep();
        if (mode == "all" || mode == "contacts") ContactAndForces();
        if (mode == "all" || mode == "pair") PairAndHinge();
        if (mode == "all" || mode == "mobility-save") MobilityPersistence();
        if (mode == "all" || mode == "scaling") Scaling();
        if (mode == "digest") Digest3D();
        if (mode == "legacy-hinge") LegacyHingeFailure();
        if ((mode.find("cold-")==0||mode.find("cold3d-")==0) && argc > 2) Cold(mode, argv[2],argc>3&&std::string(argv[3])=="--ablate-warm");
        std::printf("checks=%d failures=0\n", checks);
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL after %d checks: %s\n", checks, e.what());
        return 1;
    }
}
