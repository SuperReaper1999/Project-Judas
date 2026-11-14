// Original procedural geometry. A separation-plane query is deliberately not
// an alternative collision predicate or contact-response implementation.
#include "Contacts.h"
#include "ContactPreparedGeometry.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace {
unsigned checks = 0, failures = 0, positive = 0, negative = 0;
void Check(bool okay, const char* label) {
    ++checks;
    if (!okay) { ++failures; std::printf("FAIL %s\n", label); }
}
struct V { long double x, y, z; };
V operator+(V a, V b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
V operator-(V a, V b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
V operator*(V a, long double s) { return {a.x*s,a.y*s,a.z*s}; }
long double Dot(V a, V b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
V Vector(glm::vec3 v) { return {v.x,v.y,v.z}; }
// Independent scalar R(q)/dot(q,q), rather than the production prepared frame.
V Column(glm::quat q, unsigned column) {
    const long double w=q.w,x=q.x,y=q.y,z=q.z,d=w*w+x*x+y*y+z*z;
    if (column==0) return {(w*w+x*x-y*y-z*z)/d,2*(x*y+w*z)/d,2*(x*z-w*y)/d};
    if (column==1) return {2*(x*y-w*z)/d,(w*w-x*x+y*y-z*z)/d,2*(y*z+w*x)/d};
    return {2*(x*z+w*y)/d,2*(y*z-w*x)/d,(w*w-x*x-y*y+z*z)/d};
}
V Center(const ContactPose& pose) {
    return Vector(pose.position)+Column(pose.orientation,0)*pose.localCenter.x+
           Column(pose.orientation,1)*pose.localCenter.y+Column(pose.orientation,2)*pose.localCenter.z;
}
long double SupportRadius(const Shape& box, const ContactPose& pose, V normal) {
    return std::abs(Dot(Column(pose.orientation,0),normal))*box.halfExtents.x+
           std::abs(Dot(Column(pose.orientation,1),normal))*box.halfExtents.y+
           std::abs(Dot(Column(pose.orientation,2),normal))*box.halfExtents.z;
}
PrimitiveSeparation Verify(const Shape& a, const ContactPose& pa, const Shape& b,
                           const ContactPose& pb, float margin=0, bool facePreference=false) {
    const auto result=PrimitiveBoxSeparation(a,pa,b,pb,margin,nullptr,nullptr,facePreference);
    Check(result.supported, "boxes use the supported separation query");
    const auto manifold=PrimitiveContacts(a,pa,b,pb,margin,nullptr,nullptr,facePreference);
    if (!result.uncertain) {
        if (!result.withinMargin) {
            ++negative;
            Check(!manifold.count&&!manifold.uncertain,
                  "a certified beyond-margin separation rejects the response candidate");
        } else {
            ++positive;
            Check(manifold.uncertain||manifold.count>0,
                  "SAT candidate classification agrees with full construction when resolved");
            const V normal{result.normal.x,result.normal.y,result.normal.z};
            Check(std::abs(Dot(normal,normal)-1)<1e-12L,"selected plane normal is a unit direction");
            const auto oracle=Dot(Center(pa)-Center(pb),normal)-
                              SupportRadius(a,pa,normal)-SupportRadius(b,pb,normal);
            const auto scale=std::max(1.L,std::abs(oracle)+SupportRadius(a,pa,normal)+SupportRadius(b,pb,normal));
            Check(std::abs(oracle-result.gap)<scale*1e-10L,
                  "reported gap matches independently projected support planes");
        }
    }
    return result;
}
ContactPose Pose(glm::vec3 position={0,0,0}) { return {position,{1,0,0,0},{0,0,0}}; }
void Boundaries() {
    const auto box=Shape::Box({.5f,.5f,.5f});
    const float above=std::nextafter(1.f,std::numeric_limits<float>::infinity());
    const float below=std::nextafter(1.f,-std::numeric_limits<float>::infinity());
    Check(!Verify(box,Pose(),box,Pose({above,0,0})).withinMargin,"one-ULP positive gap is separated");
    Check(Verify(box,Pose(),box,Pose({1,0,0})).gap==0,"exact tangent retains zero gap");
    Check(Verify(box,Pose(),box,Pose({below,0,0})).gap<0,"one-ULP overlap retains negative gap");
    const auto near=Verify(box,Pose(),box,Pose({1.015625f,0,0}),.03125f);
    Check(near.withinMargin&&near.gap>0,"separated proximity exposes a positive support gap");
    Check(!Verify(box,Pose(),box,Pose({1.0625f,0,0}),.03125f).withinMargin,
          "a margin is not an arbitrary proximity acceptance");
    Verify(box,Pose(),box,Pose());
    Verify(box,Pose(),box,Pose({1,1,1}));
    for (float origin : {0.f,8192.f,1048576.f}) {
        auto first=Pose({origin,0,0}),second=Pose({origin,0,0});
        first.localCenter={-.125f,.25f,0};second.localCenter={.890625f,.25f,0};
        const auto shifted=Verify(box,first,box,second,.03125f);
        Check(shifted.withinMargin&&shifted.gap==.015625,
              "parent-relative child offsets retain a small gap at large origins");
    }
}
uint32_t randomState=0x5e93c271u;
uint32_t Random() {
    randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;
    return randomState;
}
float Between(float low,float high) { return low+(high-low)*float(Random()>>8)*(1.f/16777216.f); }
glm::vec3 RandomVector(float low,float high) { return {Between(low,high),Between(low,high),Between(low,high)}; }
glm::quat Rotation() {
    auto axis=RandomVector(-1,1);if(glm::dot(axis,axis)<.01f)axis={1,.3f,-.2f};
    return glm::angleAxis(Between(-3.1f,3.1f),glm::normalize(axis));
}
void RotationsAndPreparation() {
    for (unsigned i=0;i<512;++i) {
        const auto a=Shape::Box(RandomVector(.02f,1)),b=Shape::Box(RandomVector(.02f,1));
        auto pa=Pose(RandomVector(-1.5f,1.5f)),pb=Pose(RandomVector(-1.5f,1.5f));
        pa.orientation=Rotation();pb.orientation=Rotation();
        pa.localCenter=RandomVector(-.3f,.3f);pb.localCenter=RandomVector(-.3f,.3f);
        // Non-unit represented quaternions must retain the same geometry.
        if (i%3==0) {pa.orientation*=.25f;pb.orientation*=4.f;}
        const auto margin=i%2?0.f:.75f;
        const auto result=Verify(a,pa,b,pb,margin,i%2);
        const auto reverse=Verify(b,pb,a,pa,margin,i%2);
        Check(result.uncertain==reverse.uncertain&&result.withinMargin==reverse.withinMargin,
              "argument reversal retains candidate and uncertainty classification");
        const ContactPreparedOrientation qa(pa.orientation),qb(pb.orientation);
        const auto prepared=PrimitiveBoxSeparation(a,pa,b,pb,margin,&qa,&qb,i%2);
        Check(result.supported==prepared.supported&&result.uncertain==prepared.uncertain&&
              result.withinMargin==prepared.withinMargin&&result.gap==prepared.gap&&result.normal==prepared.normal,
              "matching prepared frames retain exact query output");
    }
    const auto slender=Shape::Box({.000125f,4,.00025f});
    for(float angle:{0.f,1e-7f,1e-5f,.2f,1.5707963f}) {
        auto first=Pose(),second=Pose({.0002f,0,.0003f});
        second.orientation=glm::angleAxis(angle,glm::normalize(glm::vec3(.2f,.7f,.3f)));
        Verify(slender,first,slender,second,.01f);
    }
}
void InvalidAndUnsupported() {
    const auto box=Shape::Box({.5f,.5f,.5f});
    Check(!PrimitiveBoxSeparation(Shape::Sphere(.5f),Pose(),box,Pose()).supported,
          "unsupported primitives are not silently approximated by boxes");
    auto invalid=Pose();invalid.orientation={0,0,0,0};
    Check(PrimitiveBoxSeparation(box,invalid,box,Pose()).uncertain,"zero orientation is uncertain");
    invalid=Pose();invalid.localCenter.x=std::numeric_limits<float>::infinity();
    Check(PrimitiveBoxSeparation(box,invalid,box,Pose()).uncertain,"nonfinite offset is uncertain");
    Check(PrimitiveBoxSeparation(Shape::Box({-.5f,.5f,.5f}),Pose(),box,Pose()).uncertain,
          "invalid extents do not produce a separating certificate");
    Check(PrimitiveBoxSeparation(box,Pose(),box,Pose(),-.1f).uncertain,
          "negative advancement margin is rejected explicitly");
    Check(PrimitiveBoxSeparation(box,Pose(),box,Pose(),std::numeric_limits<float>::quiet_NaN()).uncertain,
          "nonfinite advancement margin is uncertain");
    const ContactPreparedOrientation wrong(glm::angleAxis(.7f,glm::vec3(0,1,0)));
    const auto plain=PrimitiveBoxSeparation(box,Pose(),box,Pose({1.01f,0,0}),.02f);
    const auto stale=PrimitiveBoxSeparation(box,Pose(),box,Pose({1.01f,0,0}),.02f,&wrong,&wrong);
    Check(plain.withinMargin==stale.withinMargin&&plain.gap==stale.gap&&plain.normal==stale.normal,
          "mismatching prepared frames are recomputed");
}
}
int main() {
    Boundaries();RotationsAndPreparation();InvalidAndUnsupported();
    Check(positive>100&&negative>100,"both SAT candidates and beyond-margin certificates were exercised");
    std::printf("Contact separation: checks=%u failures=%u positive=%u negative=%u\n",
                checks,failures,positive,negative);
    return failures?1:0;
}
