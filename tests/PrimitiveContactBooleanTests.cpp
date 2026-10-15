// M87: Boolean primitive queries must match the accepted manifold boundary,
// including exact touching, uncertainty and argument order. Axis-aligned cases
// additionally use an independent represented-input overlap oracle.
#include "Contacts.h"
#include "ContactPreparedGeometry.h"
#include <glm/gtc/quaternion.hpp>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>

namespace {
unsigned checks=0,failures=0,positive=0,negative=0;
uint32_t randomState=0x793f4ab1u;
void Check(bool condition,const std::string& label){
    ++checks;
    if(!condition){++failures;std::printf("FAIL %s\n",label.c_str());}
}
uint32_t Random(){
    randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;
    return randomState;
}
float Between(float low,float high){return low+(high-low)*float(Random()>>8)*(1.f/16777216.f);}
glm::vec3 Vector(float low,float high){return {Between(low,high),Between(low,high),Between(low,high)};}
glm::quat Rotation(){
    auto axis=Vector(-1,1);if(glm::dot(axis,axis)<.01f)axis={1,.3f,-.2f};
    return glm::angleAxis(Between(-3.1415926f,3.1415926f),glm::normalize(axis));
}
bool SameDiagnostics(const ContactGeometryDiagnostics& a,const ContactGeometryDiagnostics& b){
    return a.predicates==b.predicates&&a.intervalResolved==b.intervalResolved&&
        a.exactFallbacks==b.exactFallbacks&&a.unresolved==b.unresolved&&
        a.invalidInputs==b.invalidInputs&&a.numericGapFallbacks==b.numericGapFallbacks&&
        a.preparedOrientationMisses==b.preparedOrientationMisses;
}
bool Compare(const Shape& a,const ContactPose& pa,const Shape& b,const ContactPose& pb,
             float margin,const std::string& label,
             const ContactPreparedOrientation* preparedA=nullptr,
             const ContactPreparedOrientation* preparedB=nullptr){
    ResetContactGeometryDiagnostics();
    const auto manifold=PrimitiveContacts(a,pa,b,pb,margin,preparedA,preparedB);
    const auto fullDiagnostics=GetContactGeometryDiagnostics();
    ResetContactGeometryDiagnostics();
    const bool actual=PrimitiveHasContacts(a,pa,b,pb,margin,preparedA,preparedB);
    const auto booleanDiagnostics=GetContactGeometryDiagnostics();
    Check(actual==(manifold.count>0),label+" Boolean/manifold parity");
    Check(SameDiagnostics(fullDiagnostics,booleanDiagnostics),label+" exact predicate/failure diagnostics parity");
    actual?++positive:++negative;
    return actual;
}
ContactPose Pose(glm::vec3 position={0,0,0}){return {position,{1,0,0,0},{0,0,0}};}

void ExactBoundaries(){
    const auto box=Shape::Box({.5f,.5f,.5f}),sphere=Shape::Sphere(.5f);
    const float above=std::nextafter(1.f,std::numeric_limits<float>::infinity());
    const float below=std::nextafter(1.f,-std::numeric_limits<float>::infinity());
    for(const auto& shapes:std::array<std::pair<Shape,Shape>,4>{{{box,box},{sphere,sphere},{sphere,box},{box,sphere}}}){
        Check(Compare(shapes.first,Pose(),shapes.second,Pose({1,0,0}),0,"exact face tangent"),"zero gap is accepted");
        Check(!Compare(shapes.first,Pose(),shapes.second,Pose({above,0,0}),0,"one-ULP positive gap"),"one-ULP gap remains separated");
        Check(Compare(shapes.first,Pose(),shapes.second,Pose({below,0,0}),0,"one-ULP overlap"),"one-ULP overlap is accepted");
        Check(Compare(shapes.first,Pose(),shapes.second,Pose(),0,"deep coincident"),"deep overlap is accepted");
        Check(!Compare(shapes.first,Pose(),shapes.second,Pose({5,0,0}),0,"distant negative control"),"distant body has no contact");
    }
    Check(Compare(box,Pose(),box,Pose({1,1,0}),0,"exact edge tangent"),"edge tangent is accepted");
    Check(Compare(box,Pose(),box,Pose({1,1,1}),0,"exact corner tangent"),"corner tangent is accepted");
    Check(Compare(box,Pose(),box,Pose({1.015625f,0,0}),.03125f,"positive-margin proximity"),"ordinary proximity boundary remains supported");
    Check(!Compare(box,Pose(),box,Pose({1.0625f,0,0}),.03125f,"outside proximity margin"),"proximity does not accept arbitrary gaps");
}

void AxisAlignedOracle(){
    for(unsigned i=0;i<512;++i){
        const auto halfA=Vector(.015625f,1.5f),halfB=Vector(.015625f,1.5f);
        const auto pa=Pose(Vector(-3,3)),pb=Pose(Vector(-3,3));
        bool overlap=true;
        for(unsigned k=0;k<3;++k)overlap&=std::abs(double(pa.position[k])-double(pb.position[k]))<=double(halfA[k])+double(halfB[k]);
        const bool actual=Compare(Shape::Box(halfA),pa,Shape::Box(halfB),pb,0,"independent AABB oracle "+std::to_string(i));
        Check(actual==overlap,"independent represented-input AABB overlap "+std::to_string(i));
    }
}

void RotatedAndPrepared(){
    for(unsigned i=0;i<3072;++i){
        auto a=Shape::Box(Vector(.025f,1.3f)),b=Shape::Box(Vector(.025f,1.3f));
        if(i%7==0)a=Shape::Sphere(Between(.03f,1.2f));
        if(i%11==0)b=Shape::Sphere(Between(.03f,1.2f));
        ContactPose pa{Vector(-2,2),Rotation(),Vector(-.2f,.2f)};
        ContactPose pb{Vector(-2,2),Rotation(),Vector(-.2f,.2f)};
        if(i%8==0)pb.position=pa.position;
        if(i%13==0){pa.orientation*=1.003f;pb.orientation*=.997f;}
        if(i%17==0){pa.position+=glm::vec3(65536,-32768,131072);pb.position+=glm::vec3(65536,-32768,131072);}
        const float margin=i%5==0?.03125f:0;
        const auto label="rotated represented case "+std::to_string(i);
        const bool forward=Compare(a,pa,b,pb,margin,label);
        const bool reversed=Compare(b,pb,a,pa,margin,label+" reversed");
        const ContactPreparedOrientation preparedA(pa.orientation),preparedB(pb.orientation);
        Check(Compare(a,pa,b,pb,margin,label+" prepared",&preparedA,&preparedB)==forward,label+" prepared truth");
        Check(Compare(b,pb,a,pa,margin,label+" reverse prepared",&preparedB,&preparedA)==reversed,label+" reverse prepared truth");
    }
    const auto slender=Shape::Box({.000125f,4,.00025f});
    for(float angle:{0.f,1e-7f,1e-5f,.2f,1.5707963f}){
        auto a=Pose(),b=Pose({.0002f,0,.0003f});
        b.orientation=glm::angleAxis(angle,glm::normalize(glm::vec3(.2f,.7f,.3f)));
        Compare(slender,a,slender,b,0,"thin grazing rods");
        Compare(slender,b,slender,a,0,"thin grazing rods reversed");
    }
}

void FailureAndFallbackPaths(){
    const auto box=Shape::Box({.5f,.5f,.5f});
    auto valid=Pose(),invalid=Pose();invalid.orientation={0,0,0,0};
    Check(!Compare(box,valid,box,invalid,0,"zero quaternion"),"invalid orientation remains rejected");
    invalid=Pose();invalid.position.x=std::numeric_limits<float>::quiet_NaN();
    Check(!Compare(box,invalid,box,valid,0,"nonfinite position"),"invalid position remains rejected");
    invalid=Pose();invalid.localCenter.y=std::numeric_limits<float>::infinity();
    Check(!Compare(box,valid,box,invalid,0,"nonfinite child offset"),"invalid child offset remains rejected");
    const ContactPreparedOrientation wrong(glm::angleAxis(.75f,glm::vec3(0,0,1)));
    Check(Compare(box,valid,box,Pose({.25f,.1f,0}),0,"mismatching prepared orientation",&wrong,&wrong),"stale preparation is recomputed");
    Check(!Compare(Shape::Capsule(.3f,.8f),valid,box,valid,0,"unsupported primitive fallback"),"Boolean helper invents no unsupported primitive");
    Compare(box,valid,box,Pose({1.01f,0,0}),-.015625f,"negative margin existing semantics");
    for(float scale:{std::numeric_limits<float>::denorm_min(),1e-20f,1e20f}){
        auto first=Pose(),second=Pose({.25f,.1f,0});
        first.orientation*=scale;second.orientation*=scale;
        Compare(box,first,box,second,0,"extreme finite quaternion representation");
    }
}
}

int main(){
    ExactBoundaries();AxisAlignedOracle();RotatedAndPrepared();FailureAndFallbackPaths();
    Check(positive>100&&negative>100,"both positive and negative query populations were genuinely exercised");
    std::printf("Primitive Boolean contacts: checks=%u failures=%u positive=%u negative=%u\n",checks,failures,positive,negative);
    return failures?1:0;
}
