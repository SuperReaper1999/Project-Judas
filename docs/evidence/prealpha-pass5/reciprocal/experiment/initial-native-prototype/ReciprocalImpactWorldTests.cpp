#include "PhysicsWorld.h"
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

unsigned checks=0,failures=0;
void Check(bool ok,const char* label){++checks;if(!ok){++failures;std::printf("FAIL %s\n",label);}}
void Print(float f){unsigned bits;std::memcpy(&bits,&f,4);std::printf("%08x ",bits);}
CompoundBox Sphere(glm::vec3 p,float radius){CompoundBox c(p,glm::vec3(0));c.type=ShapeType::Sphere;c.radius=radius;return c;}
int main(int argc,char** argv){
 const bool noCollision=argc==2&&std::string(argv[1])=="--no-collision";
 for(bool sphereMoving:{false,true})for(bool rotated:{false,true})for(bool translated:{false,true}){
  PhysicsWorld w;Check(w.Init(),"world initializes");
  const glm::quat chart=rotated?glm::angleAxis(1.1f,glm::normalize(glm::vec3(1,2,-3))):glm::quat(1,0,0,0);
  const glm::vec3 origin=translated?glm::vec3(100000,-200000,300000):glm::vec3(0);
  Shape fixed=sphereMoving?Shape::Compound({CompoundBox({0,20,0},{.1f,.1f,.1f}),CompoundBox({0,-20,0},{.1f,.1f,.1f}),CompoundBox({0,0,0},{.6f,.7f,.5f})}):Shape::Compound({Sphere({0,20,0},.1f),Sphere({0,0,0},.5f),Sphere({0,-20,0},.1f)});
  Shape moving=sphereMoving?Shape::Compound({Sphere({0,3,0},.1f),Sphere({0,0,0},.3f),Sphere({0,-3,0},.1f)}):Shape::Compound({CompoundBox({0,3,0},{.1f,.1f,.1f}),CompoundBox({0,-3,0},{.1f,.1f,.1f}),CompoundBox({0,0,0},{.3f,.3f,.3f})});
  const auto wall=w.CreateShape(fixed,{origin,chart},false,0,.2f,.7f);
  const auto body=w.CreateShape(moving,{origin+chart*glm::vec3(-3,0,0),chart},true,3,.2f,.7f);
  Check(wall.IsValid()&&body.IsValid()&&wall.id<body.id,"authored canonical IDs are genuinely reversed during dynamic discovery");
  if(noCollision)w.SetBodyEnabled(wall,false);
  const auto velocity=chart*glm::vec3(12,0,0);w.SetLinearVelocity(body,velocity);
  unsigned events=0,contactSteps=0;float maximumX=-100;
  for(unsigned step=0;step<40;++step){
   w.Step(1.f/120);const auto p=w.GetTransform(body);const auto v=w.GetLinearVelocity(body),spin=w.GetAngularVelocity(body);const auto local=glm::conjugate(chart)*(p.position-origin);maximumX=std::max(maximumX,local.x);events+=w.LastStepStats().impactEvents;contactSteps+=w.LastStepContactCount()>0;
   std::printf("STATE %d %d %d %u ",sphereMoving,rotated,translated,step);for(float f:{p.position.x,p.position.y,p.position.z,p.rotation.w,p.rotation.x,p.rotation.y,p.rotation.z,v.x,v.y,v.z,spin.x,spin.y,spin.z})Print(f);std::printf("%zu %zu %zu\n",w.LastStepStats().impactEvents,w.LastStepStats().impactQueries,w.LastStepContactCount());
  }
  Check(events>0&&contactSteps>0,"actual mixed compound primitive impact and solved contact rows exercised");
  Check(maximumX<-.65f,"object does not cross the physical boundary");
  Check(glm::dot(w.GetLinearVelocity(body),chart*glm::vec3(1,0,0))<0,"physical restitution reverses velocity without scripted pose changes");
  w.Shutdown();
 }
 std::printf("SUMMARY %u checks %u failures\n",checks,failures);return failures?1:0;
}
