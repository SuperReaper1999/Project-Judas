#include "ContactSolver.h"
#include "RigidBody.h"
#include <cstdio>
#include <string>

int main(int argc,char** argv) {
    const bool negative=argc==2&&std::string(argv[1])=="--point-only";
    unsigned checks=0,failures=0;
    const auto check=[&](bool value,const char* label){++checks;if(!value){++failures;std::printf("FAIL %s\n",label);}};
    for(bool block:{false,true}){
        RigidBody body,floor;
        body.position={0,.5f,0};body.inverseMass=1;
        body.inverseInertiaLocal=SolidBoxInverseInertia(1,{.5f,.5f,.5f});
        body.linearVelocity={0,-1,0};
        ContactSolver solver;solver.EnableBlockNormals(block&&!negative);
        for(unsigned i=0;i<4;++i){
            Contact c;c.hit=true;c.hasLocalAnchors=true;c.normal=c.preciseNormal={0,1,0};
            c.localAnchorA={i&1?.5:-.5,-.5,i&2?.5:-.5};
            c.localAnchorB={c.localAnchorA.x,0,c.localAnchorA.z};
            solver.AddContact(body,floor,c,0,0);
        }
        solver.Prepare();solver.SolveVelocities(1);
        const double spin=glm::length(body.angularVelocity),speed=glm::length(body.linearVelocity);
        std::printf("mode=%s normal_speed=%.12g spin=%.12g\n",block?"block":"point",speed,spin);
        if(block){
            check(speed<1e-6,"symmetric vertical impact stops without residual translation");
            check(spin<1e-6,"symmetric vertical impact does not manufacture rocking");
            for(const auto& c:solver.Constraints())
                check(std::abs(c.normalImpulse-.25f)<1e-6,"four corners share a symmetric reaction");

        }
    }
    {
        RigidBody body,floor;body.inverseMass=1;body.position={0,.5f,0};
        body.linearVelocity={1,-1,0};body.inverseInertiaLocal=glm::mat3(0);
        ContactSolver solver;solver.EnableBlockNormals(!negative);
        for(unsigned i=0;i<4;++i){Contact c;c.hit=true;c.hasLocalAnchors=true;
            c.normal=c.preciseNormal={0,1,0};c.localAnchorA={i&1?.5:-.5,-.5,i&2?.5:-.5};
            c.localAnchorB={c.localAnchorA.x,0,c.localAnchorA.z};solver.AddContact(body,floor,c,.6f,0);}
        solver.Prepare();solver.SolveVelocities();
        check(std::abs(body.linearVelocity.x-.4f)<2e-6,"Coulomb friction remains bounded by the total physical normal impulse");
        check(std::abs(body.linearVelocity.y)<1e-6,"rank-one locked rotation remains a valid normal problem");
        for(const auto& row:solver.Constraints())
            check(glm::length(row.tangentImpulse)<=row.friction*row.normalImpulse+1e-7f,"each point retains its Coulomb disc");
    }
    {
        RigidBody body, floor;
        body.position = {0, .5f, 0};
        body.inverseMass = 1;
        body.inverseInertiaLocal = SolidBoxInverseInertia(1, {.5f, .5f, .5f});
        body.linearVelocity = {0, -1, 0};
        ContactSolver solver;
        solver.EnableBlockNormals(true);
        const auto addPatch = [&] {
            for (unsigned i = 0; i < 4; ++i) {
                Contact contact;
                contact.hit = contact.hasLocalAnchors = true;
                contact.normal = contact.preciseNormal = {0, 1, 0};
                contact.localAnchorA = {i & 1 ? .5 : -.5, -.5, i & 2 ? .5 : -.5};
                contact.localAnchorB = {contact.localAnchorA.x, 0, contact.localAnchorA.z};
                solver.AddContact(body, floor, contact, 0, 0);
            }
        };
        addPatch();
        solver.PreservePointNormals(body);
        solver.Prepare();
        solver.SolveVelocities(1);
        check(glm::length(body.angularVelocity) > .1f,
              "joint-coupled rows explicitly preserve established point ordering");
        solver.Clear();
        body.linearVelocity = {0, -1, 0};
        body.angularVelocity = {0, 0, 0};
        addPatch();
        solver.Prepare();
        solver.SolveVelocities(1);
        check(glm::length(body.angularVelocity) < 1e-6f,
              "Clear discards derived coupled-row exclusions");
    }
    std::printf("physical patches: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
