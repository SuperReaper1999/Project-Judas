#include "FixtureSupport.h"
#include "WorldStreaming.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "SceneSerialization.h"
#include <glm/gtc/quaternion.hpp>
#include <chrono>
#include <thread>
using namespace LifecycleAudit;
namespace {
bool Pump(WorldStreaming& stream,const std::function<bool()>& done){for(unsigned i=0;i<4000;++i){stream.Advance(false);if(done())return true;std::this_thread::sleep_for(std::chrono::milliseconds(1));}return false;}
nlohmann::json Vec(glm::vec3 v){return {v.x,v.y,v.z};}
}
int main(int argc,char** argv){Receipt r;fs::path out;try{out=FreshOutput(argc,argv);std::string error;Project p;
 r.Require(Project::CreateNew((out/"source").string(),"Motor publication audit",p,error),"ordinary owned project",error);
 unsigned layer=0,tag=0;r.Require(p.Settings().classification.collision.Add("Actor",layer)&&p.Settings().classification.tags.Add("Motor",tag),"project-owned query classification");r.Require(p.Save(error),"project saved",error);
 Scene scene;auto& active=scene.CreateObject("motor");const auto activeLocal=active.id;active.characterMotor=CharacterMotorSettings{};active.characterMotor->offset={.8f,.2f,-.1f};active.characterMotor->collisionLayer=layer;active.characterMotor->collisionMask=CategoryBit(layer);active.tags=CategoryBit(tag);active.transform.position={2,3,4};active.transform.rotation=glm::angleAxis(.73f,glm::normalize(glm::vec3(1,2,-1)));
 auto disabled=active;disabled.id=scene.CreateObject("disabled").id;disabled.transform.position={-4,2,1};disabled.characterMotor->enabled=false;*scene.Find(disabled.id)=disabled;const auto disabledLocal=disabled.id;
 r.Require(SaveSceneToFile(scene,(fs::path(p.ScenesDir())/"motors.judas").string(),error),"authored motor scene",error);
 AssetDatabase db;db.Scan(p.RootDir(),p.AssetsDir());JobSystem jobs(2);ResourceManager resources(nullptr,&db,&jobs);RuntimeWorld world;r.Require(world.Build(Scene{},&resources,error),"ordinary world",error);
 WorldManifest manifest;manifest.unitsPerFrame=1;manifest.installMilliseconds=10;WorldRegion region;region.id="motors";region.scene="Scenes/motors.judas";manifest.regions.emplace(region.id,region);WorldStreaming stream(world,resources,p,manifest);
 auto request=[&]{auto token=stream.Request("motors",false,error);r.Require(token!=0,"normal region request",error);r.Require(Pump(stream,[&]{auto* s=stream.Status(token);return s&&s->state=="active";}),"region reaches atomic active publication");return token;};
 auto checkMotor=[&](EntityId id,bool expectedEnabled,const std::string& phase){auto* motor=world.RuntimeCharacter(id);r.Require(motor!=nullptr,phase+" motor exists");auto body=motor->observationBody;const auto transform=world.Physics().GetTransform(body);const auto expected=motor->position+motor->orientation*motor->settings.offset;
  r.Check(glm::length(transform.position-expected)<1e-6f&&std::abs(glm::dot(transform.rotation,motor->orientation))>1-1e-6f,phase+" query pose is derived from authoritative root plus rotated offset",{{"root",Vec(motor->position)},{"expectedProxy",Vec(expected)},{"actualProxy",Vec(transform.position)}});
  r.Check(world.Physics().IsBodyEnabled(body)==expectedEnabled,phase+" authored enabled state is visible before callbacks");unsigned actualLayer=99;CategoryMask mask=0;r.Check(world.Physics().GetCollisionFilter(body,actualLayer,mask)&&actualLayer==layer&&mask==CategoryBit(layer),phase+" authored physical filter is visible before callbacks",{{"layer",actualLayer},{"mask",mask}});
  PhysicsQueryFilter filter;filter.includeLayers=CategoryBit(layer);filter.requiredTags=CategoryBit(tag);auto direction=motor->orientation*glm::vec3(1,0,0);auto hit=world.Physics().Raycast(expected-direction*2.f,direction,4,filter);
  r.Check(expectedEnabled?(hit.hit&&hit.body.id==body.id):!hit.hit,phase+" filtered cast reflects real capsule geometry and tag",{{"hit",hit.hit},{"body",hit.body.id},{"expectedBody",body.id}});
 };
 auto token=request();const auto first=stream.Resolve("motors",activeLocal);const auto firstBody=world.RuntimeBody(first);checkMotor(first,true,"initial publication");checkMotor(stream.Resolve("motors",disabledLocal),false,"initial disabled publication");
 // This diagnostic recovery uses the existing zero-dt publication path. It does
 // not make the earlier assertions depend on a movement/physics step.
 world.UpdateCharacters(0);checkMotor(first,true,"ordinary zero-dt synchronization");
 auto* motor=world.RuntimeCharacter(first);EntityPhysicalState state;world.GetEntityState(first,state);state.position={6,7,8};state.rotation=glm::angleAxis(-.44f,glm::normalize(glm::vec3(-2,1,3)));state.linearVelocity={1,-2,3};r.Require(world.SetEntityState(first,state),"normal motor root and velocity edit");world.UpdateCharacters(0);const auto beforeTime=world.SimulationTimeSeconds();
 r.Require(stream.Release(token),"release own demand");r.Require(Pump(stream,[&]{return stream.Stats().active==0&&stream.Stats().pending==0;}),"ordinary retained suspension");r.Check(!world.RuntimeDefinition(first)&&!world.Physics().IsBodyEnabled(firstBody),"old entity/body do not alias after retirement");
 token=request();const auto next=stream.Resolve("motors",activeLocal);r.Check(next!=first,"revisit has fresh runtime identity");auto* resumed=world.RuntimeCharacter(next);r.Check(resumed&&resumed->position==state.position&&resumed->velocity==state.linearVelocity,"revisit preserves authoritative root and velocity");checkMotor(next,true,"retained revisit before first callback");checkMotor(stream.Resolve("motors",disabledLocal),false,"retained disabled revisit");r.Check(world.SimulationTimeSeconds()==beforeTime,"publication/revisit did not advance simulation");world.UpdateCharacters(0);checkMotor(next,true,"revisit ordinary zero-dt synchronization");
 return r.Finish(out,"M59 atomic activation publishes complete CharacterMotor query representation before the next script callback");
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';r.Check(false,"unexpected fixture exception",{{"error",e.what()}});if(!out.empty())return r.Finish(out,"streaming motor publication");return 2;}}
