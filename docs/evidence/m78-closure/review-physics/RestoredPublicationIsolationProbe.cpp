// Read-only review probe: compiled only when the parent schedules it.
// No production modifications; same completed-publication API and 60 Hz cadence.
#include "PhysicsWorld.h"
#include "SaveArchive.h"
#include <cstdio>
#include <vector>
constexpr float dt=1.f/60;
int checks=0,failures=0;
void Check(bool ok,const char* name){++checks;if(!ok)++failures;std::printf("%s %s\n",ok?"PASS":"FAIL",name);}
struct Fixture {
 PhysicsWorld world;BodyHandle floor,body,proxy;
 Fixture(){world.Init();floor=world.CreateShape(Shape::Box({3,.5f,3}),{{0,-.5f,0},{1,0,0,0}},false,0,.8f,0);body=world.CreateShape(Shape::Box({.5f,.5f,.5f}),{{0,.5f,0},{1,0,0,0}},true,4,.8f,0);proxy=world.CreateQueryCapsule(.3f,.5f,{{100,3,0},{1,0,0,0}});}
 uint64_t Id(BodyHandle h){return h.id==floor.id?1:h.id==body.id?2:h.id==proxy.id?3:0;}
 BodyHandle Resolve(uint64_t id){return id==1?floor:id==2?body:id==3?proxy:BodyHandle{};}
 std::string Save(){SaveArchive a;world.PersistCompletedContactObservations(a,[&](auto h){return Id(h);},[&](auto id){return Resolve(id);});return a.bytes;}
 void Restore(const std::string& bytes){SaveArchive a(bytes);world.PersistCompletedContactObservations(a,[&](auto h){return Id(h);},[&](auto id){return Resolve(id);});a.Finish();}
 void Match(Fixture& from){for(auto pair:{std::pair{from.floor,floor},std::pair{from.body,body},std::pair{from.proxy,proxy}}){auto t=from.world.GetTransform(pair.first);world.ResetBody(pair.second,t.position,t.rotation);world.SetLinearVelocity(pair.second,from.world.GetLinearVelocity(pair.first));world.SetAngularVelocity(pair.second,from.world.GetAngularVelocity(pair.first));}}
};
int main(){Fixture source;for(int i=0;i<4;++i){source.world.ApplyLinearAcceleration(source.body,{0,-9.81f,0},dt);source.world.Step(dt);}std::vector<PhysicsWorld::BodyContactSample> before,after;Check(source.world.ReadBodyContactSample(source.body,before)&&!before.empty(),"genuine positive completed source contact");auto saved=source.Save();Fixture unrelated;unrelated.Match(source);unrelated.Restore(saved);Check(unrelated.world.HasRestoredCompletedContactObservations(dt),"restored genuine publication begins ready");auto proxy=unrelated.world.GetTransform(unrelated.proxy);unrelated.world.ResetBody(unrelated.proxy,proxy.position,proxy.rotation);Check(unrelated.world.ReadBodyContactSample(unrelated.body,after)&&after.size()==before.size(),"unrelated proxy reset leaves actual completed rows readable");Check(unrelated.world.HasRestoredCompletedContactObservations(dt),"unrelated query-only proxy reset must not demote independent completed publication");Fixture affected;affected.Match(source);affected.Restore(saved);auto pose=affected.world.GetTransform(affected.body);affected.world.ResetBody(affected.body,pose.position+glm::vec3(0,1,0),pose.rotation);Check(!affected.world.HasRestoredCompletedContactObservations(dt),"affected contact body mutation demotes restored publication");after.clear();Check(affected.world.ReadBodyContactSample(affected.body,after)&&after.empty(),"affected teleport does not expose stale support rows");std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;}
