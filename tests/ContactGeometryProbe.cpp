// FTFT4A actual-engine adapter. Expected signs/classifications come from the
// independent Python Fraction oracle over the exact binary32 inputs in fixtures.txt.
// This file never supplies a second contact, bound, or sweep implementation.
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include "Narrowphase.h"
#include "PhysicsWorld.h"

namespace {
struct Input { Shape shape; RigidBody body; };
bool Read(std::istream& in, Input& result) {
    std::string kind; glm::vec3 c,h; glm::quat q; float r; int count;
    if (!(in>>kind>>c.x>>c.y>>c.z>>q.w>>q.x>>q.y>>q.z>>r>>h.x>>h.y>>h.z>>count)) return false;
    std::vector<CompoundBox> children;
    for(int k=0;k<count;++k) { CompoundBox b; in>>b.localCenter.x>>b.localCenter.y>>b.localCenter.z>>b.halfExtents.x>>b.halfExtents.y>>b.halfExtents.z;children.push_back(b); }
    result.shape=kind=="sphere"?Shape::Sphere(r):kind=="box"?Shape::Box(h):Shape::Compound(children);
    result.body.position=c;result.body.orientation=q;return bool(in);
}
template<class V> void Vec(const V& v) {std::cout<<'['<<v.x<<','<<v.y<<','<<v.z<<']';}
BodyHandle Create(PhysicsWorld& w,const Input& a) {
    BodyHandle h;
    if(a.shape.type==ShapeType::Sphere)h=w.CreateDynamicSphere(a.body.position,a.shape.radius,1,0,0);
    else if(a.shape.type==ShapeType::Box)h=w.CreateDynamicBox(a.body.position,a.shape.halfExtents,1,0,0);
    else h=w.CreateDynamicCompoundBoxes(a.body.position,a.shape.boxes,1,0,0);
    w.ResetBody(h,a.body.position,a.body.orientation);return h;
}
bool Contains(const std::vector<BodyHandle>& a,BodyHandle h) {return std::any_of(a.begin(),a.end(),[&](BodyHandle x){return x.id==h.id;});}
void InputJson(const Input& a) {
    std::cout<<"{\"c\":";Vec(a.body.position);std::cout<<",\"q\":["<<a.body.orientation.w<<','<<a.body.orientation.x<<','<<a.body.orientation.y<<','<<a.body.orientation.z<<"],\"r\":"<<a.shape.radius<<",\"h\":";Vec(a.shape.halfExtents);
    std::cout<<",\"children\":[";for(std::size_t i=0;i<a.shape.boxes.size();++i){if(i)std::cout<<',';std::cout<<"{\"c\":";Vec(a.shape.boxes[i].localCenter);std::cout<<",\"h\":";Vec(a.shape.boxes[i].halfExtents);std::cout<<'}';}std::cout<<"]}";
}
int Pairs(const char* path) {
    std::ifstream file(path); if(!file) {std::cerr<<"Cannot read fixtures: "<<path<<'\n';return 2;}
    ResetContactGeometryDiagnostics();std::string line;std::size_t cases=0,failures=0;
    while(std::getline(file,line)) {
        if(line.empty())continue;
        std::istringstream in(line);std::string id;float margin;int worldExpected,pairCount;
        in>>id>>margin>>worldExpected>>pairCount;std::vector<int> expected(pairCount),signs(pairCount);for(auto& e:expected)in>>e;for(auto& e:signs)in>>e;
        Input a,b;if(!Read(in,a)||!Read(in,b)){std::cerr<<"Invalid fixture "<<id<<'\n';return 2;}
        PhysicsWorld world;world.Init();const BodyHandle ah=Create(world,a),bh=Create(world,b);
        const bool worldOverlap=!world.FindCollidingPairs().empty();if(worldOverlap!=bool(worldExpected))++failures;
        const Aabb ba=ShapeAabb(a.shape,a.body.position,a.body.orientation),bb=ShapeAabb(b.shape,b.body.position,b.body.orientation);
        const bool qa=Contains(world.QueryBodiesInAabb(bb.min,bb.max),ah),qb=Contains(world.QueryBodiesInAabb(ba.min,ba.max),bh);
        if(worldExpected&&(!qa||!qb))++failures;
        std::cout<<"{\"id\":\""<<id<<"\",\"inputs\":[";InputJson(a);std::cout<<',';InputJson(b);
        std::cout<<"],\"world_overlap\":"<<worldOverlap<<",\"query_candidates\":["<<qa<<','<<qb<<"],\"bounds\":[[";Vec(ba.min);std::cout<<',';Vec(ba.max);std::cout<<"],[";Vec(bb.min);std::cout<<',';Vec(bb.max);std::cout<<"]],\"pairs\":[";
        int pair=0;
        for(int i=0;i<PrimitiveCount(a.shape);++i)for(int j=0;j<PrimitiveCount(b.shape);++j,++pair) {
            if(pair)std::cout<<',';
            const auto m=ComputeContacts(PrimitiveAt(a.shape,a.body,i),PrimitiveAt(b.shape,b.body,j),margin);
            const int hits=static_cast<int>(std::count_if(m.points,m.points+m.count,[](const Contact& c){return c.hit;}));
            if(pair>=pairCount||bool(hits)!=bool(expected[pair])||m.uncertain)++failures;
            std::cout<<"{\"a\":"<<i<<",\"b\":"<<j<<",\"uncertain\":"<<m.uncertain<<",\"contacts\":[";
            int n=0;
            for(int k=0;k<m.count;++k) {const auto& c=m.points[k];if(!c.hit)continue;if(n++)std::cout<<',';
                if(!c.hasLocalAnchors||!std::isfinite(c.signedSeparation))++failures;
                // A penetrating/touching BOX PAIR can retain separated clipped points inside
                // the requested margin. Whole-pair SAT sign is not every point's sign.
                if(pair<pairCount&&signs[pair]>0&&!(c.signedSeparation>0))++failures;
                if(pair<pairCount&&signs[pair]==0&&
                   (a.shape.type==ShapeType::Sphere||b.shape.type==ShapeType::Sphere)&&c.signedSeparation!=0)++failures;
                const auto expectedState=c.signedSeparation>0?SeparationState::Separated:
                    c.signedSeparation<0?SeparationState::Penetrating:SeparationState::Touching;
                if(c.separationState!=expectedState)++failures;
                std::cout<<"{\"gap\":"<<c.signedSeparation<<",\"penetration\":"<<c.penetration<<",\"state\":"<<static_cast<int>(c.separationState)<<",\"has_anchors\":"<<c.hasLocalAnchors<<",\"normal\":";Vec(c.preciseNormal);
                std::cout<<",\"point\":";Vec(c.point);std::cout<<",\"anchor_a\":";Vec(c.localAnchorA);std::cout<<",\"anchor_b\":";Vec(c.localAnchorB);std::cout<<",\"witness_a\":";Vec(c.localWitnessA);std::cout<<",\"witness_b\":";Vec(c.localWitnessB);std::cout<<'}';
            }
            std::cout<<"]}";
        }
        if(pair!=pairCount)++failures;
        auto d=GetContactGeometryDiagnostics();std::cout<<"],\"diagnostics\":{\"predicates\":"<<d.predicates<<",\"interval_resolved\":"<<d.intervalResolved<<",\"exact_fallbacks\":"<<d.exactFallbacks<<",\"unresolved\":"<<d.unresolved<<",\"invalid_inputs\":"<<d.invalidInputs<<"}}\n";++cases;
    }
    std::cerr<<"Exact represented-input classification: "<<cases<<" cases, "<<failures<<" failures\n";return failures?1:0;
}

// Analytical face sweep: radius .25 + core half-height .5 => capsule reaches .75
// from centre. Start separation from plane is exactly1.25 at the dyadic fixtures.
// Rotation is an exact signed-axis permutation, avoiding an oracle from q*v.
int Invalid() {
    int failures=0,cases=0;
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const float inf=std::numeric_limits<float>::infinity();
    const std::vector<glm::quat> invalid{glm::quat(0,0,0,0),glm::quat(nan,0,0,0),glm::quat(1,inf,0,0)};
    for(std::size_t k=0;k<invalid.size();++k)for(int side:{0,1}) {
        RigidBody a,b;a.orientation=side==0?invalid[k]:glm::quat(1,0,0,0);b.orientation=side==1?invalid[k]:glm::quat(1,0,0,0);
        ResetContactGeometryDiagnostics();const auto m=ComputeContacts(Shape::Box(glm::vec3(.5f)),a,Shape::Box(glm::vec3(.5f)),b);
        const auto d=GetContactGeometryDiagnostics();const bool pass=m.uncertain&&m.count==0&&d.invalidInputs>0&&d.unresolved>0;
        if(!pass)++failures;
        std::cout<<"{\"case\":\"invalid_orientation\",\"kind\":"<<k<<",\"side\":"<<side<<",\"uncertain\":"<<m.uncertain<<",\"contacts\":"<<m.count<<",\"invalid_inputs\":"<<d.invalidInputs<<",\"unresolved\":"<<d.unresolved<<",\"pass\":"<<pass<<"}\n";++cases;
    }
    for(float magnitude:{std::numeric_limits<float>::max(),std::numeric_limits<float>::min()}) {
        // Finite nonzero q: same proper orientation as (1,1,1,1), hence identical
        // concentric unit boxes overlap. Range exhaustion must be explicit rather
        // than a known-separated verdict. Resolving this range later is also valid.
        RigidBody a,b;a.orientation=b.orientation=glm::quat(magnitude,magnitude,magnitude,magnitude);
        ResetContactGeometryDiagnostics();const auto m=ComputeContacts(Shape::Box(glm::vec3(.5f)),a,Shape::Box(glm::vec3(.5f)),b);const auto d=GetContactGeometryDiagnostics();
        const bool pass=d.invalidInputs==0&&(m.uncertain?(m.count==0&&d.unresolved>0):(m.count>0&&m.points[0].hit));
        if(!pass)++failures;
        std::cout<<"{\"case\":\"finite_quaternion_arithmetic_range\",\"magnitude\":"<<magnitude<<",\"analytical_overlap\":true,\"uncertain\":"<<m.uncertain<<",\"contacts\":"<<m.count<<",\"exact_fallbacks\":"<<d.exactFallbacks<<",\"unresolved\":"<<d.unresolved<<",\"explicit_outcome_pass\":"<<pass<<"}\n";++cases;
    }
    std::cerr<<"Invalid-input and arithmetic-range diagnostics: "<<cases<<" cases, "<<failures<<" failures\n";return failures?1:0;
}

int Player() {
    int cases=0,failures=0;
    for(float T:{0.f,137.f,1000.f})for(int axis:{0,1,2})for(int sign:{-1,1}) {
        PhysicsWorld w;w.Init();w.CreatePlayerShape(.25f,.5f);glm::vec3 n(0),c(T,-T,2*T);n[axis]=float(sign);
        glm::vec3 extent(4);extent[axis]=.5f;
        auto old=w.CreateStaticBox(c-n*.5f,extent,0,0);w.DestroyBody(old);
        w.CreateStaticBox(c+glm::vec3(100,100,100),glm::vec3(.25f),0,0);
        auto floor=w.CreateStaticBox(c-n*.5f,extent,0,0);
        glm::quat q(1,0,0,0);
        if(axis==0)q=glm::quat(.7071067811865475f,0,0,sign>0?-.7071067811865475f:.7071067811865475f);
        else if(axis==2)q=glm::quat(.7071067811865475f,sign>0?.7071067811865475f:-.7071067811865475f,0,0);
        else if(sign<0)q=glm::quat(0,1,0,0);
        const auto hit=w.SweepPlayerShape(c+n*2.f,q,-n*2.f);
        // Query path is still binary32/sampled: its allowance includes 32 float ulps
        // of translated coordinate arithmetic, not the stricter primitive anchor allowance.
        const double tol=32*std::numeric_limits<float>::epsilon()*std::max(1.,double(glm::length(c)));
        const bool valid=hit.hit&&hit.hitBody.id==floor.id&&std::abs(hit.distance-1.25)<=tol&&glm::dot(hit.normal,n)>.99999f;
        if(!valid)++failures;
        std::cout<<"{\"case\":\"axis_plane\",\"translation\":"<<T<<",\"axis\":"<<axis<<",\"sign\":"<<sign<<",\"hit\":"<<hit.hit<<",\"expected_distance\":1.25,\"observed_distance\":"<<hit.distance<<",\"tolerance\":"<<tol<<",\"pass\":"<<valid<<"}\n";++cases;
    }
    for(float T:{0.f,137.f,1000.f}) {
        PhysicsWorld w;w.Init();w.CreatePlayerShape(.25f,.5f);
        const glm::vec3 c(T,0,0);
        auto h=w.CreateDynamicCompoundBoxes(c,{{{-1,0,0},{.2f,2,2}},{{1,0,0},{.2f,2,2}}},1,0,0);
        const auto gap=w.SweepPlayerShape(c+glm::vec3(0,0,-3),glm::quat(1,0,0,0),glm::vec3(0,0,6));
        const auto rail=w.SweepPlayerShape(c+glm::vec3(.9f,0,-3),glm::quat(1,0,0,0),glm::vec3(0,0,2));
        const bool valid=!gap.hit&&rail.hit&&rail.hitBody.id==h.id&&std::abs(rail.distance-.75f)<1e-4f;
        if(!valid)++failures;
        std::cout<<"{\"case\":\"compound_opening\",\"translation\":"<<T<<",\"opening_hit\":"<<gap.hit<<",\"rail_hit\":"<<rail.hit<<",\"expected_distance\":0.75,\"observed_distance\":"<<rail.distance<<",\"pass\":"<<valid<<"}\n";++cases;
    }
    std::cerr<<"Independent analytical player sweeps: "<<cases<<" cases, "<<failures<<" failures\n";return failures?1:0;
}
}
int main(int argc,char** argv) {
    std::cout<<std::setprecision(17)<<std::boolalpha;
    if(argc>1&&std::string(argv[1])=="--player")return Player();
    if(argc>1&&std::string(argv[1])=="--invalid")return Invalid();
    return Pairs(argc>1?argv[1]:"tests/fixtures/contact/pairs.txt");
}
