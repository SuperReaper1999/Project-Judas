// FTFT4 stabilization: independent mechanics checks through ordinary PhysicsWorld.
// Existing baseline probes remain unchanged; this adds the authorized pragmatic
// isolated-bounce / inelastic-coupled-event and motion-ledger acceptance cases.
#include "PhysicsWorld.h"
#include "RigidBody.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
int checks=0, failures=0, cases=0;
// MK1 resolves coupled impacts by its event policy (inelastic capture of a
// coupled event, a 16-event cap); MK2 resolves each impact at its arrival
// instant by sequential propagation. Where the expectations differ the MK2
// branch states the exact physical result instead.
const bool mk2=PhysicsWorld::Mk2Selected();
// An undisturbed free spin's endpoint. MK1's integrator is the first-order
// quaternion update (bitwise oracle); MK2 integrates the exact rotation.
bool SameFreeRotation(const glm::quat& actual,const RigidBody& start,const RigidBody& mk1Oracle,float dt) {
    if(!mk2)return actual==mk1Oracle.orientation;
    const float speed=glm::length(start.angularVelocity);
    const glm::quat exact=speed>0?glm::normalize(glm::angleAxis(speed*dt,start.angularVelocity/speed)*start.orientation):start.orientation;
    return std::abs(glm::dot(actual,exact))>=1-1e-6f;
}
void Check(bool ok,const std::string& label) {
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
void Vec(const glm::dvec3& v) { std::cout << '[' << v.x << ',' << v.y << ',' << v.z << ']'; }
bool Near(double a,double b,double tolerance) { return std::isfinite(a)&&std::abs(a-b)<=tolerance; }
bool Near(const glm::dvec3& a,const glm::dvec3& b,double tolerance) {
    return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z)&&glm::length(a-b)<=tolerance;
}
struct Budget { double energy=0;glm::dvec3 momentum{0},angular{0}; };
Budget Spheres(PhysicsWorld& w,const std::vector<BodyHandle>& bodies,const std::vector<double>& masses) {
    Budget result;
    for (std::size_t i=0;i<bodies.size();++i) {
        const glm::dvec3 p=w.GetTransform(bodies[i]).position,v=w.GetLinearVelocity(bodies[i]),omega=w.GetAngularVelocity(bodies[i]);
        // Independent uniform-sphere inertia: I=2*m*r*r/5, r=0.5.
        const double inertia=.1*masses[i];
        result.energy+=.5*masses[i]*glm::dot(v,v)+.5*inertia*glm::dot(omega,omega);
        result.momentum+=masses[i]*v;
        result.angular+=glm::cross(p,masses[i]*v)+inertia*omega;
    }
    return result;
}
template<class T,class=void>struct Events { static void Print(const T&) {} };
template<class T>struct Events<T,std::void_t<decltype(std::declval<T>().impactSafetyFallback),decltype(std::declval<T>().impactEventCapFallback)>> {
    static void Print(const T& s) {
        std::cout << ",\"impactEvents\":" << s.impactEvents
                  << ",\"impactSafetyFallback\":" << s.impactSafetyFallback
                  << ",\"impactEventCapFallback\":" << s.impactEventCapFallback
                  << ",\"impactUncertifiedAdvances\":" << s.impactUncertifiedAdvances
                  << ",\"impactSamplingFallbacks\":" << s.impactSamplingFallbacks
                  << ",\"impactSamplingTests\":" << s.impactSamplingTests
                  << ",\"impactSamplingResolutionCaps\":" << s.impactSamplingResolutionCaps
                  << ",\"impactSamplingMaxInterval\":" << s.impactSamplingMaxInterval;
    }
};
PhysicsWorld::StepStats caseEvents{},totalEvents{};
void Step(PhysicsWorld& w,float dt) {
    w.Step(dt);const auto& actual=w.LastStepStats();
    for(auto* sum:{&caseEvents,&totalEvents}) {
        sum->impactEvents+=actual.impactEvents;
        sum->impactSafetyFallback+=actual.impactSafetyFallback;
        sum->impactEventCapFallback+=actual.impactEventCapFallback;
        sum->impactUncertifiedAdvances+=actual.impactUncertifiedAdvances;
        sum->impactSamplingFallbacks+=actual.impactSamplingFallbacks;
        sum->impactSamplingTests+=actual.impactSamplingTests;
        sum->impactSamplingResolutionCaps+=actual.impactSamplingResolutionCaps;
        sum->impactSamplingMaxInterval=std::max(sum->impactSamplingMaxInterval,actual.impactSamplingMaxInterval);
    }
}
void EndCase(const char* name,int before,const PhysicsWorld& w) {
    ++cases;std::cout << "{\"kind\":\"case\",\"name\":\"" << name << "\",\"case\":" << cases
                     << ",\"pass\":" << (before==failures) << ",\"contacts\":" << w.LastStepContactCount();
    Events<PhysicsWorld::StepStats>::Print(caseEvents);caseEvents=PhysicsWorld::StepStats{};std::cout << "}\n";
}
void Isolated() {
    const float dt=1.f/60;
    for (int axis=0;axis<3;++axis) for (float sign:{-1.f,1.f}) for (bool translated:{false,true}) for (float e:{0.f,.5f}) {
        const int before=failures;
        PhysicsWorld w;w.Init();glm::vec3 n(0);n[axis]=sign;
        const glm::vec3 origin=translated?glm::vec3(137,-73,41):glm::vec3(0);
        glm::vec3 half(10);half[axis]=.5;
        w.CreateStaticBox(origin-.5f*n,half,0,0);
        const auto body=w.CreateDynamicSphere(origin+.501f*n,.5f,1,0,e);w.SetLinearVelocity(body,-n);
        const double initial=glm::dot(glm::dvec3(w.GetTransform(body).position)-glm::dvec3(origin),glm::dvec3(n))-.5;
        const double tolerance=2e-6+8*std::numeric_limits<float>::epsilon()*std::max({std::abs(double(origin.x)),std::abs(double(origin.y)),std::abs(double(origin.z))});
        for(int step=1;step<=(e==0?120:4);++step) {
            Step(w,dt);
            const double gap=glm::dot(glm::dvec3(w.GetTransform(body).position)-glm::dvec3(origin),glm::dvec3(n))-.5;
            const double expected=double(e)*(double(step)*dt-initial);
            Check(Near(gap,expected,tolerance),"isolated analytical endpoint");
            Check(Near(glm::dvec3(w.GetLinearVelocity(body)),double(e)*glm::dvec3(n),2e-6),"isolated outgoing velocity");
            if(step==1||step==120)std::cout << "{\"kind\":\"isolated\",\"axis\":" << axis << ",\"sign\":" << sign << ",\"translated\":" << translated
                << ",\"e\":" << e << ",\"step\":" << step << ",\"initial_gap\":" << initial << ",\"expected_gap\":" << expected << ",\"actual_gap\":" << gap << ",\"velocity\":" << glm::dot(w.GetLinearVelocity(body),n) << ",\"position_tolerance\":" << tolerance << "}\n";
        }
        EndCase("isolated",before,w);
    }
}
void EnergyWitness() {
    std::array<int,3> order{{0,1,2}};
    do { for(const glm::vec3 axis:{glm::vec3(1,0,0),glm::vec3(0,1,0),glm::vec3(0,0,-1)})for(bool boosted:{false,true}) {
        const int before=failures;PhysicsWorld w;w.Init();std::vector<BodyHandle> bodies(3);
        const std::vector<double> mass{10,.125,10};const float speed[]={1,-3,4};const glm::vec3 boost=boosted?glm::vec3(2,-1,3):glm::vec3(0);
        for(int i:order) {bodies[i]=w.CreateDynamicSphere(float(i)*axis,.5f,float(mass[i]),0,1);w.SetLinearVelocity(bodies[i],speed[i]*axis+boost);}
        const auto a=Spheres(w,bodies,mass);Step(w,1.f/60);const auto b=Spheres(w,bodies,mass);
        Check(b.energy-a.energy<=1e-4,"original energy witness <= 1e-4 J growth");
        Check(glm::length(b.momentum-a.momentum)<=1e-4,"original energy witness momentum <= 1e-4");
        std::cout << "{\"kind\":\"energy\",\"order\":[" << order[0] << ',' << order[1] << ',' << order[2] << "],\"axis\":";Vec(axis);
        std::cout << ",\"boost\":";Vec(boost);std::cout << ",\"before\":" << a.energy << ",\"after\":" << b.energy << ",\"gain\":" << b.energy-a.energy << ",\"momentum_residual\":" << glm::length(b.momentum-a.momentum) << "}\n";
        EndCase("energy",before,w);
    }}while(std::next_permutation(order.begin(),order.end()));
}
void Chain() {
    for(int axis=0;axis<3;++axis)for(float sign:{-1.f,1.f}) {
        const int before=failures;PhysicsWorld w;w.Init();glm::vec3 n(0);n[axis]=sign;std::vector<BodyHandle>b;
        for(float x:{0.f,1.8f,3.6f})b.push_back(w.CreateDynamicSphere(x*n,.5f,1,0,1));
        w.SetLinearVelocity(b[0],10.f*n);const auto initial=Spheres(w,b,{1,1,1});Step(w,.2f);
        // Separate binary elastic collisions at t=.08, .16; endpoint is .8,2.6,4.
        const double expectedX[]={.8,2.6,4},expectedV[]={0,0,10};
        for(int i=0;i<3;++i){const double x=glm::dot(w.GetTransform(b[i]).position,n),v=glm::dot(w.GetLinearVelocity(b[i]),n);
            Check(Near(x,expectedX[i],1e-5),"chain consumes remaining physical time");Check(Near(v,expectedV[i],1e-5),"chain induced collision velocity");
            std::cout << "{\"kind\":\"chain\",\"axis\":" << axis << ",\"sign\":" << sign << ",\"body\":" << i << ",\"x\":" << x << ",\"v\":" << v << "}\n";}
        const auto final=Spheres(w,b,{1,1,1});Check(Near(final.energy,initial.energy,1e-4),"isolated chain energy");Check(Near(final.momentum,initial.momentum,1e-4),"chain momentum");EndCase("chain",before,w);
    }
    { // A t=0 elastic transfer changes reach before the first pose advancement.
        const int before=failures;PhysicsWorld w;w.Init();std::vector<BodyHandle>b;
        for(float x:{0.f,1.f,2.5f})b.push_back(w.CreateDynamicSphere({x,0,0},.5f,1,0,1));
        w.SetLinearVelocity(b[0],{10,0,0});const auto initial=Spheres(w,b,{1,1,1});Step(w,.2f);
        // A-B at t=0, then B-C at t=.05: a stale pre-impact reach misses B-C.
        const double expectedX[]={0,1.5,4},expectedV[]={0,0,10};
        for(int i=0;i<3;++i){const double x=w.GetTransform(b[i]).position.x,v=w.GetLinearVelocity(b[i]).x;
            Check(Near(x,expectedX[i],1e-5),"t=0 chain refreshes reach and consumes time");
            Check(Near(v,expectedV[i],1e-5),"t=0 chain induced collision velocity");
            std::cout << "{\"kind\":\"touching_chain\",\"body\":" << i << ",\"x\":" << x << ",\"v\":" << v << "}\n";}
        const auto final=Spheres(w,b,{1,1,1});Check(Near(final.energy,initial.energy,1e-4),"t=0 chain energy");Check(Near(final.momentum,initial.momentum,1e-4),"t=0 chain momentum");EndCase("touching_chain",before,w);
    }
}
void ThinObstacle() {
    for(float transverse:{0.f,4096.f,8192.f})for(float restitution:{0.f,.5f}) {
        const int before=failures;PhysicsWorld w;w.Init();
        constexpr float radius=1.f/8192,halfWidth=1.f/8192,wallX=.001517f,dt=1.f/60;
        const glm::vec3 origin(0,transverse,-.5f*transverse);
        const auto wall=w.CreateStaticBox(origin+glm::vec3(wallX,0,0),{halfWidth,1,1},0,0);
        const auto sphere=w.CreateDynamicSphere(origin,radius,1,0,restitution);w.SetLinearVelocity(sphere,{1,0,0});
        // Actual represented wall coordinate and exact binary dimensions define
        // the independent one-dimensional oracle. Perpendicular translation must
        // not enlarge a TOI increment enough to jump across the thin obstacle.
        const double contact=double(w.GetTransform(wall).position.x)-double(halfWidth)-double(radius);
        const double expected=contact-double(restitution)*(double(dt)-contact);
        Step(w,dt);const auto final=w.GetTransform(sphere).position;const auto v=w.GetLinearVelocity(sphere);
        Check(Near(final.x,expected,2e-7),"thin obstacle timing unaffected by perpendicular coordinate");
        Check(Near(v,glm::dvec3(-restitution,0,0),2e-6),"thin obstacle actual impact velocity");
        Check(final.y==origin.y&&final.z==origin.z,"thin obstacle leaves perpendicular coordinates unchanged");
        std::cout << "{\"kind\":\"thin_obstacle\",\"perpendicular_offset\":" << transverse << ",\"e\":" << restitution
                  << ",\"contact_x\":" << contact << ",\"expected_x\":" << expected << ",\"actual_x\":" << final.x << ",\"velocity_x\":" << v.x << "}\n";
        EndCase("thin_obstacle",before,w);
    }
}
void GrazingRegression() {
    const int before=failures;PhysicsWorld w;w.Init();
    // Frozen represented states from the correctness4 BroadphaseTests failure:
    // docs/evidence/stabilization/ftft4/correctness4/judas_broadphase_tests.log.
    // These are the logged poses at the stalled query, not invented t=0 poses.
    const glm::vec3 half[]={
        {.89456856250762939f,.83960592746734619f,.75462836027145386f},
        {.59486919641494751f,.24062791466712952f,.67663049697875977f}};
    const glm::vec3 position[]={
        {-.35240226984024048f,.66536730527877808f,-4.5890746116638184f},
        {-1.16368567943573f,.93406188488006592f,-2.9670522212982178f}};
    const glm::quat orientation[]={
        {.24314773082733154f,-.046302787959575653f,-.65676581859588623f,-.71231591701507568f},
        {-.36610302329063416f,.52886605262756348f,-.48647767305374146f,-.59127724170684814f}};
    const glm::vec3 velocity[]={
        {.35676309466362f,-4.5118989944458008f,-2.1612741947174072f},
        {.71321898698806763f,-3.4032649993896484f,-.62842094898223877f}};
    const glm::vec3 omega[]={
        {-2.8653979301452637f,.10036845505237579f,.65744036436080933f},
        {-1.2906868457794189f,-1.538135290145874f,-1.0442986488342285f}};
    std::array<BodyHandle,2> bodies;
    glm::dvec3 beforeMomentum(0);
    for(int i=0;i<2;++i){bodies[i]=w.CreateDynamicBox(position[i],half[i],3,.5f,.1f);
        w.ResetBody(bodies[i],position[i],orientation[i]);w.SetLinearVelocity(bodies[i],velocity[i]);w.SetAngularVelocity(bodies[i],omega[i]);
        beforeMomentum+=3.*glm::dvec3(velocity[i]);}
    constexpr float remaining=float(.01666666753590107-.003476685833631653);
    Step(w,remaining);glm::dvec3 afterMomentum(0);
    for(int i=0;i<2;++i){const auto pose=w.GetTransform(bodies[i]);const auto v=w.GetLinearVelocity(bodies[i]),spin=w.GetAngularVelocity(bodies[i]);afterMomentum+=3.*glm::dvec3(v);
        Check(std::isfinite(glm::length(pose.position))&&std::isfinite(glm::length(v))&&std::isfinite(glm::length(spin)),"grazing pair returns finite state");
        Check(std::abs(glm::dot(pose.rotation,pose.rotation)-1.f)<2e-6f,"grazing pair leaves normalized orientation");}
    Check(Near(afterMomentum,beforeMomentum,1e-4),"grazing pair conserves closed-system linear momentum");
    std::cout << "{\"kind\":\"grazing_regression\",\"remaining_dt\":" << remaining << ",\"momentum_residual\":" << glm::length(afterMomentum-beforeMomentum)
              << ",\"impact_queries\":" << w.LastStepStats().impactQueries << ",\"impact_search_iterations\":" << w.LastStepStats().impactSearchIterations
              << ",\"impact_peak_iterations\":" << w.LastStepStats().impactPeakIterations << ",\"impact_search_limit\":" << w.LastStepStats().impactSearchLimit << "}\n";
    EndCase("grazing_regression",before,w);
}
void FastThin() {
    const int before=failures;PhysicsWorld w;w.Init();
    constexpr float radius=1.f/8192,halfWidth=1.f/8192,wallX=.001517f,dt=1.f/60,speed=1000,restitution=.5f;
    w.CreateStaticBox({wallX,8192,-4096},{halfWidth,1,1},0,0);
    const auto body=w.CreateDynamicSphere({0,8192,-4096},radius,1,0,restitution);w.SetLinearVelocity(body,{speed,0,0});
    const auto unrelated=w.CreateDynamicSphere({100,100,100},.5f,1,0,0);w.SetLinearVelocity(unrelated,{1,2,3});w.SetAngularVelocity(unrelated,{0,0,1000});
    RigidBody spinReference;spinReference.inverseMass=1;spinReference.position={100,100,100};spinReference.linearVelocity={1,2,3};spinReference.angularVelocity={0,0,1000};const RigidBody spinStart=spinReference;IntegrateRigidBodyPosition(spinReference,dt);
    const double contact=double(wallX)-double(halfWidth)-double(radius);
    const double expected=contact-double(restitution)*double(speed)*(double(dt)-contact/double(speed));
    // Eight binary32 relative roundoff units bound the represented endpoint
    // comparison scale; the unchanged low-speed thin tests keep their 2e-7 gate.
    const double tolerance=8*std::numeric_limits<float>::epsilon()*std::max(1.,std::abs(expected));
    Step(w,dt);const auto pose=w.GetTransform(body);const auto v=w.GetLinearVelocity(body);
    Check(Near(pose.position.x,expected,tolerance),"fast body detects a thin obstacle before crossing it");
    Check(Near(v,glm::dvec3(-500,0,0),1e-4),"fast isolated impact restitution");
    Check(w.GetTransform(unrelated).position==spinReference.position,"fast unrelated impact preserves translation anchor");
    Check(SameFreeRotation(w.GetTransform(unrelated).rotation,spinStart,spinReference,dt),"fast unrelated impact preserves exact angular endpoint");
    std::cout << "{\"kind\":\"fast_thin\",\"speed\":" << speed << ",\"expected_x\":" << expected << ",\"actual_x\":" << pose.position.x << ",\"position_tolerance\":" << tolerance << "}\n";
    EndCase("fast_thin",before,w);
}
void Coupled() {
    for(bool mixed:{false,true}) {
        const int before=failures;PhysicsWorld w;w.Init();std::vector<BodyHandle>b;
        for(int i=0;i<3;++i)b.push_back(w.CreateDynamicSphere({1.25f*float(i-1),0,0},.5f,1,0,(mixed&&i==2)?.5f:1.f));
        w.SetLinearVelocity(b[0],{3,0,0});w.SetLinearVelocity(b[2],{-3,0,0});const auto a=Spheres(w,b,{1,1,1});Step(w,.2f);const auto z=Spheres(w,b,{1,1,1});
        Check(z.energy<=a.energy+1e-4,"simultaneous event energy");Check(Near(z.momentum,a.momentum,1e-4),"simultaneous event momentum");
        if(mk2){ // Elastic (pair e = max = 1) arrivals at t=1/12 rebound for the rest of the step.
            const double rebound=3*(double(.2f)-.25/3);const double ex[]={-1-rebound,0,1+rebound},ev[]={-3,0,3};
            for(int i=0;i<3;++i){Check(Near(w.GetLinearVelocity(b[i]),glm::dvec3(ev[i],0,0),2e-4),"simultaneous elastic arrivals rebound symmetrically");Check(Near(double(w.GetTransform(b[i]).position.x),ex[i],2e-5),"simultaneous rebound endpoint");}
        } else
        for(int i=0;i<3;++i){Check(Near(w.GetLinearVelocity(b[i]),glm::dvec3(0),2e-4),"simultaneous event inelastic capture");Check(Near(double(w.GetTransform(b[i]).position.x),i-1,2e-5),"simultaneous event contact endpoint");}
        std::cout << "{\"kind\":\"simultaneous\",\"mixed_restitution\":" << mixed << ",\"before_energy\":" << a.energy << ",\"after_energy\":" << z.energy << ",\"momentum_residual\":" << glm::length(z.momentum-a.momentum) << "}\n";EndCase("simultaneous",before,w);
    }
    { // Incoming isolated body strikes two bodies already touching: capture island.
        const int before=failures;PhysicsWorld w;w.Init();std::vector<BodyHandle>b;
        for(float x:{0.f,1.5f,2.5f})b.push_back(w.CreateDynamicSphere({x,0,0},.5f,1,0,1));
        w.SetLinearVelocity(b[0],{6,0,0});const auto a=Spheres(w,b,{1,1,1});Step(w,.2f);const auto z=Spheres(w,b,{1,1,1});
        const double t=(.5/6),advance=2*(double(.2f)-t);
        if(mk2){ // Newton's cradle: the struck ball stops at contact, the far ball leaves at 6.
            const double ex[]={.5,1.5,2.5+6*(double(.2f)-t)},ev[]={0,0,6};
            for(int i=0;i<3;++i){Check(Near(w.GetLinearVelocity(b[i]),glm::dvec3(ev[i],0,0),2e-4),"impact into touching island propagates to the far ball");Check(Near(double(w.GetTransform(b[i]).position.x),ex[i],4e-5),"propagated island consumes time");}
        } else
        for(int i=0;i<3;++i){Check(Near(w.GetLinearVelocity(b[i]),glm::dvec3(2,0,0),2e-4),"impact into touching island captures all");Check(Near(double(w.GetTransform(b[i]).position.x),.5+i+advance,4e-5),"captured island consumes time");}
        Check(z.energy<=a.energy+1e-4,"touching island dissipates");Check(Near(z.momentum,a.momentum,1e-4),"touching island momentum");EndCase("persistent_island",before,w);
    }
}
void PersistentGapImpact() {
    const int before=failures;PhysicsWorld w;w.Init();
    w.CreateStaticBox({0,-.5f,0},{10,.5f,10},0,0);
    const auto b=w.CreateDynamicSphere({0,.5f,0},.5f,1,0,1);
    w.ApplyLinearAcceleration(b,{0,-9.81f,0},1.f/60);Step(w,1.f/60);
    w.SetLinearVelocity(b,{0,.1f,0});Step(w,.01f);
    const float initialHeight=w.GetTransform(b).position.y;
    Check(initialHeight>.5f,"persistent-gap fixture obtains a natural positive gap");
    const auto a=w.CreateDynamicSphere({-1.5f,initialHeight,0},.5f,1,0,1);w.SetLinearVelocity(a,{10,.1f,0});
    // B keeps the cache from ordinary support/lift steps. A-B reaches contact
    // after .05 s while B's support gap has grown by another .005 m.
    Step(w,.1f);
    if(mk2){ // e = 1 binary exchange at t=.05: A stops at contact, B leaves at 10.
        Check(Near(w.GetLinearVelocity(a),glm::dvec3(0,.1,0),2e-5)&&Near(w.GetLinearVelocity(b),glm::dvec3(10,.1,0),2e-5),"cached support incident to impact exchanges elastically");
        Check(Near(w.GetTransform(a).position.x,-1,2e-5)&&Near(w.GetTransform(b).position.x,.5,2e-5),"persistent-gap impact consumes remaining horizontal travel");
    } else {
    for(auto body:{a,b})Check(Near(w.GetLinearVelocity(body),glm::dvec3(5,.1,0),2e-5),"cached support incident to impact selects inelastic capture");
    Check(Near(w.GetTransform(a).position.x,-.75,2e-5)&&Near(w.GetTransform(b).position.x,.25,2e-5),"persistent-gap impact consumes remaining horizontal travel");
    }
    for(auto body:{a,b})Check(Near(w.GetTransform(body).position.y,double(initialHeight)+double(.1f)*double(.1f),2e-6),"cached positive gap receives no premature floor impulse");
    const float xa=w.GetTransform(a).position.x,xb=w.GetTransform(b).position.x;
    const double pairGap=double(xb)-double(xa)-1;
    Check(pairGap>0,"represented post-impact drift leaves separated sphere surfaces");
    Check(w.FindCollidingPairs().empty(),"post-impact bodies have no actual contact before the floor event");
    const double heightBefore=w.GetTransform(a).position.y;
    // Binary32 drift leaves a genuine positive A-B gap in this fixture. The
    // subsequent floor impacts are isolated and retain authored e=1, rather
    // than belonging to one touching island. Their independent event oracle is
    // tau=(height-.5)/1, final y=.5+1*(dt-tau), outgoing vy=1.
    const double expectedHeight=.5+(double(.02f)-(heightBefore-.5));
    for(auto body:{a,b})w.SetLinearVelocity(body,{5,-1,0});
    Step(w,.02f);
    for(auto body:{a,b}) {
        Check(Near(w.GetTransform(body).position.y,expectedHeight,2e-6),"post-impact isolated floor event uses actual impact time");
        Check(Near(w.GetLinearVelocity(body),glm::dvec3(5,1,0),2e-5),"post-impact isolated floor event retains authored restitution");
    }
    Check(Near(w.GetTransform(a).position.x,double(xa)+5*double(.02f),2e-5)&&Near(w.GetTransform(b).position.x,double(xb)+5*double(.02f),2e-5),"subsequent support collision consumes full tangential time");
    std::cout << "{\"kind\":\"persistent_gap_impact\",\"lifted_height\":" << initialHeight
              << ",\"pair_gap_before_fall\":" << pairGap << ",\"height_before_fall\":" << heightBefore << ",\"expected_final_height\":" << expectedHeight
              << ",\"expected_final_velocity_y\":1,\"final_a\":";Vec(w.GetTransform(a).position);
    std::cout << ",\"final_b\":";Vec(w.GetTransform(b).position);std::cout << "}\n";
    EndCase("persistent_gap_impact",before,w);
}
void LowSpeedAndFriction() {
    {const int before=failures;PhysicsWorld w;w.Init();w.CreateStaticBox({0,-.5f,0},{10,.5f,10},0,1);
        auto b=w.CreateDynamicSphere({0,.501f,0},.5f,1,0,1);w.SetLinearVelocity(b,{0,-.1f,0});Step(w,.05f);
        Check(Near(double(w.GetTransform(b).position.y),.5,2e-6),"low-speed capture reaches surface");Check(Near(w.GetLinearVelocity(b),glm::dvec3(0),2e-6),"existing restitution threshold suppresses micro bounce");EndCase("low_speed",before,w);}
    {const int before=failures;PhysicsWorld w;w.Init();w.CreateStaticBox({0,-.5f,0},{10,.5f,10},.5f,0);
        auto b=w.CreateDynamicSphere({0,.6f,0},.5f,1,.5f,.5f);w.SetLinearVelocity(b,{1,-2,0});const auto a=Spheres(w,{b},{1});Step(w,.1f);const auto z=Spheres(w,{b},{1});
        Check(Near(double(w.GetLinearVelocity(b).y),1,2e-5),"isolated friction preserves normal restitution");Check(w.GetLinearVelocity(b).x<1&&w.GetLinearVelocity(b).x>=0,"Coulomb reduces tangential speed");Check(z.energy<=a.energy+1e-4,"isolated impact friction does not create energy");EndCase("isolated_friction",before,w);}
}
void EventCap() {
    const int before=failures;PhysicsWorld w;w.Init();
    constexpr float radius=.05f,wallHalfWidth=.05f,wallX=.6f,dt=.2f;
    w.CreateStaticBox({wallX,0,0},{wallHalfWidth,4,4},0,1);
    w.CreateStaticBox({-wallX,0,0},{wallHalfWidth,4,4},0,1);
    const auto b=w.CreateDynamicSphere({0,0,0},radius,1,0,1);w.SetLinearVelocity(b,{100,0,1});
    Step(w,dt);const auto p=w.GetTransform(b).position,v=w.GetLinearVelocity(b);
    // The seventeenth impact is the positive wall. Inelastic capture removes
    // normal speed there; tangential travel must consume the full remaining dt.
    const double expectedX=double(wallX)-double(wallHalfWidth)-double(radius);
    if(mk2){ // ~18 bounces in one 0.2 s step exceed MK2's per-step bound (a 60 Hz
        // rattle is exact; judas_mk2_physics_tests). What must still hold here:
        Check(std::abs(p.x)<=expectedX+2e-6,"bounded rattle never leaves the walls");
        Check(std::abs(v.x)<=100+1e-3&&Near(v.z,1,2e-6),"bounded rattle creates no energy and keeps tangential speed");
        Check(Near(p.z,double(dt),2e-6),"event cap consumes remaining tangential travel time");
    } else {
    Check(w.LastStepStats().impactEventCapFallback>0,"event-cap fallback is observable");
    Check(Near(p.x,expectedX,2e-6),"event cap captures at contact rather than in midair");
    Check(Near(p.z,double(dt),2e-6),"event cap consumes remaining tangential travel time");
    Check(Near(v,glm::dvec3(0,0,1),2e-6),"event cap is inelastic without tangential damping");
    }
    std::cout << "{\"kind\":\"event_cap\",\"position\":";Vec(p);
    std::cout << ",\"velocity\":";Vec(v);Events<PhysicsWorld::StepStats>::Print(w.LastStepStats());std::cout << "}\n";
    EndCase("event_cap",before,w);
}
void Ledger() {
    {const int before=failures;PhysicsWorld w;w.Init();w.CreatePlayerShape(.05f,.05f);
        w.CreateStaticBox({-.5f,0,0},{.5f,4,4},0,0);
        const auto b=w.CreateDynamicSphere({.45f,0,0},.5f,1,0,0);const float initial=w.GetTransform(b).position.x;
        // MK2 recovers an overlap gently (stable stacks), never 2 cm in a step;
        // the ledger property is exercised with real departure speed instead.
        if(mk2)w.SetLinearVelocity(b,{3,0,0});
        Step(w,1.f/60);const glm::vec3 current=w.GetTransform(b).position;
        Check(current.x>initial+.02f,"endpoint correction fixture actually corrects penetration");
        const glm::vec3 from=current+glm::vec3(.65f,0,0),displacement(-.2f,0,0);
        const auto actual=w.SweepPlayerShape(from,glm::quat(1,0,0,0),displacement,false);
        const auto end=w.SweepPlayerShape(from,glm::quat(1,0,0,0),displacement,true,1,1);
        const auto start=w.SweepPlayerShape(from,glm::quat(1,0,0,0),displacement,true,0,0);
        Check(actual.hit&&end.hit&&actual.hitBody.id==b.id&&end.hitBody.id==b.id,"corrected final-pose queries find the body");
        Check(Near(actual.distance,.1,2e-5)&&Near(end.distance,.1,2e-5),"ledger final endpoint matches independent corrected sphere surface");
        Check(start.hit&&start.hitBody.id==b.id&&start.distance>end.distance+.02f,"start-of-step query preserves uncorrected starting pose");
        std::cout << "{\"kind\":\"corrected_endpoint\",\"initial_x\":" << initial << ",\"current_x\":" << current.x
                  << ",\"current_sweep\":" << actual.distance << ",\"end_sweep\":" << end.distance << ",\"start_sweep\":" << start.distance << "}\n";
        EndCase("corrected_endpoint",before,w);}
    {const int before=failures;PhysicsWorld w;w.Init();w.CreatePlayerShape(.125f,.125f);
        w.CreateStaticBox({2.5f,0,0},{.5f,4,4},0,1);auto b=w.CreateDynamicSphere({0,0,0},.5f,1,0,1);w.SetLinearVelocity(b,{3,0,0});Step(w,1.f);
        // Centre 0 -> 1.5 -> 0. Endpoint interpolation is stationary and misses x=1.
        Check(Near(w.GetTransform(b).position,glm::dvec3(0),2e-5),"out-and-back endpoint");
        const auto hit=w.SweepPlayerShape({1,0,0},glm::quat(1,0,0,0),{0,0,0},true);
        Check(hit.hit&&hit.hitBody.id==b.id,"stationary player query sees actual travelled body segments");
        const auto late=w.SweepPlayerShape({1,0,0},glm::quat(1,0,0,0),{0,0,0},true,.98f,1.f);
        Check(!late.hit,"motion range excludes earlier swept contact");EndCase("out_and_back",before,w);}
    { // A connected island member receiving no impulse must keep its anchor.
        const int before=failures;constexpr float dt=.1f;const glm::vec3 omega(2,-1,.7f);
        PhysicsWorld reference;reference.Init();const auto alone=reference.CreateDynamicSphere({0,1,0},.5f,1,0,1);
        reference.SetAngularVelocity(alone,omega);reference.Step(dt);
        PhysicsWorld w;w.Init();
        w.CreateDynamicSphere({0,0,0},.5f,1,0,1);
        const auto c=w.CreateDynamicSphere({0,1,0},.5f,1,0,1);w.SetAngularVelocity(c,omega);
        const auto a=w.CreateDynamicSphere({-1.5f,0,0},.5f,1,0,1);w.SetLinearVelocity(a,{10,0,0});
        Step(w,dt);
        // A reaches B at .05. B-C is a Y-normal frictionless contact; B's
        // change along X induces no normal impulse or torque on C. Its full
        // drift must therefore match the independent ordinary no-impact world.
        Check(w.GetLinearVelocity(c)==reference.GetLinearVelocity(alone),"unimpulsed connected body keeps linear velocity");
        Check(w.GetAngularVelocity(c)==reference.GetAngularVelocity(alone),"unimpulsed connected body keeps angular velocity");
        Check(w.GetTransform(c).position==reference.GetTransform(alone).position,"unimpulsed connected body keeps translation anchor");
        // MK2 integrates a constrained body per substep and a free one in one
        // segment, both by the exact rotation: equal to rounding, not bitwise.
        if(mk2)Check(std::abs(glm::dot(w.GetTransform(c).rotation,reference.GetTransform(alone).rotation))>=1-1e-6f,"unimpulsed connected body keeps its rotation");
        else Check(w.GetTransform(c).rotation==reference.GetTransform(alone).rotation,"unimpulsed connected body keeps exact rotation anchor");
        const auto observed=w.GetTransform(c).rotation,expected=reference.GetTransform(alone).rotation;
        std::cout << "{\"kind\":\"connected_unimpulsed_rotation\",\"expected_quaternion\":[" << expected.w << ',' << expected.x << ',' << expected.y << ',' << expected.z
                  << "],\"observed_quaternion\":[" << observed.w << ',' << observed.x << ',' << observed.y << ',' << observed.z << "]}\n";
        EndCase("connected_unimpulsed_rotation",before,w);
    }
    for(float omega:{.1f,10.f,100.f,1000.f}) {
        const int before=failures;PhysicsWorld w;w.Init();auto body=w.CreateDynamicSphere({100,100,100},.5f,1,0,0);
        w.SetLinearVelocity(body,{1,2,3});w.SetAngularVelocity(body,{0,0,omega});
        w.CreateStaticBox({0,-.5f,0},{10,.5f,10},0,1);auto impact=w.CreateDynamicSphere({0,.501f,0},.5f,1,0,1);w.SetLinearVelocity(impact,{0,-1,0});
        RigidBody oracle;oracle.inverseMass=1;oracle.position={100,100,100};oracle.linearVelocity={1,2,3};oracle.angularVelocity={0,0,omega};const RigidBody start=oracle;IntegrateRigidBodyPosition(oracle,1.f/60);Step(w,1.f/60);
        Check(w.GetTransform(body).position==oracle.position,"unaffected translation endpoint unchanged");Check(SameFreeRotation(w.GetTransform(body).rotation,start,oracle,1.f/60),"unaffected anchored rotation endpoint unchanged");EndCase("anchored_rotation",before,w);
    }
}
}
int main(int argc,char**argv) {
    std::cout << std::setprecision(17) << std::boolalpha;const std::string selection=argc>1?argv[1]:"all";
    if(selection=="all"||selection=="isolated")Isolated();
    if(selection=="all"||selection=="energy")EnergyWitness();
    if(selection=="all"||selection=="chain")Chain();
    if(selection=="all"||selection=="thin")ThinObstacle();
    if(selection=="all"||selection=="grazing")GrazingRegression();
    if(selection=="all"||selection=="fast")FastThin();
    if(selection=="all"||selection=="coupled")Coupled();
    if(selection=="all"||selection=="persistent")PersistentGapImpact();
    if(selection=="all"||selection=="friction")LowSpeedAndFriction();
    if(selection=="all"||selection=="cap")EventCap();
    if(selection=="all"||selection=="ledger")Ledger();
    Check(cases>0,"known fixture selection");std::cout << "{\"kind\":\"summary\",\"cases\":" << cases << ",\"checks\":" << checks << ",\"failures\":" << failures << ",\"pass\":" << (failures==0);Events<PhysicsWorld::StepStats>::Print(totalEvents);std::cout << "}\n";return failures?1:0;
}
