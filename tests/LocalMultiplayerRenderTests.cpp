#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "WorldPresentation.h"
#include "ScreenshotWriter.h"
#include <filesystem>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>
int checks=0,failures=0;
void Check(bool ok,const char* text){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",text);}
int main(){std::string error;EngineHost host;if(!host.Init("M89 player views",320,240,false,error)){std::puts(error.c_str());return 1;}auto& renderer=host.GetRenderer();std::filesystem::create_directories(".cache/m89/render");
 for(bool linear:{false,true}){
  Scene scene;scene.Settings().linearRendering=linear;scene.Settings().ambientColor={1,1,1};scene.Settings().sunColor={0,0,0};
  auto& box=scene.CreateObject("Physical subject");box.transform.position={0,.9f,0};box.render=SceneRenderComponent{};box.render->color={1,.01f,.01f};box.body=SceneBodyComponent{};box.body->motion=SceneBodyMotion::Dynamic;
  RuntimeWorld world;Check(world.Build(scene,nullptr,error),"ordinary physical world with optional views");auto& players=world.Players();const auto a=players.Create("A",InputMap::Defaults(),error)->id,b=players.Create("B",InputMap::Defaults(),error)->id;players.SetViewport(a,{0,0,.5f,1},error);players.SetViewport(b,{.5f,0,.5f,1},error);
  SceneTransform pose;pose.position={0,0,5};players.SetView(a,pose,60,.1f,50,error);pose.rotation=glm::angleAxis(glm::radians(180.f),glm::vec3(0,0,1));players.SetView(b,pose,60,.1f,50,error);
  const auto view=glm::lookAt(glm::vec3(0,0,5),glm::vec3(0),glm::vec3(0,1,0)),projection=glm::perspective(glm::radians(60.f),320.f/240,.1f,50.f);
  auto frame=[&](){RenderWorldFrame(renderer,320,240,world,nullptr,view,projection,{0,1,0},1);};EntityPhysicalState before,after;world.GetEntityState(1,before);frame();world.GetEntityState(1,after);
  Check(before.position==after.position&&before.linearVelocity==after.linearVelocity,"two camera renders do not step physical state");Check(renderer.RenderTargetDiagnostics().liveTargets==2&&players.Find(a)->renderError.empty()&&players.Find(b)->renderError.empty(),"both actual display targets render successfully");
  // CaptureFrame returns top-down rows, already flipped from GL readback.
  std::vector<unsigned char> pixels;renderer.CaptureFrame(320,240,pixels);double sums[2]={0,0};unsigned counts[2]={0,0};for(int y=0;y<240;y++)for(int x=0;x<320;x++){const auto k=(y*320+x)*3;if(pixels[k]>100&&pixels[k]>pixels[k+1]*2&&pixels[k]>pixels[k+2]*2){const int i=x<160?0:1;sums[i]+=y;counts[i]++;}}
  Check(counts[0]>20&&counts[1]>20&&sums[0]/std::max(1u,counts[0])<120&&sums[1]/std::max(1u,counts[1])>120,"independent camera orientation survives FBO composition upright, including HDR display conversion");WriteRgbPng(linear?".cache/m89/render/linear.png":".cache/m89/render/legacy.png",320,240,pixels);
  players.SetViewport(a,{0,0,.25f,1},error);frame();Check(renderer.RenderTargetSize(players.Find(a)->target)==glm::ivec2(80,240)&&renderer.RenderTargetDiagnostics().liveTargets==2,"viewport resizing replaces rather than leaks display resources");
  players.Find(a)->view.reset();players.Find(b)->view.reset();renderer.ResetStats();frame();Check(renderer.Stats().offscreenPasses==0,"shared screen does not render inactive player targets");world.Destroy();Check(renderer.RenderTargetDiagnostics().liveTargets==0,"world teardown retires all local-view GPU resources before renderer shutdown");
 }
 host.Shutdown();std::printf("%d checks %d failures\n",checks,failures);return failures?1:0;}
