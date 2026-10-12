#include "RuntimeWorld.h"
#include "AuthoringJSON.h"
#include <glm/gtc/quaternion.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
namespace fs=std::filesystem;
unsigned checks=0,failures=0;
nlohmann::json results=nlohmann::json::array();
void Check(bool ok,const std::string& name){
 ++checks;failures+=!ok;results.push_back({{"name",name},{"passed",ok}});
 std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';
}
bool QueryTag(RuntimeWorld& world,EntityId id,unsigned tag){
 auto* motor=world.RuntimeCharacter(id);
 if(!motor)return false;
 auto center=motor->position+motor->orientation*motor->settings.offset;
 auto direction=motor->orientation*glm::vec3(1,0,0);
 PhysicsQueryFilter filter;filter.requiredTags=CategoryBit(tag);
 auto hit=world.Physics().Raycast(center-direction*2.f,direction,4,filter);
 return hit.hit&&hit.body.id==motor->observationBody.id;
}
bool QueryRigidTag(RuntimeWorld& world,EntityId id,unsigned tag){
 auto body=world.RuntimeBody(id);
 if(!body.IsValid())return false;
 auto pose=world.Physics().GetTransform(body);auto direction=pose.rotation*glm::vec3(1,0,0);
 PhysicsQueryFilter filter;filter.requiredTags=CategoryBit(tag);
 auto hit=world.Physics().Raycast(pose.position-direction*2.f,direction,4,filter);
 return hit.hit&&hit.body.id==body.id;
}
}
int main(int argc,char** argv){
 fs::path output;
 try{
  if(argc!=2)throw std::runtime_error("usage: dynamic-motor-tags FRESH-output");
  output=fs::absolute(fs::u8path(argv[1]));
  if(fs::exists(output))throw std::runtime_error("output exists; use a fresh owned path");
  fs::create_directories(output);
  ProjectClassification classification;unsigned authored=0,runtime=0;
  Check(classification.tags.Add("Authored",authored)&&classification.tags.Add("Runtime",runtime),"ordinary finite project tag classification");
  Scene scene;auto& actor=scene.CreateObject("motor");auto id=actor.id;
  actor.characterMotor=CharacterMotorSettings{};actor.characterMotor->offset={.3f,.4f,-.2f};actor.tags=CategoryBit(authored);
  actor.transform.rotation=glm::angleAxis(.67f,glm::normalize(glm::vec3(1,2,-1)));
  // A separate ordinary rigid-body world uses the same project tags and
  // oblique physical frame. It cannot satisfy the motor queries accidentally.
  Scene rigidScene;auto& rigidActor=rigidScene.CreateObject("ordinary rigid body");auto rigidId=rigidActor.id;
  rigidActor.tags=actor.tags;rigidActor.transform=actor.transform;
  rigidActor.transform.position+=actor.transform.rotation*actor.characterMotor->offset;
  rigidActor.body=SceneBodyComponent{};rigidActor.body->motion=SceneBodyMotion::Dynamic;rigidActor.body->managed=true;
  RuntimeWorld rigidWorld;std::string rigidError;
  Check(rigidWorld.Build(rigidScene,nullptr,rigidError,&classification),"non-motor control uses ordinary dynamic rigid-body construction");
  Check(QueryRigidTag(rigidWorld,rigidId,authored)&&!QueryRigidTag(rigidWorld,rigidId,runtime),"non-motor authored-tag positive and absent-runtime-tag negative controls");
  Check(rigidWorld.AddTag(rigidId,runtime)&&rigidWorld.HasTag(rigidId,runtime),"non-motor public AddTag updates entity classification");
  Check(QueryRigidTag(rigidWorld,rigidId,runtime),"non-motor public AddTag updates actual physics query tag");
  Check(rigidWorld.RemoveTag(rigidId,authored)&&!rigidWorld.HasTag(rigidId,authored),"non-motor public RemoveTag updates entity classification");
  Check(!QueryRigidTag(rigidWorld,rigidId,authored),"non-motor public RemoveTag updates actual physics query tag");
  Check(rigidWorld.RemoveTag(rigidId,runtime)&&!QueryRigidTag(rigidWorld,rigidId,runtime),"non-motor added tag can also be removed from physics queries");
  RuntimeWorld world;std::string error;
  Check(world.Build(scene,nullptr,error,&classification),"ordinary motor world builds");
  world.UpdateCharacters(0);
  Check(QueryTag(world,id,authored)&&!QueryTag(world,id,runtime),"authored tag positive and absent runtime-tag negative controls");
  Check(world.AddTag(id,runtime)&&world.HasTag(id,runtime),"public runtime AddTag changes logical entity classification");
  Check(QueryTag(world,id,runtime),"public runtime AddTag immediately changes actual physics query tag");
  world.UpdateCharacters(0);
  Check(QueryTag(world,id,runtime),"ordinary zero-time metadata update retains runtime query tag");
  Check(world.RemoveTag(id,authored)&&!world.HasTag(id,authored),"public runtime RemoveTag changes logical entity classification");
  Check(!QueryTag(world,id,authored),"public runtime RemoveTag immediately excludes actual physics query tag");
  world.UpdateCharacters(0);
  Check(!QueryTag(world,id,authored),"ordinary metadata update does not resurrect removed authored query tag");
  RuntimeWorld lazy;Check(lazy.Build(scene,nullptr,error,&classification),"second ordinary lazy motor world builds");
  Check(lazy.AddTag(id,runtime)&&lazy.HasTag(id,runtime),"runtime tag can change before lazy capsule construction");
  Check(QueryTag(lazy,id,runtime),"first lazy capsule construction uses current logical tags");
  Check(world.SimulationTimeSeconds()==0&&lazy.SimulationTimeSeconds()==0&&rigidWorld.SimulationTimeSeconds()==0,"all observations occur without elapsed physics time");
 }catch(const std::exception& e){Check(false,std::string("unexpected fixture error: ")+e.what());}
 if(!output.empty()&&fs::exists(output))std::ofstream(output/"RESULT.json")<<nlohmann::json{{"checks",results},{"failures",failures},{"scope","Read-only reproduction of current public motor-tag/query disagreement; no runtime changes"}}.dump(2)<<'\n';
 std::cout<<"SUMMARY "<<checks<<" checks "<<failures<<" failures\n";return failures?1:0;
}
