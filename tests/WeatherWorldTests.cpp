#include "RuntimeWorld.h"
#include "WorldPersistence.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "Prefab.h"
#include "../third_party/nlohmann/json.hpp"
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <cstdio>
#include <limits>

namespace {
using V=glm::dvec3;using J=nlohmann::json;
unsigned checks=0,failures=0;
void Check(bool v,const char* what){++checks;failures+=!v;std::printf("%s %s\n",v?"PASS":"FAIL",what);}
Scene Fixture(){
 Scene scene;scene.Settings().name="M86 physical weather";
 auto& weather=scene.CreateObject("field");weather.weather=WeatherSettings{};weather.weather->profile.wind={10,0,0};weather.weather->profile.gust=0;
 for(unsigned i=0;i<3;++i){auto& b=scene.CreateObject("ordinary prop");b.transform.position={0,2,float(i*4)};b.body=SceneBodyComponent{};b.body->motion=SceneBodyMotion::Dynamic;b.body->mass=10;b.body->halfExtents={.25f,.25f,.25f};if(i!=1)b.weatherResponse=WeatherResponseSettings{true,2,1,30,0xffffffffu};}
 auto& wall=scene.CreateObject("windbreak");wall.transform.position={-2,2,8};wall.body=SceneBodyComponent{};wall.body->halfExtents={.15f,3,1};return scene;
}
void Step(RuntimeWorld& w,float dt=1.f/60){w.UpdateWeather(dt);w.Physics().Step(dt);}
bool Build(RuntimeWorld& w,const Scene& s,std::string& e){w.legacyGameplay=false;bool result=w.Build(s,nullptr,e);if(!result)std::printf("BUILD %s\n",e.c_str());return result;}
bool Near(V a,V b,double tolerance=1e-5){return glm::length(a-b)<tolerance;}
}
int main(){
 std::setvbuf(stdout,nullptr,_IOLBF,0);std::string error;const auto scene=Fixture();RuntimeWorld a,b;
 Check(Build(a,scene,error)&&Build(b,scene,error),"ordinary optional weather and opted-in bodies build headlessly");
 Check(!a.SetTransientEntity(1,true,error)&&!a.IsTransientEntity(1),"required global weather ownership cannot be excluded from the save scene");
 auto sample=a.SampleWeather({0,2,0});Check(sample.active&&Near(sample.wind,{10,0,0}),"public world query resolves the authored wind in world axes");
 const auto body=a.RuntimeBody(2),unaffected=a.RuntimeBody(3),covered=a.RuntimeBody(4);
 const auto expected=WeatherDragForce(sample,V(0),10,*scene.Find(2)->weatherResponse)/10./60.;
 a.weatherQuality=0;b.weatherQuality=2;Step(a);Step(b);
 const auto open=V(a.Physics().GetLinearVelocity(body)),blocked=V(a.Physics().GetLinearVelocity(covered));
 Check(Near(open,expected,5e-5)&&open.x>.1,"existing force accumulator integrates actual prescribed wind drag");
 Check(glm::length(a.Physics().GetLinearVelocity(unaffected))<1e-8,"negative control: ordinary bodies without opt-in receive no weather force");
 Check(blocked.x>0&&blocked.x<open.x*.2,"ordinary upstream collision geometry shelters opted-in wind response");
 Check(Near(V(b.Physics().GetLinearVelocity(b.RuntimeBody(2))),open,1e-7)&&Near(b.SampleWeather({0,2,0}).wind,a.SampleWeather({0,2,0}).wind,1e-12),"graphics quality off/high cannot change physical weather or forces");
 Check(a.WeatherStats().forceQueries==2&&a.WeatherStats().forces==2,"wind work counts actual bounded response queries and forces");
 // Layer filtering changes the real shelter ray, not a test-side visibility flag.
 Scene filtered=scene;filtered.Find(4)->weatherResponse->occlusionMask=CategoryBit(1);RuntimeWorld filterWorld;Check(Build(filterWorld,filtered,error),"filtered wind fixture builds");Step(filterWorld);Check(Near(V(filterWorld.Physics().GetLinearVelocity(filterWorld.RuntimeBody(4))),open,5e-5),"wind shelter obeys normal collision-layer query filtering");
 Scene calm=scene;calm.Find(1)->weather->profile.wind=V(0);calm.Find(2)->body->initialLinearVelocity={4,0,0};RuntimeWorld still;Check(Build(still,calm,error),"still-air relative drag fixture builds");Step(still);Check(still.Physics().GetLinearVelocity(still.RuntimeBody(2)).x<4&&still.Physics().GetLinearVelocity(still.RuntimeBody(2)).x>3.9,"moving bodies feel relative-air drag even when authored wind is zero");
 Scene disabled=scene;disabled.Find(2)->weatherResponse->enabled=false;RuntimeWorld no;Check(Build(no,disabled,error),"disabled opt-in fixture builds");Step(no);Check(glm::length(no.Physics().GetLinearVelocity(no.RuntimeBody(2)))<1e-8,"disabled response is an actual no-force negative control");
 // Rotate the entire configuration. Weather frame up is independent of the
 // support normal and of any global convention in the rigid solver.
 Scene rotated=scene;const auto q=glm::angleAxis(glm::radians(73.f),glm::normalize(glm::vec3(1,2,3)));
 for(auto& o:rotated.Objects()){o.transform.position=q*o.transform.position;o.transform.rotation=q*o.transform.rotation;}
 RuntimeWorld turned;Check(Build(turned,rotated,error),"arbitrary-orientation weather scene builds");Step(turned);Check(Near(V(turned.Physics().GetLinearVelocity(turned.RuntimeBody(2))),V(q*glm::vec3(open)),5e-5),"rotated wind and windbreak produce the same physical response");
 // A radial atmosphere supports local circulation at Earth radius; visual
 // profiles remain available above its physical air-density envelope.
 Scene radial;auto& r=radial.CreateObject("radial field");r.weather=WeatherSettings{};r.weather->frame.radial=true;r.weather->frame.center={9e11,0,0};r.weather->frame.radius=6371000;r.weather->profile.wind={10,3,0};r.weather->profile.gust=0;RuntimeWorld planet;Check(Build(planet,radial,error),"Earth-size absolute weather frame builds without float placement");
 auto ground=planet.SampleWeather({9e11,6371002,0}),orbit=planet.SampleWeather({9e11,6471000,0});Check(Near(ground.up,{0,1,0})&&std::abs(ground.altitude-2)<1e-6&&std::abs(glm::dot(ground.wind,ground.up))<1e-8,"radial wind samples a local tangent and meaningful double altitude");Check(orbit.active&&glm::length(orbit.wind)<1e-8&&orbit.airDensity==0&&orbit.profile.cloudCoverage>0,"orbit retains cloud-frame presentation while physical air fades to zero");
 // A zone feather must occur once. Missing or squared weighting produces 5 or
 // 2.5 m/s instead of this authored half blend respectively.
 Scene layers;auto& base=layers.CreateObject("lower");base.weather=WeatherSettings{};base.weather->profile.wind={0,0,0};base.weather->profile.gust=0;
 auto& over=layers.CreateObject("upper");over.weather=WeatherSettings{};over.weather->profile.wind={10,0,0};over.weather->profile.gust=0;over.weather->regionShape=WeatherRegionShape::Sphere;over.weather->regionRadius=10;over.weather->blendDistance=10;over.weather->priority=1;
 RuntimeWorld blend;Check(Build(blend,layers,error),"overlapping weather fields build");Check(std::abs(blend.SampleWeather({15,0,0}).wind.x-5)<1e-8&&std::abs(blend.SampleWeather({15,0,0}).airDensity-1.225)<1e-8,"priority feather blends the region once without squaring physical fade");Check(std::abs(blend.SampleWeather({25,0,0}).wind.x)<1e-8,"outside the regional feather only the lower field remains");
 // Chart changes cannot move absolute fields or their already-emitted events.
 const auto absolute=a.Settings().worldOrigin+V(a.Physics().GetTransform(body).position);const auto before=a.SampleWeather(absolute);a.RuntimeWeather(1)->TriggerLightning({50,0,-100},1);a.UpdateWeather(0);std::string captured;Check(a.CaptureWeather(1,captured,error),"weather event and pending propagation queue capture");
 Check(a.RequestOrigin({512,0,512},error)&&a.CommitOrigin(),"ordinary wind props permit the existing checked M85 origin transaction");auto after=a.SampleWeather(absolute);Check(Near(before.wind,after.wind,1e-12)&&Near(before.frame.center,after.frame.center,1e-12)&&Near(after.lightningPosition,{50,0,-100},1e-12),"absolute weather frame, wind and strike remain anchored through origin shift");
 // Physical ownership: a component attached to a moved ordinary body follows
 // its actual body pose, including rotation, without losing runtime transition.
 Scene mobile;auto& m=mobile.CreateObject("moving region");m.body=SceneBodyComponent{};m.body->motion=SceneBodyMotion::Dynamic;m.body->initialLinearVelocity={3,0,0};m.weather=WeatherSettings{};m.weather->regionShape=WeatherRegionShape::Sphere;m.weather->regionRadius=1;m.weather->blendDistance=0;m.weather->profile.gust=0;
 RuntimeWorld moving;Check(Build(moving,mobile,error),"weather can be owned by an ordinary moving body");for(int i=0;i<60;++i)Step(moving);moving.UpdateWeather(0);auto pose=moving.Physics().GetTransform(moving.RuntimeBody(1));Check(moving.SampleWeather(V(pose.position)).active&&!moving.SampleWeather({0,0,0}).active,"weather region tracks actual physical owner motion rather than stale authoring transform");
 // Per-owner restore is an atomic parse/validate/publish operation. Corruption
 // of a late queue entry must leave the transition, clock and strike unchanged.
 std::string record;Check(a.CaptureWeather(1,record,error),"rebased weather continuation captures");auto corrupt=J::parse(record);corrupt["thunder"].push_back({{"due",12},{"position",J::array({1,2,3})},{"energy",1.1}});const auto source=a.RuntimeWeather(1)->Capture();Check(!a.RestoreWeather(1,corrupt.dump(),error)&&a.RuntimeWeather(1)->Capture()==source,"malformed pending thunder rejects atomically without mutating live weather");Check(!a.RestoreWeather(1,"",error)&&a.RuntimeWeather(1)->Capture()==source,"missing continuation cannot silently reset an active field");
 // Full cold scene+participant restoration, not a cached status comparison.
 auto* runtime=a.RuntimeWeather(1);auto target=runtime->Current();target.rain=.8;target.wind={4,0,8};runtime->Transition(target,4);for(int i=0;i<30;++i)Step(a);
 std::map<std::string,SaveChunk> chunks;Check(WorldPersistence::Capture(a,chunks,error)&&chunks.count("weather"),"normal full save contains explicit weather participant");Scene saved;Check(WorldPersistence::SceneFrom(chunks,saved,error),"saved authored state reconstructs its current chart");RuntimeWorld cold;Check(Build(cold,saved,error)&&WorldPersistence::Prepare(cold,error)&&WorldPersistence::Restore(cold,chunks,error),"fresh-process equivalent restoration reconstructs weather before simulation resumes");
 Check(cold.RuntimeWeather(1)&&cold.RuntimeWeather(1)->Capture()==a.RuntimeWeather(1)->Capture(),"cold weather has exact transition, absolute strike, clock and tornado travel");for(int i=0;i<60;++i){Step(a);Step(cold);}const auto aa=a.Physics().GetTransform(a.RuntimeBody(2)),bb=cold.Physics().GetTransform(cold.RuntimeBody(2));Check(Near(V(aa.position),V(bb.position),2e-5)&&Near(V(a.Physics().GetLinearVelocity(a.RuntimeBody(2))),V(cold.Physics().GetLinearVelocity(cold.RuntimeBody(2)))),"actual restored wind-driven body endpoint follows uninterrupted physical control");
 auto missing=chunks;missing.erase("weather");RuntimeWorld absent;Check(Build(absent,saved,error)&&!WorldPersistence::Restore(absent,missing,error),"weather-bearing save rejects a missing participant");
 std::string text;SaveSceneToString(scene,text);Scene parsed;Check(LoadSceneFromString(text,parsed,error)&&ScenesEqual(scene,parsed),"scene serialization preserves weather profiles and response authoring");auto properties=ObjectProperties(*scene.Find(1));auto copy=*scene.Find(1);Check(ApplyObjectProperties(copy,properties,error)&&copy.weather&&EncodeWeatherSettings(*copy.weather)==EncodeWeatherSettings(*scene.Find(1)->weather),"normal prefab property roundtrip preserves complete weather configuration");
 // No stale runtime source survives owner retirement, authored reset or reuse.
 const auto stale=a.RuntimeBody(2);Check(a.DestroyEntity(2)&&!a.Physics().IsBodyEnabled(stale),"wind response destruction retires the actual physical body");Check(a.DestroyEntity(1)&&a.RuntimeWeather(1)==nullptr&&!a.SampleWeather(absolute).active,"destroyed region and stale ID cannot keep providing weather");SceneObject replacement=*scene.Find(2);replacement.id=0;replacement.transform.position={0,8,0};auto next=a.CreateEntity(replacement,nullptr,&error);Check(next&&a.RuntimeBody(next).id!=stale.id&&a.RuntimeWeather(1)==nullptr,"body slot reuse cannot resurrect the retired weather owner");a.UpdateWeather(1.f/60);
 b.RestoreAuthoredState();Check(b.RuntimeWeather(1)&&b.RuntimeWeather(1)->State().time==0,"authored reset reconstructs weather and clears runtime transitions/events");
 Scene bad=scene;bad.Find(2)->body->motion=SceneBodyMotion::Static;RuntimeWorld invalid;Check(!Build(invalid,bad,error),"negative authoring control rejects wind response without dynamic body ownership");
 // Runtime transform publication must preflight weather descriptors. A bad
 // transform must not become a later uncaught exception in fixed simulation.
 Scene bounded;auto& boundedOwner=bounded.CreateObject("bounded weather only");boundedOwner.weather=WeatherSettings{};boundedOwner.weather->regionShape=WeatherRegionShape::Sphere;boundedOwner.weather->regionRadius=2;boundedOwner.weather->blendDistance=0;
 RuntimeWorld placement;Check(Build(placement,bounded,error),"unscripted weather-only owner builds through normal identity path");
 auto invalidPlacement=boundedOwner.transform;invalidPlacement.scale.x=0;const bool rejectedPlacement=!placement.SetRuntimeTransform(1,invalidPlacement);bool safeAdvance=true;try{placement.UpdateWeather(0);}catch(const std::exception& e){safeAdvance=false;std::printf("TRANSFORM FAILURE %s\n",e.what());}
 Check(rejectedPlacement&&safeAdvance&&placement.SampleWeather({0,0,0}).active,"invalid weather scale rejects atomically before fixed simulation");
 placement.Destroy();Check(Build(placement,bounded,error),"fresh bounded weather control builds");auto movedPose=boundedOwner.transform;movedPose.position={100,0,0};Check(placement.SetRuntimeTransform(1,movedPose),"valid bounded weather transform remains scriptable");placement.UpdateWeather(0);Check(placement.SampleWeather({100,0,0}).active&&!placement.SampleWeather({0,0,0}).active,"moving bounded owner changes actual field location");
 placement.RestoreAuthoredState();Check(placement.SampleWeather({0,0,0}).active&&!placement.SampleWeather({100,0,0}).active,"authored reset restores field location before another fixed callback");
 auto outside=boundedOwner.transform;outside.position.x=2e12f;const auto originalPose=placement.RuntimeDefinition(1)->transform;Check(!placement.SetRuntimeTransform(1,outside)&&placement.RuntimeDefinition(1)->transform.position==originalPose.position,"out-of-contract absolute weather placement cannot publish partial state");
 std::printf("M86 WORLD %u checks %u failures\n",checks,failures);return failures?1:0;
}
