#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "WorldPresentation.h"
#include "SceneSerialization.h"
#include "ModelLoader.h"
#include "SkeletalAnimation.h"
#include "PoseComposition.h"
#include "PerformanceProfiler.h"
#include "ScreenshotWriter.h"
#include "Project.h"
#include "Prefab.h"
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <cstdio>
#include <chrono>
#include <algorithm>
namespace fs=std::filesystem;
unsigned checks=0,failures=0;void Check(bool ok,const char* s){++checks;failures+=!ok;printf("%s %s\n",ok?"PASS":"FAIL",s);fflush(stdout);}
int main(int argc,char** argv){fs::path out=argc>1?argv[1]:"/tmp/m66-runtime";fs::create_directories(out);std::string error;const fs::path fixture="tests/fixtures/model-runtime";
 EngineHost host;Check(host.Init("M66 runtime GL",640,480,false,error),"normal GL 3.3 EngineHost");if(failures)return 1;host.OpenProjectAssets(fixture.string(),(fixture/"Assets").string());auto& r=host.GetRenderer();auto& resources=host.Resources();const std::string asset="d7bc89f7d9bc43f8fd3f2cb61702dfa5";auto begin=std::chrono::steady_clock::now();resources.AddRef(asset);resources.GetMesh(asset,error);resources.WaitForAll();auto model=resources.GetMesh(asset,error);double cold=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();Check(model.IsValid()&&resources.TryGetSkeletal(asset),"cold cooked async load and owner-thread GPU publication");if(!model.IsValid()){puts(error.c_str());return 1;}
 auto frame=[&](MeshHandle mesh,const std::vector<glm::mat4>* palette,const std::vector<std::string>* hidden=nullptr){r.BeginFrame(640,480);r.SetSceneAppearance(true,1,{},1,{1,0,0,0},false,{0,0,0});r.SetDynamicLights({});r.SetMaterialBindings({});r.SetLighting({0,0,1},{1,1,1},{.2,.2,.2});r.SetCamera(glm::lookAt(glm::vec3(1,1,8),glm::vec3(1,1,0),glm::vec3(0,1,0)),glm::ortho(-2.f,2.f,-1.5f,1.5f,.1f,30.f));r.DrawMesh(mesh,{0,0,0},{1,0,0,0},{1,1,1},{},{1,1,1},1,palette,hidden);r.EndFrame();std::vector<unsigned char> p;r.CaptureFrame(640,480,p);return p;};
 const std::string capacityId="00f95bedb4129c33beab3ab1bc22dd34";resources.AddRef(capacityId);resources.GetMesh(capacityId,error);resources.WaitForAll();auto capacity=resources.GetMesh(capacityId,error);auto high=resources.TryGetSkeletal(capacityId);Check(capacity.IsValid()&&high&&high->skeleton.skinNodes.size()==256,"256-joint resource reaches same GPU architecture");if(!high)return 1;auto p0=ResolveSkinMatrices(high->skeleton,SampleClip(high->skeleton,high->clips[0],0)),p1=ResolveSkinMatrices(high->skeleton,SampleClip(high->skeleton,high->clips[0],1));auto pixels0=frame(capacity,&p0),pixels1=frame(capacity,&p1);auto centroid=[](auto& p){double x=0,n=0;for(size_t i=0;i<p.size();i+=3)if(p[i]>40||p[i+1]>40||p[i+2]>40){x+=(i/3)%640;++n;}return n?x/n:-1;};double shift=centroid(pixels1)-centroid(pixels0);Check(std::abs(shift-80)<2,"actual high-index eight-influence GPU skinning moves image by analytic 0.5 metres");WriteRgbPng((out/"capacity-256.png").string(),640,480,pixels1);
 auto full=resources.TryGetSkeletal(asset);auto rest=ResolveSkinMatrices(full->skeleton,full->skeleton.rest),moving=ResolveSkinMatrices(full->skeleton,SampleClip(full->skeleton,full->clips[0],.5));auto a=frame(model,&rest),b=frame(model,&moving);Check(a!=b,"multiple skins and affine attachment use sampled pose palettes");std::vector<std::string> hidden;for(auto& part:*resources.TryGetModelParts(asset))hidden.push_back(part.part);auto blank=frame(model,&moving,&hidden);Check(blank!=b&&centroid(blank)<0,"all four authored primitive parts can be hidden through the normal renderer");WriteRgbPng((out/"original-rest.png").string(),640,480,a);WriteRgbPng((out/"original-push.png").string(),640,480,b);
 MeshData uvProof;Check(LoadModelMesh("tests/fixtures/m66/material-uv.gltf",uvProof,error),"normal glTF UV1/mirrored/mask material fixture");auto uvMesh=r.CreateMesh(uvProof);auto uvPixels=frame(uvMesh,nullptr);size_t red=0,blue=0;for(size_t i=0;i<uvPixels.size();i+=3){red+=uvPixels[i]>uvPixels[i+2]+15;blue+=uvPixels[i+2]>uvPixels[i]+15;}Check(red>500&&blue==0,"actual GL samples opaque red UV1, not transparent blue UV0, on both reflected parts");uvProof.materials[0].maps[2].embedded={};uvProof.materials[0].maps[2].encodedImage.clear();auto plain=r.CreateMesh(uvProof);auto noNormal=frame(plain,nullptr);Check(noNormal!=uvPixels,"UV1 Mikk tangent-space normal map changes actual lighting");r.DestroyMesh(uvMesh);r.DestroyMesh(plain);
 Scene scene;
 auto& firstActor=scene.CreateObject("Multipart animated actor");
 const auto firstId=firstActor.id;
 firstActor.render=SceneRenderComponent{};
 firstActor.render->shape=SceneShape::Mesh;
 firstActor.render->meshAsset=asset;
 firstActor.animation=SceneAnimationComponent{};
 firstActor.animation->clip="Translate";
 const auto actorRender=*firstActor.render;
 const auto actorAnimation=*firstActor.animation;
 auto& secondActor=scene.CreateObject("Independent animation clock");
 const auto secondId=secondActor.id;
 secondActor.render=actorRender;
 secondActor.animation=actorAnimation;
 secondActor.animation->time=.5f;
 secondActor.transform.position={0,0,-1};
 auto& camera=scene.CreateObject("Secondary camera");
 const auto cameraId=camera.id;
 camera.transform.position={1,1,8};
 camera.renderCamera=SceneRenderCameraComponent{};
 RuntimeWorld world;
 world.legacyGameplay=false;
 Check(world.Build(scene,&resources,error),"synthetic model/camera world uses the ordinary runtime");
 resources.WaitForAll();world.UpdateAnimations(.4);
 const auto* first=world.RuntimeAnimation(firstId);
 const auto* second=world.RuntimeAnimation(secondId);
 Check(first&&second&&first->asset==second->asset&&first->playback.time!=second->playback.time,
       "immutable multipart asset is shared while playback clocks remain independent");
 Check(world.AnimationSkin(firstId)&&world.AnimationSkin(firstId)->size()==6,
       "independently known six-entry per-skin palette reaches runtime composition");
 SceneTransform bone;
 Check(world.JointPose(firstId,"BoneA","world",1,bone),"stable synthetic joint resolves through normal runtime socket query");
 r.ResetStats();
 RenderWorldFrame(r,640,480,world,nullptr,
     glm::lookAt(glm::vec3(1,1,8),glm::vec3(1,1,0),glm::vec3(0,1,0)),
     glm::perspective(glm::radians(60.f),640.f/480,.1f,100.f),{0,1,0},1);
 TextureData secondary;
 Check(r.Stats().shadowPasses>0&&r.Stats().offscreenPasses==1&&
       r.ReadTextureForDiagnostics(world.CameraTexture(cameraId),secondary),
       "multipart actors use ordinary shadow and secondary-camera passes");
 size_t occupied=0;
 for(size_t i=0;i<secondary.pixels.size();i+=4)
     occupied+=secondary.pixels[i]>50||secondary.pixels[i+1]>50;
 Check(occupied>100,"secondary camera receives actual model pixels");
 std::vector<unsigned char> screenshot;r.CaptureFrame(640,480,screenshot);
 WriteRgbPng((out/"synthetic-world.png").string(),640,480,screenshot);
 world.Destroy();
 Check(!world.AnimationSkin(firstId)&&world.Build(scene,&resources,error),
       "Stop/rebuild releases model pose ownership and restores the same authored components");
 world.Destroy();host.Shutdown();
 printf("PERF cold_load_upload_ms=%.4f palette_entries=%zu\n",cold,moving.size());
 printf("SUMMARY checks=%u failures=%u\n",checks,failures);
 return failures?1:0;
}
