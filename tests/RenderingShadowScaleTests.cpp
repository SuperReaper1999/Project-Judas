// Identical geometry/lights/shadows on baseline GL3.3 and M92. Baseline compiles
// with frozen baseline headers; candidate enables only pass-scoped instancing.
#include "EngineHost.h"
#include "PerformanceProfiler.h"
#include "ShadowTransforms.h"
#include "ScreenshotWriter.h"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cstdio>
using Clock=std::chrono::steady_clock;
static double Q(std::vector<double> v,double q){if(v.empty())return -1;std::sort(v.begin(),v.end());return v[size_t(q*(v.size()-1))];}
int main(int argc,char**argv){std::setvbuf(stdout,nullptr,_IOLBF,0);std::filesystem::path out=argc>1?argv[1]:".cache/m92/shadow-measure";std::filesystem::create_directories(out);EngineHost host;std::string error;if(!host.Init("Shadow scale",960,540,false,error)){std::puts(error.c_str());return 1;}SDL_GL_SetSwapInterval(0);auto& r=host.GetRenderer();std::ofstream csv(out/"samples.csv");csv<<"objects,shadowed,views,sample,cpu_ms,draws,triangles\n";
 for(unsigned objects:{256u,2048u})for(unsigned shadowed:{0u,2u})for(unsigned views:{1u,2u}){
  auto target=r.CreateRenderTarget(480,270,error);auto& profiler=PerformanceProfiler::Get();profiler.Enable(false);std::vector<double> cpu,processCpu;std::vector<DynamicLight> lights(2);
  for(unsigned i=0;i<2;++i){auto& l=lights[i];l.kind=LightKind::Spot;l.position={i?14.f:-14.f,15,10};l.direction=glm::normalize(-l.position);l.color={25,22,20};l.range=65;l.innerConeDegrees=30;l.outerConeDegrees=48;l.shadowMapIndex=shadowed?int(i+1):-1;}
  auto geometry=[&](){r.SetMaterialBindings({});
#ifdef JUDAS_M92
   r.BeginMeshBatch();
#endif
   r.DrawBox({0,-.25f,0},{1,0,0,0},{45,.25f,30},{.3f,.32f,.36f});
   for(unsigned i=0;i<objects;++i)r.DrawBox({float(int(i%64)-32)*1.1f,.5f,float(int(i/64)-16)*1.1f},{1,0,0,0},{.4f,.5f,.4f},{.4f,.5f,.6f});
#ifdef JUDAS_M92
   r.EndMeshBatch();
#endif
  };
  for(int frame=-12;frame<60;++frame){if(frame==0){r.FinishForDiagnostics();profiler.Enable(true);profiler.Clear();}ProfileFrame pf("shadow scale");r.ResetStats();auto begin=Clock::now();auto cpuBegin=std::clock();auto token=r.BeginProfilePass("Whole scene");
   r.SetSceneAppearance(false,1,{},1,{1,0,0,0},false,{.025f,.035f,.05f});
   for(unsigned v=0;v<views;++v){glm::vec3 eye{v?18.f:0.f,22,48};r.SetCamera(glm::lookAt(eye,{0,0,0},{0,1,0}),glm::perspective(glm::radians(65.f),960.f/540,.1f,200.f));
    for(auto& l:lights)if(l.shadowMapIndex>0){r.BeginShadowPass(l.shadowMapIndex,ComputeSpotShadowMatrix(l.position,l.direction,l.outerConeDegrees,l.range));geometry();r.EndShadowPass();}
    if(v)r.BeginRenderTarget(target);else r.BeginFrame(960,540);r.SetCamera(glm::lookAt(eye,{0,0,0},{0,1,0}),glm::perspective(glm::radians(65.f),960.f/540,.1f,200.f));r.SetLighting({0,1,0},{0,0,0},{.12f,.12f,.12f});r.SetDynamicLights(lights);geometry();if(v)r.EndRenderTarget();
   }r.EndFrame();r.EndProfilePass(token);double ms=std::chrono::duration<double,std::milli>(Clock::now()-begin).count();host.GetWindow().SwapBuffers();if(frame>=0){cpu.push_back(ms);processCpu.push_back(1000.0*double(std::clock()-cpuBegin)/CLOCKS_PER_SEC);auto s=r.Stats();csv<<objects<<','<<shadowed<<','<<views<<','<<frame<<','<<ms<<','<<s.drawCalls<<','<<s.triangles<<'\n';}}
  r.FinishForDiagnostics();{ProfileFrame p("drain");r.PollProfileGPU();}std::vector<double> gpu;for(auto& f:profiler.History())for(auto& g:f.gpu)if(!g.pending&&g.pass=="Whole scene")gpu.push_back(g.milliseconds);auto name=std::to_string(objects)+"-"+std::to_string(shadowed)+"-"+std::to_string(views);profiler.Export((out/(name+".json")).string(),error);std::vector<unsigned char> pixels;r.CaptureFrame(960,540,pixels);WriteRgbPng((out/(name+".png")).string(),960,540,pixels);
  std::printf("MEASURE objects=%u shadows=%u views=%u cpu=%.6f p95=%.6f max=%.6f gpu=%.6f gp95=%.6f draws=%u\n",objects,shadowed,views,Q(cpu,.5),Q(cpu,.95),Q(cpu,1),Q(gpu,.5),Q(gpu,.95),r.Stats().drawCalls);std::printf("CPU_WORK median=%.6f p95=%.6f max=%.6f\n",Q(processCpu,.5),Q(processCpu,.95),Q(processCpu,1));
#ifdef JUDAS_M92
  std::printf("PAYLOAD light_bytes=%zu shadow_bytes=%zu max_instance_upload=%zu\n",r.Stats().lightBytes,r.Stats().shadowBytes,r.Stats().instanceBytes);
#endif
  r.DestroyRenderTarget(target);
 }return 0;}
