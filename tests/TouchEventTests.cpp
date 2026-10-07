#include "PhysicsWorld.h"
#include "RuntimeWorld.h"
#include "ScriptSystem.h"
#include "ResourceManager.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "Prefab.h"
#include "InputSystem.h"
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cmath>
namespace fs=std::filesystem;
int checks=0,failures=0;
void Check(bool ok,const char* label){++checks;if(!ok)++failures;std::printf("%s %s\n",ok?"PASS":"FAIL",label);}
bool Phase(const PhysicsWorld& p,PhysicsWorld::TouchPhase phase,bool sensor){auto& e=p.LastStepTouchEvents();return e.size()==1&&e[0].phase==phase&&e[0].sensor==sensor;}
int main(int argc,char** argv){
 fs::path out="build/m42-touch-tests";if(argc==3&&std::string(argv[1])=="--output")out=argv[2];fs::create_directories(out/"Assets");
 // Reruns: this test imports its own assets, and import refuses files that already have metadata.
 std::setvbuf(stdout,nullptr,_IOLBF,0);for(auto name:{"events.js","participant.judasprefab"}){fs::remove(out/"Assets"/name);fs::remove(out/"Assets"/(std::string(name)+".judasmeta"));}
 constexpr float dt=1.f/60;
 PhysicsWorld p;p.Init();auto floor=p.CreateStaticBox({0,-.5f,0},{3,.5f,3},0,0);auto ball=p.CreateDynamicSphere({0,.5f,0},.5f,1,0,0);
 p.SetLinearVelocity(ball,{0,-1,0});p.Step(dt);
 Check(Phase(p,PhysicsWorld::TouchPhase::Enter,false)&&std::abs(p.GetLinearVelocity(ball).y)<1e-5f,"ordinary collision responds and enters once");
 Check(p.LastStepTouchEvents()[0].a.id==floor.id&&std::isfinite(p.LastStepTouchEvents()[0].point.y),"real manifold event data and generation-safe bodies");
 p.Step(dt);Check(Phase(p,PhysicsWorld::TouchPhase::Stay,false),"collision stay per pair, not per point");
 p.ResetBody(ball,{0,3,0},glm::quat(1,0,0,0));p.Step(dt);Check(Phase(p,PhysicsWorld::TouchPhase::Exit,false),"collision exit");p.Step(dt);Check(p.LastStepTouchEvents().empty(),"no duplicate exit");p.Shutdown();
 p.Init();auto sensor=p.CreateStaticBox({0,0,0},{1,1,1},0,1);p.SetBodySensor(sensor,true);ball=p.CreateDynamicSphere({0,0,0},.25f,1,0,1);p.SetLinearVelocity(ball,{1,0,0});p.Step(dt);
 Check(Phase(p,PhysicsWorld::TouchPhase::Enter,true)&&p.GetLinearVelocity(ball).x==1&&p.LastStepContactCount()==0,"sensor enters without impulses or contact solver rows");
 p.Step(dt);Check(Phase(p,PhysicsWorld::TouchPhase::Stay,true),"trigger stay");p.SetBodyEnabled(ball,false);p.Step(dt);Check(Phase(p,PhysicsWorld::TouchPhase::Exit,true),"disable causes one exit");
 p.SetBodyEnabled(ball,true);p.SetCollisionFilter(ball,1,CategoryBit(1));p.Step(dt);Check(p.LastStepTouchEvents().empty(),"M39 bilateral mask rejects trigger");
 p.SetCollisionFilter(ball,0,kAllCategories);p.Step(dt);Check(Phase(p,PhysicsWorld::TouchPhase::Enter,true),"reenabled permitted pair enters");
 p.DestroyBody(ball);auto replacement=p.CreateDynamicSphere({4,0,0},.25f,1,0,0);p.Step(dt);Check(Phase(p,PhysicsWorld::TouchPhase::Exit,true)&&replacement.id!=ball.id,"destruction and slot reuse cannot alias event identity");
 PhysicsQueryFilter filter;Check(p.QueryBodiesInAabb({-1,-1,-1},{1,1,1},filter).empty(),"physical queries omit sensor by default");filter.includeSensors=true;Check(p.QueryBodiesInAabb({-1,-1,-1},{1,1,1},filter).size()==1,"query explicitly includes sensor");p.Shutdown();
 auto scriptPath=out/"Assets/events.js";std::ofstream(scriptPath)<<R"JS(import {world,time} from 'judas';
 export const properties={destroy:{type:'boolean',default:false},disable:{type:'boolean',default:false}};
 export default class {constructor({entity,properties}){this.entity=entity;this.properties=properties;this.state={enter:0,stay:0,exit:0};}
 onTriggerEnter(e){this.state.enter++;this.state.other=e.other.id;this.state.safe=e.other.valid;this.state.point=Number.isFinite(e.point.x);this.state.fixed=time.fixed;
 if(this.properties.destroy)e.other.destroy();if(this.properties.disable)this.entity.setColliderEnabled(false);}
 onTriggerStay(e){this.state.stay++;}
 onTriggerExit(e){this.state.exit++;this.state.otherValid=e.other.valid;}
 onCollisionEnter(e){this.state.collision=true;}
 })JS";
 AssetDatabase db;db.Scan(fs::absolute(out).string(),fs::absolute(out/"Assets").string());AssetRecord script;std::string error;Check(db.Track(fs::absolute(scriptPath).string(),script,error),"normal registered event script asset");if(script.id.empty()){std::printf("ABORT %s\nSUMMARY %d checks %d failures\n",error.c_str(),checks,failures);return 1;}
 Scene scene;auto& a=scene.CreateObject("Sensor");a.body=SceneBodyComponent{};a.body->sensor=true;a.body->halfExtents={1,1,1};a.scripts.push_back({1,script.id,true,"{}"});auto aid=a.id;
 auto& b=scene.CreateObject("Participant");b.body=SceneBodyComponent{};b.body->shape=SceneShape::Sphere;b.body->radius=.25f;b.body->motion=SceneBodyMotion::Dynamic;b.transform.position={0,0,0};auto bid=b.id;
 std::string text;SaveSceneToString(scene,text);Scene copy;Check(LoadSceneFromString(text,copy,error)&&ScenesEqual(scene,copy),"sensor/enabled scene round-trip");
 std::string hash1,hash2;ComputeSceneFingerprint(scene,hash1,error);copy.Find(aid)->body->sensor=false;ComputeSceneFingerprint(copy,hash2,error);Check(hash1!=hash2,"sensor changes authored compatibility fingerprint");
 ResourceManager resources(nullptr,&db);InputSystem input;
 auto step=[&](RuntimeWorld& w){w.FixedScripts(&input,dt);w.Physics().Step(dt);w.DispatchPhysicsEvents(&input,dt);};
 auto state=[&](RuntimeWorld& w,EntityId id){if(!w.Scripts())return std::string{};for(auto& r:w.Scripts()->Capture())if(r.entity==id)return r.json;return std::string{};};
 RuntimeWorld w;if(!w.Build(scene,&resources,error)){Check(false,"real RuntimeWorld constructs sensor and participant");std::printf("ABORT %s\nSUMMARY %d checks %d failures\n",error.c_str(),checks,failures);return 1;}Check(true,"real RuntimeWorld constructs sensor and participant");step(w);step(w);auto t=w.Physics().GetTransform(w.RuntimeBody(bid));SceneTransform moved=scene.Find(bid)->transform;moved.position={4,0,0};w.SetRuntimeTransform(bid,moved);step(w);
 auto st=state(w,aid);std::printf("STATE sequence %s\n",st.c_str());Check(st.find("\"enter\":1")!=std::string::npos&&st.find("\"stay\":1")!=std::string::npos&&st.find("\"exit\":1")!=std::string::npos&&st.find("\"safe\":true")!=std::string::npos&&st.find("\"point\":true")!=std::string::npos&&st.find("\"fixed\":true")!=std::string::npos,"JS receives enter/stay/exit and safe data");(void)t;
 Check(w.Scripts()->Diagnostics().empty(),"event callbacks fault-free");w.Destroy();
 scene.Find(aid)->body->sensor=false;w.Build(scene,&resources,error);step(w);Check(state(w,aid).find("\"collision\":true")!=std::string::npos,"JS receives actual ordinary collision callback");w.Destroy();scene.Find(aid)->body->sensor=true;
 scene.Find(aid)->scripts[0].properties="{\"destroy\":true}";w.Build(scene,&resources,error);step(w);Check(!w.RuntimeDefinition(bid),"callback may destroy other safely");step(w);st=state(w,aid);Check(st.find("\"exit\":1")!=std::string::npos&&st.find("\"otherValid\":false")!=std::string::npos,"survivor exit exposes stale other safely");w.Destroy();
 scene.Find(aid)->scripts[0].properties="{\"disable\":true}";w.Build(scene,&resources,error);step(w);step(w);st=state(w,aid);Check(st.find("\"enter\":1")!=std::string::npos&&st.find("\"exit\":1")!=std::string::npos&&!w.Physics().IsBodyEnabled(w.RuntimeBody(aid)),"callback disabling collider is safe and exits once");w.Destroy();
 // Spawn through normal prefab resolution, not a special event fixture path.
 Scene prefab;auto& root=prefab.CreateObject("Runtime participant");root.body=*scene.Find(bid)->body;root.scripts.push_back({1,script.id,true,"{}"});SaveSceneToFile(prefab,(out/"Assets/participant.judasprefab").string(),error);AssetRecord pref;Check(db.Track(fs::absolute(out/"Assets/participant.judasprefab").string(),pref,error),"ordinary scripted prefab registered");
 scene.DestroyObject(bid);scene.Find(aid)->scripts[0].properties="{}";w.Build(scene,&resources,error);auto spawned=w.SpawnPrefab(pref.id,SceneTransform{},error);step(w);Check(spawned&&state(w,aid).find("\"enter\":1")!=std::string::npos&&state(w,spawned).find("\"enter\":1")!=std::string::npos,"prefab spawned participant receives normal event callbacks");Check(w.Scripts()->Diagnostics().empty(),"prefab callback data has no VM faults");w.Destroy();resources.Shutdown();
 std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
