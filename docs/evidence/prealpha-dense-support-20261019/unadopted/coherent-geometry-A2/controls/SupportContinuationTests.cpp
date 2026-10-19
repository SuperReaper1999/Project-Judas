// Exercises the actual private support-continuation admission, not an oracle copy.
#include <bits/stdc++.h>
#define private public
#include "physics/WorldInternal.h"
#undef private
int main() {
    unsigned checks=0,failures=0;
    auto check=[&](bool ok,const char *name){++checks;if(!ok){++failures;std::cout<<"FAIL "<<name<<'\n';}};
    for(float translation:{0.f,10.f,200.f})for(unsigned mode=0;mode<3;++mode){
        PhysicsWorld::Impl world;world.bodies.resize(2);
        const float half=.3f,duration=.02f;
        for(unsigned i=0;i<2;++i){
            auto &b=world.bodies[i];b.shape=Shape::Box(glm::vec3(half));
            b.boundingRadius=ShapeBoundingRadius(b.shape);b.isDynamic=true;b.generation=1;
            b.rigidBody.inverseMass=1;b.rigidBody.inverseInertiaLocal=glm::mat3(1);
            b.rigidBody.position={translation+2*half*float(i),2,3};
            b.rigidBody.linearVelocity={2,0,0};
            if(i==1&&mode==1)b.rigidBody.linearVelocity.x-=1;
            if(i==1&&mode==2)b.rigidBody.linearVelocity.x+=1;
            b.motion.Begin(i,b.rigidBody,duration);
        }
        const bool allowed=world.SupportContinuationSafe({0,1,0,0},0,duration);
        if(mode==1)check(!allowed,"closing boxes exceeding unchanged penetration allowance must not be certified as safe support");
        else check(allowed,"co-moving or separating boxes retain safe support independent of absolute translation");
    }
    std::cout<<"support_continuation checks="<<checks<<" failures="<<failures<<'\n';return failures?1:0;
}
