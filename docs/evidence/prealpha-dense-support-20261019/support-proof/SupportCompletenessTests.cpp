#include <bits/stdc++.h>
#define private public
#include "WorldInternal.h"
#undef private

struct Sample { bool support; double maxDepth; double endDepth; std::size_t partitions; };
Sample Run(bool translating,bool midViolation,bool closing){
    PhysicsWorld physics;physics.Init();auto& world=*physics.m_impl;world.bodies.resize(2);
    constexpr double dt=1.0/60;
    for(unsigned side=0;side<2;++side){
        auto& body=world.bodies[side];body.alive=body.enabled=body.isDynamic=true;body.generation=0;
        body.shape=Shape::Box(side==0&&midViolation?glm::vec3(.5f,.1f,.5f):glm::vec3(.5f));
        body.boundingRadius=ShapeBoundingRadius(body.shape);
        body.rigidBody.position={float(side),0,0};body.rigidBody.orientation={1,0,0,0};
        body.rigidBody.inverseMass=1;body.rigidBody.inverseInertiaLocal=glm::mat3(1);
        body.rigidBody.linearVelocity=translating?glm::vec3(13.173f,-7.19f,3.27f):glm::vec3(0);
        if(side==1&&closing)body.rigidBody.linearVelocity.x-=1;
        body.rigidBody.angularVelocity=midViolation
            ?(side==0?glm::vec3(0,0,120):glm::vec3(0)):glm::vec3(side==0?10.f:-10.f,0,0);
        RigidMotionSegment segment;segment.owner=side;segment.begin=0;segment.end=dt;
        segment.position=body.rigidBody.position;segment.orientation=body.rigidBody.orientation;
        segment.inverseMass=1;segment.inverseInertiaLocal=glm::mat3(1);
        segment.linearVelocity=body.rigidBody.linearVelocity;
        segment.angularVelocity=segment.driftAngularVelocity=body.rigidBody.angularVelocity;
        const auto end=segment.Evaluate(dt);segment.endPosition=end.position;segment.endOrientation=end.orientation;
        body.motion.SetRecorded({segment});
    }
    world.aliveSlots={0,1};world.stepDuration=dt;world.impactQueryAnchorTime=0;world.work.active=true;
    const std::array<unsigned,4> key{0,1,0,0};
    const bool supported=world.SupportContinuationSafe(key,0,dt);
    double maximum=0,endDepth=0;
    for(unsigned sample=0;sample<=512;++sample){const double time=dt*double(sample)/512;
        const auto m=world.PairAt(0,1,0,0,time,0);
        for(int row=0;row<m.count;++row){const double d=-m.points[row].signedSeparation;maximum=std::max(maximum,d);if(sample==512)endDepth=std::max(endDepth,d);}
    }
    return {supported,maximum,endDepth,world.work.continuationPartitions};
}
int main(int argc,char**argv){
    const bool originalExpected=argc>1&&std::string(argv[1])=="--expect-original-failure";
    unsigned checks=0,failures=0;
    auto check=[&](bool good,const char*name){++checks;if(!good){++failures;std::cout<<"FAIL "<<name<<'\n';}};
    for(bool moving:{false,true}){
        const auto result=Run(moving,false,false);
        std::cout<<std::setprecision(17)<<"rotation_about_normal moving="<<moving<<" accepted="<<result.support<<" partitions="<<result.partitions<<" max_depth="<<result.maxDepth<<'\n';
        check(originalExpected?!result.support:result.support,"time-varying co-moving rotation is certified with unchanged allowance and depth3");
        check(result.maxDepth<ContactSolver::kPenetrationSlop,"positive fixture has no actual over-allowance penetration");
        check(originalExpected||result.partitions>1,"positive fixture exercises real interval subdivision");
    }
    for(bool middle:{false,true}){
        const auto result=Run(false,middle,!middle);
        std::cout<<"over_allowance middle="<<middle<<" accepted="<<result.support<<" partitions="<<result.partitions<<" max_depth="<<result.maxDepth<<" end_depth="<<result.endDepth<<'\n';
        check(!result.support,"actual over5mm penetration must remain rejected");
        check(result.maxDepth>ContactSolver::kPenetrationSlop,"negative fixture actually exceeds unchanged allowance");
        if(middle)check(result.endDepth==0,"mid-interval failure is not replaced by endpoint-only acceptance");
    }
    std::cout<<"support completeness checks="<<checks<<" failures="<<failures<<'\n';return failures?1:0;
}
