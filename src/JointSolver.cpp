#include "JointSolver.h"
#include <algorithm>
#include <limits>
void JointSolver::Apply(Row& r,float p){r.a->linearVelocity+=r.linearA*(p*r.a->inverseMass);r.b->linearVelocity+=r.linearB*(p*r.b->inverseMass);
 r.a->angularVelocity+=r.inertiaA*r.angularA*p;r.b->angularVelocity+=r.inertiaB*r.angularB*p;}
void JointSolver::Prepare(const std::vector<JointInput>& inputs,float dt,bool warm,bool impact){
 m_rows.clear();m_bodies.clear();const float inf=std::numeric_limits<float>::infinity();
 for(auto& input:inputs){auto& s=input.state->settings;auto& a=*input.a;auto& b=*input.b;if(!impact){input.state->motorImpulse=0;input.state->reactionImpulse={0,0,0};input.state->reactionAngularImpulse={0,0,0};}
  for(auto* body:{&a,&b})if(std::find(m_bodies.begin(),m_bodies.end(),body)==m_bodies.end())m_bodies.push_back(body);
  glm::quat qa=glm::normalize(a.orientation),qb=glm::normalize(b.orientation),fa=qa*glm::normalize(s.frameA),fb=qb*glm::normalize(s.frameB);
  auto basis=glm::mat3_cast(fa);auto ra=qa*(s.anchorA-input.pivotOffsetA),rb=qb*(s.anchorB-input.pivotOffsetB),delta=(b.position-a.position)+rb-ra;
  auto ia=a.InverseInertiaWorld(),ib=b.InverseInertiaWorld();int index=0;
  auto row=[&](glm::vec3 la,glm::vec3 lb,glm::vec3 aa,glm::vec3 ab,float error,float low=-std::numeric_limits<float>::infinity(),float high=std::numeric_limits<float>::infinity(),bool soft=false,bool motor=false,bool resistance=false){
   float gamma=0,bias=impact?0:.2f*error/dt;
   if(soft){float denominator=dt*(s.damping+dt*s.stiffness);if(denominator<=0)return;gamma=1/denominator;bias=error*dt*s.stiffness*gamma;}
   if(resistance){gamma=1/(dt*s.rotationalResistance);bias=0;}
   if(motor)bias=-s.speed;
   float k=a.inverseMass*glm::dot(la,la)+b.inverseMass*glm::dot(lb,lb)+glm::dot(aa,ia*aa)+glm::dot(ab,ib*ab);
   if(k+gamma<=0)return;
   float* cache=impact?nullptr:&(*input.warm)[index];float lambda=warm&&cache?std::clamp(*cache,low,high):0;
   m_rows.push_back({&a,&b,la,lb,aa,ab,ia,ib,1/(k+gamma),bias,gamma,low,high,lambda,cache,motor?&input.state->motorImpulse:nullptr,input.state});++index;
   if(lambda!=0){Apply(m_rows.back(),lambda);input.state->reactionImpulse+=la*lambda;input.state->reactionAngularImpulse+=aa*lambda;}
  };
  auto linear=[&](glm::vec3 n,float error,float low=-std::numeric_limits<float>::infinity(),float high=std::numeric_limits<float>::infinity(),bool soft=false,bool motor=false){row(-n,n,-glm::cross(ra,n)+glm::cross(n,delta),glm::cross(rb,n),error,low,high,soft,motor);};
  auto angular=[&](glm::vec3 n,float error,float low=-std::numeric_limits<float>::infinity(),float high=std::numeric_limits<float>::infinity(),bool soft=false,bool motor=false){row({0,0,0},{0,0,0},-n,n,error,low,high,soft,motor);};
  if(s.type==JointType::Slider){
   // Perpendicular rows measured at B's anchor give body A a lever arm |delta| that
   // grows with travel; Gauss-Seidel against the orientation rows then converges too
   // slowly and long-travel sliders diverged. Adding the (bilateral) orientation rows,
   // weighted by c = n x delta, is an equivalent constraint set that moves that lever
   // arm onto the body that resists rotation most (all of it onto a static/world anchor).
   auto fq=glm::normalize(fb*glm::inverse(fa));if(fq.w<0)fq=-fq;glm::vec3 fv(fq.x,fq.y,fq.z);float fl=glm::length(fv);
   auto orientationError=fl>1e-7f?fv*(2*std::atan2(fl,fq.w)/fl):2.f*fv;
   float ta=ia[0][0]+ia[1][1]+ia[2][2],tb=ib[0][0]+ib[1][1]+ib[2][2],share=ta+tb>0?ta/(ta+tb):.5f;
   for(int k=1;k<3;++k){auto n=basis[k];auto c=glm::cross(n,delta);
    row(-n,n,-glm::cross(ra,n)+(1-share)*c,glm::cross(rb,n)+share*c,glm::dot(delta,n)+share*glm::dot(c,orientationError));}}
  else for(int k=0;k<3;++k)linear(basis[k],glm::dot(delta,basis[k]));
  if(s.type==JointType::Fixed||s.type==JointType::Slider){auto q=glm::normalize(fb*glm::inverse(fa));if(q.w<0)q=-q;glm::vec3 v(q.x,q.y,q.z);float length=glm::length(v);auto error=length>1e-7f?v*(2*std::atan2(length,q.w)/length):2.f*v;
   for(int k=0;k<3;++k)angular(basis[k],glm::dot(error,basis[k]));}
  if(s.type==JointType::Hinge){auto axisB=fb*glm::vec3(1,0,0);auto error=glm::cross(basis[0],axisB);
   for(int k=1;k<3;++k)angular(basis[k],glm::dot(error,basis[k]));}
  if(s.type==JointType::Hinge||s.type==JointType::Slider){
   float coordinate=s.type==JointType::Slider?glm::dot(delta,basis[0]):std::atan2(glm::dot(basis[0],glm::cross(basis[1],fb*glm::vec3(0,1,0))),glm::dot(basis[1],fb*glm::vec3(0,1,0)));
   input.state->coordinate=coordinate;
   auto axisRow=[&](float error,float low,float high,bool soft=false,bool motor=false){if(s.type==JointType::Slider)linear(basis[0],error,low,high,soft,motor);else angular(basis[0],error,low,high,soft,motor);};
   // Predict limit crossing from current velocity, so limits act before overshoot.
   float velocity=s.type==JointType::Slider?glm::dot(b.linearVelocity+glm::cross(b.angularVelocity,rb)-a.linearVelocity-glm::cross(a.angularVelocity,ra),basis[0]):glm::dot(b.angularVelocity-a.angularVelocity,basis[0]);
   index=7;if(!impact&&s.spring)axisRow(coordinate-s.rest,-inf,inf,true);
   index=8;if(!impact&&s.motor)axisRow(0,-s.maxForce*dt,s.maxForce*dt,false,true);
   index=6;
   // A violated limit is corrected through the 0.2/dt velocity bias; uncapped, that bias
   // becomes rebound velocity proportional to the violation (e.g. a limit enabled past its
   // range). Cap the correction speed (m/s for sliders, rad/s for hinges).
   const float maxCorrection=.5f*dt/.2f;
   if(s.limits){if(coordinate<=s.lower||(!impact&&coordinate+velocity*dt<s.lower))axisRow(coordinate<s.lower?std::max(coordinate-s.lower,-maxCorrection):5*(coordinate-s.lower),0,inf);
    else if(coordinate>=s.upper||(!impact&&coordinate+velocity*dt>s.upper))axisRow(coordinate>s.upper?std::min(coordinate-s.upper,maxCorrection):5*(coordinate-s.upper),-inf,0);}

  }
  const auto& drive=input.state->angularDrive;
  if(!impact&&drive.enabled&&drive.maxTorque>0&&(drive.stiffness>0||drive.damping>0)){
   // Relative quaternion logarithm. Equal/opposite angular rows are internal
   // effort; neither COM nor an owner transform receives an external servo.
   auto error=glm::normalize((qb*glm::inverse(qa*drive.target)));
   if(error.w<0)error=-error;
   glm::vec3 vector(error.x,error.y,error.z);float length=glm::length(vector);
   auto rotationError=length>1e-7f?vector*(2*std::atan2(length,error.w)/length):2.f*vector;
   const float denominator=dt*(drive.damping+dt*drive.stiffness);
   const float gamma=1/denominator;
   const int axes=s.type==JointType::Hinge?1:3;
   const float cap=drive.maxTorque*dt/std::sqrt(float(axes));
   for(int k=0;k<axes;++k){auto n=basis[k];float stiffnessBias=(glm::dot(rotationError,n)*dt*drive.stiffness-glm::dot(drive.relativeAngularVelocity,n)*dt*drive.damping)*gamma;
    float effective=glm::dot(n,(ia+ib)*n);if(effective+gamma<=0)continue;
    auto* cache=&input.state->angularDrive.impulses[k];float lambda=warm?std::clamp(*cache,-cap,cap):0;
    m_rows.push_back({&a,&b,{0,0,0},{0,0,0},-n,n,ia,ib,1/(effective+gamma),stiffnessBias,gamma,-cap,cap,lambda,cache,nullptr,input.state});
    if(lambda!=0){Apply(m_rows.back(),lambda);input.state->reactionAngularImpulse-=n*lambda;}
   }
  }
  // Passive viscous joint resistance is solved implicitly with contacts and
  // anchors. It opposes relative angular motion, never drives a rest pose.
  if(!impact&&s.rotationalResistance>0&&(s.type==JointType::Ball||s.type==JointType::Hinge)){
   index=9;const int axes=s.type==JointType::Ball?3:1;
   for(int k=0;k<axes;++k)row({0,0,0},{0,0,0},-basis[k],basis[k],0,-inf,inf,false,false,true);
  }
 }
}
void JointSolver::SolveIteration(){for(auto& r:m_rows){float velocity=glm::dot(r.linearA,r.a->linearVelocity)+glm::dot(r.linearB,r.b->linearVelocity)+glm::dot(r.angularA,r.a->angularVelocity)+glm::dot(r.angularB,r.b->angularVelocity);
 float total=std::clamp(r.impulse-(velocity+r.bias+r.gamma*r.impulse)*r.mass,r.low,r.high),change=total-r.impulse;r.impulse=total;Apply(r,change);r.observation->reactionImpulse+=r.linearA*change;r.observation->reactionAngularImpulse+=r.angularA*change;if(r.cache)*r.cache=total;if(r.motor)*r.motor=total;}}
void JointSolver::ResetImpulses(){for(auto& r:m_rows)r.impulse=0;}
