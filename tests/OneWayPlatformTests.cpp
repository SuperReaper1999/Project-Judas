#include "PhysicsWorld.h"
#include "PlanarGeometry.h"
#include "SceneSerialization.h"
#include "Scene.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "WorldPersistence.h"
#include "Prefab.h"
#include <filesystem>
#include "SaveArchive.h"
#include <glm/gtc/quaternion.hpp>
#include <cstdio>
#include <stdexcept>
unsigned checks=0;
void Check(bool ok,const char* text){++checks;std::printf("%s %s\n",ok?"PASS":"FAIL",text);if(!ok)throw std::runtime_error(text);}
glm::vec3 Run(float start,float speed,bool oneWay,glm::quat q={1,0,0,0}){
 PhysicsWorld w;w.Init();w.CreateShape(Shape::Planar(PlanarChain({{-3,0},{3,0}},oneWay)),{{0,0,0},q},false,1,.5f,0);
 auto h=w.CreateShape(Shape::Planar(PlanarCapsule(.25,.25,true)),{q*glm::vec3(0,start,0),q},true,1,.5f,0);
 w.SetLinearVelocity(h,q*glm::vec3(0,speed,0));float peak=-100;for(int i=0;i<240;++i){w.ApplyLinearAcceleration(h,q*glm::vec3(0,-9.81f,0),1.f/60);w.Step(1.f/60);auto p=glm::inverse(q)*w.GetTransform(h).position;peak=std::max(peak,p.y);}
 auto p=glm::inverse(q)*w.GetTransform(h).position;std::printf("TRAJECTORY start=%g speed=%g oneWay=%d peak=%g final=%g\n",start,speed,oneWay,peak,p.y);
 if(start<0&&oneWay)Check(peak>1,"body passes upward completely from nonblocking side");
 return p;
}
void Advance(RuntimeWorld& w,int count){for(int i=0;i<count;++i){w.Physics().ApplyLinearAcceleration(w.RuntimeBody(2),{0,-9.81f,0},1.f/60);w.Physics().Step(1.f/60);}}
void Persistence(const std::string& directory,bool cold){
 std::string error;ResourceManager resources(nullptr,nullptr);RuntimeWorld w;w.legacyGameplay=false;Scene scene;GameSnapshot snapshot;snapshot.project="one-way";snapshot.scene="main";snapshot.content=std::string(64,'e');SaveStorage storage(directory);
 if(cold){bool recovered=false;Check(storage.Read("crossing",snapshot,recovered,error)&&WorldPersistence::SceneFrom(snapshot.participants,scene,error),"fresh process reads one-way crossing snapshot");}
 else{auto& floor=scene.CreateObject("One way");floor.body=SceneBodyComponent{};floor.body->shape=SceneShape::Planar;floor.body->planar=PlanarChain({{-3,0},{3,0}},true);
  auto& actor=scene.CreateObject("Jumper");actor.transform.position={0,-1.2f,0};actor.body=SceneBodyComponent{};actor.body->shape=SceneShape::Planar;actor.body->planar=PlanarCapsule(.25,.25,true);actor.body->motion=SceneBodyMotion::Dynamic;actor.body->initialLinearVelocity={0,9,0};}
 Check(w.Build(scene,&resources,error,nullptr,nullptr,cold),"build real persistent one-way world");
 if(cold){Check(WorldPersistence::Prepare(w,error)&&WorldPersistence::Restore(w,snapshot.participants,error),"cold restores exact physical state without a one-way cache");Advance(w,232);Check(std::abs(w.Physics().GetTransform(w.RuntimeBody(2)).position.y-.5f)<.02f,"cold crossing resumes and lands on the blocking side");return;}
 Advance(w,8);Check(w.Physics().GetTransform(w.RuntimeBody(2)).position.y<.5f&&w.Physics().GetLinearVelocity(w.RuntimeBody(2)).y>0,"capture occurs while moving upward through the platform");
 Check(WorldPersistence::Capture(w,snapshot.participants,error)&&storage.Write("crossing",snapshot,error),"durably save in-progress one-way crossing");
 Scene restoredScene;Check(WorldPersistence::SceneFrom(snapshot.participants,restoredScene,error),"restore scene carries versioned side geometry");RuntimeWorld restored;restored.legacyGameplay=false;
 Check(restored.Build(restoredScene,&resources,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(restored,error)&&WorldPersistence::Restore(restored,snapshot.participants,error),"restore crossing into independent native world");
 Advance(w,232);Advance(restored,232);Check(w.Physics().GetTransform(w.RuntimeBody(2)).position==restored.Physics().GetTransform(restored.RuntimeBody(2)).position&&w.Physics().GetLinearVelocity(w.RuntimeBody(2))==restored.Physics().GetLinearVelocity(restored.RuntimeBody(2)),"restored crossing continues to identical actual pose and velocity");
}
int main(int argc,char** argv){try{
 if(argc==3){Persistence(argv[1],true);std::printf("ONE_WAY_COLD checks=%u failures=0\n",checks);return 0;}
 Check(std::abs(Run(-1.2,9,true).y-.5f)<.02f,"upward crossing then landing and persistent support");
 Check(Run(-1.2,9,false).y < -10,"unchanged two-sided chain blocks the same upward launch");
 Check(std::abs(Run(3,-120,true).y-.5f)<.02f,"fast downward arrival is caught by normal CCD");
 Check(std::abs(Run(-1.2,9,true,glm::angleAxis(.7f,glm::normalize(glm::vec3(1,2,3)))).y-.5f)<.02f,"one-way response follows arbitrary fixed rotated plane/gravity");
 {auto bad=*PlanarBox({1,1});bad.oneWay=true;std::string e;Check(!ValidatePlanar(bad,e),"one-way policy rejects unsupported solid shapes rather than guessing a side");}
 auto original=PlanarChain({{-2,0},{2,0}},true);std::shared_ptr<const PlanarCollider> decoded;std::string error;
 Check(DecodePlanar(EncodePlanar(*original),decoded,error)&&decoded->oneWay,"versioned geometry roundtrip preserves authored one-way side");
 const auto retained=decoded;Check(!DecodePlanar("{\"version\":2,\"kind\":4,\"radius\":0,\"lockRotation\":true,\"oneWay\":\"yes\",\"vertices\":[[-1,0],[1,0]]}",decoded,error)&&decoded==retained,"malformed one-way flag rejects without replacing immutable geometry");
 Check(EncodePlanar(*PlanarChain({{-2,0},{2,0}})).find("oneWay")==std::string::npos,"ordinary version 1 geometry bytes remain unchanged");
 Scene scene;auto& platform=scene.CreateObject("One way");platform.body=SceneBodyComponent{};platform.body->shape=SceneShape::Planar;platform.body->planar=original;std::string text;SaveSceneToString(scene,text);Scene restored;
 Check(LoadSceneFromString(text,restored,error)&&restored.Find(platform.id)->body->planar->oneWay,"normal scene save/load retains one-way geometry");
 Scene prefab,instances;SceneObjectId instance=0;SceneTransform placement;placement.position={4,2,0};
 Check(CreatePrefab(scene,platform.id,prefab,error)&&InstantiatePrefab(instances,prefab,"11111111111111111111111111111111",placement,instance,error)&&instances.Find(instance)->body->planar->oneWay,"ordinary prefab construction retains one-way side at new placement");
 {
 PhysicsWorld w;w.Init();w.CreateShape(Shape::Planar(PlanarChain({{-3,0},{3,0}},true)),{{0,0,0},{1,0,0,0}},false,1,.5f,0);
 auto up=w.CreateShape(Shape::Planar(PlanarCircle(.25)),{{-1,-1,0},{1,0,0,0}},true,1,.5f,0),down=w.CreateShape(Shape::Planar(PlanarCircle(.25)),{{1,1,0},{1,0,0,0}},true,1,.5f,0);w.SetLinearVelocity(up,{0,8,0});
 for(int i=0;i<30;++i){w.ApplyLinearAcceleration(up,{0,-9.81f,0},1.f/60);w.ApplyLinearAcceleration(down,{0,-9.81f,0},1.f/60);w.Step(1.f/60);}
 Check(w.GetTransform(up).position.y>1&&std::abs(w.GetTransform(down).position.y-.25f)<.02f,"ascending and descending bodies make independent decisions on the same platform");
 PhysicsWorld q;q.Init();q.CreateShape(Shape::Planar(PlanarChain({{-3,0},{3,0}},true)),{{0,0,0},{1,0,0,0}},false,1,.5f,0);auto shape=Shape::Planar(PlanarCircle(.2));
 Check(!q.CastPlanar(shape,{{0,-1,0},{1,0,0,0}},{0,1,0},2).hit&&q.CastPlanar(shape,{{0,1,0},{1,0,0,0}},{0,-1,0},2).hit,"native support queries observe the same blocking side");
 }
 {
 PhysicsWorld w;w.Init();w.CreateShape(Shape::Planar(PlanarChain({{-3,0},{3,0}},true)),{{0,0,0},{1,0,0,0}},false,1,.5f,0);
 auto actor=w.CreateShape(Shape::Planar(PlanarCircle(.25)),{{0,-.1f,0},{1,0,0,0}},false,1,.5f,0);Check(w.SetMotionType(actor,BodyMotionType::Kinematic),"create genuine kinematic crossing observer");w.Step(1.f/60);
 Check(w.LastStepTouchEvents().empty(),"nonblocking kinematic overlap does not invent a physical collision event");w.SetBodySensor(actor,true);w.Step(1.f/60);
 Check(!w.LastStepTouchEvents().empty()&&w.LastStepTouchEvents()[0].sensor,"explicit sensors retain geometric overlap on the nonblocking side");
 }
 if(argc==2)Persistence(argv[1],false);
 std::printf("ONE_WAY checks=%u failures=0\n",checks);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
