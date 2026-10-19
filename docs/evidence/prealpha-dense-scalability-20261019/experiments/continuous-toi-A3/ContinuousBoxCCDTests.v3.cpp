#include "ContinuousBoxCCD.v3.h"
#include <cstdio>
#include <string>
using namespace continuous_box_ccd;
namespace {
unsigned checks=0,failures=0;
bool endpointOnly=false;
void Check(bool value,const char *name) {
    ++checks;if(!value){++failures;std::printf("FAIL %s\n",name);}
}
RigidMotionSegment Segment(glm::vec3 p,glm::vec3 v={},glm::vec3 omega={}) {
    RigidMotionSegment s;s.inverseMass=1;s.position=p;s.linearVelocity=v;
    s.driftAngularVelocity=omega;s.begin=0;s.end=2;return s;
}
MotionView View(const RigidMotionSegment &s,glm::vec3 half=glm::vec3(.5f)) {
    return {&s,nullptr,half};
}
PrefixResult Prefix(const MotionView&a,const MotionView&b,double from,double to) {
    if(endpointOnly){const auto end=Point(a,b,to);return {end.supported,end.strictlySeparated,1,1,0};}
    return SeparatedPrefix(a,b,from,to);
}
void TranslationControls() {
    auto a=Segment({0,0,0}),b=Segment({2,0,0},{-4,0,0});
    const auto av=View(a),bv=View(b);
    const auto first=Point(av,bv,0);
    Check(first.strictlySeparated,"moving boxes start separated");
    const auto advance=SafeAdvance(av,bv,0,1,first);
    Check(advance&&*advance>.249999&&*advance<=.25,
          "gap/speed advancement cannot jump across analytic first contact");
    Check(!Point(av,bv,.5).strictlySeparated,"real future overlap is never separated");
    Check(!Prefix(av,bv,0,1).separated,"future collision cannot be discarded by separated endpoints");
    Check(Prefix(av,bv,0,.2).separated,"strictly separated approaching prefix is certified");
    Check(!Prefix(av,bv,0,.25).separated,"closed prefix ending at exact contact is not strictly separated");
    b.position={-2,1.0001f,0};b.linearVelocity={4,0,0};
    Check(Prefix(av,bv,0,1).separated,"thin but genuine near miss remains separated throughout");
    b.position={std::nextafter(1.f,2.f),0,0};
    a.linearVelocity=b.linearVelocity={1e20f,-1e19f,7e18f};
    const double expected=double(b.position.x)-1;
    const auto point=Point(av,bv,1);
    Check(point.strictlySeparated&&point.gapLower>0&&point.gapLower<=expected,
          "large identical drift preserves one-binary32-ulp gap without inventing distance");
    Check(Prefix(av,bv,0,1).separated,"co-moving boxes retain whole-interval separation");
    const auto still=SafeAdvance(av,bv,1,2,point);
    Check(still&&*still==2,"zero relative motion proves the whole remaining interval safe");
    a=Segment({0,0,0},{2,0,0});b=Segment({3.25f,0,0},{2,0,0});b.begin=1;
    Check(Prefix(View(a),View(b),1,2).separated,"different anchors on same drift retain relative frame");
}
void RotatingControls() {
    auto a=Segment({0,0,0}),b=Segment({1,0,0},{},{0,8,0});
    b.orientation={1,0,1,0}; // Exact 90-degree orientation without trig rounding.
    const auto av=View(a,glm::vec3(.15f)),bv=View(b,{1,.1f,.1f});
    Check(Point(av,bv,0).strictlySeparated,"rotating thin bar starts separated");
    Check(Point(av,bv,1).strictlySeparated,"rotating thin bar ends separated");
    Check(!Point(av,bv,.25).strictlySeparated,"intermediate Cayley rotation hits the box");
    Check(!Prefix(av,bv,0,1).separated,"nonlinear return cannot be cleared by endpoint tests");
    const auto advance=SafeAdvance(av,bv,0,1,Point(av,bv,0));
    Check(advance&&*advance>0&&*advance<.25,
          "angular corner speed keeps fallback advance before the actual rotating collision");
    a=Segment({0,0,0},{2,-4,1});b=Segment({1,2,-2},{2,-4,1});
    a.orientation=b.orientation={1,1,1,0};
    const auto result=Point(View(a),View(b),.75);
    Check(result.strictlySeparated&&result.gapLower>1.99&&result.gapLower<=2.0000001,
          "non-axis-aligned rational rotation has the expected physical gap");
    Check(Prefix(View(a),View(b),0,1).separated,"arbitrary rotated frames do not assume world-up");
}
void FallbackControls() {
    auto a=Segment({0,0,0}),b=Segment({2,0,0});
    Check(Point(View(a),View(b),0).supported,"ordinary simple box motion is supported");
    b.prescribed=true;
    Check(!Point(View(a),View(b),0).supported,"prescribed exponential rotation retains original query");
    Check(!Prefix(View(a),View(b),0,1).supported,"unsupported whole intervals do not supply rejection");
    b.prescribed=false;b.orientation={0,0,0,0};
    Check(!Point(View(a),View(b),0).supported,"zero quaternion is unsupported");
    b=Segment({2,0,0});b.begin=.25;
    Check(!Point(View(a),View(b),0).supported,"no query may escape the recorded segment");
    Check(!Prefix(View(a),View(b),.5,3).supported,"cross-segment prefixes retain the original search");
    Check(!Point(View(a),View(b,{0,1,1}),.5).supported,"invalid physical half extents retain fallback");
    b=Segment({2,0,0});b.position.x=std::numeric_limits<float>::infinity();
    Check(!Point(View(a),View(b),0).supported,"nonfinite sources retain fallback");
    RigidBody fixed;fixed.position={2,0,0};
    Check(Point(View(a),{nullptr,&fixed,glm::vec3(.5f)},1).strictlySeparated,
          "ordinary static rigid body shares the same separation mechanism");
    const int original=std::fegetround();
    for(int rounding:{FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO})if(std::fesetround(rounding)==0) {
        Check(!Point(View(a),{nullptr,&fixed,glm::vec3(.5f)},1).supported,
              "unsupported arithmetic environment retains native path");
        std::fesetround(original);
    }
    Check(std::fegetround()==original,"caller floating environment is restored by test");
    b=Segment({2,0,0});a.end=b.end=std::numeric_limits<double>::max();
    Check(!Point(View(a),View(b),1e300).supported,"huge finite time cannot overflow a certified point");
    Check(!Prefix(View(a),View(b),0,1e300).supported,"finite but unsupported polynomial range retains fallback");
}
}
int main(int argc,char**argv) {
    endpointOnly=argc==2&&std::string(argv[1])=="--endpoint-only";
    if(argc>2||(argc==2&&!endpointOnly))return 2;
    TranslationControls();RotatingControls();FallbackControls();
    std::printf("Continuous box controls: checks=%u failures=%u endpoint_only_mutation=%d\n",checks,failures,endpointOnly);
    return failures?1:0;
}
