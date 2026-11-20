#include "GraphicsApi.h"
#include "EngineHost.h"
#include "RenderLightGrid.h"
#include "WorldPresentation.h"
#include "RuntimeWorld.h"
#include "WorldPersistence.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "PlanetTerrain.h"
#include "PlanetTiles.h"
#include "ScreenshotWriter.h"
#include "ShadowTransforms.h"
#include "lighting/ShadowSystem.h"
#include "particles/GpuParticles.h"
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <algorithm>
static unsigned checks=0,failures=0;
static void Check(bool yes,const char* what){++checks;failures+=!yes;std::printf("%s %s\n",yes?"PASS":"FAIL",what);}
using Pixels=std::vector<unsigned char>;
static double Diff(const Pixels& a,const Pixels& b){if(a.size()!=b.size()||a.empty())return 1e9;double sum=0;for(size_t i=0;i<a.size();++i)sum+=std::abs(int(a[i])-int(b[i]));return sum/a.size();}
static std::vector<float> DirectionalDepth(){
 GLint activeTexture=0,texture=0,readFramebuffer=0;
 glGetIntegerv(GL_ACTIVE_TEXTURE,&activeTexture);
 glActiveTexture(GL_TEXTURE1);glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY,&texture);
 glActiveTexture(GLenum(activeTexture));
 if(!texture)return {};
 glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFramebuffer);
 GLuint framebuffer=0;
 glGenFramebuffers(1,&framebuffer);glBindFramebuffer(GL_READ_FRAMEBUFFER,framebuffer);
 glReadBuffer(GL_NONE);
 constexpr auto size=kDefaultShadowResolution;
 std::vector<float> depth(3*size*size);
 for(int cascade=0;cascade<3;++cascade){
  glFramebufferTextureLayer(GL_READ_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GLuint(texture),0,cascade);
  glReadPixels(0,0,size,size,GL_DEPTH_COMPONENT,GL_FLOAT,depth.data()+cascade*size*size);
 }
 glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(readFramebuffer));glDeleteFramebuffers(1,&framebuffer);
 return depth;
}
int main(int argc,char**argv){std::setvbuf(stdout,nullptr,_IOLBF,0);std::filesystem::path out=argc>1?argv[1]:".cache/m92/architecture";std::filesystem::create_directories(out);std::string error;
 const auto view=glm::lookAt(glm::vec3(0,4,12),glm::vec3(0,0,0),glm::vec3(0,1,0));const auto projection=glm::perspective(glm::radians(65.f),640.f/360,.1f,100.f);
 std::vector<DynamicLight> lights;for(int i=0;i<140;++i){DynamicLight l;l.position={float(i%14-7)*2,3,float(i/14-5)*2};l.range=3;l.color={5,4,3};lights.push_back(l);}RenderLightGrid grid;grid.Build(lights,view,projection,640,360);Check(grid.visible>5&&grid.maxPerTile<128,"screen bins retain many visible lights beyond five with local lists");
 auto far=lights;for(auto& l:far)l.position.x+=1000;grid.Build(far,view,projection,640,360);Check(grid.visible==0&&grid.rejected==far.size(),"negative control: offscreen volumes produce no light work");
 auto crowded=lights;crowded.resize(1200,crowded[0]);for(auto& l:crowded){l.position={0,3,0};l.range=100;}grid.Build(crowded,view,projection,640,360);Check(grid.visible==1024&&grid.maxPerTile==128&&grid.overflow>0,"global and per-tile overload are bounded and observable");
 DynamicLight spot;spot.kind=LightKind::Spot;spot.position={0,5,0};spot.range=10;spot.castsShadow=true;auto shadowRequests=std::vector<DynamicLight>(10,spot);for(unsigned i=0;i<shadowRequests.size();++i)shadowRequests[i].shadowPriority=float(i);auto selection=SelectShadowLights(shadowRequests,view,projection,4);Check(selection.back().shadowMapIndex==kLocalShadowBase&&std::count_if(selection.begin(),selection.end(),[](auto l){return l.shadowMapIndex>0;})==4,"visible shadow priority selects four sources beyond historical indices");
 auto none=SelectShadowLights(shadowRequests,view,projection,0);Check(std::all_of(none.begin(),none.end(),[](auto l){return l.shadowMapIndex<0;}),"zero shadow budget leaves every source lit without shadows");
 PlanetSettings ps;ps.radius=1000;ps.segments=8;ps.maxTiles=192;ps.maxResident=384;ps.bands.push_back({8,{.02,.013,.017}});PlanetSurface planet(ps);std::vector<PlanetView> views{{{0,1003,0},1.1,360},{{0,-1003,0},1.1,360}};auto one=SelectPlanetTiles(planet,views[0].eye,views[0].fov,views[0].pixels),two=SelectPlanetTiles(planet,views);unsigned north=0,south=0;for(auto k:two.leaves){if(k.level>2){if(planet.TileCenter(k).y>0)++north;else ++south;}}Check(north>0&&south>0&&two.leaves.size()<=ps.maxTiles,"opposite planetary cameras refine both hemispheres under one leaf budget");Check(one.leaves!=two.leaves,"negative control: second physical viewpoint changes actual selected keys");
 Scene scene;scene.Settings().name="M92 light ownership";auto& owner=scene.CreateObject("Lamp");auto id=owner.id;owner.light=SceneLightComponent{};owner.light->castsShadow=true;owner.light->intensity=2;std::string text;SaveSceneToString(scene,text);Scene round;Check(LoadSceneFromString(text,round,error)&&ScenesEqual(scene,round),"new light settings serialize through ordinary scene authoring");
 RuntimeWorld world;Check(world.Build(scene,nullptr,error),"headless light world builds");auto setting=*owner.light;setting.enabled=false;setting.intensity=6;Check(world.SetLight(id,setting)&&BuildWorldLights(world,1).empty(),"disabled runtime lamp is absent from world light submission");setting.enabled=true;Check(world.SetLight(id,setting)&&BuildWorldLights(world,1).at(0).color.x==6,"runtime intensity affects the actual submitted source");auto bad=setting;bad.range=-1;Check(!world.SetLight(id,bad)&&world.RuntimeDefinition(id)->light->range==setting.range,"invalid patch does not partly mutate runtime state");
 std::map<std::string,SaveChunk> chunks;Check(WorldPersistence::Capture(world,chunks,error)&&WorldPersistence::SceneFrom(chunks,round,error)&&round.Find(id)->light->intensity==6,"modern durable snapshot contains changed light state, no GPU handle");world.DestroyEntity(id);Check(!world.SetLight(id,setting),"destroyed light safely rejects mutation");world.Destroy();
 EngineHost host;Check(host.Init("M92 renderer contracts",640,360,false,error),"real native GL renderer compiles all revised shaders");if(failures){std::puts(error.c_str());return 1;}GraphicsSetSwapInterval(0);auto& r=host.GetRenderer();
 auto probeRoot=std::filesystem::absolute(out/"api");std::filesystem::create_directories(probeRoot/"Assets");
 std::ofstream(probeRoot/"Assets/probe.js")<<R"JS(import {entity} from 'judas';
 export default class {constructor(){this.state={};}start(){const l=entity('1');
  const copy=l.light;copy.intensity=99;this.state.copy=l.light.intensity===1;
  try{l.setLight({banana:1});this.state.unknown=false;}catch{this.state.unknown=true;}
  try{l.setLight({intensity:-1});this.state.negative=false;}catch{this.state.negative=true;}
  try{l.setLight({enabled:1});this.state.flag=false;}catch{this.state.flag=true;}
  l.setLight({kind:'spot',enabled:true,intensity:3,color:{x:1,y:.5,z:.25},castsShadow:true});this.state.applied=l.light.intensity===3&&l.light.kind==='spot';
  const other=entity('2');this.state.absent=other.light===null;try{other.setLight({enabled:false});this.state.absentError=false;}catch{this.state.absentError=true;}
  l.destroy();try{l.setLight({enabled:true});this.state.stale=false;}catch{this.state.stale=true;}
 }})JS";
 host.OpenProjectAssets(probeRoot.string(),(probeRoot/"Assets").string());AssetRecord probeAsset;Check(host.Assets().Track((probeRoot/"Assets/probe.js").string(),probeAsset,error),"API probe is an ordinary registered script");
 Scene scriptScene;auto& scriptedLamp=scriptScene.CreateObject("Scripted lamp");scriptedLamp.light=SceneLightComponent{};auto& controller=scriptScene.CreateObject("Probe");controller.scripts.push_back({1,probeAsset.id,true,"{}"});RuntimeWorld scripted;Check(scripted.Build(scriptScene,&host.Resources(),error),"public API world builds");host.Resources().WaitForAll();scripted.UpdateScripts(nullptr,0);std::string state;for(auto& record:scripted.Scripts()->Capture())if(record.entity==2)state=record.json;
 for(auto key:{"copy","unknown","negative","flag","applied","absent","absentError","stale"})Check(state.find(std::string("\"")+key+"\":true")!=std::string::npos,key);
 Check(scripted.Scripts()->Diagnostics().empty(),"public light API negative cases are caught without VM faults");scripted.Destroy();

 MaterialDefinition pbr;pbr.model=MaterialModel::PBR;pbr.roughness=.4f;auto pbrHandle=r.CreateMaterial(pbr);
 auto image=[&](bool batch,const std::vector<DynamicLight>& sources,bool modern=false){r.ResetStats();r.SetBatchingEnabled(batch);r.SetSceneAppearance(modern,1,{},1,{1,0,0,0},false,{.02f,.03f,.04f});r.BeginFrame(640,360);r.SetCamera(view,projection);r.SetLighting({0,1,0},{.3f,.3f,.3f},{.15f,.15f,.15f});r.SetDynamicLights(sources);if(modern){MaterialBinding binding;binding.handle=pbrHandle;r.SetMaterialBindings({binding});}else r.SetMaterialBindings({});r.BeginMeshBatch();
  for(int z=0;z<10;++z)for(int x=0;x<14;++x){float sx=x%4==0?-.35f:.35f;r.DrawBox({float(x-7)*.8f,.5f,float(z-5)*.8f},glm::angleAxis(.12f*x,glm::vec3(0,1,0)),{sx,.3f+.03f*z,.3f},{float(x%3)*.25f+.2f,.4f,float(z%3)*.2f+.25f});}
  r.EndMeshBatch();r.EndFrame();Pixels pixels;r.CaptureFrame(640,360,pixels);return pixels;};
 auto ordinary=image(false,{});auto draws=r.Stats().drawCalls;auto instanced=image(true,{});Check(Diff(ordinary,instanced)<.02,"instancing preserves mirrored/nonuniform rotated geometry and per-instance tint pixels");Check(r.Stats().drawCalls<draws/10&&r.Stats().instances==140,"same visible geometry becomes a bounded number of actual GPU instanced draws");WriteRgbPng((out/"instances.png").string(),640,360,instanced);
 auto masked=[&](bool batching){r.SetBatchingEnabled(batching);r.SetSceneAppearance(false,1,{},1,{1,0,0,0},false);r.BeginFrame(640,360);r.SetCamera(view,projection);r.SetLighting({0,1,0},{0,0,0},{1,1,1});r.SetDynamicLights({});r.SetMaterialBindings({});r.SetRenderMask(CategoryBit(1));r.BeginMeshBatch();r.SetRenderLayer(1);r.DrawBox({0,1,0},{1,0,0,0},{1,1,1},{1,.1f,.1f});r.SetRenderLayer(2);r.DrawBox({0,1,2},{1,0,0,0},{1,1,1},{.1f,.1f,1});r.SetRenderLayer(0);r.EndMeshBatch();r.EndFrame();Pixels p;r.CaptureFrame(640,360,p);r.SetRenderMask(kAllCategories);return p;};
 auto layerReference=masked(false),layerInstances=masked(true);Check(Diff(layerReference,layerInstances)<.02,"batch flush preserves prior per-instance layer acceptance when Default layer is hidden");unsigned redPixels=0;for(size_t i=0;i<layerInstances.size();i+=3)redPixels+=layerInstances[i]>100&&layerInstances[i]>layerInstances[i+2]*2;Check(redPixels>100,"non-default accepted layer remains visible; rejected closer layer does not occlude it");
 auto pbrImmediate=image(false,lights,true),pbrInstances=image(true,lights,true);Check(Diff(pbrImmediate,pbrInstances)<.02,"PBR material shading remains pixel-equivalent through instancing");
 auto lit=image(true,lights);Check(r.Stats().dynamicLights>5&&Diff(instanced,lit)>2,"more than five lights contribute to actual framebuffer output");WriteRgbPng((out/"lights.png").string(),640,360,lit);
 std::vector<DynamicLight> late(8);for(auto& l:late){l.position={1000,0,0};l.range=1;}late.back().position={0,3,0};late.back().range=12;late.back().color={30,5,1};auto lateImage=image(true,late);Check(r.Stats().dynamicLights==1&&Diff(instanced,lateImage)>1,"late registered visible source lights geometry despite earlier invisible sources");
 // Shadow passes restore the active target rather than sending its colour draw
 // to the desktop. Exercise a layer beyond the historical 0/1/2 slots.
 auto target=r.CreateRenderTarget(123,87,error);Check(r.BeginRenderTarget(target),"secondary target begins");auto before=r.RenderTargetDiagnostics();r.BeginShadowPass(5,glm::ortho(-5.f,5.f,-5.f,5.f,.1f,30.f)*view);r.SetMaterialBindings({});r.BeginMeshBatch();r.DrawBox({0,0,0},{1,0,0,0},{1,1,1},{1,1,1});r.EndMeshBatch();r.EndShadowPass();auto after=r.RenderTargetDiagnostics();Check(before.viewport==after.viewport&&!after.defaultFramebuffer&&after.passActive,"shadow array layer five restores secondary framebuffer and viewport");r.EndRenderTarget();r.DestroyRenderTarget(target);
 // World path tests shared state and per-camera selection, not just low level.
 Scene s;s.Settings().sunEnabled=false;s.Settings().ambientColor={.05f,.05f,.05f};auto& geometry=s.CreateObject("Visible cube");geometry.render=SceneRenderComponent{};auto& lamp=s.CreateObject("Spot");lamp.light=SceneLightComponent{};lamp.light->kind=SceneLightKind::Spot;lamp.light->castsShadow=true;lamp.light->color={6,6,6};lamp.transform.position={0,3,3};lamp.transform.rotation=glm::quatLookAt(glm::normalize(glm::vec3(0,-3,-3)),glm::vec3(0,1,0));auto& camera=s.CreateObject("Independent view");camera.transform.position={0,3,6};camera.renderCamera=SceneRenderCameraComponent{};camera.renderCamera->width=128;camera.renderCamera->height=96;RuntimeWorld rendered;Check(rendered.Build(s,nullptr,error),"ordinary multi-view shadow world builds");RenderWorldFrame(r,640,360,rendered,view,projection,{0,0,0},1);Check(r.Stats().shadowPasses>=2&&rendered.PresentationCameras()[0].updates==1,"each independent camera gets its own selected shadow pass");rendered.Destroy();
 Scene cast;cast.Settings().sunEnabled=false;cast.Settings().ambientColor={.02f,.02f,.02f};auto& floor=cast.CreateObject("Receiver");floor.render=SceneRenderComponent{};floor.render->halfExtents={6,.1f,6};auto& blocker=cast.CreateObject("Occluder");blocker.transform.position={0,1,0};blocker.render=SceneRenderComponent{};blocker.render->halfExtents={.6f,1,.6f};auto& sun=cast.CreateObject("Generic spot");sun.light=SceneLightComponent{};sun.light->kind=SceneLightKind::Spot;sun.light->castsShadow=true;sun.light->range=20;sun.light->outerConeDegrees=55;sun.light->color={40,40,40};sun.transform.position={-2,6,0};sun.transform.rotation=glm::quatLookAt(glm::normalize(glm::vec3(2,-6,0)),glm::vec3(0,0,-1));auto sunId=sun.id;
 RuntimeWorld shadows;Check(shadows.Build(cast,nullptr,error),"ordinary authored spot/receiver/occluder fixture builds");RenderWorldFrame(r,640,360,shadows,view,projection,{0,0,0},1);Pixels withShadow;r.CaptureFrame(640,360,withShadow);auto config=*shadows.RuntimeDefinition(sunId)->light;config.castsShadow=false;shadows.SetLight(sunId,config);RenderWorldFrame(r,640,360,shadows,view,projection,{0,0,0},1);Pixels withoutShadow;r.CaptureFrame(640,360,withoutShadow);Check(Diff(withShadow,withoutShadow)>.15,"selected authored spotlight actually shadows receiver pixels; disabling request removes shadow");WriteRgbPng((out/"spot-shadow.png").string(),640,360,withShadow);shadows.Destroy();
 cast.Settings().sunEnabled=true;
 cast.Settings().sunDirection=glm::normalize(glm::vec3(1,4,1));
 const auto casterId=cast.Objects()[1].id;
 auto& optionalView=cast.CreateObject("Optional shadow view");
 optionalView.renderCamera=SceneRenderCameraComponent{};
 optionalView.transform.position={0,4,12};
 optionalView.transform.rotation=glm::quat_cast(glm::transpose(glm::mat3(view)));
 RuntimeWorld cameraPolicy;
 Check(cameraPolicy.Build(cast,nullptr,error),"per-camera shadow fixture builds with sun and local light");
 auto& optionalCamera=cameraPolicy.PresentationCameras().front();
 std::vector<float> previousDepth;
 for(int tick=0;tick<2;++tick){
  if(tick){
   auto moved=cameraPolicy.RuntimeDefinition(casterId)->transform;
   moved.position.x+=2;
   Check(cameraPolicy.SetRuntimeTransform(casterId,moved),"move a presented caster");
  }
  optionalCamera.settings.shadows=true;
  r.ResetStats();
  RenderWorldFrame(r,640,360,cameraPolicy,view,projection,{0,0,0},1);
  Pixels mainShadowed;r.CaptureFrame(640,360,mainShadowed);
  const auto depth=DirectionalDepth();
  TextureData secondaryShadowed,secondaryUnshadowed;
  const bool readShadowed=r.ReadTextureForDiagnostics(cameraPolicy.CameraTexture(optionalCamera.id),secondaryShadowed);
  const auto shadowed=r.Stats().shadowPasses+r.Stats().reusedShadowPasses;
  const auto updates=optionalCamera.updates;
  optionalCamera.settings.shadows=false;
  r.ResetStats();
  RenderWorldFrame(r,640,360,cameraPolicy,view,projection,{0,0,0},1);
  Pixels mainUnchanged;r.CaptureFrame(640,360,mainUnchanged);
  Check(r.Stats().shadowPasses+r.Stats().reusedShadowPasses==4&&shadowed==8,
        "secondary opt-out skips its sun and local shadow charts only");
  Check(optionalCamera.updates==updates+1&&readShadowed&&
        r.ReadTextureForDiagnostics(cameraPolicy.CameraTexture(optionalCamera.id),secondaryUnshadowed)&&
        Diff(secondaryShadowed.pixels,secondaryUnshadowed.pixels)>.05,
        "secondary target refreshes without sampling sun or local shadow charts");
  Check(mainShadowed==mainUnchanged&&!depth.empty()&&depth==DirectionalDepth(),
        "main colour and directional depth exactly match with secondary shadows disabled");
  if(tick)Check(depth!=previousDepth,"main cascade depth still updates after caster movement");
  previousDepth=depth;
 }
 std::map<std::string,SaveChunk> cameraSave;
 Scene restoredCamera;
 Check(WorldPersistence::Capture(cameraPolicy,cameraSave,error)&&
       WorldPersistence::SceneFrom(cameraSave,restoredCamera,error)&&
       !restoredCamera.Find(optionalCamera.id)->renderCamera->shadows,
       "world snapshot stores the runtime camera shadow setting");
 if(r.SupportsIndependentShadowResolution()){
  const auto original=r.Quality();
  auto split=original;
  split.shadowResolution=256;
  split.directionalShadowResolution=768;
  split.localShadowBudget=1;
  Check(r.SetGraphicsQuality(split,error),"change directional and local sizes independently");
  r.BeginShadowPass(0,glm::mat4(1));
  Check(r.RenderTargetDiagnostics().viewport[2]==768,"directional pass uses directional extent");
  GLint directionalFramebuffer=0;
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&directionalFramebuffer);
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
  glClearDepthf(.25f);glClear(GL_DEPTH_BUFFER_BIT);glClearDepthf(1);
#else
  glClearDepth(.25);glClear(GL_DEPTH_BUFFER_BIT);glClearDepth(1);
#endif
  r.EndShadowPass();
  for(int face=0;face<6;++face){
      r.BeginShadowPass(kLocalShadowBase+6+face,glm::mat4(1));
      Check(r.RenderTargetDiagnostics().viewport[2]==256,"each point chart uses local extent");
      r.EndShadowPass();
  }
  GLint read=0;
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
  glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(directionalFramebuffer));
  float sunDepth=0;
  glReadPixels(384,384,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&sunDepth);
  glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(read));
  Check(std::abs(sunDepth-.25f)<1e-6f,"local growth preserves already-published sun depth");
  Check(r.ShadowStorageBytes()==(size_t(6)*768*768+size_t(12)*256*256)*4,
        "estimator counts directional and local depth separately");
  RenderWorldFrame(r,640,360,cameraPolicy,view,projection,{0,0,0},1);
  Pixels splitShadowed;r.CaptureFrame(640,360,splitShadowed);
  config.castsShadow=false;cameraPolicy.SetLight(sunId,config);
  RenderWorldFrame(r,640,360,cameraPolicy,view,projection,{0,0,0},1);
  Pixels splitUnshadowed;r.CaptureFrame(640,360,splitUnshadowed);
  Check(Diff(splitShadowed,splitUnshadowed)>.05,"spotlight sampling uses the remapped local depth");
  Check(r.SetGraphicsQuality(original,error),"return to shared shadow storage");
 }
 cameraPolicy.Destroy();
 {
  Scene decoration;
  decoration.Settings().sunEnabled=false;
  auto& essential=decoration.CreateObject("Required marker");
  essential.render=SceneRenderComponent{};
  essential.render->decorativeDistance=1;
  essential.transform.position={0,1,0};
  const auto essentialId=essential.id;
  for(int i=0;i<16;++i){
      auto& tree=decoration.CreateObject("Optional detail");
      tree.render=SceneRenderComponent{};
      tree.render->decorative=true;
      tree.render->decorativeDistance=20;
      tree.transform.position={float(i%8)-3.5f,0,float(i/8)*2};
  }
  for(bool optional:{false,true}){
      auto& emitter=decoration.CreateObject(optional?"Optional atmosphere":"Required effect");
      emitter.transform.position={optional?2.f:-2.f,2,0};
      emitter.particleEmitter=ParticleEmitterSettings{};
      emitter.optionalParticlePresentation=optional;
  }
  std::string encoded;
  Scene roundTrip;
  Check(SaveSceneToString(decoration,encoded)&&LoadSceneFromString(encoded,roundTrip,error)&&
        ScenesEqual(decoration,roundTrip),"quality authoring metadata round trips");
  RuntimeWorld visuals;
  Check(visuals.Build(roundTrip,nullptr,error),"build opt-in decoration and particle fixture");
  for(auto& emitter:visuals.VisualEmitters())emitter.pool.Burst(8,emitter.transform.position,{1,0,0,0},{1,1,1});
  const auto original=r.Quality();
  const auto sample=[&](const GraphicsQuality& q, WorldRenderPurpose purpose) {
      Check(r.SetGraphicsQuality(q,error),"apply visual-only fixture quality");
      r.ResetStats();
      RenderWorldFrame(r,640,360,visuals,view,projection,{0,0,0},1,0,purpose);
      Pixels pixels;
      r.CaptureFrame(640,360,pixels);
      return std::pair(pixels,r.Stats());
  };
  const auto [full,high]=sample(original,WorldRenderPurpose::Runtime);
  auto reduced=original;
  reduced.decorativeDensity=.5f;
  reduced.optionalParticleDensity=0;
  const auto [thinned,half]=sample(reduced,WorldRenderPurpose::Runtime);
  const auto [repeat,again]=sample(reduced,WorldRenderPurpose::Runtime);
  Check(full!=thinned&&thinned==repeat&&half.triangles==again.triangles&&half.triangles<high.triangles,
        "decoration density is a deterministic rendered subset");
  auto distance=original;
  distance.decorativeDistanceScale=.25f;
  const auto [nearOnly,near]=sample(distance,WorldRenderPurpose::Runtime);
  const auto [editorView,editor]=sample(distance,WorldRenderPurpose::Observer);
  Check(near.triangles<high.triangles&&high.triangles==editor.triangles&&nearOnly!=editorView&&
        visuals.RuntimeDefinition(essentialId)&&visuals.RenderVisible(essentialId),
        "distance affects opted-in runtime colour only");
  Check(high.particlesSubmitted==16&&half.particlesSubmitted==8&&
        std::all_of(visuals.VisualEmitters().begin(),visuals.VisualEmitters().end(),
          [](auto& emitter){return emitter.pool.Particles().size()==8;})&&visuals.SimulationTimeSeconds()==0,
        "optional emitters save draws without removing CPU particles or advancing gameplay");
  std::map<std::string,SaveChunk> save;
  Scene restored;
  Check(WorldPersistence::Capture(visuals,save,error)&&WorldPersistence::SceneFrom(save,restored,error)&&
        restored.Objects().size()==decoration.Objects().size(),
        "quality has no effect on persisted entity ownership");
  std::printf("MEASURE visual detail full=%u half=%u distance=%u cpu-particles=%u/%u\n",
              high.triangles,half.triangles,near.triangles,high.particlesSubmitted,half.particlesSubmitted);
  Check(r.SetGraphicsQuality(original,error),"restore full authored presentation");
  visuals.Destroy();

  if(host.GraphicsCaps().modernTier&&host.GraphicsCaps().compute){
      Scene gpuScene;
      gpuScene.Settings().sunEnabled=false;
      auto& effect=gpuScene.CreateObject("Optional resident effect");
      effect.transform.position={0,2,0};
      effect.optionalParticlePresentation=true;
      effect.particleEmitter=ParticleEmitterSettings{};
      effect.particleEmitter->backend=ParticleBackend::Gpu;
      effect.particleEmitter->burst=16;
      effect.particleEmitter->rate=0;
      effect.particleEmitter->maxParticles=32;
      RuntimeWorld gpuWorld;
      Check(gpuWorld.Build(gpuScene,&host.Resources(),error),"build optional GPU emitter");
      gpuWorld.UpdateVisualParticles(1.f/60);
      const auto &pool=gpuWorld.VisualEmitters().front().gpu;
      if(pool){
          std::vector<JudasParticles::GpuParticleState> before,after;
          Check(pool->ReadStateForDiagnostics(before,error),"read original resident particle state");
          r.ResetStats();
          RenderWorldFrame(r,640,360,gpuWorld,view,projection,{0,0,0},1);
          const auto shown=r.Stats().gpuParticleDraws;
          auto hidden=original;
          hidden.optionalParticleDensity=0;
          Check(r.SetGraphicsQuality(hidden,error),"hide optional GPU draws only");
          r.ResetStats();
          RenderWorldFrame(r,640,360,gpuWorld,view,projection,{0,0,0},1);
          Check(shown==1&&r.Stats().gpuParticleDraws==0&&pool->ReadStateForDiagnostics(after,error)&&
                before.size()==after.size()&&std::memcmp(before.data(),after.data(),before.size()*sizeof(before[0]))==0,
                "omitting GPU presentation preserves every resident particle");
          Check(r.SetGraphicsQuality(original,error),"restore optional GPU draws");
      } else Check(false,"optional GPU emitter remains resident");
      gpuWorld.Destroy();
  }
 }
 Scene planetary;planetary.Settings().worldOrigin={0,1000,0};planetary.Settings().sunEnabled=false;planetary.Settings().ambientColor={1,1,1};auto& surface=planetary.CreateObject("Shared terrain");surface.planet=ps;auto planetId=surface.id;
 auto& opposite=planetary.CreateObject("Opposite hemisphere camera");opposite.transform.position={0,-2003,0};opposite.transform.rotation=glm::quatLookAt(glm::vec3(0,1,0),glm::vec3(0,0,-1));opposite.renderCamera=SceneRenderCameraComponent{};opposite.renderCamera->width=128;opposite.renderCamera->height=96;opposite.renderCamera->farPlane=10000;
 auto& unrelated=planetary.CreateObject("Offscreen material owner");unrelated.transform.position={100000,0,0};unrelated.render=SceneRenderComponent{};unrelated.render->instanceOverrides.baseColor=glm::vec4(1,0,0,1);auto unrelatedId=unrelated.id;
 RuntimeWorld planetWorld;Check(planetWorld.Build(planetary,nullptr,error),"actual planetary world with opposite authored camera builds");auto pv=glm::lookAt(glm::vec3(0,3,0),glm::vec3(0,0,-6),glm::vec3(0,1,0)),pp=glm::perspective(glm::radians(65.f),640.f/360,.1f,10000.f);
 RenderWorldFrame(r,640,360,planetWorld,pv,pp,{0,0,0},1,0);auto* selectedPlanet=planetWorld.Planet(planetId);north=south=0;for(auto k:selectedPlanet->Leaves())if(k.level>2){if(selectedPlanet->surface.TileCenter(k).y>0)++north;else ++south;}Check(north&&south,"actual RenderWorldFrame passes both physical camera positions into shared planetary refinement");
 Pixels withUnrelated;r.CaptureFrame(640,360,withUnrelated);planetWorld.DestroyEntity(unrelatedId);RenderWorldFrame(r,640,360,planetWorld,pv,pp,{0,0,0},1,0);Pixels alone;r.CaptureFrame(640,360,alone);Check(Diff(withUnrelated,alone)<.02,"planet does not inherit a preceding offscreen object's instance material");WriteRgbPng((out/"planet-two-eyes.png").string(),640,360,alone);planetWorld.Destroy();
 Scene edit;edit.Settings().sunDirection={0,1,0};edit.Settings().sunColor={1,1,1};edit.Settings().ambientColor={0,0,0};auto& receiver=edit.CreateObject("Edit-only receiver");receiver.render=SceneRenderComponent{};receiver.render->halfExtents={6,.1f,6};receiver.render->color={1,1,1};
 r.SetMaterialBindings({});r.BeginShadowPass(0,ComputeDirectionalShadowMatrix({0,0,0},{0,1,0},25,40));r.DrawBox({0,1,0},{1,0,0,0},{1,1,1},{1,1,1});r.EndShadowPass();
 auto authoredImage=[&](){r.BeginFrame(640,360);r.SetCamera(view,projection);r.SetDynamicLights({});DrawAuthoredScene(r,edit,host.Resources());r.EndFrame();Pixels p;r.CaptureFrame(640,360,p);return p;};auto inherited=authoredImage();r.InvalidateShadowSlot(0);auto cleanEdit=authoredImage();Check(Diff(inherited,cleanEdit)<.02,"authored edit view cannot inherit a previous runtime camera's directional shadow map");
 // Edit preview uses the same planner and exact caster ownership as runtime.
 edit.Settings().sunEnabled=false;edit.Settings().ambientColor={.02f,.02f,.02f};
 auto& editBlocker=edit.CreateObject("Authored moving occluder");const auto editBlockerId=editBlocker.id;editBlocker.transform.position={0,1,0};editBlocker.render=SceneRenderComponent{};editBlocker.render->halfExtents={.6f,1,.6f};
 auto& editLamp=edit.CreateObject("Authored spot");editLamp.light=*cast.Find(sunId)->light;editLamp.transform=cast.Find(sunId)->transform;
 auto editPreview=[&](unsigned budget){r.ResetStats();r.SetShadowBudget(budget);r.BeginFrame(640,360);r.SetCamera(view,projection);JudasLighting::PrepareShadows(r,edit.Settings(),BuildAuthoredLights(edit),JudasLighting::AuthoredShadowScene(edit,host.Resources()),[&]{DrawAuthoredScene(r,edit,host.Resources());});DrawAuthoredScene(r,edit,host.Resources());r.EndFrame();Pixels pixels;r.CaptureFrame(640,360,pixels);return pixels;};
 const auto authoredShadowed=editPreview(1),authoredUnshadowed=editPreview(0);
 Check(Diff(authoredShadowed,authoredUnshadowed)>.15,"shared Edit-preview planner produces real authored spotlight occlusion");
 editPreview(1);edit.Find(editBlockerId)->transform.position.x+=2;
 const auto authoredMoved=editPreview(1);
 Check(r.Stats().shadowPasses==1&&Diff(authoredMoved,authoredShadowed)>.15,"authored geometry edits redraw their shadow in the immediately consuming preview");
 WriteRgbPng((out/"edit-shadow.png").string(),640,360,authoredShadowed);
 Check(glGetError()==GL_NO_ERROR,"no graphics errors after instancing, tiled lighting, shadows and target teardown");host.Shutdown();std::printf("SUMMARY %u checks %u failures\n",checks,failures);return failures?1:0;
}
