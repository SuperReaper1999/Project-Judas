// Robustness of fast, small rigid bodies against sloped surfaces.
//  1. A light box spun up by hard impacts must not make PhysicsWorld::Step throw
//     ("angular midpoint did not converge").
//  2. A thin box hitting a tilted triangle mesh or box slab fast, or sliding down one, must not
//     end up through it.
#include "CollisionAsset.h"
#include "CollisionGeometry.h"
#include "MeshData.h"
#include "PhysicsWorld.h"
#include "RigidBody.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int checks=0,failures=0;
void Check(bool ok,const std::string& label){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<label<<'\n';}}

// Flat one-sided sheet over x,z in [-1.5,1.5] (normal +y), cells x cells quads.
std::shared_ptr<CollisionAsset> Sheet(int cells){
    MeshData m;const float half=1.5f;
    for(int j=0;j<=cells;++j)for(int i=0;i<=cells;++i){MeshVertex v;v.position={-half+2*half*float(i)/float(cells),0.f,-half+2*half*float(j)/float(cells)};m.vertices.push_back(v);}
    const auto at=[&](int i,int j){return std::uint32_t(j*(cells+1)+i);};
    for(int j=0;j<cells;++j)for(int i=0;i<cells;++i)m.indices.insert(m.indices.end(),{at(i,j),at(i,j+1),at(i+1,j+1),at(i,j),at(i+1,j+1),at(i+1,j)});
    CollisionCookSettings settings;auto asset=std::make_shared<CollisionAsset>();std::string error;
    Check(CookCollision(m,settings,*asset,error),"sheet cooks: "+error);
    return asset;
}

// A finite triangle's separating edge cannot become an infinite contact plane
// when a response normal is corrected. The independent x+z half-space proof
// below does not depend on Judas's SAT/contact implementation.
void CheckFiniteTriangleMisses(){
    for(bool twoSided:{false,true}){
        MeshData m;for(auto v:{glm::vec3(-1,0,-1),glm::vec3(1,0,-1),glm::vec3(-1,0,1)}){MeshVertex p;p.position=v;m.vertices.push_back(p);}
        m.indices={0,2,1};CollisionCookSettings settings;settings.twoSided=twoSided;
        auto asset=std::make_shared<CollisionAsset>();std::string error;
        Check(CookCollision(m,settings,*asset,error),"finite triangle cooks");
        for(bool rotated:{false,true}){
            const glm::quat frame=rotated?glm::angleAxis(.79f,glm::normalize(glm::vec3(1,2,-3))):glm::quat(1,0,0,0);
            PrimitivePose mesh;mesh.shape=Shape::Cooked(asset,"finite triangle");mesh.parentOrientation=frame;
            PrimitivePose box;box.shape=Shape::Box({.2f,.015f,.3f});
            const glm::vec3 position(.738190f,.0436529f,.993021f);
            box.parentPosition=frame*position;
            box.parentOrientation=frame*glm::normalize(glm::quat(.997289f,-.0196676f,-.0393352f,-.0590029f));
            const double lowerGap=(double(position.x)+position.z)/std::sqrt(2.)-glm::length(glm::dvec3(box.shape.halfExtents));
            Check(lowerGap>.86,"independent edge separation exceeds 86 cm");
            for(float margin:{0.f,.01f,.1f}){
                Check(!CookedContacts(box,mesh,margin).count,"edge miss stays a miss with contact margin");
                Check(!CookedHasContacts(box,mesh,margin),"Boolean edge miss agrees with full contact query");
            }
            // A genuine intersecting pose must still report contacts.
            box.shape=Shape::Box({.2f,.1f,.2f});box.parentPosition=frame*glm::vec3(-.5f,.03f,-.5f);box.parentOrientation=frame;
            Check(CookedContacts(box,mesh,0).count>0&&CookedHasContacts(box,mesh,0),"actual front-side overlap remains a contact");
            box.parentPosition=frame*glm::vec3(-.5f,-.03f,-.5f);
            Check(bool(CookedContacts(box,mesh,0).count)==twoSided,"one-sided backface remains culled; two-sided overlap remains valid");
        }
        std::mt19937 rng(1234);std::uniform_real_distribution<float> horizontal(-1.5f,1.5f),height(-.1f,.1f),angle(-3.14f,3.14f);
        PrimitivePose mesh;mesh.shape=Shape::Cooked(asset,"random finite triangle");unsigned misses=0;
        for(int i=0;i<2048;++i){
            PrimitivePose box;box.shape=Shape::Box({.2f,.015f,.3f});box.parentPosition={horizontal(rng),height(rng),horizontal(rng)};
            box.parentOrientation=glm::angleAxis(angle(rng),glm::normalize(glm::vec3(1,2,3)));
            const auto distance=PolyGeometry(box,mesh);
            if(distance.valid&&distance.gap>1e-7){++misses;
                Check(!CookedContacts(box,mesh,0).count,"separated random finite triangle has no manifold");
                Check(!CookedHasContacts(box,mesh,0),"separated random finite triangle has no Boolean contact");
            }
        }
        Check(misses>500,"random negative controls exercise finite triangle edges");
    }
}

struct Drop {double deepest=0,deepestCorner=0;bool threw=false;std::string error;float maxSpin=0;};
// A 4 x 2 x 6 cm, 0.2 kg box (axis-aligned, 1.2 m up; or set down lying on the support)
// falls onto a support tilted `degrees` about x: a triangle-mesh
// sheet (`mesh`) or a 1 m thick box slab. Records how far the box centre, and its deepest
// corner, ever get beneath the support plane while over the support, in metres.
Drop DropBox(bool mesh,float degrees,glm::vec3 velocity,glm::vec3 spin,int steps=60,bool setDown=false){
    const float height=setDown?.012f:1.2f;   // set down: lying on the support, aligned with it
    Drop r;PhysicsWorld w;w.Init();const glm::quat tilt=glm::angleAxis(glm::radians(degrees),glm::vec3(1,0,0));
    if(mesh)w.CreateShape(Shape::Cooked(Sheet(60),"tilted sheet"),{{0,0,0},tilt},false,1,.6f,0);
    else w.CreateShape(Shape::Box({1.5f,.5f,1.5f}),{tilt*glm::vec3(0,-.5f,0),tilt},false,1,.6f,0);
    auto h=w.CreateDynamicBox(tilt*glm::vec3(0,height,0),{.02f,.01f,.03f},.2f,.4f,0);if(setDown)w.ResetBody(h,tilt*glm::vec3(0,height,0),tilt);w.SetLinearVelocity(h,velocity);w.SetAngularVelocity(h,spin);
    try{
        for(int i=0;i<steps;++i){
            w.ApplyLinearAcceleration(h,{0,-9.81f,0},1.f/60.f);w.Step(1.f/60.f);
            const auto t=w.GetTransform(h);const glm::vec3 local=glm::inverse(tilt)*t.position;
            if(std::abs(local.x)<=1.5f&&std::abs(local.z)<=1.5f){
                r.deepest=std::max(r.deepest,-double(local.y));
                for(int c=0;c<8;++c){const glm::vec3 corner=glm::inverse(tilt)*(t.position+t.rotation*glm::vec3(c&1?.02f:-.02f,c&2?.01f:-.01f,c&4?.03f:-.03f));
                    r.deepestCorner=std::max(r.deepestCorner,-double(corner.y));}
            }
            r.maxSpin=std::max(r.maxSpin,glm::length(w.GetAngularVelocity(h)));
        }
    }catch(const std::exception& e){r.threw=true;r.error=e.what();}
    return r;
}

// Inputs captured from PhysicsWorld::Step throwing "angular midpoint did not converge" while the
// 4 x 2 x 6 cm box below was being spun up by impacts: diagonal local inverse inertia, orientation,
// world angular velocity (250-1000 rad/s) and duration (one 60 Hz step).
struct DriftCase {glm::vec3 inverseInertia;glm::quat orientation;glm::vec3 velocity;float duration;};
const DriftCase kCapturedDrift[]={
    {{0x1.d4cp+13f,0x1.6893bp+13f,0x1.d4cp+14f},{0x1.7a70eep-2f,0x1.25a136p-5f,0x1.1d9732p-1f,0x1.7c0dbcp-1f},{-0x1.735146p+9f,-0x1.0a23ecp+8f,-0x1.fd273cp+8f},0x1.111112p-6f},
    {{0x1.d4cp+13f,0x1.6893bp+13f,0x1.d4cp+14f},{-0x1.391c76p-2f,0x1.06ca0ep-7f,-0x1.31f29cp-1f,-0x1.7b7db8p-1f},{-0x1.3e2ep+9f,-0x1.f5b3f8p+8f,-0x1.1fc678p+9f},0x1.111112p-6f},
    {{0x1.d4cp+13f,0x1.6893bp+13f,0x1.d4cp+14f},{0x1.94c88ap-1f,0x1.be2398p-2f,-0x1.820dfp-3f,0x1.8c1412p-2f},{0x1.76be52p+9f,0x1.759faep+6f,-0x1.0d7362p+5f},0x1.111112p-6f},
    {{0x1.d4cp+13f,0x1.6893bp+13f,0x1.d4cp+14f},{-0x1.b6f9b4p-1f,0x1.03e4ecp-1f,-0x1.2020bap-4f,-0x1.88b69cp-5f},{0x1.636bep+8f,0x1.e3dedcp+4f,0x1.aaf614p+1f},0x1.111112p-6f},
    {{0x1.d4cp+13f,0x1.6893bp+13f,0x1.d4cp+14f},{-0x1.620a3cp-4f,0x1.fdaabep-1f,0x1.3449b6p-5f,-0x1.d55bfp-7f},{0x1.fd4c96p+7f,0x1.40e5d6p+1f,0x1.31c9bp+3f},0x1.111112p-6f},
    {{0x1.d4cp+13f,0x1.6893bp+13f,0x1.d4cp+14f},{-0x1.6fc692p-4f,-0x1.56745p-4f,0x1.d9aacap-1f,-0x1.6fe67ap-2f},{0x1.d7f93p+8f,0x1.978414p+5f,0x1.49a3dap+4f},0x1.111112p-6f},
    {{0x1.d4cp+13f,0x1.6893bp+13f,0x1.d4cp+14f},{0x1.44023cp-6f,0x1.ed21a8p-4f,0x1.e7aaaap-1f,0x1.1dd174p-2f},{0x1.d0b7aap+8f,0x1.a527dcp+2f,-0x1.ad3b5ep+5f},0x1.111112p-6f},
    {{0x1.d4cp+13f,0x1.6893bp+13f,0x1.d4cp+14f},{-0x1.2e5186p-1f,0x1.9586b2p-1f,-0x1.378494p-3f,-0x1.e63f8ep-6f},{0x1.c5aebap+9f,0x1.8b0e68p+6f,-0x1.66ed4p+6f},0x1.111112p-6f},
    {{0x1.d4cp+13f,0x1.6893bp+13f,0x1.d4cp+14f},{0x1.8b69dep-1f,0x1.4541bep-1f,0x1.9fd8cep-12f,-0x1.131832p-11f},{0x1.fce45cp+7f,0x1.fd1e9cp-3f,-0x1.47c302p-3f},0x1.111112p-6f}
};
// The generator is the midpoint angular velocity of the implicit-midpoint body-momentum step.
// Its end momentum m1 = 2 m_mid - m0 must keep the momentum norm and kinetic energy of m0.
void CheckDrift(const DriftCase& c,int index){
    const glm::mat3 inverseInertia(glm::vec3(c.inverseInertia.x,0,0),glm::vec3(0,c.inverseInertia.y,0),glm::vec3(0,0,c.inverseInertia.z));
    const std::string label="captured fast spin "+std::to_string(index);
    glm::vec3 generator;
    try{generator=AngularDriftGenerator(inverseInertia,c.orientation,c.velocity,c.duration);}
    catch(const std::exception& e){Check(false,label+" solves: "+e.what());return;}
    Check(std::isfinite(generator.x)&&std::isfinite(generator.y)&&std::isfinite(generator.z),label+" is finite");
    const glm::dmat3 R=glm::mat3_cast(glm::normalize(glm::dquat(c.orientation)));const glm::dmat3 inverseBody(inverseInertia);
    const glm::dvec3 m0=glm::inverse(inverseBody)*glm::transpose(R)*glm::dvec3(c.velocity);
    const glm::dvec3 middle=glm::inverse(inverseBody)*glm::transpose(R)*glm::dvec3(generator),m1=2.*middle-m0;
    const double energy0=glm::dot(m0,inverseBody*m0),energy1=glm::dot(m1,inverseBody*m1);
    const glm::dvec3 residual=middle-m0-.5*double(c.duration)*glm::cross(middle,inverseBody*middle);
    Check(glm::length(residual)<=1e-5*glm::length(m0),label+" solves the original midpoint equation after float rounding");
    // The generator is returned in single precision: allow its rounding, nothing more.
    Check(std::abs(glm::length(m1)-glm::length(m0))<=1e-5*glm::length(m0),label+" keeps the momentum norm");
    Check(std::abs(energy1-energy0)<=1e-5*energy0,label+" keeps the kinetic energy");
}
}

int main(){
    CheckFiniteTriangleMisses();
    int index=0;for(const auto& c:kCapturedDrift)CheckDrift(c,index++);
    // A light box spun up by hard impacts, horizontal support: Step must not throw, and the box
    // must never pass through (its centre stays on the support side).
    double worstCorner=0;
    const auto judge=[&](const std::string& label,const Drop& r){
        std::cout<<"  "<<label<<" maxSpin "<<r.maxSpin<<" centre below "<<r.deepest<<" corner below "<<r.deepestCorner<<(r.threw?" THREW "+r.error:"")<<'\n';
        Check(!r.threw,label+": Step does not throw ("+r.error+")");
        Check(r.deepest<=.005,label+": box does not pass through the support");
        Check(r.deepestCorner<=.03,label+": no corner sinks a box depth into the support");
        worstCorner=std::max(worstCorner,r.deepestCorner);
    };
    std::cout<<"spin-up drops onto a horizontal support\n";
    for(int mesh=0;mesh<2;++mesh)for(float down:{20.f,40.f})for(float side:{0.f,30.f})for(float spin:{0.f,15.f,45.f})
        judge(std::string(mesh?"mesh":"slab")+" down "+std::to_string(int(down))+" side "+std::to_string(int(side))+" spin "+std::to_string(int(spin)),
              DropBox(mesh,0,{0,-down,side},{spin,0,spin*.6f},90));
    // Thin box falling onto supports tilted 0-45 degrees: it lands corner first and pivots.
    std::cout<<"tilted supports\n";
    for(int mesh=0;mesh<2;++mesh)for(float degrees:{0.f,5.f,15.f,30.f,45.f})for(float speed:{10.f,20.f,40.f})
        judge(std::string(mesh?"mesh":"slab")+" tilt "+std::to_string(int(degrees))+" speed "+std::to_string(int(speed)),DropBox(mesh,degrees,{0,-speed,0},{0,0,0},60));
    // The same box set down gently and left to slide (no fast impact at all).
    std::cout<<"gentle slides\n";
    for(int mesh=0;mesh<2;++mesh)for(float degrees:{15.f,30.f,45.f})
        judge(std::string(mesh?"mesh":"slab")+" slide "+std::to_string(int(degrees)),DropBox(mesh,degrees,{0,0,0},{0,0,0},180,true));
    std::cout<<"deepest corner below any support "<<worstCorner<<" m\n";
    std::cout<<"checks "<<checks<<" failures "<<failures<<'\n';
    return failures?1:0;
}
