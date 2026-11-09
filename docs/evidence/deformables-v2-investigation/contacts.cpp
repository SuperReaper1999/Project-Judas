#include "Deformable.h"
#include "UniformGravity.h"
#include <iostream>
int main(){
 auto asset=std::make_shared<DeformableAsset>(MakeDeformableSheet(3,3,.4,.4));DeformableSettings cfg;cfg.selfContact=false;UniformGravity zero({0,0,0});
 for(bool capsule:{true,false}){PhysicsWorld p;p.Init();BodyTransform pose;pose.position={0,.2f,0};p.CreateShape(capsule?Shape::Capsule(.5f,.5f):Shape::Sphere(.7f),pose,false,1,.5f,0);
  DeformableInstance s;s.Initialize(asset,cfg,glm::dmat4(1),1);const auto initial=s.positions;s.Step(1./60,p,zero,{});double delta=0;for(unsigned i=0;i<s.positions.size();++i)delta=std::max(delta,glm::length(s.positions[i]-initial[i]));
  std::cout<<"STATIC_CONTACT kind="<<(capsule?"capsule":"sphere")<<" contacts="<<s.stats.contacts<<" displacement="<<delta<<" error="<<s.error<<'\n';}
 {PhysicsWorld p;p.Init();DeformableInstance s;s.Initialize(asset,cfg,glm::dmat4(1),2);for(unsigned i=0;i<90;++i)s.Step(1./60,p,zero,{});const auto before=s.positions;p.CreateStaticSphere({0,.2f,0},.7f,.5f,0);s.Step(1./60,p,zero,{});
  double delta=0;for(unsigned i=0;i<s.positions.size();++i)delta=std::max(delta,glm::length(s.positions[i]-before[i]));std::cout<<"INSERT_STATIC sleeping="<<s.sleeping<<" contacts="<<s.stats.contacts<<" displacement="<<delta<<'\n';
  s.Wake();s.Step(1./60,p,zero,{});std::cout<<"EXPLICIT_WAKE contacts="<<s.stats.contacts<<'\n';}
 {PhysicsWorld p;p.Init();auto carrier=p.CreateDynamicBox({0,0,0},{.1f,.1f,.1f},10,0,0);p.SetCollisionFilter(carrier,1,0);for(unsigned i=0;i<120;++i)p.Step(1.f/60);
  cfg.attachments={DeformableAttachment{}};cfg.attachments[0].kind=DeformableAttachment::Kind::Body;cfg.attachments[0].target=1;cfg.attachments[0].group="top";
  DeformableTarget target;target.valid=true;target.body=carrier;DeformableInstance s;s.Initialize(asset,cfg,glm::dmat4(1),3);
  for(unsigned i=0;i<120;++i){s.Step(1./60,p,zero,{target});p.Step(1.f/60);}
  std::cout<<"QUIET_DYNAMIC_ATTACHMENT body_sleeping="<<p.IsSleeping(carrier)<<" cloth_sleeping="<<s.sleeping<<" cloth_quiet_seconds="<<s.quietSeconds<<" body_speed="<<glm::length(p.GetLinearVelocity(carrier))<<'\n';}
}
