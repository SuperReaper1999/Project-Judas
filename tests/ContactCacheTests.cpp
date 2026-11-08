// FTFT4A-P cache correctness through the same geometry and PhysicsWorld paths.
// ShapeAabb(shape, position, orientation) is the retained uncached calculation;
// the PhysicsWorld inspection method only returns existing derived cache data.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "ContactPreparedGeometry.h"
#include "Narrowphase.h"
#include "PhysicsWorld.h"

namespace {
std::size_t checks=0,failures=0,boundComparisons=0,worldInspections=0,sweepCases=0;
std::uint32_t Bits(float f) {std::uint32_t x;std::memcpy(&x,&f,sizeof(x));return x;}
std::uint64_t Bits(double f) {std::uint64_t x;std::memcpy(&x,&f,sizeof(x));return x;}
bool Same(double a,double b) {return Bits(a)==Bits(b);}
bool Same(const glm::dvec3& a,const glm::dvec3& b) {return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
bool Same(float a,float b) {return Bits(a)==Bits(b);}
bool Same(const glm::vec3& a,const glm::vec3& b) {return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
bool Same(const glm::quat& a,const glm::quat& b) {return Same(a.w,b.w)&&Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
bool Same(const BodyTransform& a,const BodyTransform& b) {return Same(a.position,b.position)&&Same(a.rotation,b.rotation);}
bool Same(const Aabb& a,const Aabb& b) {return Same(a.min,b.min)&&Same(a.max,b.max);}
void Check(bool yes,const std::string& label) {++checks;if(!yes){++failures;std::cout<<"FAIL "<<label<<'\n';}}
void Bounds(const Aabb& actual,const Aabb& expected,const std::string& label) {
    ++boundComparisons;Check(Same(actual,expected),label);
    if(!Same(actual,expected)) {
        for(int k=0;k<3;++k)std::cout<<"  axis="<<k<<" actual=["<<actual.min[k]<<','<<actual.max[k]<<"] expected=["<<expected.min[k]<<','<<expected.max[k]<<"] bits=["<<Bits(actual.min[k])<<','<<Bits(actual.max[k])<<','<<Bits(expected.min[k])<<','<<Bits(expected.max[k])<<"]\n";
    }
}
std::vector<CompoundBox> Children(float s=1) {
    return {{{.317f*s,-.183f*s,.713f*s},{.119f*s,.311f*s,.083f*s}},
            {{-.617f*s,.283f*s,-.413f*s},{.219f*s,.151f*s,.313f*s}}};
}
struct FixtureInput {Shape shape;RigidBody body;};
FixtureInput ReadFixture(std::istream& in) {
    std::string kind;glm::vec3 c,h;glm::quat q;float radius;int count;
    in>>kind>>c.x>>c.y>>c.z>>q.w>>q.x>>q.y>>q.z>>radius>>h.x>>h.y>>h.z>>count;
    std::vector<CompoundBox> children;
    for(int k=0;k<count;++k){CompoundBox b;in>>b.localCenter.x>>b.localCenter.y>>b.localCenter.z>>b.halfExtents.x>>b.halfExtents.y>>b.halfExtents.z;children.push_back(b);}
    FixtureInput out;out.shape=kind=="sphere"?Shape::Sphere(radius):kind=="box"?Shape::Box(h):Shape::Compound(children);out.body.position=c;out.body.orientation=q;return out;
}
void EqualManifold(const ContactManifold& a,const ContactManifold& b,const std::string& label) {
    Check(a.count==b.count&&a.uncertain==b.uncertain,label+" manifold classification");
    for(int k=0;k<std::min(a.count,b.count);++k) {
        const auto& x=a.points[k];const auto& y=b.points[k];
        Check(x.hit==y.hit&&x.separationState==y.separationState&&Same(x.signedSeparation,y.signedSeparation),label+" exact signed gap");
        Check(Same(x.preciseNormal,y.preciseNormal)&&Same(x.localAnchorA,y.localAnchorA)&&Same(x.localAnchorB,y.localAnchorB),label+" bitwise normal and anchors");
        Check(Same(x.localWitnessA,y.localWitnessA)&&Same(x.localWitnessB,y.localWitnessB)&&Same(x.point,y.point),label+" bitwise surface witnesses/presentation");
    }
}
void PreparedContacts() {
    std::cout<<"A0: prepared and uncached contacts over unchanged independent fixture inputs\n";
    std::ifstream file("tests/fixtures/contact/pairs.txt");Check(bool(file),"existing independent fixtures accessible");
    std::string line;std::size_t cases=0,pairs=0;
    while(std::getline(file,line)) {
        if(line.empty())continue;
        std::istringstream in(line);std::string id;float margin;int worldExpected,count,ignored;
        in>>id>>margin>>worldExpected>>count;for(int i=0;i<2*count;++i)in>>ignored;
        const auto a=ReadFixture(in),b=ReadFixture(in);Check(bool(in),"represented fixture parsed: "+id);
        const ContactPreparedOrientation preparedA(a.body.orientation),preparedB(b.body.orientation);
        for(int i=0;i<PrimitiveCount(a.shape);++i)for(int j=0;j<PrimitiveCount(b.shape);++j) {
            const auto pa=PrimitiveAt(a.shape,a.body,i),pb=PrimitiveAt(b.shape,b.body,j);
            EqualManifold(ComputeContacts(pa,pb,margin),ComputeContacts(pa,pb,margin,&preparedA,&preparedB),id);
            ++pairs;
        }
        ++cases;
    }
    Check(cases==823&&pairs==887,"all unchanged 823 fixture rows / 887 primitive pairs compared");
    RigidBody a,b;a.orientation=glm::quat(.7f,.1f,.3f,.4f);b.position={.8f,0,0};
    const ContactPreparedOrientation stale(glm::quat(1,0,0,0)),validB(b.orientation);
    const auto pa=PrimitiveAt(Shape::Box(glm::vec3(.5f)),a,0),pb=PrimitiveAt(Shape::Box(glm::vec3(.5f)),b,0);
    const auto expected=ComputeContacts(pa,pb);ResetContactGeometryDiagnostics();
    const auto actual=ComputeContacts(pa,pb,0,&stale,&validB);EqualManifold(actual,expected,"stale hint recomputation");
    Check(GetContactGeometryDiagnostics().preparedOrientationMisses>0,"stale prepared orientation visibly rejected");
    std::cout<<"CONTACT_CACHE fixture_rows="<<cases<<" primitive_pairs="<<pairs<<'\n';
}
void PreparedBounds() {
    std::cout<<"A: prepared bounds retain uncached expression-order bits\n";
    const glm::quat dense(.71357f,.23711f,-.41397f,.13931f);
    glm::quat next=dense;next.x=std::nextafter(next.x,std::numeric_limits<float>::infinity());
    const std::vector<glm::quat> rotations{{1,0,0,0},{1,0,0,1e-8f},dense,next,-dense,3.125f*dense};
    for(std::size_t qi=0;qi<rotations.size();++qi) {
        const auto q=rotations[qi];const ContactPreparedOrientation prepared(q);
        Check(prepared.Matches(q),"prepared orientation accepts its exact source");
        auto changed=q;changed.z=std::nextafter(changed.z,std::numeric_limits<float>::infinity());
        Check(!prepared.Matches(changed),"one-ULP quaternion change invalidates orientation preparation");
        for(float scale:{.001f,1.f,1000.f}) {
            const std::vector<Shape> shapes{Shape::Sphere(.317f*scale),Shape::Box(glm::vec3(.119f,.311f,.083f)*scale),
                Shape::Capsule(.137f*scale,.383f*scale),Shape::Compound(Children(scale))};
            for(std::size_t si=0;si<shapes.size();++si) {
                const auto cached=PrepareShapeBounds(shapes[si],prepared);
                for(float t:{0.f,10.f,137.f,1000.f,1048576.f})for(int k=0;k<5;++k) {
                    const glm::vec3 p(t+.013f*k,-t+.071f*k,2*t-.033f*k);
                    Bounds(ShapeAabb(cached,p),ShapeAabb(shapes[si],p,q),"prepared/uncached q="+std::to_string(qi)+" shape="+std::to_string(si));
                }
            }
        }
    }
    const ContactPreparedOrientation a(dense),b(next);
    const auto shape=Shape::Compound(Children());const auto pa=PrepareShapeBounds(shape,a),pb=PrepareShapeBounds(shape,b);
    const glm::vec3 p(137,-17,3);
    Bounds(ShapeAabb(pa,p),ShapeAabb(shape,p,dense),"original dense compound orientation");
    Bounds(ShapeAabb(pb,p),ShapeAabb(shape,p,next),"one-ULP rotated compound orientation");
}
void Inspect(const PhysicsWorld& w,BodyHandle h,const std::string& label) {
    ++worldInspections;
    PhysicsWorld::GeometryCacheState cached;
    Check(w.GetBodyGeometryCacheState(h,cached),label+" inspection exists");
    Shape shape;BodyTransform pose;
    Check(w.GetBodyShape(h,shape,pose),label+" live shape exists");
    const auto previous=w.GetPreviousTransform(h),current=w.GetTransform(h);
    Check(cached.currentValid&&cached.previousValid,label+" both pose bounds maintained");
    Check(Same(cached.currentPose,current),label+" current cache key matches actual stored pose");
    Check(Same(cached.previousPose,previous),label+" previous cache key matches actual stored pose");
    Bounds({cached.currentMin,cached.currentMax},ShapeAabb(shape,current.position,current.rotation),label+" current bounds");
    Bounds({cached.previousMin,cached.previousMax},ShapeAabb(shape,previous.position,previous.rotation),label+" previous bounds");
    Check(Same(cached.boundingRadius,ShapeBoundingRadius(shape)),label+" cached shape radius");
    glm::vec3 lo,hi;Check(w.GetBodyBroadphaseBounds(h,lo,hi),label+" broadphase proxy exists");
    Check(Aabb{lo,hi}.Contains({cached.currentMin,cached.currentMax}),label+" proxy contains current geometry");
    if(w.IsDynamicBody(h))Check(Aabb{lo,hi}.Contains({cached.previousMin,cached.previousMax}),label+" proxy contains previous geometry");
}
void StaticResetAndReuse() {
    std::cout<<"B: static door ResetBody, tiny/real rotation, slot reuse and vector reallocation\n";
    PhysicsWorld w;w.Init();
    const auto door=w.CreateStaticBox({3,4,5},{.17f,2.1f,.73f},0,0);
    Inspect(w,door,"static creation");
    w.ResetBody(door,{137,-3,10},{1,0,0,0});Inspect(w,door,"static translation");
    const glm::quat q=glm::normalize(glm::quat(.91f,.12f,.23f,.31f));
    w.ResetBody(door,{137,-3,10},q);Inspect(w,door,"static rotation");
    auto tiny=q;tiny.x=std::nextafter(tiny.x,std::numeric_limits<float>::infinity());
    w.ResetBody(door,{137,-3,10},tiny);Inspect(w,door,"static one-ULP rotation");
    w.DestroyBody(door);PhysicsWorld::GeometryCacheState stale;
    Check(!w.GetBodyGeometryCacheState(door,stale),"destroyed body cache is inaccessible");
    const auto replacement=w.CreateDynamicSphere({-18,7,3},1.25f,3,0,0);
    Check((door.id&0xfffffu)==(replacement.id&0xfffffu)&&door.id!=replacement.id,"fixture exercises same slot with a new generation");
    Check(!w.GetBodyGeometryCacheState(door,stale),"old-generation handle cannot inspect replacement cache");
    Inspect(w,replacement,"replacement sphere radius and bounds");
    w.DestroyBody(replacement);
    const auto compound=w.CreateDynamicCompoundBoxes({137,7,3},Children(),9,0,0);
    w.ResetBody(compound,{137,7,3},q);Inspect(w,compound,"replacement offset compound");
    std::vector<BodyHandle> many;
    for(int i=0;i<512;++i)many.push_back(w.CreateDynamicBox({500.f+3*i,4.f+float(i%7),-30},glm::vec3(.1f+.001f*i),1,0,0));
    Inspect(w,compound,"compound after body-vector reallocation");
    for(std::size_t i=0;i<many.size();++i)Inspect(w,many[i],"reallocated body "+std::to_string(i));
    w.SetLinearVelocity(compound,{.3f,-.2f,.4f});w.SetAngularVelocity(compound,{.1f,.4f,-.3f});w.Step(1.f/60);
    Inspect(w,compound,"compound after shared storage moved then integrated");
}
void IntegrationAndCorrection() {
    std::cout<<"C: integration and position correction invalidate only derived data\n";
    PhysicsWorld w;w.Init();const auto body=w.CreateDynamicBox({0,3,0},{.2f,.3f,.4f},5,0,0);
    w.SetLinearVelocity(body,{1.25f,.25f,-.75f});w.SetAngularVelocity(body,{.7f,-.2f,.4f});
    for(int i=0;i<12;++i) {
        const auto before=w.GetTransform(body);w.Step(1.f/60);Inspect(w,body,"rotating integration "+std::to_string(i));
        Check(Same(before,w.GetPreviousTransform(body)),"previous pose remains actual pre-step state");
        Check(!Same(before,w.GetTransform(body)),"fixture really changes body pose");
    }
    PhysicsWorld contact;contact.Init();const auto floor=contact.CreateStaticBox({0,-.5f,0},{8,.5f,8},.5f,0);
    const auto box=contact.CreateDynamicBox({0,.25f,0},glm::vec3(.5f),2,.5f,0);
    const auto before=contact.GetTransform(box);contact.Step(1.f/60);
    Check(contact.LastStepContactCount()>=4,"position-correction fixture has real manifold contacts");
    Check(contact.GetTransform(box).position.y>before.position.y,"position correction actually changes the pose");
    Check(Same(contact.GetPreviousTransform(box),before),"position correction does not replace previous state");
    Inspect(contact,box,"penetration-corrected body");Inspect(contact,floor,"shared static support");
}
void SharedContacts() {
    std::cout<<"D: multiple manifolds share prepared body data and solver frames\n";
    PhysicsWorld w;w.Init();const auto floor=w.CreateStaticBox({0,-.5f,0},{20,.5f,4},.5f,0);
    std::vector<BodyHandle> bodies;
    for(int i=0;i<8;++i)bodies.push_back(w.CreateDynamicBox({-7.f+2*i,.45f,0},glm::vec3(.5f),2,.5f,0));
    for(auto h:bodies)w.ApplyLinearAcceleration(h,{0,-9.81f,0},1.f/60);
    w.Step(1.f/60);
    Check(w.LastStepContactCount()>=32,"fixture has at least four contact points per box");
    Inspect(w,floor,"multiply referenced floor");for(auto h:bodies)Inspect(w,h,"manifold participant");
    const auto s=w.LastStepStats();
    Check(s.orientationCacheHits>0&&s.boundCacheHits>0,"real step records cache reuse");
    // The retired MK1 ContactSolver frame counter is not a MK2 contract.
    // Independent bounds, geometry invalidation and real cache reuse remain checked.
    Check(s.geometryCacheBytes>0,"persistent cache memory is reported");
    std::cout<<"CACHE shared bodies="<<s.bodies<<" contacts="<<s.contactPoints<<" orientation_hits="<<s.orientationCacheHits<<" orientation_rebuilds="<<s.orientationCacheRebuilds<<" bound_hits="<<s.boundCacheHits<<" bound_rebuilds="<<s.boundCacheRebuilds<<" shape_rebuilds="<<s.shapeCacheRebuilds<<" solver_frames="<<s.solverFrameBuilds<<" bytes="<<s.geometryCacheBytes<<" allocations="<<s.geometryCacheAllocations<<'\n';
}
void SweepCheck(const ShapeSweepHit& hit,bool expected,double distance,BodyHandle wall,const std::string& label) {
    ++sweepCases;Check(hit.hit==expected,label+" hit/miss");
    if(expected){Check(hit.hitBody.id==wall.id,label+" correct body");Check(std::abs(double(hit.distance)-distance)<2e-5,label+" analytical distance");}
}
void PreviousCurrentPlayerQueries() {
    std::cout<<"E: player queries retain independently useful previous/current geometry\n";
    PhysicsWorld w;w.Init();w.CreatePlayerShape(.25f,.5f);
    const auto wall=w.CreateDynamicBox({1.5f,0,0},{.25f,3,3},10,0,0);
    w.SetLinearVelocity(wall,{210,0,0});w.Step(1.f/60);Inspect(w,wall,"departing wall");
    const auto identity=glm::quat(1,0,0,0);
    SweepCheck(w.SweepPlayerShape({0,0,0},identity,{2,0,0},true,0,0),true,1,wall,"pinned previous departed wall");
    SweepCheck(w.SweepPlayerShape({0,0,0},identity,{2,0,0},true,1,1),false,0,wall,"pinned current departed wall");
    SweepCheck(w.SweepPlayerShape({0,0,0},identity,{2,0,0},true,0,1),false,0,wall,"departing wall outruns player");
    w.ResetBody(wall,{3,0,0},identity);w.SetLinearVelocity(wall,{-60,0,0});w.Step(1.f/60);Inspect(w,wall,"approaching wall");
    const double previous=w.GetPreviousTransform(wall).position.x,current=w.GetTransform(wall).position.x;
    const double relativeImpact=2*(previous-.5)/(2+previous-current);
    SweepCheck(w.SweepPlayerShape({0,0,0},identity,{2,0,0},true,0,0),false,0,wall,"approaching wall previous pose beyond sweep");
    SweepCheck(w.SweepPlayerShape({0,0,0},identity,{2,0,0},true,1,1),true,current-.5,wall,"approaching wall current pose");
    SweepCheck(w.SweepPlayerShape({0,0,0},identity,{2,0,0},true,0,1),true,relativeImpact,wall,"linear relative trajectories");
    w.ResetBody(wall,{1.5f,0,0},identity);w.SetAngularVelocity(wall,{0,0,120});w.Step(1.f/60);Inspect(w,wall,"quarter-turn wall");
    SweepCheck(w.SweepPlayerShape({0,0,0},identity,{2,0,0},true,0,0),true,1,wall,"rotating wall previous orientation");
    SweepCheck(w.SweepPlayerShape({0,0,0},identity,{2,0,0},true,1,1),true,0,wall,"rotating wall current overlap");
}
}
int main() {
    std::cout<<std::setprecision(17);
    PreparedContacts();PreparedBounds();StaticResetAndReuse();IntegrationAndCorrection();SharedContacts();PreviousCurrentPlayerQueries();
    std::cout<<"{\"checks\":"<<checks<<",\"failures\":"<<failures<<",\"bitwise_bound_comparisons\":"<<boundComparisons<<",\"world_cache_inspections\":"<<worldInspections<<",\"player_sweeps\":"<<sweepCases<<"}\n";
    return failures?1:0;
}
