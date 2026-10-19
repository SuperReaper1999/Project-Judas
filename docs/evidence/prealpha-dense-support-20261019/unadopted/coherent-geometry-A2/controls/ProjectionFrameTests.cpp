#include "RigidMotionBounds.h"
#include "ContactSolver.h"
#include <iostream>
int main(int argc,char **argv){
    const bool doubleCenter=argc>1&&std::string(argv[1])=="--double-center-mutation";
    unsigned checks=0,failures=0;
    auto check=[&](bool ok,const char *s){++checks;if(!ok){++failures;std::cout<<"FAIL "<<s<<'\n';}};
    const Shape box=Shape::Box(glm::vec3(.3f));const double duration=.02;
    for(double base:{0.,10.,200.}){
        RigidBody a,b;a.inverseMass=b.inverseMass=1;
        a.inverseInertiaLocal=b.inverseInertiaLocal=glm::mat3(1);
        a.position={float(base),2,3};b.position=a.position+glm::vec3(.6f,0,0);
        a.linearVelocity={2,0,0};b.linearVelocity={1,0,0};
        RigidMotion ma,mb;ma.Begin(0,a,duration);mb.Begin(1,b,duration);
        const auto &sa=ma.Segments()[0],&sb=mb.Segments()[0];
        const auto pa=sa.EvaluateGeometry(0),pb=sb.EvaluateGeometry(0);
        ContactPreparedOrientation qa(pa.orientation),qb(pb.orientation);
        const glm::dvec3 normal(-1,0,0);
        const auto radialA=rigid_motion_bounds::TryPrimitiveRotationProjection(box,&sa,0,duration,normal,qa);
        const auto radialB=rigid_motion_bounds::TryPrimitiveRotationProjection(box,&sb,0,duration,normal,qb);
        const auto fullA=rigid_motion_bounds::TryPrimitiveSweepProjection(box,&sa,pa,sa.EvaluateGeometry(duration),0,duration,normal,glm::dvec3(0),qa);
        const auto fullB=rigid_motion_bounds::TryPrimitiveSweepProjection(box,&sb,pb,sb.EvaluateGeometry(duration),0,duration,normal,glm::dvec3(0),qb);
        const auto center=rigid_motion_bounds::TryRelativeCentreProjection(&sa,&sb,pa,pb,0,duration,normal);
        check(radialA&&radialB&&fullA&&fullB&&center,"rotation/full/relative proof domains are all exercised");
        if(!radialA||!radialB||!fullA||!fullB||!center)continue;
        check(radialA->lo==radialB->lo&&radialA->hi==radialB->hi,"pure rotational support projection is independent of both COM and linear velocity");
        const auto first=doubleCenter?*fullA:*radialA,second=doubleCenter?*fullB:*radialB;
        const auto gap=*center+contact_geometry::Iv(first.lo)-contact_geometry::Iv(second.hi)+contact_geometry::Iv(double(ContactSolver::kPenetrationSlop));
        check(!(gap.lo>0),"converging trajectory is not falsely certified by counting its center twice");
        for(unsigned sample=0;sample<=1000;++sample){
            const double time=duration*sample/1000;
            const auto g=sa.EvaluateGeometry(time);const auto rotation=ContactRotation(g.orientation);
            for(unsigned corner=0;corner<8;++corner){
                const glm::dvec3 local=glm::dvec3(box.halfExtents)*glm::dvec3(corner&1?1:-1,corner&2?1:-1,corner&4?1:-1);
                const double radius=glm::dot(normal,rotation*local),world=glm::dot(normal,g.position+rotation*local);
                check(radius>=radialA->lo&&radius<=radialA->hi,"rotation proof contains actual vertex offsets");
                check(world>=fullA->lo&&world<=fullA->hi,"full proof contains moving world vertices");
            }
        }
    }
    std::cout<<"projection_frames checks="<<checks<<" failures="<<failures<<'\n';return failures?1:0;
}
