// M91: interval enclosures and real persistent-contact/first-touch interaction.
#include "RigidMotionBounds.h"
#include "PhysicsWorld.h"
#include "PerformanceProfiler.h"
#include <cstdlib>
#include <cstdio>
#include <random>
#include <limits>
#include <string>

namespace {
unsigned checks=0,failures=0;
void Check(bool ok,const char* label){++checks;if(!ok){++failures;std::printf("FAIL %s\n",label);}}
constexpr double dt=1.0/60;
void Enclosures(){
    std::mt19937 random(910016);std::uniform_real_distribution<float> unit(-1,1);
    unsigned relativeFailures=0,shapeFailures=0,representedDeviations=0;
    for(unsigned index=0;index<384;++index){
        const glm::dvec3 axis=glm::normalize(glm::dvec3(unit(random),unit(random),unit(random)));
        RigidBody a,b;a.inverseMass=b.inverseMass=1;
        a.inverseInertiaLocal=b.inverseInertiaLocal=glm::mat3(1);
        const float distance=index%3==0?8000.f:index%3==1?1e6f:1.f;
        a.position=glm::vec3(unit(random),unit(random),unit(random))*distance;b.position=a.position+glm::vec3(.5f,1,.25f);
        a.orientation=glm::angleAxis(unit(random)*3,glm::normalize(glm::vec3(unit(random),unit(random),unit(random))));
        b.orientation=glm::angleAxis(unit(random)*3,glm::normalize(glm::vec3(unit(random),unit(random),unit(random))));
        a.linearVelocity=glm::vec3(unit(random),unit(random),unit(random))*80.f;
        b.linearVelocity=index%2?a.linearVelocity:a.linearVelocity+glm::vec3(.03f,-.02f,.01f);
        a.angularVelocity=glm::vec3(unit(random),unit(random),unit(random))*(index%5==0?300.f:.5f);
        b.angularVelocity=glm::vec3(unit(random),unit(random),unit(random))*.4f;
        RigidMotion ma,mb;ma.Begin(1,a,dt);mb.Begin(2,b,dt);
        if(index%4==0){const auto t=dt*.17;a=ma.EvaluateAfterEvents(t);a.linearVelocity+=glm::vec3(.04f,.03f,-.02f);ma.ChangeVelocity(a,t);}
        const double start=dt*.25;
        const auto fromA=ma.EvaluateAfterEvents(start),fromB=mb.EvaluateAfterEvents(start);
        const auto toA=ma.EvaluateAfterEvents(dt);
        const auto& sa=ma.Segments().back();const auto& sb=mb.Segments().back();
        const auto relative=rigid_motion_bounds::TryRelativeCentreProjection(&sa,&sb,fromA,fromB,start,dt,axis);
        const auto shape=index%2?Shape::Box({.4f,.2f,.3f}):Shape::Sphere(.3f);
        ContactPreparedOrientation orientation(fromA.orientation);
        const auto extent=rigid_motion_bounds::TryPrimitiveSweepProjection(shape,&sa,fromA,toA,start,dt,axis,glm::dvec3(fromB.position),orientation);
        Check(bool(relative)&&bool(extent),"finite represented primitive motions produce enclosures");
        if(!relative||!extent)continue;
        for(unsigned sample=0;sample<=256;++sample){
            const double t=start+(dt-start)*sample/256;
            const auto pa=ma.EvaluateGeometryAfterEvents(t),pb=mb.EvaluateGeometryAfterEvents(t);
            const double delta=glm::dot(glm::dvec3(pa.position)-glm::dvec3(pb.position),axis);
            relativeFailures+=delta<relative->lo||delta>relative->hi;
            const glm::dvec3 idealA=glm::dvec3(sa.position)+glm::dvec3(sa.linearVelocity)*(t-sa.begin);
            const glm::dvec3 idealB=glm::dvec3(sb.position)+glm::dvec3(sb.linearVelocity)*(t-sb.begin);
            representedDeviations+=glm::length((glm::dvec3(pa.position)-glm::dvec3(pb.position))-(idealA-idealB))>1e-9;
            const auto centre=glm::dvec3(pa.position)-glm::dvec3(fromB.position);
            if(shape.type==ShapeType::Sphere){const double p=glm::dot(centre,axis),radius=double(shape.radius)*glm::length(axis);shapeFailures+=p-radius<extent->lo||p+radius>extent->hi;}
            else for(unsigned corner=0;corner<8;++corner){
                const auto local=glm::dvec3(shape.halfExtents)*glm::dvec3(corner&1?1:-1,corner&2?1:-1,corner&4?1:-1);
                const double p=glm::dot(centre+ContactRotation(pa.orientation)*local,axis);
                shapeFailures+=p<extent->lo||p>extent->hi;
            }
        }
    }
    Check(relativeFailures==0,"relative-motion bounds contain represented positions across staggered anchors and large coordinates");
    Check(shapeFailures==0,"swept projection bounds contain all sampled box corners and sphere extrema under arbitrary orientation");
    Check(representedDeviations>100,"negative control: exact-real affine drift alone does not describe represented relative positions");
    std::printf("ENCLOSURES relative_failures=%u shape_failures=%u rounded_vs_ideal_deviations=%u\n",relativeFailures,shapeFailures,representedDeviations);
}
void Unsupported(){
    RigidBody b;b.inverseMass=1;b.inverseInertiaLocal=glm::mat3(1);b.angularVelocity={0,2,0};
    RigidMotion motion;motion.BeginPrescribed(1,b,dt,dt*.5);const auto from=motion.EvaluateAfterEvents(0),to=motion.EvaluateAfterEvents(dt);
    ContactPreparedOrientation q(from.orientation);auto shape=Shape::Box({.2f,.3f,.4f});
    Check(!rigid_motion_bounds::TryPrimitiveSweepProjection(shape,&motion.Segments().front(),from,to,0,dt,{1,0,0},{0,0,0},q),"prescribed stop/rotation discontinuity cannot receive a whole-interval certificate");
    motion.Begin(1,b,dt);shape.pivotOffset={.1f,0,0};
    Check(!rigid_motion_bounds::TryPrimitiveSweepProjection(shape,&motion.Segments().front(),from,to,0,dt,{1,0,0},{0,0,0},q),"unsupported displaced primitive retains ordinary CCD");
    Check(!rigid_motion_bounds::TryPrimitiveSweepProjection(Shape::Sphere(.2f),nullptr,from,from,std::numeric_limits<double>::quiet_NaN(),dt,{1,0,0},{0,0,0},q),"nonfinite interval cannot acquire a projection certificate");
    shape=Shape::Sphere(.2f);auto segment=motion.Segments().front();segment.linearVelocity.x=std::numeric_limits<float>::infinity();
    Check(!rigid_motion_bounds::TryPrimitiveSweepProjection(shape,&segment,from,to,0,dt,{1,0,0},{0,0,0},q),"nonfinite motion fails closed");
}
void ContactInteraction(bool rotated){
    const auto q=rotated?glm::angleAxis(.71f,glm::normalize(glm::vec3(1,2,-3))):glm::quat(1,0,0,0);
    PhysicsWorld world;world.Init();world.CreateStaticBox(q*glm::vec3(0,-.5f,0),q,{3,.5f,3},0,0);
    const auto a=world.CreateShape(Shape::Sphere(.25f),{q*glm::vec3(0,.25f,0),q},true,1,0,0);
    const auto b=world.CreateShape(Shape::Sphere(.25f),{q*glm::vec3(.505f,.25f,0),q},true,1,0,0);
    world.Step(float(dt));world.SetLinearVelocity(a,q*glm::vec3(.5f,.02f,0));
    auto& profiler=PerformanceProfiler::Get();profiler.Clear();profiler.Enable(true);
    {ProfileFrame frame("M91 actual impact on continuing support");world.Step(float(dt));}
    profiler.Enable(false);
    bool proved=false;for(const auto& frame:profiler.History())for(const auto& counter:frame.counters)
        if(counter.name=="Physics support continuation accepted")proved|=counter.value>0;
    const auto va=glm::inverse(q)*world.GetLinearVelocity(a),vb=glm::inverse(q)*world.GetLinearVelocity(b);
    const auto pa=glm::inverse(q)*world.GetTransform(a).position,pb=glm::inverse(q)*world.GetTransform(b).position;
    // MK1 resolves the arrival as a TOI event and revalidates the gapped
    // support by interval proof; MK2's speculative rows own both, so only the
    // physical outcomes below apply to it.
    const bool mk2=PhysicsWorld::Mk2Selected();
    Check((mk2||world.LastStepStats().impactEvents>0)&&vb.x>.1f,"new separated body receives actual collision impulse");
    Check(std::abs(va.x+vb.x-.5f)<2e-5f,"frictionless impact preserves tangential momentum against static support");
    Check(glm::dot(va,va)+glm::dot(vb,vb)<=.2504f+2e-5f,"contact continuation does not inject kinetic energy");
    Check(pa.y>=.2499f&&pb.y>=.2499f&&glm::length(pa-pb)>=.4999f,"actual supported and colliding geometry remains nonpenetrating in physical oracle");
    Check(mk2||proved,"real impact revalidates a gapped support using a complete interval proof");
    const auto d=profiler.Diagnostics();Check(!d.droppedEvents&&!d.truncatedFrames&&!d.openScopes,"continuation diagnostics fit bounded profiler");
    std::printf("INTERACTION rotated=%d events=%zu momentum=%.9g proved=%d\n",int(rotated),world.LastStepStats().impactEvents,va.x+vb.x,int(proved));
    profiler.Clear();
}
void ElasticSeparation(){
    PhysicsWorld world;world.Init();world.CreateStaticBox({0,-.5f,0},{3,.5f,3},0,0);
    const float delta=1.f/60;const float gap=3*(delta-.001f);
    const auto body=world.CreateDynamicSphere({0,.25f+gap,0},.25f,1,0,.75f);
    if(PhysicsWorld::Mk2Selected()){
        // The gap is sized for MK1, which applies the step's velocity change at
        // its start (50 mm of travel). MK2 integrates it over its substeps (31 mm;
        // exact 25 mm), so the first arrival comes a step later and inside it.
        // Restated on the bounces that actually occur. In a bounce step the
        // velocity changes only by gravity (-g dt) and the impact J = (1+e)u, so
        // the implied arrival speed u = J/(1+e) must lie between the speed
        // entering the step plus one substep of gravity and that plus the whole
        // step's gravity (an inelastic stop implies far less). The departure
        // never becomes support.
        const float e=.75f,g=180,substep=delta/4;
        float implied[2]={0,0},low[2]={0,0},high[2]={0,0};int bounces=0;bool above=true;
        for(int k=0;k<16&&bounces<2;++k){
            const float before=world.GetLinearVelocity(body).y;
            world.ApplyLinearAcceleration(body,{0,-g,0},delta);world.Step(delta);
            above=above&&world.GetTransform(body).position.y>=.24999f;
            const float after=world.GetLinearVelocity(body).y;
            if(before<=0&&after>0){implied[bounces]=(after-before+g*delta)/(1+e);low[bounces]=-before+g*substep;high[bounces]=-before+g*delta;++bounces;}
        }
        const auto authored=[&](int i){return bounces>i&&implied[i]>=low[i]-1e-3f&&implied[i]<=high[i]+1e-3f;};
        Check(authored(0),"first isolated impact retains authored restitution");
        Check(authored(1),"elastic departure does not become persistent support or suppress the next real bounce");
        Check(above,"short airborne return keeps actual geometry above floor");
        std::printf("ELASTIC mk2 bounces=%d arrival %.4f in [%.4f, %.4f], %.4f in [%.4f, %.4f]\n",bounces,implied[0],low[0],high[0],implied[1],low[1],high[1]);
        return;
    }
    world.ApplyLinearAcceleration(body,{0,-180,0},delta);world.Step(delta);
    const float first=world.GetLinearVelocity(body).y;
    world.ApplyLinearAcceleration(body,{0,-180,0},delta);world.Step(delta);
    const float second=world.GetLinearVelocity(body).y;
    Check(first>2.24f,"first isolated impact retains authored restitution");
    Check(second>.5f&&world.LastStepStats().impactEvents>0,"elastic departure does not become persistent support or suppress the next real bounce");
    Check(world.GetTransform(body).position.y>=.24999f,"short airborne return keeps actual geometry above floor");
    std::printf("ELASTIC first=%.9g second=%.9g second_events=%zu\n",first,second,world.LastStepStats().impactEvents);
}

}
int main(int argc,char** argv){if(argc==2&&std::string(argv[1])=="--elastic-only"){ElasticSeparation();return failures?1:0;}if(argc!=1)return 2;Enclosures();Unsupported();ContactInteraction(false);ContactInteraction(true);ElasticSeparation();std::printf("M91 continuity checks=%u failures=%u\n",checks,failures);return failures?1:0;}
