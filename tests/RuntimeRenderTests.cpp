#include "EngineHost.h"
#include "Environment.h"
#include "ModelArchive.h"
#include "PerformanceProfiler.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "ScreenshotWriter.h"
#include "WorldPresentation.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <set>

namespace {
constexpr int Width=256,Height=192;
unsigned checks=0,failures=0;
void Check(bool pass,const char* label){++checks;failures+=!pass;std::printf("%s %s\n",pass?"PASS":"FAIL",label);}
glm::mat4 View(){return glm::lookAt(glm::vec3(0,0,6),glm::vec3(0),glm::vec3(0,1,0));}
glm::mat4 Projection(){return glm::ortho(-3.f,3.f,-2.25f,2.25f,.1f,20.f);}
glm::vec3 Pixel(const std::vector<unsigned char>& pixels,int x,int y){auto i=size_t(y*Width+x)*3;return {pixels.at(i),pixels.at(i+1),pixels.at(i+2)};}
glm::ivec2 Project(glm::vec3 p,const glm::mat4& view,const glm::mat4& projection,int width=Width,int height=Height){
    auto clip=projection*view*glm::vec4(p,1);auto uv=glm::vec2(clip)/clip.w*.5f+.5f;
    return {std::clamp(int(uv.x*width),0,width-1),std::clamp(int((1-uv.y)*height),0,height-1)};
}
glm::vec3 Probe(const std::vector<unsigned char>& pixels,glm::vec3 p,const glm::mat4& view=View(),const glm::mat4& projection=Projection()){
    auto center=Project(p,view,projection);glm::vec3 total(0);
    for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x)total+=Pixel(pixels,center.x+x,center.y+y);
    return total/9.f;
}
bool Near(glm::vec3 a,glm::vec3 b,float tolerance=5){return glm::all(glm::lessThanEqual(glm::abs(a-b),glm::vec3(tolerance)));}
bool StrongChannel(glm::vec3 colour,unsigned channel){return colour[channel]>180&&colour[(channel+1)%3]<5&&colour[(channel+2)%3]<5;}
std::vector<unsigned char> Frame(Renderer& r,RuntimeWorld& world,const glm::mat4& view=View(),const glm::mat4& projection=Projection()){
    RenderWorldFrame(r,Width,Height,world,nullptr,view,projection,{0,0,0},1);
    std::vector<unsigned char> pixels;r.CaptureFrame(Width,Height,pixels);return pixels;
}
float Depth(){float depth=1;GLint viewport[4];glGetIntegerv(GL_VIEWPORT,viewport);glReadPixels(viewport[2]/2,viewport[3]/2,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&depth);return depth;}
float WorldShadow(Renderer& r,RuntimeWorld& world){
    r.BeginShadowPass(0,glm::ortho(-3.f,3.f,-2.25f,2.25f,.1f,20.f)*View());
    DrawWorldGeometry(r,world,nullptr,1,{});auto depth=Depth();r.EndShadowPass();return depth;
}
Scene PrimitiveScene(){
    Scene scene;scene.Settings().sunDirection={0,0,1};scene.Settings().sunColor={1,1,1};scene.Settings().ambientColor={0,0,0};scene.Settings().backgroundColor={0,0,0};
    auto& object=scene.CreateObject("Solid visual");object.render=SceneRenderComponent{};object.render->halfExtents={.6f,.6f,.1f};object.render->color={1,1,1};
    object.body=SceneBodyComponent{};object.body->halfExtents=object.render->halfExtents;
    return scene;
}
void Meta(const std::filesystem::path& path,const std::string& id,const char* type){std::ofstream(path.string()+".judasmeta")<<"JudasAssetMeta 1\nid \""<<id<<"\"\ntype "<<type<<"\nsource \"\"\n";}
MeshData Multipart(){
    MeshData mesh;MaterialDefinition material;material.model=MaterialModel::Unlit;material.baseColor={0,1,0,1};mesh.materials.push_back(material);mesh.materialKeys.push_back("Paint");
    for(int part=0;part<2;++part){float center=part? .55f:-.55f;unsigned base=unsigned(mesh.vertices.size()),first=unsigned(mesh.indices.size());
        for(auto p:std::vector<glm::vec2>{{-.4f,-.65f},{.4f,-.65f},{.4f,.65f},{-.4f,.65f}}){MeshVertex v;v.position={center+p.x,p.y,0};v.normal={0,0,1};v.uv={(p.x+.4f)/.8f,(p.y+.65f)/1.3f};v.uv1=v.uv;mesh.vertices.push_back(v);}
        for(unsigned index:{0u,1u,2u,0u,2u,3u})mesh.indices.push_back(base+index);
        mesh.primitives.push_back({first,6,0,part?"Panel/Trim/material/Paint":"Panel/Face/material/Paint"});
    }return mesh;
}
using Timer=std::chrono::steady_clock;
double Milliseconds(Timer::time_point start){return std::chrono::duration<double,std::milli>(Timer::now()-start).count();}
void ReportTiming(const char* label,std::vector<double> times){
    const double mean=std::accumulate(times.begin(),times.end(),0.0)/times.size();std::sort(times.begin(),times.end());
    std::printf("PERF %s frames=%zu mean_ms=%.4f median_ms=%.4f p95_ms=%.4f max_ms=%.4f\n",label,times.size(),mean,times[times.size()/2],times[std::min(times.size()-1,size_t(std::ceil(times.size()*.95)-1))],times.back());
}
}

int main(int argc,char** argv){
    namespace fs=std::filesystem;const auto out=fs::absolute(argc>1?argv[1]:"build/m72-render");fs::create_directories(out);
    std::string error;unsigned textureCreates=0,textureDestroys=0,meshCreates=0;std::set<unsigned> liveTextures;
    EngineHost host;Check(host.Init("M72 rendered controls",Width,Height,false,error,[&](const ResourceTraceEvent& event){
        if(event.point==ResourceTracePoint::TextureCreated){++textureCreates;liveTextures.insert(event.handle);}
        if(event.point==ResourceTracePoint::TextureDestroyed){++textureDestroys;liveTextures.erase(event.handle);}
        if(event.point==ResourceTracePoint::MeshCreated)++meshCreates;
    }),"normal GL host starts without warming skin or material paths");if(failures){std::puts(error.c_str());return 1;}
    auto& r=host.GetRenderer();std::printf("GPU %s / %s\n",glGetString(GL_VENDOR),glGetString(GL_RENDERER));

    // Independent expectations use saturated channels, ordinary depth readback
    // and disabled controls. They do not reproduce the renderer's predicates.
    MaterialDefinition white;white.model=MaterialModel::Unlit;const auto shared=r.CreateMaterial(white);
    auto plain=[&](){r.SetSceneAppearance(false,1,{},1,{1,0,0,0},false,{0,0,0});r.BeginFrame(Width,Height);r.SetCamera(View(),Projection());r.SetLighting({0,0,1},{0,0,0},{1,1,1});r.SetDynamicLights({});r.InvalidateShadowSlot(0);};
    auto capture=[&](){r.EndFrame();std::vector<unsigned char> pixels;r.CaptureFrame(Width,Height,pixels);return pixels;};
    plain();MaterialBinding foreground{shared,{}};foreground.overrides.alpha=MaterialAlpha::Blend;foreground.overrides.baseColor=glm::vec4(1,0,0,.5f);
    r.SetMaterialBindings({foreground});r.DrawBox({0,0,1},{1,0,0,0},{1,1,.05f},{1,1,1});
    MaterialBinding background{shared,{}};background.overrides.baseColor=glm::vec4(0,0,1,1);r.SetMaterialBindings({background});r.DrawBox({0,0,0},{1,0,0,0},{1,1,.05f},{1,1,1});auto opaqueDepth=Depth();
    auto pixels=capture();Check(Near(Probe(pixels,{0,0,0}),{128,0,128},4),"instance alpha mode blends separated surfaces despite reversed submission order");
    Check(std::abs(Depth()-opaqueDepth)<1e-6f,"blended instance writes no depth over its opaque neighbour");WriteRgbPng((out/"overlapping-blend.png").string(),Width,Height,pixels);
    r.BeginShadowPass(0,Projection()*View());r.SetMaterialBindings({foreground});r.DrawBox({0,0,0},{1,0,0,0},{1,1,.05f},{1,1,1});auto blendShadow=Depth();r.EndShadowPass();
    Check(blendShadow>.9999f,"runtime Blend override removes source Opaque shadow submission");
    MaterialBinding mask{shared,{}};mask.overrides.alpha=MaterialAlpha::Mask;mask.overrides.alphaCutoff=.5f;mask.overrides.baseColor=glm::vec4(1,0,0,.25f);
    plain();r.SetMaterialBindings({mask});r.DrawBox({0,0,0},{1,0,0,0},{1,1,.05f},{1,1,1});pixels=capture();Check(Near(Probe(pixels,{0,0,0}),{0,0,0}),"runtime cutout factor alpha visibly discards main fragments");
    r.BeginShadowPass(0,Projection()*View());r.SetMaterialBindings({mask});r.DrawBox({0,0,0},{1,0,0,0},{1,1,.05f},{1,1,1});auto maskShadow=Depth();r.EndShadowPass();Check(maskShadow>.9999f,"runtime cutout factor alpha also discards shadow depth");
    const auto alphaImage=r.CreateTexture({1,1,{255,0,0,0}});mask.overrides.baseColor=glm::vec4(1);mask.textures[0]=alphaImage;mask.textureOverrides[0]=true;
    plain();r.SetMaterialBindings({mask});r.DrawBox({0,0,0},{1,0,0,0},{1,1,.05f},{1,1,1});pixels=capture();Check(Near(Probe(pixels,{0,0,0}),{0,0,0}),"cutout includes borrowed texture alpha");
    plain();r.SetMaterialBindings({{shared,{}}});r.DrawBox({0,0,0},{1,0,0,0},{1,1,.05f},{1,1,1});pixels=capture();Check(Near(Probe(pixels,{0,0,0}),{255,255,255})&&Depth()<.9f,"cleared binding restores unchanged shared Opaque source and depth");
    r.DestroyTexture(alphaImage);r.DestroyMaterial(shared);
    {
        MaterialDefinition source;source.model=MaterialModel::Unlit;source.maps[0].embedded={1,1,{0,255,0,255}};const auto material=r.CreateMaterial(source);
        const auto emissiveImage=r.CreateTexture({1,1,{255,0,0,255}});MaterialBinding binding{material,{}};binding.overrides.emissive=glm::vec3(.25f);binding.textures[4]=emissiveImage;binding.textureOverrides[4]=true;
        plain();r.SetSceneAppearance(true,1,{},1,{1,0,0,0},false,{0,0,0});r.BeginFrame(Width,Height);r.SetMaterialBindings({binding});r.DrawBox({0,0,0},{1,0,0,0},{1,1,.05f},{1,1,1});pixels=capture();auto colour=Probe(pixels,{0,0,0});
        Check(colour.x>70&&colour.y>150&&colour.z<5,"first emissive-map colour view preserves separately bound source base texture");
        r.DestroyTexture(emissiveImage);r.DestroyMaterial(material);
    }

    {
        RuntimeWorld world;Check(world.Build(PrimitiveScene(),nullptr,error),"ordinary solid world builds");pixels=Frame(r,world);auto lit=Probe(pixels,{0,0,0});Check(lit.x>220&&lit.y>220,"direct sun illuminates front surface at fixed exposure");
        auto appearance=world.Settings();appearance.sunIntensity=.25f;Check(world.SetAppearance(appearance),"runtime sun intensity publishes");pixels=Frame(r,world);Check(Near(Probe(pixels,{0,0,0}),{64,64,64},8),"sun intensity scales actual direct contribution with exposure unchanged");
        appearance.sunIntensity=1;appearance.sunColor={1,0,0};world.SetAppearance(appearance);pixels=Frame(r,world);Check(Probe(pixels,{0,0,0}).x>220&&Probe(pixels,{0,0,0}).y<5,"sun colour changes lit channels");
        appearance.sunColor={1,1,1};appearance.sunDirection={1,0,0};world.SetAppearance(appearance);pixels=Frame(r,world);Check(Probe(pixels,{0,0,0}).x<8,"sun direction changes actual surface lighting");
        appearance.sunDirection={0,0,1};appearance.sunEnabled=false;appearance.ambientColor={.2f,.2f,.2f};world.SetAppearance(appearance);r.ResetStats();pixels=Frame(r,world);Check(Near(Probe(pixels,{0,0,0}),{51,51,51},5)&&r.Stats().shadowPasses==0,"disabled sun removes direct lighting and shadows while retaining ambient");
        Check(world.ResetAppearance(),"appearance reset publishes authored defaults");pixels=Frame(r,world);Check(Near(Probe(pixels,{0,0,0}),lit,8),"sun reset restores rendered authored output");
        const auto body=world.RuntimeBody(1);const auto bodyCount=world.Physics().AliveBodyCount();Check(WorldShadow(r,world)<.9f,"visible solid writes actual shadow depth");
        Check(world.SetRenderVisible(1,false,false),"component visibility publishes");pixels=Frame(r,world);Check(Near(Probe(pixels,{0,0,0}),{0,0,0})&&Depth()>.9999f&&WorldShadow(r,world)>.9999f,"hidden component contributes no colour, main depth or shadow depth");
        world.SetRenderVisible(1,true,false);world.SetRenderVisible(1,true,true);pixels=Frame(r,world);Check(Near(Probe(pixels,{0,0,0}),{0,0,0}),"entity hide/show preserves intentionally hidden component");
        world.SetRenderVisible(1,false,true);pixels=Frame(r,world);Check(Probe(pixels,{0,0,0}).x>220&&world.RuntimeBody(1).id==body.id&&world.Physics().AliveBodyCount()==bodyCount&&world.Physics().IsBodyEnabled(body),"restored renderer keeps original enabled collision body");world.Destroy();
    }
    {
        Scene scene;scene.Settings().backgroundColor={0,0,0};scene.Settings().ambientColor={.03f,.03f,.03f};scene.Settings().sunDirection={1,2,0};
        auto& floor=scene.CreateObject("Shadow receiver");floor.transform.position={0,-.15f,0};floor.render=SceneRenderComponent{};floor.render->halfExtents={3,.1f,3};floor.render->color={1,1,1};
        auto& caster=scene.CreateObject("Shadow caster");caster.transform.position={0,.75f,0};caster.render=SceneRenderComponent{};caster.render->halfExtents={.3f,.75f,.3f};caster.render->color={1,1,1};
        RuntimeWorld world;Check(world.Build(scene,nullptr,error),"independent receiver/caster world builds");auto view=glm::lookAt(glm::vec3(0,5,6),glm::vec3(0),glm::vec3(0,1,0));auto projection=glm::ortho(-3.f,3.f,-2.25f,2.25f,.1f,20.f);
        pixels=Frame(r,world,view,projection);auto west=Probe(pixels,{-.75f,-.05f,0},view,projection),east=Probe(pixels,{.75f,-.05f,0},view,projection);WriteRgbPng((out/"sun-east.png").string(),Width,Height,pixels);
        auto settings=world.Settings();settings.sunDirection={-1,2,0};world.SetAppearance(settings);pixels=Frame(r,world,view,projection);auto movedWest=Probe(pixels,{-.75f,-.05f,0},view,projection),movedEast=Probe(pixels,{.75f,-.05f,0},view,projection);WriteRgbPng((out/"sun-west.png").string(),Width,Height,pixels);
        std::printf("SHADOW probes east sun %.1f %.1f west sun %.1f %.1f\n",west.x,east.x,movedWest.x,movedEast.x);
        Check(east.x>west.x+60&&movedWest.x>movedEast.x+60,"moving sun moves real floor shadow to independently expected opposite side");world.Destroy();
    }

    // Normal asset references exercise pending/failed replacement fallback,
    // retirement, primitive slots, stable imported parts and independent instances.
    const auto assets=out/"fixture"/"Assets";fs::create_directories(assets);
    const std::string materialId="72727272727272727272727272727201",greenId="72727272727272727272727272727202",redId="72727272727272727272727272727203",badId="72727272727272727272727272727204",modelId="72727272727272727272727272727205";
    WriteRgbPng((assets/"green.png").string(),1,1,{0,255,0});Meta(assets/"green.png",greenId,"texture");WriteRgbPng((assets/"red.png").string(),1,1,{255,0,0});Meta(assets/"red.png",redId,"texture");std::ofstream(assets/"broken.png")<<"Expected M72 texture decode failure";Meta(assets/"broken.png",badId,"texture");
    MaterialDefinition green;green.model=MaterialModel::Unlit;green.maps[0].asset=greenId;Check(SaveMaterial((assets/"shared.judasmat").string(),green,error),"immutable shared material fixture saves");Meta(assets/"shared.judasmat",materialId,"material");
    std::vector<uint8_t> modelBytes;Check(EncodeModelArchive(Multipart(),modelBytes,error),"multipart model uses normal immutable cooked representation");{std::ofstream file(assets/"panels.judasmodel",std::ios::binary);file.write(reinterpret_cast<const char*>(modelBytes.data()),std::streamsize(modelBytes.size()));}Meta(assets/"panels.judasmodel",modelId,"mesh");
    host.OpenProjectAssets((out/"fixture").string(),assets.string());
    {
        Scene scene;scene.Settings().sunEnabled=false;scene.Settings().ambientColor={1,1,1};scene.Settings().backgroundColor={0,0,0};
        for(int i=0;i<2;++i){auto& object=scene.CreateObject("Shared primitive "+std::to_string(i));object.transform.position={i?1.f:-1.f,0,0};object.render=SceneRenderComponent{};object.render->halfExtents={.65f,.65f,.05f};object.render->color={1,1,1};object.render->materials.push_back({materialId,{}});}
        auto& camera=scene.CreateObject("Cheap live camera");camera.transform.position={0,0,6};camera.renderCamera=SceneRenderCameraComponent{};camera.renderCamera->width=128;camera.renderCamera->height=96;camera.renderCamera->verticalFovDegrees=50;
        auto& screen=scene.CreateObject("In-world live screen");screen.transform.position={0,-1.3f,2};screen.render=SceneRenderComponent{};screen.render->halfExtents={.6f,.3f,.02f};screen.render->color={1,1,1};screen.render->textureCamera=3;
        RuntimeWorld world;Check(world.Build(scene,&host.Resources(),error),"shared primitive/camera world builds through resources");host.Resources().WaitForAll();pixels=Frame(r,world);Check(Near(Probe(pixels,{-1,0,0}),{0,255,0})&&Near(Probe(pixels,{1,0,0}),{0,255,0}),"both material instances inherit identical source texture pixels");
        MaterialOverride replacement;replacement.textures[0]=redId;Check(world.SetRuntimeMaterial(1,"*",{"",replacement}),"instance-local texture request publishes durable intent");pixels=Frame(r,world);Check(Near(Probe(pixels,{-1,0,0}),{0,255,0}),"pending replacement retains inherited rendered map");host.Resources().WaitForAll();pixels=Frame(r,world);Check(Near(Probe(pixels,{-1,0,0}),{255,0,0})&&Near(Probe(pixels,{1,0,0}),{0,255,0}),"Ready texture changes only selected shared-material instance");
        TextureData auxiliary;auto target=world.CameraTexture(3);Check(r.ReadTextureForDiagnostics(target,auxiliary),"live auxiliary target contains actual pixels");auto cameraView=glm::lookAt(glm::vec3(0,0,6),glm::vec3(0),glm::vec3(0,1,0));auto cameraProjection=glm::perspective(glm::radians(50.f),128.f/96,.1f,1000.f);auto point=Project({-1,0,0},cameraView,cameraProjection,128,96);auto offset=size_t((95-point.y)*128+point.x)*4;Check(auxiliary.pixels[offset]>245&&auxiliary.pixels[offset+1]<5,"auxiliary observes coherent supported texture state without advancing time");
        auto time=world.SimulationTimeSeconds();world.SetRenderVisible(1,true,false);pixels=Frame(r,world);r.ReadTextureForDiagnostics(target,auxiliary);Check(Near(Probe(pixels,{-1,0,0}),{0,0,0})&&auxiliary.pixels[offset]<5&&world.SimulationTimeSeconds()==time,"entity visibility governs main and cheaper camera while simulation time holds");world.SetRenderVisible(1,true,true);
        replacement.textures[0]=badId;world.SetRuntimeMaterial(1,"*",{"",replacement});host.Resources().WaitForAll();pixels=Frame(r,world);Check(host.Resources().StateOf(badId)==ResourceState::Failed&&Near(Probe(pixels,{-1,0,0}),{0,255,0}),"failed replacement keeps documented inherited map without corrupting neighbour");
        replacement.textures[0]=std::string{};world.SetRuntimeMaterial(1,"*",{"",replacement});pixels=Frame(r,world);Check(Near(Probe(pixels,{-1,0,0}),{255,255,255})&&Near(Probe(pixels,{1,0,0}),{0,255,0}),"explicit empty texture removes only instance map");world.ClearRuntimeMaterial(1,"*");pixels=Frame(r,world);Check(Near(Probe(pixels,{-1,0,0}),{0,255,0}),"clear restores authored source texture");
        replacement.textures[0]=redId;world.SetRuntimeMaterial(1,"*",{"",replacement});pixels=Frame(r,world);const auto steadyCreates=textureCreates;const auto bytes=r.AppearanceBytes();for(int i=0;i<20;++i){world.SetRenderVisible(1,true,i%2==0);replacement.baseColor=glm::vec4(1,1,1,1);world.SetRuntimeMaterial(1,"*",{"",replacement});Frame(r,world);}Check(textureCreates==steadyCreates&&r.AppearanceBytes()==bytes,"steady material/visibility updates allocate no textures or appearance cache entries");world.SetRenderVisible(1,true,true);
        const auto retired=host.Resources().TryGetTexture(redId);host.Resources().Release(redId);Check(!r.ReadTextureForDiagnostics(retired,auxiliary),"texture retirement rejects borrowed stale GPU handle");Frame(r,world);host.Resources().WaitForAll();pixels=Frame(r,world);Check(Near(Probe(pixels,{-1,0,0}),{255,0,0})&&glGetError()==GL_NO_ERROR,"normal reload resolves replacement without main-view GPU state leakage");WriteRgbPng((out/"live-material-screen.png").string(),Width,Height,pixels);world.Destroy();Check(!r.ReadTextureForDiagnostics(target,auxiliary)&&r.RenderTargetDiagnostics().liveTargets==0,"world retirement releases live camera target");
    }
    {
        Scene scene;scene.Settings().sunEnabled=false;scene.Settings().linearRendering=true;scene.Settings().backgroundColor={0,0,0};
        for(int i=0;i<2;++i){auto& object=scene.CreateObject("Shared imported model "+std::to_string(i));object.transform.position={i?1.25f:-1.25f,0,0};object.render=SceneRenderComponent{};object.render->shape=SceneShape::Mesh;object.render->meshAsset=modelId;object.render->color={1,1,1};}
        RuntimeWorld world;Check(world.Build(scene,&host.Resources(),error),"shared multipart world builds");host.Resources().WaitForAll();pixels=Frame(r,world);const auto importedGreen=Probe(pixels,{-1.8f,0,0});Check(StrongChannel(importedGreen,1)&&Near(Probe(pixels,{1.8f,0,0}),importedGreen,4),"imported default material renders in both instances");
        const std::string part="Panel/Face/material/Paint";MaterialOverride tint;tint.baseColor=glm::vec4(1,0,0,1);Check(world.SetRuntimeMaterial(1,part,{"",tint}),"stable part material override publishes");pixels=Frame(r,world);Check(StrongChannel(Probe(pixels,{-1.8f,0,0}),0)&&Near(Probe(pixels,{-.7f,0,0}),importedGreen,4)&&Near(Probe(pixels,{.7f,0,0}),importedGreen,4),"targeted imported part changes without recolouring another part or shared instance");WriteRgbPng((out/"multipart-targeted.png").string(),Width,Height,pixels);
        world.SetModelPartVisible(1,part,false);world.SetRenderVisible(1,true,false);world.SetRenderVisible(1,true,true);pixels=Frame(r,world);Check(Near(Probe(pixels,{-1.8f,0,0}),{0,0,0},5)&&Near(Probe(pixels,{-.7f,0,0}),importedGreen,4),"part-hidden setting survives entity hide/show in actual imported draws");world.SetModelPartVisible(1,part,true);world.ClearRuntimeMaterial(1,part);pixels=Frame(r,world);Check(Near(Probe(pixels,{-1.8f,0,0}),importedGreen,4),"part reset restores imported shared default pixels");WriteRgbPng((out/"multipart-restored.png").string(),Width,Height,pixels);world.Destroy();
    }
    {
        // This bounded observation measures this process and driver, including
        // any GL submission waits. It is not a frame-rate target or GPU timer.
        // The existing M56 recorder provides the detailed scopes separately.
        constexpr unsigned Instances=128,DefaultFrames=20,ChangedFrames=60,Reloads=8;
        std::printf("PERF workload resolution=%dx%d shared_unlit_instances=%u default_visible=%u frequent_visible=%u frequent_sun_disabled_frames=%u/%u\n",Width,Height,Instances,Instances,Instances*3/4,ChangedFrames/10,ChangedFrames);
        host.Resources().Release(materialId);host.Resources().Release(greenId);host.Resources().Release(redId);
        Scene scene;scene.Settings().sunEnabled=true;scene.Settings().sunDirection={0,0,1};scene.Settings().ambientColor={.1f,.1f,.1f};scene.Settings().backgroundColor={0,0,0};
        for(unsigned i=0;i<Instances;++i){auto& object=scene.CreateObject("Measured shared instance "+std::to_string(i));object.transform.position={-2.7f+float(i%16)*.36f,-1.4f+float(i/16)*.4f,0};object.render=SceneRenderComponent{};object.render->halfExtents={.13f,.13f,.05f};object.render->color={1,1,1};object.render->materials.push_back({materialId,{}});}
        const auto coldTextures=textureCreates,coldMeshes=meshCreates;auto start=Timer::now();RuntimeWorld world;
        Check(world.Build(scene,&host.Resources(),error),"bounded 128-instance observation builds through shared source references");host.Resources().WaitForAll();const double prepareMs=Milliseconds(start);
        const auto source=host.Resources().TryGetMaterial(materialId);
        std::printf("PERF cold_world_and_shared_source_prepare_ms=%.4f instances=%u new_texture_uploads=%u new_mesh_uploads=%u shared_material_handles=%u disk_cache=uncontrolled\n",prepareMs,Instances,textureCreates-coldTextures,meshCreates-coldMeshes,unsigned(source.IsValid()));
        for(unsigned i=0;i<2;++i)RenderWorldFrame(r,Width,Height,world,nullptr,View(),Projection(),{0,0,0},1);
        r.FinishForDiagnostics();const auto steadyTextures=textureCreates,steadyMeshes=meshCreates;const auto steadyBytes=r.AppearanceBytes();
        auto& profiler=PerformanceProfiler::Get();const bool wasEnabled=profiler.Enabled();profiler.Enable(true);profiler.Clear();profiler.RegisterThread("M72 render observation");
        auto measure=[&](unsigned frames,bool change){std::vector<double> times;times.reserve(frames);
            for(unsigned frame=0;frame<frames;++frame){ProfileFrame record(change?"M72 frequent controls":"M72 shared defaults");auto frameStart=Timer::now();
                if(change){auto settings=world.Settings();settings.sunIntensity=frame%2?.4f:1.f;settings.sunColor=frame%2?glm::vec3(1,.7f,.4f):glm::vec3(1);settings.sunEnabled=frame%10!=0;world.SetAppearance(settings);
                    for(unsigned i=1;i<=Instances;++i){MaterialOverride patch;patch.baseColor=glm::vec4(1,frame%2?.6f:1.f,1,1);world.SetRuntimeMaterial(i,"*",{"",patch});world.SetRenderVisible(i,true,(i+frame)%4!=0);}
                }
                RenderWorldFrame(r,Width,Height,world,nullptr,View(),Projection(),{0,0,0},1);times.push_back(Milliseconds(frameStart));
            }return times;
        };
        ReportTiming("shared_defaults_cpu_submission",measure(DefaultFrames,false));r.FinishForDiagnostics();
        ReportTiming("frequent_factor_sun_visibility_cpu_submission",measure(ChangedFrames,true));r.FinishForDiagnostics();r.PollProfileGPU();
        Check(textureCreates==steadyTextures&&meshCreates==steadyMeshes&&r.AppearanceBytes()==steadyBytes&&host.Resources().TryGetMaterial(materialId).id==source.id,"128-instance steady controls create no source GPU uploads or appearance cache entries");
        std::printf("PERF steady new_texture_uploads=%u new_mesh_uploads=%u appearance_bytes_before=%zu after=%zu shared_material_handle_before=%u after=%u\n",textureCreates-steadyTextures,meshCreates-steadyMeshes,steadyBytes,r.AppearanceBytes(),source.id,host.Resources().TryGetMaterial(materialId).id);
        Check(profiler.Export((out/"shared-instance-profile.json").string(),error),"bounded workload exports existing M56 scope collection");profiler.Enable(wasEnabled);
        for(unsigned i=1;i<=Instances;++i)world.SetRenderVisible(i,true,true);
        const auto reloadCreates=textureCreates,reloadDestroys=textureDestroys;std::vector<double> reloadTimes;bool retiredAll=true;
        for(unsigned i=0;i<Reloads;++i){auto reloadStart=Timer::now();MaterialOverride replacement;replacement.textures[0]=redId;world.SetRuntimeMaterial(1,"*",{"",replacement});RenderWorldFrame(r,Width,Height,world,nullptr,View(),Projection(),{0,0,0},1);host.Resources().WaitForAll();RenderWorldFrame(r,Width,Height,world,nullptr,View(),Projection(),{0,0,0},1);
            const auto retired=host.Resources().TryGetTexture(redId);host.Resources().Release(redId);TextureData data;retiredAll=retiredAll&&retired.IsValid()&&!r.ReadTextureForDiagnostics(retired,data);reloadTimes.push_back(Milliseconds(reloadStart));
        }
        ReportTiming("replacement_prepare_submit_retire",reloadTimes);std::printf("PERF repeated_retirement cycles=%u texture_creates=%u texture_destroys=%u\n",Reloads,textureCreates-reloadCreates,textureDestroys-reloadDestroys);
        Check(retiredAll&&textureCreates-reloadCreates==textureDestroys-reloadDestroys,"repeated replacement resources retire with balanced actual GPU texture operations");world.Destroy();
    }
    Check(glGetError()==GL_NO_ERROR,"focused static/material/shadow/secondary paths retain cold GL coverage");host.Shutdown();Check(liveTextures.empty(),"all observed texture resources retire before context teardown");std::printf("M72 rendered controls %u checks %u failures\n",checks,failures);return failures?1:0;
}
