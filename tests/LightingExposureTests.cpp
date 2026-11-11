#include "lighting/LightingQueries.h"
#include "lighting/LightingWorld.h"
#include "LightAttenuation.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "ScriptSystem.h"
#include "WorldPersistence.h"
#include "InputSystem.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <glm/gtc/quaternion.hpp>
namespace {unsigned checks=0,failures=0;
void Check(bool yes,const char* text){++checks;failures+=!yes;std::printf("%s %s\n",yes?"PASS":"FAIL",text);}
}
void ParentDirection(){
 Scene scene;std::string error;
 auto& parent=scene.CreateObject("Lamp mount");const auto parentId=parent.id;
 parent.transform.rotation=glm::angleAxis(glm::half_pi<float>(),glm::vec3(0,1,0));
 auto& child=scene.CreateObject("Attached spotlight");child.parent=parentId;child.transform.position={0,0,-1};child.light=SceneLightComponent{};child.light->kind=SceneLightKind::Spot;
 RuntimeWorld world;Check(world.Build(scene,nullptr,error),"parented spotlight fixture builds through ordinary hierarchy");
 SceneTransform pose;Check(world.SetRuntimeTransform(parentId,pose),"lamp mount rotates back to the exact identity orientation");
 const auto lights=BuildWorldLights(world,1);
 if(!lights.empty())std::printf("CARDINAL_LIGHT direction %.9g %.9g %.9g position %.9g %.9g %.9g\n",lights[0].direction.x,lights[0].direction.y,lights[0].direction.z,lights[0].position.x,lights[0].position.y,lights[0].position.z);
 Check(lights.size()==1&&glm::length(lights[0].direction-glm::vec3(0,0,-1))<1e-6f&&glm::length(lights[0].position-glm::vec3(0,0,-1))<1e-6f,"attached light direction follows its current mount even when the resulting rotation is exactly identity");
}
int main(){ParentDirection();using namespace JudasLighting;std::string error;Scene scene;scene.Settings().sunEnabled=false;scene.Settings().ambientColor=glm::vec3(0);
 auto& lamp=scene.CreateObject("Emitter");const auto lightId=lamp.id;lamp.transform.position={0,2,0};lamp.light=SceneLightComponent{};lamp.light->intensity=10;lamp.light->range=10;lamp.body=SceneBodyComponent{};lamp.body->shape=SceneShape::Sphere;lamp.body->radius=.1f;
 auto& wall=scene.CreateObject("Occluder");const auto wallId=wall.id;wall.transform.position={0,1,0};wall.body=SceneBodyComponent{};wall.body->halfExtents={1,.1f,1};wall.render=SceneRenderComponent{};
 RuntimeWorld world;Check(world.Build(scene,nullptr,error),"ordinary simulation world owns light and occluder");ExposureOptions options;ExposureSample sample;
 Check(SampleExposure(world,{0,0,0},options,sample,error)&&sample.complete&&sample.queries==1&&sample.relativeIlluminance==0&&sample.upperIlluminance==0&&sample.contributors[0].occluded,"solid geometry occludes a current gameplay light without GPU readback");
 auto moved=scene.Find(wallId)->transform;moved.position.x=20;Check(world.SetRuntimeTransform(wallId,moved),"static geometry changes through current runtime placement");
 const float expected=10*ComputeDistanceAttenuation(2,10);
 Check(SampleExposure(world,{0,0,0},options,sample,error)&&std::abs(sample.relativeIlluminance-expected)<1e-5f&&sample.complete&&sample.contributors[0].occlusionKnown,"removed occlusion and ignored emitter collider produce independent attenuation answer immediately");
 options.maxOcclusionQueries=0;SampleExposure(world,{0,0,0},options,sample,error);
 Check(!sample.complete&&sample.relativeIlluminance==0&&std::abs(sample.upperIlluminance-expected)<1e-5f&&sample.unresolvedSources==1&&!sample.contributors[0].occlusionKnown,"exhausted ray budget reports unknown visibility with lower and upper estimates");
 options.maxOcclusionQueries=16;options.maxLights=0;SampleExposure(world,{0,0,0},options,sample,error);
 Check(!sample.complete&&sample.contributors.empty()&&sample.unresolvedSources==1&&sample.upperIlluminance>0,"contribution budget never silently discards possible illumination");
 options.maxLights=32;options.maxSources=0;SampleExposure(world,{0,0,0},options,sample,error);
 Check(!sample.complete&&!sample.upperBoundKnown&&sample.sourcesExamined==0,"unscanned sources explicitly invalidate the upper bound");
 options.maxSources=1024;options.normal=glm::vec3(1,0,0);SampleExposure(world,{0,0,0},options,sample,error);
 Check(sample.complete&&sample.queries==0&&sample.relativeIlluminance==0,"optional receiving normal respects incidence");options.normal.reset();
 auto light=*scene.Find(lightId)->light;light.enabled=false;world.SetLight(lightId,light);SampleExposure(world,{0,0,0},options,sample,error);
 Check(sample.complete&&sample.relativeIlluminance==0&&sample.queries==0,"disabled source immediately vanishes from gameplay exposure");
 light.enabled=true;light.kind=SceneLightKind::Spot;world.SetLight(lightId,light);SampleExposure(world,{0,0,0},options,sample,error);Check(sample.relativeIlluminance==0,"outside-cone spot negative control");
 auto aim=scene.Find(lightId)->transform;aim.rotation=glm::angleAxis(-glm::half_pi<float>(),glm::vec3(1,0,0));world.SetRuntimeTransform(lightId,aim);SampleExposure(world,{0,0,0},options,sample,error);
 Check(std::abs(sample.relativeIlluminance-expected)<1e-5f,"rotated spotlight uses authoritative world orientation");
 const auto retained=sample;options.maxLights=65;Check(!SampleExposure(world,{0,0,0},options,sample,error)&&sample.relativeIlluminance==retained.relativeIlluminance,"invalid budget rejects without changing the previous sample");options.maxLights=32;
 std::map<std::string,SaveChunk> chunks;Scene saved;RuntimeWorld cold;
 Check(WorldPersistence::Capture(world,chunks,error)&&WorldPersistence::SceneFrom(chunks,saved,error)&&cold.Build(saved,nullptr,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(cold,error)&&WorldPersistence::Restore(cold,chunks,error),"light orientation and parameters cold-restore through modern world persistence");ExposureSample restored;
 Check(SampleExposure(cold,{0,0,0},options,restored,error)&&restored.relativeIlluminance==retained.relativeIlluminance,"fresh restored world reproduces current exposure exactly");
 namespace fs=std::filesystem;const auto root=fs::absolute("build/lucifer-exposure-script");fs::create_directories(root/"Assets");
 std::ofstream(root/"Assets/probe.js")<<R"JS(import {world,entity} from 'judas';
export default class {constructor(){this.state={};}start(){
 this.state.sample=world.lightExposure({x:0,y:0,z:0},{ignored:[entity('2')]});
 this.state.limited=world.lightExposure({x:0,y:0,z:0},{maxSources:0});
 this.state.rejections=0;for(const fn of [()=>world.lightExposure({x:NaN,y:0,z:0}),()=>world.lightExposure({x:0,y:0,z:0},{maxLights:65}),()=>world.lightExposure({x:0,y:0,z:0},{maxOcclusionQueries:-1}),()=>world.lightExposure({x:0,y:0,z:0},{normal:{x:0,y:0,z:0}}),()=>world.lightExposure({x:0,y:0,z:0},{madeUp:true}),()=>world.lightExposure({x:0,y:0,z:0},{ignored:[entity('999999')]})])try{fn();}catch(e){if(e instanceof TypeError)++this.state.rejections;}
 }} )JS";
 AssetDatabase assets;assets.Scan(root.string(),(root/"Assets").string());AssetRecord script;Check(assets.Track((root/"Assets/probe.js").string(),script,error),"register original query probe through packaged scripting contracts");
 auto& owner=scene.CreateObject("Observer");const auto ownerId=owner.id;owner.scripts={{1,script.id,true,"{}"}};ResourceManager resources(nullptr,&assets);RuntimeWorld scripted;
 Check(scripted.Build(scene,&resources,error),"actual JudasJS world builds with module-local exposure binding");InputSystem input;scripted.UpdateScripts(&input,.016f);
 nlohmann::ordered_json state=nlohmann::ordered_json::object();if(scripted.Scripts())for(const auto& value:scripted.Scripts()->Capture())if(value.entity==ownerId)state=nlohmann::ordered_json::parse(value.json);
 if(scripted.Scripts())for(const auto& d:scripted.Scripts()->Diagnostics())std::printf("JS DIAGNOSTIC %s\n",d.message.c_str());
 Check(scripted.Scripts()&&scripted.Scripts()->Diagnostics().empty()&&state.value("rejections",0)==6,"public JS validation and exposure run without central dispatcher changes");
 Check(state.contains("sample")&&state["sample"].value("complete",false)&&state["sample"].value("relativeIlluminance",0.f)>0&&state["sample"].value("ageSeconds",-1.)==0,"public JS returns current, complete relative illumination with explicit age");
 Check(state.contains("limited")&&!state["limited"].value("complete",true)&&state["limited"]["upperBound"].is_null(),"public JS preserves incomplete/unknown-bound semantics");
 std::printf("Lighting exposure: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
