// Independent mechanical oracles for rotation and elastic arrival timing.
// Energy is measured from analytic cuboid inertia, not the solver's budget.
#include "PhysicsWorld.h"
#include "RigidMotion.h"
#include "RigidMotionBounds.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {
constexpr float kDt = 1.f / 60;
constexpr glm::vec3 kHalf(.08f, .2f, .05f);
unsigned checks = 0, failures = 0;

void Check(bool value, const char* label) {
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", label); }
}

glm::dvec3 Inertia(glm::vec3 half, double mass) {
    return mass / 3 * glm::dvec3(half.y*half.y + half.z*half.z,
        half.x*half.x + half.z*half.z, half.x*half.x + half.y*half.y);
}

double Energy(PhysicsWorld& world, BodyHandle handle, glm::vec3 half = kHalf) {
    const glm::dvec3 v(world.GetLinearVelocity(handle));
    const auto q = glm::normalize(glm::dquat(world.GetTransform(handle).rotation));
    const auto omega = glm::conjugate(q) * glm::dvec3(world.GetAngularVelocity(handle));
    return .5 * glm::dot(v,v) + .5 * glm::dot(omega, Inertia(half,1) * omega);
}

glm::dquat Rate(glm::dquat q, glm::dvec3 momentum, glm::dmat3 inverse) {
    const auto rotation = glm::mat3_cast(glm::normalize(q));
    const auto omega = rotation * inverse * glm::transpose(rotation) * momentum;
    return .5 * glm::dquat(0, omega.x, omega.y, omega.z) * q;
}

// Independent, converged RK4 integration of q'=1/2 omega_world(q)*q.
// No midpoint generator or motion-ledger implementation is used by this oracle.
glm::dquat Reference(glm::dvec3 momentum, glm::dmat3 inverse, double duration, unsigned n) {
    glm::dquat q(1,0,0,0);
    const double h = duration / n;
    for (unsigned i=0; i<n; ++i) {
        const auto a=Rate(q,momentum,inverse), b=Rate(q+.5*h*a,momentum,inverse);
        const auto c=Rate(q+.5*h*b,momentum,inverse), d=Rate(q+h*c,momentum,inverse);
        q = glm::normalize(q + (h/6)*(a+2.*b+2.*c+d));
    }
    return q;
}

double Angle(glm::dquat a, glm::dquat b) {
    return 2*std::min(glm::length(a-b),glm::length(a+b));
}

void FreeRotation() {
    for (float speed : {1.f,5.f,20.f,100.f,1000.f}) {
        PhysicsWorld world; world.Init();
        const auto body=world.CreateDynamicBox({0,0,0},kHalf,1,0,1);
        const glm::dvec3 omega(speed,.7f*speed,.3f*speed), inertia=Inertia(kHalf,1);
        world.SetAngularVelocity(body,glm::vec3(omega));
        const auto momentum=inertia*omega;
        glm::dmat3 inverse(0);
        for (int k=0; k<3; ++k) inverse[k][k]=1/inertia[k];
        const double initial=Energy(world,body);
        double peakError=0, momentumError=0;
        std::size_t leaves=0;
        for (int step=0; step<120; ++step) {
            world.Step(kDt);
            peakError=std::max(peakError,std::abs(Energy(world,body)/initial-1));
            const auto q=glm::normalize(glm::dquat(world.GetTransform(body).rotation));
            const auto local=glm::conjugate(q)*glm::dvec3(world.GetAngularVelocity(body));
            momentumError=std::max(momentumError,glm::length(q*(inertia*local)-momentum)/glm::length(momentum));
            leaves+=world.GetBodyMotionSegments(body).size();
        }
        const auto n=unsigned(std::max(4000.f,1000*speed));
        const double duration=120*double(kDt);
        const auto reference=Reference(momentum,inverse,duration,n);
        const auto refined=Reference(momentum,inverse,duration,2*n);
        const double referenceError=Angle(reference,refined);
        const double error=Angle(glm::dquat(world.GetTransform(body).rotation),refined);
        std::printf("FREE speed=%g energy_error=%.9g momentum_error=%.9g angle_error=%.9g reference_error=%.9g leaves=%zu\n",
            speed,peakError,momentumError,error,referenceError,leaves);
        Check(referenceError<1e-6,"independent rotation reference converged");
        Check(peakError<1e-4 && momentumError<1e-4,"free asymmetric spin conserves energy and angular momentum");
        // Geometry accuracy is tested separately from conserved invariants:
        // <0.01 rad at 100 rad/s; very high spins retain a stated relative phase bound.
        Check(error<.001+1e-4*speed*duration,"orientation agrees with independent rotation reference");
    }
}

void ElasticEnvelope() {
    for (int frame : {0,1}) for (bool dynamicFirst : {false,true})
        for (float restitution : {0.f,.2f,.5f,1.f})
            for (float speed : {1.f,5.f,20.f,100.f})
                for (float spin : {0.f,5.f,20.f,100.f}) {
        PhysicsWorld world; world.Init();
        const auto q=glm::angleAxis(.79f*frame,glm::normalize(glm::vec3(1,2,-3)));
        BodyHandle body;
        const auto createBody=[&] {
            body=world.CreateDynamicBox({0,0,0},kHalf,1,0,restitution);
            world.ResetBody(body,{0,0,0},q);
        };
        if (dynamicFirst) createBody();
        for (float sign : {-1.f,1.f})
            world.CreateStaticBox(q*glm::vec3(sign*.6f,0,0),q,{.05f,1,1},0,restitution);
        if (!dynamicFirst) createBody();
        world.SetLinearVelocity(body,q*glm::vec3(speed,0,0));
        world.SetAngularVelocity(body,q*glm::vec3(0,0,spin));
        const double initial=Energy(world,body);
        double peak=initial;
        std::size_t events=0,holds=0;
        for (int step=0; step<120; ++step) {
            world.Step(kDt);
            peak=std::max(peak,Energy(world,body));
            events+=world.LastStepStats().impactEvents;
            holds+=world.LastStepStats().impactSafetyFallback;
            const auto p=glm::conjugate(q)*world.GetTransform(body).position;
            Check(std::abs(p.x)<.6f,"energy fixture independently retains collision containment");
        }
        std::printf("ENVELOPE frame=%d dynamic_first=%d restitution=%g speed=%g spin=%g initial=%.12g peak=%.12g final=%.12g events=%zu holds=%zu\n",
            frame,int(dynamicFirst),restitution,speed,spin,initial,peak,Energy(world,body),events,holds);
        Check(peak<=initial*1.0001,"unchanged original envelope energy bound");
        if (restitution==1 && speed==5 && spin==20) {
            Check(holds==0,"ordinary-speed energy correction does not rely on safety holds");
            Check(std::abs(Energy(world,body)/initial-1)<1e-4,"ordinary-speed elastic rebound retains energy");
        }
    }
}

void ArrivalTiming() {
    PhysicsWorld world; world.Init();
    world.CreateStaticBox({0,0,0},{.05f,1,1},0,1);
    const auto sphere=world.CreateDynamicSphere({-.4f,0,0},.05f,1,0,1);
    world.SetLinearVelocity(sphere,{5,0,0});
    world.SetAngularVelocity(sphere,{0,20,0});
    world.Step(.1f);
    const double arrival=(.4-.05-.05)/5;
    Check(std::abs(world.GetTransform(sphere).position.x-(-.1-5*(.1-arrival)))<2e-6,
        "elastic arrival advances the real remaining flight time");
    Check(std::abs(world.GetLinearVelocity(sphere).x+5)<1e-5,"elastic timing fixture retains rebound speed");
    bool found=false;
    for (const auto& leaf:world.GetBodyMotionSegments(sphere)) {
        if (leaf.linearVelocity.x<0) {
            Check(std::abs(leaf.begin-arrival)<2e-6,"independent analytic collision time");
            found=true;
            break;
        }
    }
    Check(found,"the observed ledger actually contains an elastic rebound");
}

void FreePairMomentum() {
    for (int frame:{0,1}) {
        PhysicsWorld world;world.Init();
        const auto q=glm::angleAxis(.79f*frame,glm::normalize(glm::vec3(1,2,-3)));
        const auto a=world.CreateShape(Shape::Box(kHalf),{q*glm::vec3(-.3f,.1f,0),q},true,1,0,1);
        const auto b=world.CreateShape(Shape::Box(kHalf),{q*glm::vec3(.3f,0,0),q},true,1,0,1);
        world.SetLinearVelocity(a,q*glm::vec3(20,0,0));world.SetLinearVelocity(b,q*glm::vec3(-5,0,0));
        world.SetAngularVelocity(a,q*glm::vec3(3,7,20));world.SetAngularVelocity(b,q*glm::vec3(-5,3,10));
        const auto momentum=[&] {
            glm::dvec3 linear(0),angular(0);
            for(auto h:{a,b}) {
                const auto t=world.GetTransform(h);
                const auto r=glm::normalize(glm::dquat(t.rotation));
                const glm::dvec3 v(world.GetLinearVelocity(h)),w(world.GetAngularVelocity(h));
                linear+=v;angular+=glm::cross(glm::dvec3(t.position),v)+r*(Inertia(kHalf,1)*(glm::conjugate(r)*w));
            }
            return std::pair<glm::dvec3,glm::dvec3>{linear,angular};
        };
        const auto before=momentum();const double incoming=Energy(world,a)+Energy(world,b);
        double peak=incoming,linearError=0,angularError=0;std::size_t impacts=0;
        for(int step=0;step<30;++step) {
            world.Step(kDt);const auto now=momentum();
            peak=std::max(peak,Energy(world,a)+Energy(world,b));
            linearError=std::max(linearError,glm::length(now.first-before.first));
            angularError=std::max(angularError,glm::length(now.second-before.second));
            impacts+=world.LastStepStats().impactEvents;
        }
        std::printf("PAIR frame=%d energy_ratio=%.9g linear_error=%.9g angular_error=%.9g impacts=%zu\n",
            frame,peak/incoming,linearError,angularError,impacts);
        Check(impacts>0,"free spinning-pair fixture actually impacts");
        Check(peak<=incoming*1.0001 && linearError<1e-4 && angularError<1e-4,
            "elastic rotating bodies independently conserve energy and both momenta");
    }
}

void BoundedContactContinuation() {
    for(bool blocker:{false,true}) {
        PhysicsWorld world;world.Init();
        for(float sign:{-1.f,1.f})world.CreateStaticBox({sign*.6f,0,0},{.05f,4,4},0,1);
        if(blocker)world.CreateStaticBox({0,0,.13f},{4,4,.01f},0,1);
        const auto body=world.CreateDynamicSphere({0,0,0},.05f,1,0,1);
        world.SetLinearVelocity(body,{100,0,1});world.Step(.2f);
        Check(world.LastStepStats().impactBodyCapHolds>0,"fallback fixture actually exhausts normal arrival work");
        if(!blocker)Check(std::abs(world.GetTransform(body).position.z-.2f)<2e-6,
            "a bounded normal hold preserves certified tangential travel");
        else for(const auto& leaf:world.GetBodyMotionSegments(body))
            Check(leaf.start.position.z<=.070002f && leaf.endPose.position.z<=.070002f,
                "tangential continuation stops at a real secondary obstacle");
        Check(glm::length(world.GetLinearVelocity(body))<=glm::length(glm::vec3(100,0,1))*1.0001f,
            "bounded geometry continuation never changes physical energy");
    }
}

void ReactionObservations() {
    PhysicsWorld world;world.Init();
    for(float sign:{-1.f,1.f})world.CreateStaticBox({sign*.6f,0,0},{.05f,1,1},0,1);
    const auto body=world.CreateDynamicBox({0,0,0},kHalf,1,0,1);
    world.SetLinearVelocity(body,{5,0,0});world.SetAngularVelocity(body,{0,0,20});
    double maximum=0;std::size_t observed=0;
    for(int step=0;step<120;++step) {
        const glm::dvec3 before(world.GetLinearVelocity(body));world.Step(kDt);
        std::vector<PhysicsWorld::BodyContactSample> contacts;
        Check(world.ReadBodyContactSample(body,contacts),"repeated response patches fit the unchanged contact query budget");
        glm::dvec3 reaction(0);
        for(const auto& contact:contacts) {reaction+=glm::dvec3(contact.normal)*double(contact.normalImpulse);++observed;}
        const auto change=glm::dvec3(world.GetLinearVelocity(body))-before;
        maximum=std::max(maximum,glm::length(change-reaction));
    }
    std::printf("OBSERVATION samples=%zu maximum_impulse_error=%.9g\n",observed,maximum);
    Check(observed>0 && maximum<1e-4,"coalesced observations retain actual complete physical reaction");
}

void RestitutionCutoff() {
    for(float speed:{.49f,.51f}) {
        PhysicsWorld world;world.Init();
        world.CreateStaticBox({0,-.5f,0},{2,.5f,2},0,1);
        const auto body=world.CreateDynamicSphere({0,.101f,0},.1f,1,0,1);
        world.SetLinearVelocity(body,{0,-speed,0});world.Step(.02f);
        const double expectedSpeed=speed>.5f ? speed : 0;
        const double expectedHeight=speed>.5f ? .1+speed*.02-.001 : .1;
        Check(std::abs(world.GetLinearVelocity(body).y-expectedSpeed)<2e-6,
            "existing restitution cutoff selects physical non-bounce versus bounce");
        Check(std::abs(world.GetTransform(body).position.y-expectedHeight)<2e-6,
            "effective restitution retains correct contact time and endpoint");
    }
}

void LateWake() {
    for(bool awakeControl:{false,true}) for(bool absentTarget:{false,true}) {
        PhysicsWorld world;world.Init();
        const auto a=world.CreateDynamicSphere({-1,0,0},.1f,1,0,1);
        const auto b=world.CreateDynamicSphere({0,0,0},.1f,.1f,0,1);
        const auto c=world.CreateDynamicSphere({3,0,0},.1f,.1f,0,1);
        for(int step=0;step<120;++step)world.Step(kDt);
        Check(world.IsSleeping(c),"late-arrival target starts genuinely asleep");
        if(awakeControl)world.SetSleepingEnabled(false);
        if(absentTarget)world.SetBodyEnabled(c,false);
        world.SetLinearVelocity(a,{200,0,0});world.Step(kDt);
        // Analytic one-dimensional elastic impacts. The light middle body
        // outruns the original shooter's reach and strikes the distant target.
        const double first=.8/200, outgoing=400/1.1;
        const double second=first+2.8/outgoing;
        const double expected=absentTarget?(double(kDt)-first)*outgoing:3+(double(kDt)-second)*outgoing;
        const auto moving=absentTarget?b:c;
        Check(std::abs(world.GetTransform(moving).position.x-expected)<3e-6,
            "late wake consumes exactly the remaining physical flight time");
        Check(std::abs(world.GetLinearVelocity(moving).x-outgoing)<1e-3,
            "late wake preserves the analytic transferred momentum");
        if(!absentTarget) {
            const auto path=world.GetBodyMotionSegments(c);
            Check(!path.empty() && path.front().begin==0 && path.back().end==double(kDt),
                "late-arrival ledger covers the entire fixed step");
            for(const auto& leaf:path)
                Check(PhysicsWorld::EvaluateBodyMotionSegment(leaf,leaf.end).position==leaf.endPose.position,
                    "late-arrival motion view matches actual travelled geometry");
        }
    }
}

void MotionView() {
    PhysicsWorld world; world.Init();
    const auto body=world.CreateDynamicBox({100,100,100},kHalf,1,0,1);
    world.SetLinearVelocity(body,{1,2,3});
    world.SetAngularVelocity(body,{10,7,3});
    world.Step(kDt);
    auto leaves=world.GetBodyMotionSegments(body);
    Check(!leaves.empty(),"public motion view exists");
    for (const auto& leaf:leaves) {
        const auto a=PhysicsWorld::EvaluateBodyMotionSegment(leaf,leaf.begin);
        const auto b=PhysicsWorld::EvaluateBodyMotionSegment(leaf,leaf.end);
        Check(a.position==leaf.start.position && Angle(glm::dquat(a.rotation),glm::dquat(leaf.start.rotation))<1e-6,
            "public anchored motion start is consistent");
        Check(b.position==leaf.endPose.position && Angle(glm::dquat(b.rotation),glm::dquat(leaf.endPose.rotation))<1e-6,
            "public anchored motion end is consistent");
    }
    for(unsigned intervals:{2u,3u,8u,32u}) {
        PhysicsWorld coupled;coupled.Init();
        const auto moving=coupled.CreateDynamicBox({100,100,100},kHalf,1,0,1);
        coupled.SetLinearVelocity(moving,{1,2,3});coupled.SetAngularVelocity(moving,{10,7,3});
        coupled.StepCoupled(kDt,intervals,{},{});
        const auto path=coupled.GetBodyMotionSegments(moving);
        for(const auto& leaf:path) {
            const auto first=PhysicsWorld::EvaluateBodyMotionSegment(leaf,leaf.begin);
            const auto last=PhysicsWorld::EvaluateBodyMotionSegment(leaf,leaf.end);
            Check(first.position==leaf.start.position && last.position==leaf.endPose.position,
                "coupled interval concatenation preserves the velocity-episode clock");
        }
    }
    RigidBody spin;spin.inverseMass=1;spin.inverseInertiaLocal=SolidBoxInverseInertia(1,kHalf);
    spin.angularVelocity={0,0,100};RigidMotion motion;motion.Begin(42,spin,kDt,true);
    Check(motion.Segments().size()==1 && motion.Segments().front().exactRotation,
        "principal-axis trajectory remains analytic");
    const auto beforeShift=motion.EvaluateAfterEvents(kDt);
    const glm::dvec3 shift(3,-2,1);motion.ShiftOrigin(shift);
    const auto afterShift=motion.EvaluateAfterEvents(kDt);
    Check(afterShift.position==glm::vec3(glm::dvec3(beforeShift.position)-shift) &&
        afterShift.orientation==beforeShift.orientation && afterShift.angularVelocity==beforeShift.angularVelocity,
        "origin shifting preserves anchored trajectory geometry and physical spin");
    RigidBody asymmetric=spin;asymmetric.angularVelocity={10,7,3};
    RigidMotion zero;zero.Begin(7,asymmetric,0,true);
    Check(zero.EvaluateAfterEvents(0).orientation==asymmetric.orientation &&
        zero.EvaluateAfterEvents(0).angularVelocity==asymmetric.angularVelocity,
        "zero-duration asymmetric rotation preserves its physical anchor");
    Check(!rigid_motion_bounds::TryDynamicRotationInterval(motion.Segments().front(),0,kDt),
        "Cayley-only interval proof cannot reject an exponential trajectory");
}
}

int main() {
    try { FreeRotation(); ElasticEnvelope(); ArrivalTiming(); FreePairMomentum(); BoundedContactContinuation(); ReactionObservations(); RestitutionCutoff(); LateWake(); MotionView(); }
    catch (const std::exception& error) { ++failures; std::printf("EXCEPTION %s\n",error.what()); }
    std::printf("MK2 rotation: %u checks %u failures\n",checks,failures);
    return failures?1:0;
}
