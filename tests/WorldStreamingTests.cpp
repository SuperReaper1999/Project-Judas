#include "WorldStreaming.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "SceneSession.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "Prefab.h"
#include "NavigationAsset.h"
#include "WorldState.h"
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <thread>
#include <atomic>
#include <cstdio>
using namespace std::chrono_literals;
namespace {
int checks=0,failures=0;
void check(bool ok,const char* name){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",name);}
Scene source(){Scene s;s.SetNextId(20);SceneObject floor;floor.id=1;floor.body=SceneBodyComponent{};floor.body->halfExtents={4,.5f,4};floor.transform.position={0,-.5f,0};s.Objects().push_back(floor);SceneObject box;box.id=2;box.body=SceneBodyComponent{};box.body->motion=SceneBodyMotion::Dynamic;box.transform.position={0,.5f,0};s.Objects().push_back(box);SceneObject gravity;gravity.id=3;gravity.gravity=SceneGravityComponent{};gravity.gravity->kind=SceneGravityKind::Uniform;gravity.gravity->regionShape=SceneRegionShape::Box;gravity.gravity->regionHalfExtents={4,4,4};s.Objects().push_back(gravity);return s;}
bool pump(WorldStreaming& stream,const std::function<bool()>& done,int max=4000){for(int i=0;i<max;++i){stream.Advance(false);if(done())return true;std::this_thread::sleep_for(1ms);}return false;}
}
int main(){namespace fs=std::filesystem;fs::path root="build/m59-unit-project";fs::remove_all(root);fs::create_directories(root);std::string error;
 Project p;check(Project::CreateNew(root.string(),"stream",p,error),"ordinary project");
 Scene s=source();check(SaveSceneToFile(s,(root/"Scenes/a.judas").string(),error),"source A");check(SaveSceneToFile(s,(root/"Scenes/b.judas").string(),error),"source B with same IDs");
 AssetDatabase db;db.Scan(p.RootDir(),p.AssetsDir());JobSystem jobs(2);ResourceManager resources(nullptr,&db,&jobs);
 Scene bootstrap;bootstrap.Settings().worldOrigin={1e12,1e12,1e12};RuntimeWorld world;check(world.Build(bootstrap,&resources,error),"one authoritative world");
 WorldManifest manifest;manifest.unitsPerFrame=1;manifest.installMilliseconds=10;WorldRegion a;a.id="a";a.scene="Scenes/a.judas";a.origin=bootstrap.Settings().worldOrigin;WorldRegion b=a;b.id="b";b.scene="Scenes/b.judas";b.origin.x+=8;manifest.regions={{"a",a},{"b",b}};
 check(ValidateWorldManifest(manifest,p,error),"registered scene sources");WorldStreaming stream(world,resources,p,manifest);
 auto first=stream.Request("a",false,error),duplicate=stream.Request("a",false,error),other=stream.Request("b",false,error);
 check(pump(stream,[&]{auto x=stream.Status(first),y=stream.Status(other);return x&&y&&x->state=="active"&&y->state=="active";}),"asynchronous parse/staged activation");
 auto one=stream.Resolve("a",2),two=stream.Resolve("b",2);check(one&&two&&one!=two,"colliding source IDs remapped independently");check(world.Physics().AliveBodyCount()==4,"both groups inhabit same physics inventory");
 EntityPhysicalState pos;world.GetEntityState(two,pos);check(std::abs(pos.position.x-8)<1e-6,"double origin subtracted before float narrowing");
 auto stale=world.RuntimeBody(one);stream.Release(first);stream.Unload("a");check(stream.Status(duplicate)->state=="active","independent duplicate demand retained");
 auto t=world.RuntimeDefinition(one)->transform;t.position.y=2;world.SetRuntimeTransform(one,t);
 stream.Release(duplicate);check(pump(stream,[&]{auto rows=stream.Regions();return rows.front().state=="unloaded";}),"budgeted real unregistration");check(!world.RuntimeDefinition(one)&&!world.Physics().IsBodyEnabled(stale),"old entity and body invalidated");
 first=stream.Request("a",false,error);check(pump(stream,[&]{return stream.Status(first)->state=="active";}),"return reconstructs region");auto returned=stream.Resolve("a",2);world.GetEntityState(returned,pos);check(returned!=one&&std::abs(pos.position.y-2)<1e-6,"changed pose retained with fresh identity");
 check(world.DestroyEntity(returned,&error),"ordinary gameplay destruction");stream.Release(first);check(pump(stream,[&]{return stream.Regions().front().state=="unloaded";}),"destroyed region suspends");first=stream.Request("a",false,error);check(pump(stream,[&]{return stream.Status(first)->state=="active";}),"destroyed region returns");check(!stream.Resolve("a",2),"destroyed source does not respawn on return");
 auto adopted=stream.Resolve("b",2);auto velocity=EntityPhysicalState{};world.GetEntityState(adopted,velocity);velocity.linearVelocity={1,2,3};world.SetEntityState(adopted,velocity);check(stream.Adopt(adopted,"root",error),"transfer ordinary hierarchy to root");stream.Release(other);stream.Advance(false);check(!stream.Regions()[1].pins.empty(),"adopted body pins its in-use gravity region");velocity.position.x=20;world.SetEntityState(adopted,velocity);check(pump(stream,[&]{return stream.Regions()[1].state=="unloaded";}),"source region unloads after adoption");world.GetEntityState(adopted,pos);check(world.RuntimeDefinition(adopted)&&pos.linearVelocity==glm::vec3(1,2,3),"adoption preserves runtime identity and motion");
 // Real worker gate: parsing cannot complete while ordinary world steps continue.
 std::atomic<bool> gate{false};stream.SetPreparationGate([&](const JobContext& ctx){while(!gate.load()&&!ctx.CancelRequested())std::this_thread::sleep_for(1ms);return !ctx.CancelRequested();});
 other=stream.Request("b",true,error);stream.Advance(false);auto before=world.SimulationTimeSeconds();for(int i=0;i<30;++i){world.Physics().Step(1.f/60);world.AdvanceSimulationTime(1./60);stream.Advance(false);}
 check(stream.Status(other)->state=="preparing"&&world.SimulationTimeSeconds()>before+.49,"held real job does not stall simulation");gate=true;
 check(pump(stream,[&]{return stream.Status(other)->state=="prepared";}),"preload completes without publishing");check(!stream.Resolve("b",1),"preload has no public colliders/entities");stream.Advance(true);check(stream.Status(other)->state=="prepared","pause prevents activation");stream.Activate(other);stream.Advance(false);check(stream.Status(other)->state=="installing"&&!stream.Resolve("b",1),"one-unit staged installation remains private");stream.Release(other);stream.Advance(false);check(pump(stream,[&]{return stream.Regions()[1].state=="unloaded";}),"cancel during installation rolls back staged registrations");
 check(world.Physics().AliveBodyCount()==2,"cancel left no orphan bodies"); // A floor + adopted B box
 // Tiny session cap cannot silently lose unique state.
 auto pressureSource=source();for(unsigned i=10;i<30;++i){SceneObject o;o.id=i;pressureSource.Objects().push_back(o);}pressureSource.SetNextId(40);SaveSceneToFile(pressureSource,(root/"Scenes/pressure.judas").string(),error);
 WorldManifest cap=manifest;cap.regions.at("a").scene="Scenes/pressure.judas";cap.retainedBytes=4096;RuntimeWorld limited;check(limited.Build(bootstrap,&resources,error),"pressure world");WorldStreaming pressure(limited,resources,p,cap);auto demand=pressure.Request("a",false,error);check(pump(pressure,[&]{return pressure.Status(demand)->state=="active";}),"pressure candidate active");pressure.Release(demand);pressure.Advance(false);check(pressure.Regions()[0].state=="active"&&!pressure.Regions()[0].pins.empty(),"state pressure pins instead of resets");
 // An oversized consumer-owned state pins without loss, then can retire when
 // that consumer legitimately replaces its own payload with smaller state.
 std::ofstream(root/"Assets/retention.js")<<R"JS(export default class {constructor({entity}){this.entity=entity;this.state={payload:'x'.repeat(8192),marker:17};}fixedUpdate(){if(this.entity.transform.position.x>0)this.state.payload='';}})JS";
 AssetRecord retentionScript;check(db.Track((root/"Assets/retention.js").string(),retentionScript,error),"retention script registered");
 Scene retainedScene;retainedScene.CreateObject("retained actor").scripts={{1,retentionScript.id,true,"{}"}};check(SaveSceneToFile(retainedScene,(root/"Scenes/retention.judas").string(),error),"retention scene saved");
 RuntimeWorld retainedWorld;check(retainedWorld.Build(Scene{},&resources,error),"retention world builds");WorldManifest retainedManifest;retainedManifest.retainedBytes=16*1024;WorldRegion retainedRegion;retainedRegion.id="state";retainedRegion.scene="Scenes/retention.judas";retainedManifest.regions.emplace("state",retainedRegion);WorldStreaming retainedStream(retainedWorld,resources,p,retainedManifest);
 auto retainedToken=retainedStream.Request("state",false,error);check(pump(retainedStream,[&]{auto status=retainedStream.Status(retainedToken);return status&&status->state=="active";}),"oversized state source active");retainedWorld.FixedScripts(nullptr,1.f/60);auto retainedEntity=retainedStream.Resolve("state",1);retainedStream.Release(retainedToken);
 check(pump(retainedStream,[&]{auto rows=retainedStream.Regions();return std::find(rows[0].pins.begin(),rows[0].pins.end(),"retained session byte budget")!=rows[0].pins.end();}),"over-budget pin reports exact retention reason");auto records=retainedWorld.Scripts()->Capture();check(records.size()==1&&records[0].json.find(std::string(8192,'x'))!=std::string::npos,"pinned actor retains complete oversized state");
 auto reducedTransform=retainedWorld.RuntimeDefinition(retainedEntity)->transform;reducedTransform.position.x=1;check(retainedWorld.SetRuntimeTransform(retainedEntity,reducedTransform),"ordinary state change removes consumer retention condition");retainedWorld.FixedScripts(nullptr,1.f/60);
 check(pump(retainedStream,[&]{return retainedStream.Stats().active==0&&retainedStream.Stats().pending==0;}),"pressure clears and retirement proceeds without budget change");check(retainedStream.Stats().retainedBytes>0&&retainedStream.Stats().retainedBytes<=retainedManifest.retainedBytes,"retired bytes honestly accounted within declared budget");std::printf("RETENTION bytes=%zu budget=%zu\n",retainedStream.Stats().retainedBytes,retainedManifest.retainedBytes);
 retainedToken=retainedStream.Request("state",false,error);check(pump(retainedStream,[&]{auto status=retainedStream.Status(retainedToken);return status&&status->state=="active";}),"shrunk retained state restores through normal request");retainedWorld.FixedScripts(nullptr,1.f/60);records=retainedWorld.Scripts()->Capture();check(records.size()==1&&records[0].json.find("17")!=std::string::npos&&records[0].json.find(std::string(8192,'x'))==std::string::npos,"restored state preserves marker and consumer-authored payload change");
 std::string invalid="JudasWorld 1\nregion \"a\" \"Scenes/a.judas\" 0 0 0 0 0 0 1 4 4 4 0 \"snapshot\" 10000 \"b\"\nregion \"b\" \"Scenes/b.judas\" 0 0 0 0 0 0 1 4 4 4 0 \"snapshot\" 10000 \"a\"\n";WorldManifest parsed;check(!ParseWorldManifest(invalid,parsed,error)&&error.find("cyclic")!=std::string::npos,"dependency cycle fails clearly");
 // A region with malformed content fails without touching an active group.
 std::ofstream(root/"Scenes/b.judas")<<"not a scene";other=stream.Request("b",false,error);check(pump(stream,[&]{return stream.Status(other)->state=="failed";}),"malformed source safely fails");check(stream.Resolve("a",1)!=0,"unrelated active region remains valid");
  // Completion order cannot change gravity ownership; placement rotates geometry/field together.
 auto rotated=source();rotated.Find(3)->gravity->magnitude=3;SaveSceneToFile(rotated,(root/"Scenes/a.judas").string(),error);
 auto radial=source();radial.Find(3)->gravity->kind=SceneGravityKind::Radial;radial.Find(3)->gravity->magnitude=9;SaveSceneToFile(radial,(root/"Scenes/b.judas").string(),error);
 WorldManifest overlap=manifest;overlap.regions.at("a").rotation=glm::angleAxis(glm::radians(90.f),glm::vec3(0,0,1));overlap.regions.at("a").priority=5;overlap.regions.at("b").origin=a.origin;
 auto gravityRun=[&](bool reverse){RuntimeWorld w;w.Build(bootstrap,&resources,error);WorldStreaming regions(w,resources,p,overlap);auto x=regions.Request(reverse?"b":"a",false,error);pump(regions,[&]{return regions.Status(x)->state=="active";});auto y=regions.Request(reverse?"a":"b",false,error);pump(regions,[&]{return regions.Status(y)->state=="active";});auto g=w.Gravity().Sample({0,1,0});auto body=w.RuntimeBody(regions.Resolve("a",2));auto pose=w.Physics().GetTransform(body);check(glm::length(pose.position-glm::vec3(-.5f,0,0))<1e-5,"oblique body and field use same relocated frame");return g;};
 auto g1=gravityRun(false),g2=gravityRun(true);check(glm::length(g1-g2)<1e-6&&glm::length(g1-glm::vec3(3,0,0))<1e-5,"reversed completion preserves authored uniform precedence");
 // Many authored declarations do not create many live worlds. Repeated visits retain a bounded set.
 WorldManifest many=manifest;many.regions.clear();for(int i=0;i<96;++i){auto r=a;r.id="sector-"+std::to_string(i);r.origin.x+=i*16;many.regions[r.id]=r;}many.unitsPerFrame=16;
 RuntimeWorld modest;modest.Build(bootstrap,&resources,error);WorldStreaming travel(modest,resources,p,many);size_t peak=0;uint64_t token=0;bool visits=true;
 for(int i=0;i<20;++i){if(token)travel.Release(token);auto id="sector-"+std::to_string(i%4);token=travel.Request(id,false,error);visits&=pump(travel,[&]{return travel.Status(token)->state=="active"&&travel.Stats().active==1;});peak=std::max(peak,modest.Physics().AliveBodyCount());}
 check(visits&&peak==2&&travel.Stats().active==1,"96 declared regions / repeated visits retain one physical working set");check(travel.Stats().retainedBytes<many.retainedBytes&&travel.Stats().pendingBytes==0,"retained metadata and pending bytes plateau after visits");
 // Cancellation before parsing and after preparation must release its own demand only.
 std::atomic<bool> held{false};travel.SetPreparationGate([&](const JobContext& c){while(!held&&!c.CancelRequested())std::this_thread::sleep_for(1ms);return !c.CancelRequested();});
 auto cancel=travel.Request("sector-8",false,error);travel.Advance(false);travel.Release(cancel);check(pump(travel,[&]{for(auto& r:travel.Regions())if(r.id=="sector-8")return r.state=="cancelled";return false;}),"cancellation while real reader is held");held=true;cancel=travel.Request("sector-9",true,error);check(pump(travel,[&]{return travel.Status(cancel)->state=="prepared";}),"second preload reaches immutable prepared state");travel.Release(cancel);check(pump(travel,[&]{for(auto& r:travel.Regions())if(r.id=="sector-9")return r.state=="unloaded";return false;}),"cancellation after preparation discards product without activation");

  // Existing script.state retention is used, not a separate VM snapshot.
 std::ofstream(root/"Assets/state.js")<<"export default class {constructor(){this.state={ticks:0};} fixedUpdate(){this.state.ticks++;}}";
 AssetRecord script;check(db.Track((root/"Assets/state.js").string(),script,error),"registered session-state script");auto jsSource=source();jsSource.Find(2)->scripts.push_back({1,script.id,true,"{}"});SaveSceneToFile(jsSource,(root/"Scenes/js.judas").string(),error);
 WorldManifest jsManifest=manifest;jsManifest.regions.clear();auto jsRegion=a;jsRegion.id="js";jsRegion.scene="Scenes/js.judas";jsManifest.regions["js"]=jsRegion;
 RuntimeWorld scripted;scripted.Build(bootstrap,&resources,error);WorldStreaming jsRegions(scripted,resources,p,jsManifest);auto jsToken=jsRegions.Request("js",false,error);pump(jsRegions,[&]{return jsRegions.Status(jsToken)->state=="active";});
 for(int i=0;i<7;++i){scripted.FixedScripts(nullptr,1.f/60);}
 auto captured=scripted.Scripts()->Capture();check(captured.size()==1&&captured[0].json.find("7")!=std::string::npos,"real VM produces retained JSON state");
 jsRegions.Release(jsToken);check(pump(jsRegions,[&]{return jsRegions.Regions()[0].state=="unloaded";}),"scripted region suspension removes instance");jsToken=jsRegions.Request("js",false,error);pump(jsRegions,[&]{return jsRegions.Status(jsToken)->state=="active";});scripted.FixedScripts(nullptr,1.f/60);captured=scripted.Scripts()->Capture();check(captured.size()==1&&captured[0].json.find("8")!=std::string::npos,"recreated script resumes JSON rather than resetting");
 // Scope-bound demands release independently of another consumer.
 auto scoped=jsRegions.Request("js",true,error,jsRegions.Resolve("js",2),1);jsRegions.ReleaseRequester(jsRegions.Resolve("js",2),1);check(!jsRegions.Status(scoped)&&jsRegions.Status(jsToken),"removed requester releases only its slot demand");

  // Qualified body and parent references connect normal registrations across groups.
 auto joined=source();SceneObject joint;joint.id=4;joint.joint=SceneJointComponent{};joint.joint->bodyA=2;joined.Objects().push_back(joint);joined.SetNextId(20);SaveSceneToFile(joined,(root/"Scenes/joined.judas").string(),error);
 WorldManifest qualified=manifest;qualified.regions.at("b").scene="Scenes/joined.judas";qualified.regions.at("b").dependencies={"a"};qualified.references.push_back({"b","bodyB","a",4,2,true});
 RuntimeWorld coupled;coupled.Build(bootstrap,&resources,error);WorldStreaming groups(coupled,resources,p,qualified);auto qb=groups.Request("b",false,error);check(pump(groups,[&]{return groups.Status(qb)->state=="active";}),"hard dependency installs qualified joint target first");auto linked=groups.Resolve("b",4);check(coupled.RuntimeJoint(linked).IsValid(),"cross-region joint uses ordinary M45 handle");auto qa=groups.Request("a",false,error);groups.Release(qa);groups.Advance(false);check(groups.Regions()[0].state=="active","dependent group retains external constrained body");
 auto bodyB=groups.Resolve("b",2);auto poseB=*coupled.RuntimeDefinition(bodyB);poseB.transform.position={0,.4f,0};coupled.SetRuntimeTransform(bodyB,poseB.transform);coupled.Physics().Step(1.f/60);check(coupled.Physics().AliveBodyCount()==4&&coupled.Physics().Raycast({0,4,0},{0,-1,0},10).hit,"ordinary cross-group geometry queried in shared broadphase");

 auto floorA=groups.Resolve("a",1);bool crossContact=false;for(auto& event:coupled.Physics().LastStepTouchEvents()){auto left=coupled.EntityIdOfBody(event.a),right=coupled.EntityIdOfBody(event.b);crossContact|=(left==floorA&&right==bodyB)||(right==floorA&&left==bodyB);}check(crossContact,"actual contacts from additive geometry reach the shared authoritative solver");
 JointState changedJoint;coupled.Physics().GetJoint(coupled.RuntimeJoint(linked),changedJoint);changedJoint.settings.motor=true;changedJoint.settings.speed=2.5f;coupled.Physics().SetJoint(coupled.RuntimeJoint(linked),changedJoint.settings);
 groups.Release(qb);check(pump(groups,[&]{return groups.Stats().active==0;}),"qualified constrained groups suspend in dependency order");qb=groups.Request("b",false,error);pump(groups,[&]{return groups.Status(qb)->state=="active";});check(coupled.RuntimeJoint(groups.Resolve("b",4)).IsValid(),"retained qualified joint rebinds fresh neighbour identity");
 JointState resumedJoint;check(coupled.Physics().GetJoint(coupled.RuntimeJoint(groups.Resolve("b",4)),resumedJoint)&&resumedJoint.settings.motor&&resumedJoint.settings.speed==2.5f,"runtime joint control settings survive suspension without stale native body handles");
 // Removing a body-less regional object must not invalidate unrelated world-anchored joints.
 SceneObject anchored;anchored.id=coupled.AllocateRuntimeEntityId();anchored.joint=SceneJointComponent{};anchored.joint->bodyA=groups.Resolve("b",2);
 check(coupled.StageRegionObject(anchored,anchored,error),"ordinary world-anchored regional joint registration");coupled.PublishRegion({anchored.id});
 auto retainedHinge=coupled.RuntimeJoint(anchored.id);check(retainedHinge.IsValid(),"world-anchored hinge has a real native handle");
 SceneObject plain;plain.id=coupled.AllocateRuntimeEntityId();plain.name="body-less regional marker";
 check(coupled.StageRegionObject(plain,plain,error),"body-less regional registration");coupled.PublishRegion({plain.id});coupled.RemoveRegionObject(plain.id);
 JointState intact;check(coupled.Physics().GetJoint(retainedHinge,intact),"body-less region teardown preserves existing physical joint handles");
 auto motorSource=source();SceneObject motor;motor.id=8;motor.characterMotor=CharacterMotorSettings{};motor.transform.position={0,2,0};motorSource.InsertObject(motor);SaveSceneToFile(motorSource,(root/"Scenes/motor.judas").string(),error);WorldManifest motorManifest=manifest;motorManifest.regions.clear();motorManifest.regions["a"]=a;motorManifest.regions["a"].scene="Scenes/motor.judas";RuntimeWorld motorWorld;motorWorld.Build(bootstrap,&resources,error);WorldStreaming motorStream(motorWorld,resources,p,motorManifest);auto motorToken=motorStream.Request("a",false,error);pump(motorStream,[&]{return motorStream.Status(motorToken)->state=="active";});auto* motion=motorWorld.RuntimeCharacter(motorStream.Resolve("a",8));motion->velocity={1,2,3};motorWorld.UpdateCharacters(0);motorStream.Release(motorToken);pump(motorStream,[&]{return motorStream.Stats().active==0;});motorToken=motorStream.Request("a",false,error);pump(motorStream,[&]{return motorStream.Status(motorToken)->state=="active";});motion=motorWorld.RuntimeCharacter(motorStream.Resolve("a",8));check(motion&&motion->velocity==glm::vec3(1,2,3),"idle motor root velocity survives ordinary region suspension");
 auto delta=CaptureWorldState(world);check(!ApplyWorldState(world,delta,error)&&error.find("composed")!=std::string::npos,"composed disk delta explicitly rejected");
 // A world endpoint is joint-entity-local authored data, transformed once.
 auto anchorScene=source();SceneObject localJoint;localJoint.id=9;localJoint.transform.position={1,2,3};localJoint.joint=SceneJointComponent{};localJoint.joint->bodyA=2;localJoint.joint->settings.anchorB={2,0,0};localJoint.joint->settings.frameB=glm::angleAxis(.4f,glm::vec3(1,0,0));anchorScene.InsertObject(localJoint);SaveSceneToFile(anchorScene,(root/"Scenes/anchors.judas").string(),error);
 auto anchorRegion=a;anchorRegion.scene="Scenes/anchors.judas";anchorRegion.origin+=glm::dvec3(10,0,0);anchorRegion.rotation=glm::angleAxis(glm::half_pi<float>(),glm::vec3(0,0,1));WorldManifest anchorManifest=manifest;anchorManifest.regions={{"a",anchorRegion}};RuntimeWorld anchorWorld;anchorWorld.Build(bootstrap,&resources,error);WorldStreaming anchorStream(anchorWorld,resources,p,anchorManifest);auto anchorToken=anchorStream.Request("a",false,error);pump(anchorStream,[&]{return anchorStream.Status(anchorToken)->state=="active";});
 const auto expectedAnchor=glm::vec3(10,0,0)+anchorRegion.rotation*glm::vec3(3,2,3);const auto expectedFrame=anchorRegion.rotation*localJoint.joint->settings.frameB;JointState anchorState;
 auto correctAnchor=[&]{return anchorWorld.Physics().GetJoint(anchorWorld.RuntimeJoint(anchorStream.Resolve("a",9)),anchorState)&&glm::length(anchorState.settings.anchorB-expectedAnchor)<1e-5f&&std::abs(glm::dot(anchorState.settings.frameB,expectedFrame))>1-1e-5f;};
 check(correctAnchor(),"translated/oblique world joint endpoint transforms exactly once");anchorStream.Release(anchorToken);pump(anchorStream,[&]{return anchorStream.Stats().active==0;});anchorToken=anchorStream.Request("a",false,error);pump(anchorStream,[&]{return anchorStream.Status(anchorToken)->state=="active";});check(correctAnchor(),"suspended world joint endpoint retains correct local frame on revisit");
 // A physics articulation cannot publish before its shared skeleton geometry is ready.
 Scene authoredRagdoll;check(LoadSceneFromFile("projects/ragdoll_demo/Scenes/main.judas",authoredRagdoll,error),"existing ordinary ragdoll fixture loads");
 std::filesystem::copy_file("projects/animation_demo/Assets/models/bend_bar.glb",root/"Assets/skin.glb",std::filesystem::copy_options::overwrite_existing);AssetRecord skeletalAsset;db.Track((root/"Assets/skin.glb").string(),skeletalAsset,error);
 Scene skinSource;auto skin=*authoredRagdoll.Find(10);skin.id=1;skin.parent=0;skin.scripts.clear();skin.render->meshAsset=skeletalAsset.id;skinSource.InsertObject(skin);skinSource.SetNextId(2);SaveSceneToFile(skinSource,(root/"Scenes/skin.judas").string(),error);
 auto skinRegion=a;skinRegion.scene="Scenes/skin.judas";PreparedWorldRegion skinProduct;
 check(PrepareWorldRegion(skinRegion,p,db,skinProduct,error)&&std::find(skinProduct.requiredGeometry.begin(),skinProduct.requiredGeometry.end(),skeletalAsset.id)!=skinProduct.requiredGeometry.end(),"skeletal geometry declared as required before ragdoll publication");
 // Matching interest priority must not boost distant explicit demands.
 WorldManifest priorities=manifest;priorities.maxPreparing=1;priorities.regions.at("a").halfExtents={1,1,1};priorities.regions.at("b").halfExtents={1,1,1};
 RuntimeWorld priorityWorld;priorityWorld.Build(bootstrap,&resources,error);WorldStreaming priorityStream(priorityWorld,resources,p,priorities);
 std::atomic<bool> priorityGate{false};priorityStream.SetPreparationGate([&](const JobContext& c){while(!priorityGate&&!c.CancelRequested())std::this_thread::sleep_for(1ms);return !c.CancelRequested();});
 auto priorityToken=priorityStream.Request("a",false,error);priorityStream.Interest("near-b",{8,0,0},0,1,100,error);priorityStream.Advance(false);
 auto rows=priorityStream.Regions();check(rows[0].state=="blocked"&&rows[1].state=="preparing","localized high priority wins one-reader budget without boosting remote demand");priorityGate=true;priorityStream.Release(priorityToken);priorityStream.RemoveInterest("near-b");
 check(!priorityStream.Interest("invalid",{0,0,0},0,1,100001,error),"interest rejects out-of-range priority before native conversion");
 // Actual reserved CPU particle storage participates in preparation/live accounting.
 auto particleScene=source();particleScene.Find(2)->particleEmitter=ParticleEmitterSettings{};particleScene.Find(2)->particleEmitter->maxParticles=10000;
 SaveSceneToFile(particleScene,(root/"Scenes/particles.judas").string(),error);WorldRegion particleRegion=a;particleRegion.scene="Scenes/particles.judas";PreparedWorldRegion particleProduct;
 check(PrepareWorldRegion(particleRegion,p,db,particleProduct,error)&&particleProduct.bytes>=10000*(sizeof(VisualParticle)+sizeof(ParticleBillboard)),"reserved particle pool memory included before installation");
 WorldManifest byteCap=manifest;byteCap.regions.clear();byteCap.regions["a"]=particleRegion;byteCap.pendingBytes=100000;
 RuntimeWorld bytesWorld;bytesWorld.Build(bootstrap,&resources,error);WorldStreaming byteStream(bytesWorld,resources,p,byteCap);auto byteToken=byteStream.Request("a",false,error);
 check(pump(byteStream,[&]{return byteStream.Status(byteToken)->state=="failed";})&&bytesWorld.Physics().AliveBodyCount()==0,"oversized prepared particle storage fails cleanly instead of changing quality");
 WorldManifest distant=manifest;distant.regions.clear();distant.regions["a"]=a;distant.regions["a"].origin.x=1e300;
 RuntimeWorld distantWorld;distantWorld.Build(bootstrap,&resources,error);WorldStreaming distantStream(distantWorld,resources,p,distant);auto distantToken=distantStream.Request("a",false,error);
 check(pump(distantStream,[&]{return distantStream.Status(distantToken)->state=="failed";})&&distantWorld.Physics().AliveBodyCount()==0,"unrepresentable fixed-frame placement fails before entity registration");
 auto ordinary=std::make_shared<SceneSession>(p,"Scenes/a.judas");auto begin=std::chrono::steady_clock::now();for(int i=0;i<100000;++i)ordinary->AdvanceStreaming(world,false);auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();std::printf("M59_INACTIVE 100000 single-scene early returns %.6f ms\n",elapsed);
 std::printf("M59 %d checks %d failures\n",checks,failures);return failures?1:0;
}
