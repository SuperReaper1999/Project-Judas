#include "RigidMotionBounds.h"
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <random>
#include <limits>
using namespace rigid_motion_bounds;
int main(){
    std::mt19937_64 rng(0x8720261015ULL);
    std::uniform_real_distribution<float> unit(-1.f,1.f),positive(.02f,4.f);
    std::size_t checks=0,fail=0,accepted=0,rejected=0;
    auto check=[&](bool pass,const char* what){++checks;if(!pass){++fail;if(fail<10)std::cerr<<what<<'\n';}};
    for(unsigned test=0;test<2400;++test){
        const int exponent=int(test%9)*3;
        const float scale=std::pow(10.f,float(exponent));
        RigidBody body;body.inverseMass=.5f;body.inverseInertiaLocal=glm::mat3(1.f);
        body.position=scale*glm::vec3(unit(rng),unit(rng),unit(rng));
        body.linearVelocity=std::pow(10.f,float(int(test%7)-2))*glm::vec3(unit(rng),unit(rng),unit(rng));
        body.orientation=glm::normalize(glm::quat(unit(rng),unit(rng),unit(rng),unit(rng)));
        const bool sphere=test%3==0;
        if(sphere)body.angularVelocity=10.f*glm::vec3(unit(rng),unit(rng),unit(rng));
        const auto shape=sphere?Shape::Sphere(positive(rng)):Shape::Box(glm::vec3(positive(rng),positive(rng),positive(rng)));
        RigidMotion motion;const double duration=.001+double(test%71)*.004;
        motion.Begin(test+1,body,duration);
        const double start=duration*double(test%11)/12.,finish=duration;
        const auto from=motion.EvaluateAfterEvents(start),to=motion.EvaluateAfterEvents(finish);
        const auto tight=TryTranslationSweepBound(shape,motion.Segments().back(),from,to,start,finish);
        if(!tight){++rejected;continue;}++accepted;
        const auto radial=Shape::Sphere(ShapeBoundingRadius(shape));
        const auto fallback=ShapeAabb(radial,from.position,glm::quat(1,0,0,0)).Union(ShapeAabb(radial,to.position,glm::quat(1,0,0,0)));
        const auto intersection=IntersectConservativeBounds(fallback,*tight);
        for(int sample=0;sample<=32;++sample){
            const double time=sample==32?finish:start+(finish-start)*double(sample)/32.;
            const auto pose=motion.EvaluateAfterEvents(time);
            const auto actual=ShapeAabb(shape,pose.position,pose.orientation);
            check(tight->Contains(actual),"tight bound misses existing outward interior bound");
            check(intersection.Contains(actual),"intersection misses existing outward interior bound");
        }
    }
    RigidBody rotating;rotating.inverseMass=1;rotating.inverseInertiaLocal=glm::mat3(1);rotating.angularVelocity={0,30,0};
    RigidMotion spin;spin.Begin(1,rotating,.016);
    auto shape=Shape::Box({.3f,.3f,.3f});
    check(!TryTranslationSweepBound(shape,spin.Segments().back(),spin.EvaluateAfterEvents(0),spin.EvaluateAfterEvents(.016),0,.016),"nonzero spin accepted as fixed orientation");
    RigidBody stopped;RigidMotion stop;stop.BeginPrescribed(1,stopped,.016,.008);
    check(!TryTranslationSweepBound(shape,stop.Segments().front(),stop.EvaluateAfterEvents(0),stop.EvaluateAfterEvents(.016),0,.016),"cross-segment interval accepted");
    RigidBody first;first.inverseMass=1;first.inverseInertiaLocal=glm::mat3(1);RigidBody second=first;second.position.x=.74f;
    RigidMotion a,b;a.Begin(1,first,.016);b.Begin(2,second,.016);
    const auto aa=TryTranslationSweepBound(shape,a.Segments().back(),first,first,0,.016),bb=TryTranslationSweepBound(shape,b.Segments().back(),second,second,0,.016);
    check(aa&&bb&&!aa->Overlaps(*bb),"separated neighbour-box rejection absent");
    const auto radius=Shape::Sphere(ShapeBoundingRadius(shape));
    check(ShapeAabb(radius,first.position,first.orientation).Overlaps(ShapeAabb(radius,second.position,second.orientation)),"negative control should overlap old sphere bounds");
    std::size_t rotatingAccepted=0,rotatingFallback=0;
    for(unsigned i=0;i<2400;++i){
        RigidBody body;body.inverseMass=1;body.inverseInertiaLocal=glm::mat3(1);
        body.orientation=glm::normalize(glm::quat(unit(rng),unit(rng),unit(rng),unit(rng)));
        body.position=glm::vec3(unit(rng),unit(rng),unit(rng))*std::pow(10.f,float(i%9)*3);
        body.linearVelocity=glm::vec3(unit(rng),unit(rng),unit(rng))*std::pow(10.f,float(i%7)-2);
        body.angularVelocity=glm::vec3(unit(rng),unit(rng),unit(rng))*std::pow(10.f,float(i%8)-3);
        const double duration=.016;RigidMotion motion;motion.Begin(i+1,body,duration);
        const double start=duration*double(i%16)/17.;
        const auto from=motion.EvaluateAfterEvents(start),to=motion.EvaluateAfterEvents(duration);
        const auto rotatingShape=Shape::Box(glm::vec3(positive(rng),positive(rng),positive(rng)));
        const auto bound=TryDynamicBoxSweepBound(rotatingShape,motion.Segments().back(),from,to,start,duration);
        if(!bound){++rotatingFallback;continue;}++rotatingAccepted;
        const auto radial=Shape::Sphere(ShapeBoundingRadius(rotatingShape));
        const auto sphereBound=ShapeAabb(radial,from.position,glm::quat(1,0,0,0)).Union(ShapeAabb(radial,to.position,glm::quat(1,0,0,0)));
        const auto intersection=IntersectConservativeBounds(sphereBound,*bound);
        for(unsigned sample=0;sample<=64;++sample){
            const double time=sample==64?duration:start+(duration-start)*double(sample)/64.;
            const auto pose=motion.EvaluateAfterEvents(time);
            const auto actual=ShapeAabb(rotatingShape,pose.position,pose.orientation);
            check(bound->Contains(actual),"dynamic rotation bound misses outward interior bound");
            check(intersection.Contains(actual),"dynamic rotation intersection misses outward interior bound");
        }
    }
    check(!TryDynamicBoxSweepBound(shape,stop.Segments().front(),stop.EvaluateAfterEvents(0),stop.EvaluateAfterEvents(.016),0,.016),"dynamic bound accepted prescribed segment");
    auto offset=shape;offset.pivotOffset={.1f,0,0};
    check(!TryDynamicBoxSweepBound(offset,a.Segments().back(),first,first,0,.016),"dynamic bound accepted unsupported pivot offset");
    auto invalid=a.Segments().back();invalid.orientation={0,0,0,0};
    check(!TryDynamicBoxSweepBound(shape,invalid,first,first,0,.016),"dynamic bound accepted zero quaternion");
    auto overflow=a.Segments().back();overflow.driftAngularVelocity={1e30f,1e30f,1e30f};
    check(!TryDynamicBoxSweepBound(shape,overflow,first,first,0,.016),"dynamic bound accepted overflowing normalized quaternion");
    std::cout<<"rotating accepted="<<rotatingAccepted<<" fallback="<<rotatingFallback<<'\n';
    std::cout<<"checks="<<checks<<" failures="<<fail<<" accepted="<<accepted<<" rejected="<<rejected<<'\n';
    return fail?1:0;
}
