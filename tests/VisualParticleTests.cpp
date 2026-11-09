#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "WorldPresentation.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "UniformGravity.h"
#include "Prefab.h"
#include "editor/EditorDocument.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>
#include <chrono>
#include <algorithm>
#include <filesystem>
#include "ScreenshotWriter.h"
namespace {
int checks=0,failures=0;
void Check(bool b,const char* label){++checks;failures+=!b;std::printf("%s %s\n",b?"PASS":"FAIL",label);}
Scene Demo(){Scene s;s.Settings().name="Visibility and visual emitters";s.Settings().ambientColor={.5f,.5f,.5f};
 auto& floor=s.CreateObject("Floor");floor.transform.position={0,-.3f,0};floor.render=SceneRenderComponent{};floor.render->halfExtents={25,.3f,25};floor.body=SceneBodyComponent{};floor.body->halfExtents=floor.render->halfExtents;
 auto& player=s.CreateObject("Player start");player.transform.position={0,1.5f,8};
 auto& gravity=s.CreateObject("Uniform gravity");gravity.gravity=SceneGravityComponent{};gravity.gravity->kind=SceneGravityKind::Uniform;gravity.gravity->magnitude=3;gravity.gravity->regionShape=SceneRegionShape::Box;gravity.gravity->regionHalfExtents={50,50,50};
 for(int i=0;i<30;++i){auto& box=s.CreateObject("Ordinary prop");box.transform.position={float(i%10-5)*2,0.5f,float(i/10)*-4};box.render=SceneRenderComponent{};box.render->color={.2f+.06f*(i%5),.4f,.7f};}
 auto& smoke=s.CreateObject("Looping smoke");smoke.transform.position={-2,0.5f,0};smoke.particleEmitter=ParticleEmitterSettings{};smoke.particleEmitter->textureAsset="37373737373737373737373737373737";
 smoke.particleEmitter->rate=35;smoke.particleEmitter->lifetime=3;smoke.particleEmitter->endSize=.9f;
 auto& sparks=s.CreateObject("Gravity sparks");sparks.transform.position={2,2,0};sparks.particleEmitter=ParticleEmitterSettings{};auto& e=*sparks.particleEmitter;e.useGravity=true;e.velocity={0,3,0};e.velocityVariation={2,1,2};e.rate=12;e.burst=60;e.lifetime=2;e.size=.09f;e.endSize=.02f;e.color={1,.65f,.05f,1};e.endColor={1,.1f,0,0};
 auto& burst=s.CreateObject("Startup burst");burst.transform.position={0,2,-3};burst.particleEmitter=ParticleEmitterSettings{};burst.particleEmitter->loop=false;burst.particleEmitter->burst=100;burst.particleEmitter->velocityVariation={2,2,2};burst.particleEmitter->lifetime=4;burst.particleEmitter->color={.1f,.7f,1,1};burst.particleEmitter->endSize=.02f;
 auto& camera=s.CreateObject("Rear camera");camera.transform.position={0,3,-16};camera.transform.rotation=glm::angleAxis(glm::pi<float>(),glm::vec3(0,1,0));camera.renderCamera=SceneRenderCameraComponent{};
 const auto cameraId=camera.id;
 auto& screen=s.CreateObject("Rear-view screen");screen.transform.position={4,2,2};screen.render=SceneRenderComponent{};screen.render->halfExtents={1.5f,.9f,.05f};screen.render->color={1,1,1};screen.render->textureCamera=cameraId;
 return s;
}
}
int main(int argc,char** argv){
 std::string error; if(argc==3&&std::string(argv[1])=="--demo")return SaveSceneToFile(Demo(),argv[2],error)?0:2;
 std::filesystem::path out="build/m37-focused";if(argc==3&&std::string(argv[1])=="--output")out=argv[2];std::filesystem::create_directories(out);
 const auto projection=glm::perspective(glm::radians(60.f),1.f,.1f,100.f);Frustum f(projection);
 VisualBounds bounds;bounds.Include({-1,-1,-6});bounds.Include({1,1,-4});Check(f.IsVisible(bounds),"frustum accepts visible bounds");
 VisualBounds outside;outside.Include({99,0,-5});outside.Include({101,1,-4});Check(!f.IsVisible(outside),"frustum rejects outside bounds");
 auto model=glm::translate(glm::mat4(1),glm::vec3(0,0,-5))*glm::rotate(glm::mat4(1),.7f,glm::vec3(0,1,0))*glm::scale(glm::mat4(1),glm::vec3(2,.5f,3));
 VisualBounds local;local.Include({-1,-1,-1});local.Include({1,1,1});auto transformed=TransformBounds(local,model);bool encloses=true;
 for(int i=0;i<8;++i){auto v=glm::vec3(model*glm::vec4(i&1?1:-1,i&2?1:-1,i&4?1:-1,1));encloses&=glm::all(glm::greaterThanEqual(v,transformed.min))&&glm::all(glm::lessThanEqual(v,transformed.max));}
 Check(encloses&&f.IsVisible(transformed),"rotated scaled corner bounds conservative");
 auto rear=glm::lookAt(glm::vec3(100,0,0),glm::vec3(100,0,-1),glm::vec3(0,1,0));Check(Frustum(projection*rear).IsVisible(outside),"independent camera accepts other bounds");
 Check(Frustum(glm::ortho(-2.f,2.f,-2.f,2.f,.1f,10.f)).IsVisible(bounds),"orthographic projection uses same extraction");
 ParticleEmitterSettings config;config.rate=0;config.burst=4;config.lifetime=.5f;config.velocity={0,0,0};config.velocityVariation={0,0,0};config.spread={0,0,0};config.useGravity=true;
 UniformGravity gravity({3,0,0});VisualParticlePool pool(config),repeat(config);const glm::quat q(1,0,0,0);
 pool.Update(.1f,{0,0,-5},q,{1,1,1},gravity);repeat.Update(.1f,{0,0,-5},q,{1,1,1},gravity);
 Check(pool.Particles().size()==4,"startup burst fills reusable pool");pool.Update(.1f,{0,0,-5},q,{1,1,1},gravity);repeat.Update(.1f,{0,0,-5},q,{1,1,1},gravity);
 Check(pool.Particles()[0].velocity.x==.3f&&pool.Particles()[0].velocity.y==0,"actual arbitrary Judas gravity acceleration");
 Check(pool.Particles()[0].position==repeat.Particles()[0].position,"seed and updates repeat deterministically");
 const auto& billboards=pool.Presentation({0,0,-5},q,{1,1,1});bool contained=true;
 for(const auto& b:billboards)contained &= glm::all(glm::greaterThanEqual(b.position-glm::vec3(b.size*.7071068f),pool.Bounds().min))&&glm::all(glm::lessThanEqual(b.position+glm::vec3(b.size*.7071068f),pool.Bounds().max));
 Check(contained,"emitter bounds contain active billboard extents");
 pool.Update(.6f,{0,0,-5},q,{1,1,1},gravity);Check(pool.Particles().empty(),"expiry returns particles to pool");
 ParticleEmitterSettings moving;moving.rate=0;moving.burst=3;moving.localSpace=true;moving.spread={.4f,.2f,.3f};moving.velocityVariation={1,2,3};moving.seed=77;
 VisualParticlePool localPool(moving),localRepeat(moving);localPool.Update(.1f,{0,0,0},q,{1,1,1},gravity);localRepeat.Update(.1f,{0,0,0},q,{1,1,1},gravity);
 Check(localPool.Particles()[0].position==localRepeat.Particles()[0].position&&localPool.Particles()[0].velocity==localRepeat.Particles()[0].velocity,"nonzero random variation repeats with emitter seed");
 const auto beforeLocal=localPool.Particles()[0].position;const auto& localView=localPool.Presentation({3,2,1},q,{2,2,2});Check(glm::length(localView[0].position-(glm::vec3(3,2,1)+2.f*beforeLocal))<1e-6f,"local particles follow transformed emitter");
 Scene scene;auto& emitter=scene.CreateObject("Emitter");emitter.particleEmitter=config;emitter.transform.position={0,0,-5};const auto id=emitter.id;
 std::string serialized;SaveSceneToString(scene,serialized);Scene loaded;Check(LoadSceneFromString(serialized,loaded,error)&&ScenesEqual(scene,loaded),"all authored particle properties roundtrip");
 std::string a,b;ComputeSceneFingerprint(scene,a,error);loaded.Find(id)->particleEmitter->rate=1;ComputeSceneFingerprint(loaded,b,error);Check(a!=b,"particle changes invalidate authored baseline");
 Scene prefab;Check(CreatePrefab(scene,id,prefab,error)&&ValidatePrefab(prefab,error),"ordinary component works in prefab source");
 RuntimeWorld world;Check(world.Build(scene,nullptr,error),"ordinary runtime builds emitter");world.UpdateVisualParticles(.1f);
 Check(world.VisualEmitters().size()==1&&world.VisualEmitters()[0].pool.Particles().size()==4,"runtime update owns particle state");
 auto definition=scene.Objects()[0];definition.id=0;auto created=world.CreateEntity(definition,nullptr,&error);Check(created!=0&&world.EmitParticleBurst(created,2),"ordinary runtime entity supports burst API");Check(world.DestroyEntity(created,&error)&&world.VisualEmitters().size()==1,"destroying emitter releases its pool");
 EditorDocument doc;doc.GetScene()=scene;const auto authored=doc.GetScene();RuntimeWorld play;play.Build(doc.GetScene(),nullptr,error);play.UpdateVisualParticles(.2f);play.Destroy();Check(ScenesEqual(doc.GetScene(),authored),"Play particle state discarded without authored mutation");
 EngineHost host;Check(host.Init("M37 focused",256,256,false,error),"actual engine GL context");if(failures)return 1;auto& r=host.GetRenderer();
 r.SetCamera(glm::mat4(1),projection);r.BeginFrame(256,256);r.ResetStats();r.DrawBox({0,0,-5},q,{1,1,1},{1,0,0});r.DrawBox({100,0,-5},q,{1,1,1},{0,1,0});
 Check(r.Stats().renderablesConsidered==2&&r.Stats().renderablesCulled==1&&r.Stats().drawCalls==1,"culled ordinary mesh has no draw submission");
 std::vector<unsigned char> pixels;r.CaptureFrame(256,256,pixels);bool red=false;for(size_t i=0;i<pixels.size();i+=3)red |= pixels[i]>pixels[i+1]+20;Check(red,"visible ordinary mesh still renders actual pixels");
 VisualParticlePool visual(config);visual.Update(.1f,{100,0,-5},q,{1,1,1},gravity);const auto age=visual.Particles()[0].age;
 r.ResetStats();const auto& first=visual.Presentation({0,0,0},q,{1,1,1});r.DrawParticles(first,visual.Bounds());Check(r.Stats().particleEmittersCulled==1&&r.Stats().particlesSubmitted==0,"offscreen emitter no particle submission");
 visual.Update(.1f,{100,0,-5},q,{1,1,1},gravity);Check(visual.Particles()[0].age>age,"offscreen particles age coherently");
 r.SetCamera(rear,projection);const auto& second=visual.Presentation({0,0,0},q,{1,1,1});r.DrawParticles(second,visual.Bounds());Check(r.Stats().particleEmittersVisible==1&&r.Stats().particlesSubmitted==4,"same emitter visible from different camera");
 auto target=r.CreateRenderTarget(128,128,error);r.SetCamera(glm::mat4(1),projection);r.BeginRenderTarget(target);r.SetCamera(rear,projection);r.DrawParticles(second,visual.Bounds());r.EndRenderTarget();Check(!r.IsVisible(visual.Bounds()),"target end restores camera frustum");r.DestroyRenderTarget(target);
 {
 const auto root=std::filesystem::absolute(out/"particle-texture-swap");
 std::filesystem::create_directories(root/"Assets");
 const auto firstPath=root/"Assets/red.png",secondPath=root/"Assets/green.png";
 Check(WriteRgbPng(firstPath.string(),1,1,{255,0,0})&&
       WriteRgbPng(secondPath.string(),1,1,{0,255,0}),"write known-colour particle textures");
 AssetDatabase assets;
 assets.Scan(root.string(),(root/"Assets").string());
 AssetRecord redTexture,greenTexture;
 Check(assets.Track(firstPath.string(),redTexture,error)&&assets.Track(secondPath.string(),greenTexture,error),
       "register both runtime particle textures");
 ResourceManager resources(&r,&assets);
 auto texturedScene=scene;
 auto& settings=*texturedScene.Find(id)->particleEmitter;
 settings.textureAsset=redTexture.id;
 settings.color=settings.endColor=glm::vec4(1);
 settings.velocity=settings.velocityVariation=glm::vec3(0);
 RuntimeWorld texturedWorld;
 Check(texturedWorld.Build(texturedScene,&resources,error),"build textured particle world");
 resources.WaitForAll();
 texturedWorld.UpdateVisualParticles(.1f);
 auto capture=[&]{
    RenderWorldFrame(r,256,256,texturedWorld,glm::mat4(1),projection,{0,0,0},1);
    std::vector<unsigned char> pixels;
    r.CaptureFrame(256,256,pixels);
    return pixels;
 };
 const auto redPixels=capture();
 const auto count=texturedWorld.VisualEmitters().front().pool.Particles().size();
 auto next=settings;
 next.textureAsset=greenTexture.id;
 Check(texturedWorld.SetParticleSettings(id,next),"runtime texture swap uses registered texture");
 resources.WaitForAll();
 const auto greenPixels=capture();
 Check(redPixels!=greenPixels&&texturedWorld.VisualEmitters().front().pool.Particles().size()==count,
       "new texture changes actual pixels without resetting the live particles");
 texturedWorld.Destroy();
 }
 // Measured normal colour submission path; no physics or assets substituted.
 for(bool enabled:{false,true})for(bool facing:{true,false}){
  r.SetCullingEnabled(enabled);r.SetCamera(facing?glm::mat4(1):glm::rotate(glm::mat4(1),glm::pi<float>(),glm::vec3(0,1,0)),projection);
  std::vector<double> times;unsigned draws=0,culled=0;
  for(int frame=0;frame<20;++frame){r.BeginFrame(256,256);r.ResetStats();auto start=std::chrono::steady_clock::now();for(int i=0;i<300;++i)r.DrawBox({float(i%20-10),float(i/20-7),-20},q,{.2f,.2f,.2f},{.4f,.6f,.8f});times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());draws=r.Stats().drawCalls;culled=r.Stats().renderablesCulled;}
  std::sort(times.begin(),times.end());std::printf("PERFORMANCE culling=%d facing=%d median_cpu_ms=%.6f draws=%u culled=%u\n",enabled,facing,times[times.size()/2],draws,culled);
 }
 r.SetCullingEnabled(true);
 for(bool visible:{true,false}){r.SetCamera(visible?rear:glm::mat4(1),projection);r.ResetStats();auto start=std::chrono::steady_clock::now();for(int i=0;i<8;++i)r.DrawParticles(second,visual.Bounds());std::printf("PERFORMANCE emitters=8 visible=%d cpu_ms=%.6f draws=%u particles=%u\n",visible,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(),r.Stats().drawCalls,r.Stats().particlesSubmitted);}
 world.Destroy();host.Shutdown();Check(world.VisualEmitters().empty(),"world shutdown releases visual state");std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
