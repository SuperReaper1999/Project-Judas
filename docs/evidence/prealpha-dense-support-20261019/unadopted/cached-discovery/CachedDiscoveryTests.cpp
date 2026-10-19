#include <bits/stdc++.h>
#define private public
#include "physics/WorldInternal.h"
#undef private
#ifndef JUDAS_EVENT_SOURCE
#define JUDAS_EVENT_SOURCE "src/physics/WorldImpactEvents.cpp"
#endif
#include JUDAS_EVENT_SOURCE
int main(){
 using Body=PhysicsWorld::Impl::Body;using Sample=PhysicsWorld::Impl::ImpactPoseSample;
 Body a,b;a.shape=Shape::Box(glm::vec3(.5f));b.shape=Shape::Sphere(.5f);
 a.boundingRadius=b.boundingRadius=1;a.generation=7;b.generation=9;
 Sample ca,cb;ca.boundStart=cb.boundStart=.1;ca.boundFinish=cb.boundFinish=.2;
 ca.boundGeneration=a.generation;cb.boundGeneration=b.generation;
 ca.remainingBound={glm::vec3(-2),glm::vec3(-1)};cb.remainingBound={glm::vec3(1),glm::vec3(2)};
 unsigned checks=0,failures=0;
 auto check=[&](bool ok,const char *name){++checks;if(!ok){++failures;std::cout<<"FAIL "<<name<<'\n';}};
 auto reject=[&](double time=.15){return CachedRemainingEnvelopesAreDisjoint(a,&ca,b,&cb,time);};
 check(reject(),"valid separated cached trajectory envelopes prove no contact");
 check(reject(.1)&&reject(.2),"both closed cached interval endpoints are covered");
 check(!reject(.099)&&!reject(.201),"outside cached time interval retains original contact path");
 check(!CachedRemainingEnvelopesAreDisjoint(a,static_cast<const Sample*>(nullptr),b,&cb,.15),"missing cache retains original path");
 ++a.generation;check(!reject(),"reused slot generation cannot reuse historical enclosure");--a.generation;
 const double start=ca.boundStart;ca.boundStart=std::numeric_limits<double>::quiet_NaN();
 check(!reject(),"ChangeVelocity trajectory invalidation defeats cached proof");ca.boundStart=start;
 const auto oldBound=ca.remainingBound;ca.remainingBound.max.x=std::numeric_limits<float>::infinity();
 check(!reject(),"nonfinite bound cannot prove separation");ca.remainingBound=oldBound;
 ca.remainingBound.min.x=std::numeric_limits<float>::quiet_NaN();
 check(!reject(),"NaN cannot turn failed overlap arithmetic into a separation proof");ca.remainingBound=oldBound;
 ca.remainingBound.max=glm::vec3(2);check(!reject(),"touching or overlapping envelopes require actual geometry");ca.remainingBound=oldBound;
 const auto oldShape=a.shape;a.shape=Shape::Capsule(.5f,.5f);check(!reject(),"unsupported capsule remains on original path");a.shape=oldShape;
 a.shape.pivotOffset={.01f,0,0};check(!reject(),"offset primitive remains on original proof domain");a.shape.pivotOffset=glm::vec3(0);
 a.boundingRadius=0;check(!reject(),"invalid radius cannot certify remaining envelope");a.boundingRadius=1;
 std::cout<<"cached_discovery checks="<<checks<<" failures="<<failures<<'\n';return failures?1:0;
}
