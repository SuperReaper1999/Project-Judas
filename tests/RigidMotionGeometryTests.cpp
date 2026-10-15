#include "RigidMotion.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>

namespace {
// Independent pre-extraction implementation. Comparing geometry to Evaluate
// alone would let an accidental common change pass both sides of the check.
RigidBody OriginalEvaluate(const RigidMotionSegment& segment,double time) {
    if(time<segment.begin||time>segment.end)throw std::out_of_range("motion segment time");
    RigidBody result;result.inverseMass=segment.inverseMass;result.inverseInertiaLocal=segment.inverseInertiaLocal;
    result.position=segment.position;result.orientation=segment.orientation;
    result.linearVelocity=segment.linearVelocity;result.angularVelocity=segment.angularVelocity;
    if(segment.prescribed||!result.IsStatic()){
        const float elapsed=static_cast<float>(time-segment.begin);
        result.position+=result.linearVelocity*elapsed;
        const glm::quat omega(0.f,segment.driftAngularVelocity.x,segment.driftAngularVelocity.y,segment.driftAngularVelocity.z);
        const float speed=glm::length(segment.driftAngularVelocity);
        const glm::quat advanced=segment.prescribed
            ?(speed>0&&elapsed!=0?glm::angleAxis(speed*elapsed,segment.driftAngularVelocity/speed)*segment.orientation:segment.orientation)
            :segment.orientation+(omega*segment.orientation)*(.5f*elapsed);
        const auto same=[](float a,float b){return std::memcmp(&a,&b,sizeof(float))==0;};
        if(!same(advanced.w,segment.orientation.w)||!same(advanced.x,segment.orientation.x)||
           !same(advanced.y,segment.orientation.y)||!same(advanced.z,segment.orientation.z))result.orientation=glm::normalize(advanced);
        if(!segment.prescribed)result.angularVelocity=TransportAngularVelocity(segment.inverseInertiaLocal,segment.orientation,result.orientation,segment.angularVelocity);
    }
    return result;
}
bool Same(float a,float b){return std::memcmp(&a,&b,sizeof(float))==0;}
bool Same(const glm::vec3& a,const glm::vec3& b){return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
bool Same(const glm::quat& a,const glm::quat& b){return Same(a.w,b.w)&&Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
bool SameGeometry(const RigidMotionGeometry& geometry,const RigidBody& full){
    return Same(geometry.position,full.position)&&Same(geometry.orientation,full.orientation)&&Same(geometry.linearVelocity,full.linearVelocity);
}
bool SamePhysical(const RigidBody& a,const RigidBody& b){
    if(!Same(a.position,b.position)||!Same(a.orientation,b.orientation)||!Same(a.linearVelocity,b.linearVelocity)||!Same(a.angularVelocity,b.angularVelocity)||!Same(a.inverseMass,b.inverseMass))return false;
    for(unsigned column=0;column<3;++column)if(!Same(a.inverseInertiaLocal[column],b.inverseInertiaLocal[column]))return false;
    return true;
}
const RigidMotionSegment& OriginalRightSegment(const RigidMotion& motion,double time){
    const RigidMotionSegment* selected=nullptr;
    for(const auto& segment:motion.Segments())if(time>=segment.begin&&time<=segment.end)selected=&segment;
    if(!selected)throw std::out_of_range("motion ledger time");
    return *selected;
}
}
int main(){
    unsigned checks=0,transportedSamples=0;
    auto check=[&](bool pass,const char* reason){++checks;if(!pass)throw std::runtime_error(reason);};
    auto sample=[&](const RigidMotion& motion,double time){
        const auto& segment=OriginalRightSegment(motion,time);
        const auto oracle=OriginalEvaluate(segment,time);
        const auto geometry=motion.EvaluateGeometryAfterEvents(time);
        check(SameGeometry(geometry,oracle),"geometry differs from original motion arithmetic");
        check(SameGeometry(segment.EvaluateGeometry(time),oracle),"segment geometry differs from original arithmetic");
        check(SamePhysical(motion.EvaluateAfterEvents(time),oracle),"physical sampling changed angular transport or motion");
        if(!Same(oracle.angularVelocity,segment.angularVelocity))++transportedSamples;
    };
    std::mt19937_64 random(0x8720261016ULL);
    std::uniform_real_distribution<float> unit(-1,1),dimension(.15f,1.3f);
    for(unsigned index=0;index<480;++index){
        RigidBody body;body.inverseMass=index%3==1?.4f:0.f;
        body.position=glm::vec3(unit(random),unit(random),unit(random))*std::pow(10.f,float(index%8)*3);
        body.orientation=glm::normalize(glm::quat(unit(random),unit(random),unit(random),unit(random)));
        body.linearVelocity=glm::vec3(unit(random),unit(random),unit(random))*std::pow(10.f,float(index%5)-1);
        body.angularVelocity=glm::vec3(unit(random),unit(random),unit(random))*12.f;
        body.inverseInertiaLocal=body.IsStatic()?glm::mat3(0):SolidBoxInverseInertia(2.5f,{dimension(random),dimension(random),dimension(random)});
        const double duration=index%2==0?double(1.f/60):double(1.f/240);
        RigidMotion motion;
        if(index%3==2)motion.BeginPrescribed(index+1,body,duration,duration*.375);
        else motion.Begin(index+1,body,duration);
        for(const auto& segment:motion.Segments()){
            const auto endpoint=OriginalEvaluate(segment,segment.end);
            check(Same(segment.endPosition,endpoint.position)&&Same(segment.endOrientation,endpoint.orientation),"stored endpoint changed during geometry extraction");
            sample(motion,segment.begin);sample(motion,segment.end);
            for(unsigned step=1;step<17;++step)sample(motion,segment.begin+(segment.end-segment.begin)*double(step)/17.);
            if(segment.begin<segment.end){sample(motion,std::nextafter(segment.begin,segment.end));sample(motion,std::nextafter(segment.end,segment.begin));}
        }
    }
    check(transportedSamples>100,"tests did not exercise transported anisotropic angular velocity");
    RigidBody body;body.inverseMass=1;body.inverseInertiaLocal=SolidBoxInverseInertia(1,{.2f,.5f,.9f});
    body.orientation=glm::normalize(glm::quat(.6f,.3f,-.1f,.4f));body.linearVelocity={1,2,3};body.angularVelocity={.1f,.2f,.3f};
    RigidMotion events;events.Begin(91,body,1);
    body.linearVelocity={3,-4,5};events.ChangeVelocity(body,.5);
    body.linearVelocity={-2,8,9};events.ChangeVelocity(body,.5);
    body.linearVelocity={7,4,-2};events.ChangeVelocity(body,1);
    for(double time:{0.,std::nextafter(.5,0.),.5,std::nextafter(.5,1.),std::nextafter(1.,0.),1.})sample(events,time);
    check(events.EvaluateGeometryAfterEvents(.5).linearVelocity==glm::vec3(-2,8,9),"geometry chose left limit at repeated event time");
    check(events.Evaluate(.5).linearVelocity==glm::vec3(1,2,3),"historical left-limit query changed");
    check(events.EvaluateGeometryAfterEvents(1).linearVelocity==glm::vec3(7,4,-2),"geometry missed zero-duration final segment");
    RigidMotion instant;instant.Begin(92,body,0);body.linearVelocity={9,-7,4};instant.ChangeVelocity(body,0);sample(instant,0);
    check(instant.EvaluateGeometryAfterEvents(0).linearVelocity==body.linearVelocity,"geometry missed zero-duration replacement");
    // Frozen contact anchor with a normalization two-cycle: zero/sub-ULP
    // observations must preserve exactly the same represented quaternion.
    RigidBody anchor;anchor.inverseMass=1;
    anchor.orientation={.6061418652534485f,.25942757725715637f,.005194163415580988f,.751839280128479f};
    anchor.angularVelocity={-.9205920696258545f,-.5645646452903748f,.3782026171684265f};
    RigidMotion contact;contact.Begin(93,anchor,1.f/60);
    for(double time:{0.,5.014435e-19,3.388132e-21}){sample(contact,time);check(Same(contact.EvaluateGeometryAfterEvents(time).orientation,anchor.orientation),"sub-ULP geometry normalized contact anchor");}
    auto rejects=[&](const RigidMotion& motion,double time){
        bool rejected=false;try{(void)motion.EvaluateGeometryAfterEvents(time);}catch(const std::out_of_range& error){rejected=std::strcmp(error.what(),"motion ledger time")==0;}
        check(rejected,"geometry accepted invalid ledger time");
    };
    for(double time:{-1.,std::nextafter(0.,-1.),std::nextafter(1.,2.),2.,-std::numeric_limits<double>::infinity(),std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})rejects(events,time);
    RigidMotion empty;rejects(empty,0);rejects(empty,std::numeric_limits<double>::quiet_NaN());events.Clear();rejects(events,.5);
    std::cout<<"Rigid motion geometry checks="<<checks<<" transported_samples="<<transportedSamples<<" PASS\n";
}
