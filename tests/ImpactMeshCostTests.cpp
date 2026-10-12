// Continuous-impact cost of a jointed box ragdoll resting in a static triangle mesh.
// Reproduces the consumer report "ragdoll in a concave mesh ramp stalls CCD for seconds per step"
// (Skate & Parkour art half-pipe) without game content: a 12-body ragdoll is thrown into a
// procedurally cooked trough (curved) or onto a flat sheet, at several triangle densities.
// Prints per-scenario step time, impact counters, collision-geometry work and a bit-exact
// trajectory digest. The digest is the evidence that a pure performance change preserved the
// simulation; timings are reported, never asserted.
#include "CollisionAsset.h"
#include "CollisionGeometry.h"
#include "MeshData.h"
#include "PhysicsWorld.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
int checks=0,failures=0;
void Check(bool ok,const std::string& label){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<label<<'\n';}}

// Open sheet over x,z in [-1.5,1.5] with cellsX by cellsZ quads. curved: circular trough
// across z (radius 1.6 m, bottom at y=0), like a skate half-pipe; otherwise flat at y=0.
std::shared_ptr<CollisionAsset> Sheet(int cellsX,int cellsZ,bool curved){
    MeshData m;const float half=1.5f,radius=1.6f;
    for(int j=0;j<=cellsZ;++j)for(int i=0;i<=cellsX;++i){
        MeshVertex v;const float x=-half+2*half*float(i)/float(cellsX),z=-half+2*half*float(j)/float(cellsZ);
        v.position={x,curved?radius-std::sqrt(radius*radius-z*z):0.f,z};m.vertices.push_back(v);
    }
    const auto at=[&](int i,int j){return std::uint32_t(j*(cellsX+1)+i);};
    for(int j=0;j<cellsZ;++j)for(int i=0;i<cellsX;++i){
        // Upward winding (+y normal on the flat sheet).
        m.indices.insert(m.indices.end(),{at(i,j),at(i,j+1),at(i+1,j+1),at(i,j),at(i+1,j+1),at(i+1,j)});
    }
    CollisionCookSettings settings;auto asset=std::make_shared<CollisionAsset>();std::string error;
    Check(CookCollision(m,settings,*asset,error),"sheet cooks: "+error);
    return asset;
}

struct Part {glm::vec3 position,half;float mass;bool sphere=false;};
// Rider-sized ragdoll standing at the origin (feet at y=0). Index order is the joint parent order.
const std::vector<Part> kParts={
    {{0,1.00f,0},{.15f,.10f,.10f},10},           // 0 hips
    {{0,1.30f,0},{.15f,.15f,.10f},10},           // 1 spine
    {{0,1.62f,0},{.11f,.11f,.11f},5,true},       // 2 head (sphere)
    {{-.30f,1.40f,0},{.13f,.05f,.05f},2.5f},     // 3 left arm
    {{-.58f,1.40f,0},{.15f,.045f,.045f},2},      // 4 left forearm
    {{.30f,1.40f,0},{.13f,.05f,.05f},2.5f},      // 5 right arm
    {{.58f,1.40f,0},{.15f,.045f,.045f},2},       // 6 right forearm
    {{-.10f,.70f,0},{.075f,.20f,.075f},8},       // 7 left thigh
    {{-.10f,.31f,0},{.06f,.19f,.06f},4},         // 8 left shin
    {{-.10f,.05f,.06f},{.05f,.04f,.10f},1.2f},   // 9 left foot
    {{.10f,.70f,0},{.075f,.20f,.075f},8},        // 10 right thigh
    {{.10f,.31f,0},{.06f,.19f,.06f},4},          // 11 right shin
    {{.10f,.05f,.06f},{.05f,.04f,.10f},1.2f}};   // 12 right foot
struct JointSpec {int a,b;glm::vec3 at;};
const std::vector<JointSpec> kJoints={{0,1,{0,1.15f,0}},{1,2,{0,1.50f,0}},{1,3,{-.17f,1.40f,0}},{3,4,{-.43f,1.40f,0}},
    {1,5,{.17f,1.40f,0}},{5,6,{.43f,1.40f,0}},{0,7,{-.10f,.90f,0}},{7,8,{-.10f,.50f,0}},{8,9,{-.10f,.10f,0}},
    {0,10,{.10f,.90f,0}},{10,11,{.10f,.50f,0}},{11,12,{.10f,.10f,0}}};

struct Result {double totalMs=0,worstMs=0,maxPenetration=0,meanPenetration=0;int deepestStep=0;std::uint64_t digest=1469598103934665603ull;PhysicsWorld::StepStats impacts{};
    std::uint64_t convexTests=0,triangles=0,nodes=0;};
void Mix(std::uint64_t& h,const void* data,std::size_t n){auto p=static_cast<const unsigned char*>(data);for(std::size_t i=0;i<n;++i){h^=p[i];h*=1099511628211ull;}}

// Deepest point of a bone below the analytic support surface: the y=0 plane, or the trough's
// circle (radius 1.6 about y=1.6, z=0), over the sheet's footprint only. A tessellated trough
// sits up to its chord sagitta above the circle, so this slightly understates penetration there
// (0.2 mm dense, 1 mm coarse).
double Penetration(const BodyTransform& t,const Part& part,bool curved){
    const float radius=1.6f;std::vector<glm::vec3> points;float inflate=0;
    if(part.sphere){points.push_back(t.position);inflate=part.half.x;}
    else for(int i=0;i<8;++i)points.push_back(t.position+t.rotation*glm::vec3(i&1?part.half.x:-part.half.x,i&2?part.half.y:-part.half.y,i&4?part.half.z:-part.half.z));
    double deepest=0;
    for(const auto& p:points){
        if(std::abs(p.x)>1.5f||std::abs(p.z)>1.5f)continue;   // beyond the sheet: nothing to penetrate
        const double below=curved?std::hypot(double(p.y)-radius,double(p.z))-radius:-double(p.y);
        deepest=std::max(deepest,below+inflate);
    }
    return deepest;
}
// The ragdoll is laid on its back along the trough, 0.75 m up, thrown along +z (a bail into the
// ramp, rocking up the far wall) with a little spin, then left to settle for `steps` fixed steps.
Result Run(const std::shared_ptr<CollisionAsset>& ground,bool curved,int steps){
    PhysicsWorld w;w.Init();
    if(ground)w.CreateShape(Shape::Cooked(ground,"impact cost sheet"),{},false,1,.6f,0);
    else w.CreateStaticBox({0,-.5f,0},{1.5f,.5f,1.5f},.6f,0);
    // Lay the standing pose face up along the trough (standing up -> +x, forward -> +y, arms across z).
    const glm::quat lie=glm::quat_cast(glm::mat3(glm::vec3(0,0,1),glm::vec3(1,0,0),glm::vec3(0,1,0)));const glm::vec3 offset{-.8f,.75f,-.5f};
    std::vector<BodyHandle> bodies;
    for(const auto& p:kParts){
        const glm::vec3 at=lie*p.position+offset;
        BodyHandle h=p.sphere?w.CreateDynamicSphere(at,p.half.x,p.mass,.6f,0):w.CreateDynamicBox(at,p.half,p.mass,.6f,0);
        w.ResetBody(h,at,lie);
        w.SetLinearVelocity(h,{0,-.5f,1.2f});w.SetAngularVelocity(h,{1.5f,0,.8f});bodies.push_back(h);
    }
    // Like WorldRagdoll without self-collision: bones never collide with each other.
    for(std::size_t a=0;a<bodies.size();++a)for(std::size_t b=a+1;b<bodies.size();++b)w.SetPairCollisionEnabled(bodies[a],bodies[b],false);
    for(const auto& j:kJoints){
        JointSettings s;s.type=JointType::Ball;s.bodyA=bodies[std::size_t(j.a)];s.bodyB=bodies[std::size_t(j.b)];
        // Anchors are body-local; every body starts with the same orientation `lie`.
        s.anchorA=glm::inverse(lie)*(j.at-kParts[std::size_t(j.a)].position);s.anchorB=glm::inverse(lie)*(j.at-kParts[std::size_t(j.b)].position);
        Check(w.CreateJoint(s).id!=0,"ragdoll joint created");
    }
    Result r;const float dt=1.f/60.f;ResetCollisionGeometryStats();
    for(int step=0;step<steps;++step){
        for(auto h:bodies)w.ApplyLinearAcceleration(h,{0,-9.81f,0},dt);
        const auto begin=std::chrono::steady_clock::now();w.Step(dt);
        const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
        r.totalMs+=ms;r.worstMs=std::max(r.worstMs,ms);
        const auto& s=w.LastStepStats();
        r.impacts.impactQueries+=s.impactQueries;r.impacts.impactSearchIterations+=s.impactSearchIterations;r.impacts.impactSearchLimit+=s.impactSearchLimit;
        r.impacts.impactSamplingFallbacks+=s.impactSamplingFallbacks;r.impacts.impactSamplingTests+=s.impactSamplingTests;r.impacts.impactEvents+=s.impactEvents;
        double deepest=0;for(std::size_t i=0;i<bodies.size();++i)deepest=std::max(deepest,Penetration(w.GetTransform(bodies[i]),kParts[i],curved));
        r.meanPenetration+=deepest/steps;if(deepest>r.maxPenetration){r.maxPenetration=deepest;r.deepestStep=step;}
        for(auto h:bodies){const auto t=w.GetTransform(h);const auto v=w.GetLinearVelocity(h),o=w.GetAngularVelocity(h);
            Mix(r.digest,&t.position,sizeof(t.position));Mix(r.digest,&t.rotation,sizeof(t.rotation));Mix(r.digest,&v,sizeof(v));Mix(r.digest,&o,sizeof(o));}
    }
    const auto g=GetCollisionGeometryStats();r.convexTests=g.convexTests;r.triangles=g.triangles;r.nodes=g.nodes;
    // Settled: nothing fell through or flew off.
    for(auto h:bodies){const auto p=w.GetTransform(h).position;Check(std::isfinite(p.x)&&p.y>-.3f&&p.y<3.f&&std::abs(p.x)<4&&std::abs(p.z)<4,"ragdoll stays on its support");}
    return r;
}
}

int main(int argc,char** argv){
    // Optional: step count, then a scenario-name filter (substring).
    const int steps=argc>1?std::max(1,std::atoi(argv[1])):300;const std::string only=argc>2?argv[2]:"";
    struct Scenario {const char* name;int cellsX,cellsZ;bool curved,primitive;};
    const std::vector<Scenario> scenarios={{"box primitive floor",0,0,false,true},{"flat sheet 6x6",6,6,false,false},{"flat sheet 60x60",60,60,false,false},
        {"trough 2x26 coarse",2,26,true,false},{"trough 30x30",30,30,true,false},{"trough 60x60 dense",60,60,true,false}};
    std::cout<<std::fixed<<std::setprecision(2);
    for(const auto& s:scenarios){
        if(!only.empty()&&std::string(s.name).find(only)==std::string::npos)continue;
        auto ground=s.primitive?nullptr:Sheet(s.cellsX,s.cellsZ,s.curved);
        const auto a=Run(ground,s.curved,steps),b=Run(ground,s.curved,steps);
        Check(a.digest==b.digest,std::string(s.name)+" is deterministic run to run");
        std::cout<<"SCENARIO \""<<s.name<<"\" triangles "<<(ground?ground->faces.size():0)<<" steps "<<steps
                 <<" totalMs "<<a.totalMs<<" worstStepMs "<<a.worstMs<<" maxPenetrationMm "<<a.maxPenetration*1000<<" atStep "<<a.deepestStep<<" meanDeepestMm "<<a.meanPenetration*1000
                 <<" impactQueries "<<a.impacts.impactQueries<<" searchIterations "<<a.impacts.impactSearchIterations
                 <<" searchLimits "<<a.impacts.impactSearchLimit<<" samplingFallbacks "<<a.impacts.impactSamplingFallbacks
                 <<" samplingTests "<<a.impacts.impactSamplingTests<<" impactEvents "<<a.impacts.impactEvents
                 <<" convexTests "<<a.convexTests<<" meshTriangles "<<a.triangles<<" bvhNodes "<<a.nodes
                 <<" digest "<<std::hex<<a.digest<<std::dec<<'\n';
    }
    std::cout<<"checks "<<checks<<" failures "<<failures<<'\n';
    return failures?1:0;
}
