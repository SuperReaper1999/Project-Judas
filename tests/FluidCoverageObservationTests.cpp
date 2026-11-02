#include "FluidCoverageObservation.h"
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <iomanip>
#include <stdexcept>

namespace {
using namespace FluidCoverageObservation;
unsigned checks=0,failures=0;
void Check(bool value,const char* label){++checks;if(!value){++failures;std::cout<<"FAIL "<<label<<'\n';}}
bool Contains(const Result& r,double exact){return r.lowerFraction<=exact+1e-11&&r.upperFraction>=exact-1e-11;}
// Transformed analytic references include input-construction roundoff. The
// dyadic identity-frame controls below additionally require strict enclosure.
void Bounds(const Result& r,double exact,const char* label) {
    Check(Contains(r,exact),label);
    Check(r.lowerFraction>=0&&r.upperFraction<=1&&r.lowerFraction<=r.upperFraction,"ordered finite fraction bounds");
    Check(std::isfinite(r.lowerFraction)&&std::isfinite(r.upperFraction),"finite reported fractions");
}
Box Move(Box b,glm::dquat q,glm::dvec3 translation) {
    b.center=q*b.center+translation;b.rotation=q*b.rotation;return b;
}
void Invalid(const Box& body,const std::vector<Box>& cells,Options options,const char* label) {
    bool rejected=false;try{(void)Measure(body,cells,options);}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,label);
}
}
int main() try {
    std::cout<<std::setprecision(17);
    const Box body;
    const Box lower{{0,-.5,0},{2,.5,2},{1,0,0,0}};
    const Box full{{0,0,0},{2,2,2},{1,0,0,0}};
    const Box remote{{5,0,0},{1,1,1},{1,0,0,0}};
    const auto dry=Measure(body,{}),filled=Measure(body,{full});
    Bounds(dry,0,"empty represented union is dry");
    Check(dry.lowerFraction==0&&dry.upperFraction==0,"empty model gives exact zero coverage");
    Bounds(filled,1,"covering cell fills entire body");
    Check(filled.unresolvedLeaves==0,"full containment requires no subdivision");
    Bounds(Measure(body,{remote}),0,"separated cell is dry");
    const auto half=Measure(body,{lower},{6,100000});
    Bounds(half,.5,"analytic half-space slab coverage enclosed");
    Check(half.lowerFraction<=.5&&half.upperFraction>=.5,"exact dyadic half is strictly enclosed");
    Check(half.upperFraction-half.lowerFraction<.08,"half slab bounds resolve below eight percentage points");
    for(const double fraction:{.3,.5,.7}) {
        const Box slab{{0,-1+fraction,0},{2,fraction,2},{1,0,0,0}};
        Bounds(Measure(body,{slab},{6,100000}),fraction,"30/50/70 percent slabs enclosed");
    }
    // Duplicate/overlapping cells are a union, never summed as separate fluid.
    const auto duplicate=Measure(body,{lower,lower},{6,100000});
    Bounds(duplicate,.5,"duplicate cells cannot double covered volume");
    Check(std::abs(duplicate.lowerFraction-half.lowerFraction)<1e-12&&
          std::abs(duplicate.upperFraction-half.upperFraction)<1e-12,"duplicates retain the same numerical bounds");
    const Box left{{-.75,0,0},{.25,2,2},{1,0,0,0}},right{{.75,0,0},{.25,2,2},{1,0,0,0}};
    const auto gap=Measure(body,{left,right},{6,100000});
    Bounds(gap,.5,"two disconnected side cells preserve central dry gap");
    Check(gap.lowerFraction<=.5&&gap.upperFraction>=.5,"exact dyadic gap union is strictly enclosed");
    Check(gap.upperFraction<.6,"central dry gap is not filled by an outer hull");
    const auto hullShortcut=Measure(body,{full});
    Check(!Contains(hullShortcut,.5),"known-answer gap fixture rejects a convex-hull fill shortcut");
    Check(!Contains(dry,.5)&&!Contains(filled,.5),"dry/full controls reject constant-half output");

    for(const double angle:{.37,1.137}) {
        const auto q=glm::angleAxis(angle,glm::normalize(glm::dvec3(1,2,-3)));
        const glm::dvec3 translation(13,-7,4);
        const auto shifted=Measure(Move(body,q,translation),{Move(lower,q,translation)},{6,100000});
        Bounds(shifted,.5,"common rotation/translation preserves analytic half geometry");
        Check(shifted.upperFraction-shifted.lowerFraction<.15,"oblique intervals remain useful at declared depth");
    }
    // A tilted wet OBB through a centrally symmetric body divides volume in two.
    Box angled=full;
    angled.rotation=glm::angleAxis(.63,glm::normalize(glm::dvec3(1,0,1)));
    const auto normal=angled.rotation*glm::dvec3(0,1,0);
    angled.half={4,2,4};angled.center=-normal*2.;
    const auto oblique=Measure(body,{angled},{7,100000});
    Bounds(oblique,.5,"independently tilted half-space has symmetric half volume");

    Result previous=Measure(body,{lower},{6,1});
    Bounds(previous,.5,"one-node budget returns unresolved enclosure");
    Check(previous.budgetExhausted&&previous.unresolvedLeaves==1,"budget exhaustion is explicit");
    for(const std::size_t budget:{9u,73u,585u,4681u,37449u}) {
        const auto next=Measure(body,{lower},{6,budget});Bounds(next,.5,"refined bounds retain analytic answer");
        Check(next.nodes<=budget,"strict node count budget respected");
        Check(next.lowerFraction>=previous.lowerFraction-1e-11&&
              next.upperFraction<=previous.upperFraction+1e-11,"larger BFS budgets give nested bounds up to roundoff");
        previous=next;
    }
    const auto shallow=Measure(body,{lower},{0,100});
    Check(shallow.depthExhausted&&!shallow.budgetExhausted,"depth exhaustion is separately reported");
    const auto replay=Measure(body,{lower},{6,4681});
    const auto replay2=Measure(body,{lower},{6,4681});
    Check(replay.lowerFraction==replay2.lowerFraction&&replay.upperFraction==replay2.upperFraction&&
          replay.nodes==replay2.nodes&&replay.cellTests==replay2.cellTests,"same input/budget gives identical diagnostic output");
    Invalid(body,{lower},{6,0},"zero node budget rejected");
    Invalid(body,{lower},{21,100},"unbounded depth rejected");
    Box bad=body;bad.half.x=0;Invalid(bad,{lower},{},"zero body dimension rejected");
    bad=body;bad.rotation={0,0,0,0};Invalid(bad,{lower},{},"zero quaternion rejected");
    bad=lower;bad.center.x=std::numeric_limits<double>::quiet_NaN();Invalid(body,{bad},{},"nonfinite cells rejected");
    bad=lower;bad.half.y=-1;Invalid(body,{bad},{},"negative cell dimensions rejected");
    std::cout<<"HALF lower="<<half.lowerFraction<<" upper="<<half.upperFraction<<" nodes="<<half.nodes<<" cell_tests="<<half.cellTests<<'\n';
    std::cout<<"OBLIQUE lower="<<oblique.lowerFraction<<" upper="<<oblique.upperFraction<<" nodes="<<oblique.nodes<<" cell_tests="<<oblique.cellTests<<'\n';
    std::cout<<"Represented coverage: "<<checks<<" checks, "<<failures<<" failures; continuum/ownership inference not tested\n";
    return failures?1:0;
} catch(const std::exception& error){std::cerr<<"Coverage diagnostic error: "<<error.what()<<'\n';return 2;}
