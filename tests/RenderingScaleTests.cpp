// Same native workloads are linked against baseline and candidate renderers.
#include "EngineHost.h"
#include "PerformanceProfiler.h"
#include "ScreenshotWriter.h"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cstdio>
#include <numeric>
using Clock=std::chrono::steady_clock;
static double Q(std::vector<double> v,double q){if(v.empty())return -1;std::sort(v.begin(),v.end());return v[size_t(q*(v.size()-1))];}
int main(int argc,char** argv){
 std::setvbuf(stdout,nullptr,_IOLBF,0);std::filesystem::path out=argc>1?argv[1]:"/tmp/m92-render";std::filesystem::create_directories(out);
 EngineHost host;std::string error;if(!host.Init("Rendering scale measurements",960,540,false,error)){std::fprintf(stderr,"%s\n",error.c_str());return 1;}SDL_GL_SetSwapInterval(0);auto& r=host.GetRenderer();
 std::ofstream csv(out/"samples.csv");csv<<"objects,lights,views,frame,cpu_ms,draws,triangles\n";
 for(unsigned objects:{256u,2048u})for(unsigned lightCount:{0u,5u,128u})for(unsigned views:{1u,2u}){
  std::vector<DynamicLight> lights;for(unsigned i=0;i<lightCount;++i){DynamicLight l;l.position={float(int(i%16)*4-30),3,float(int(i/16)*4-16)};l.range=7;l.color={4,2,1};lights.push_back(l);}
  auto target=r.CreateRenderTarget(480,270,error);std::vector<double> cpu,processCpu;auto& profiler=PerformanceProfiler::Get();profiler.Enable(false);
  for(int frame=-12;frame<60;++frame){if(frame==0){r.FinishForDiagnostics();profiler.Enable(true);profiler.Clear();}ProfileFrame pf("render scale");r.ResetStats();auto begin=Clock::now();auto cpuBegin=std::clock();auto token=r.BeginProfilePass("Whole scene");
   r.SetSceneAppearance(false,1,{},1,{1,0,0,0},false,{.025f,.035f,.05f});r.BeginFrame(960,540);
   for(unsigned view=0;view<views;++view){if(view)r.BeginRenderTarget(target);
    glm::vec3 eye{view?18.f:0.f,22,48};r.SetCamera(glm::lookAt(eye,{0,0,0},{0,1,0}),glm::perspective(glm::radians(65.f),960.f/540,.1f,200.f));r.SetLighting({0,1,0},{.4f,.4f,.4f},{.13f,.13f,.13f});r.SetDynamicLights(lights);r.SetMaterialBindings({});
#ifdef JUDAS_M92
    r.BeginMeshBatch();
#endif
    for(unsigned i=0;i<objects;++i){float x=float(int(i%64)-32)*1.1f,z=float(int(i/64)-16)*1.1f;r.DrawBox({x,.5f,z},{1,0,0,0},{.4f,.5f,.4f},{.45f,.55f,.65f});}
#ifdef JUDAS_M92
    r.EndMeshBatch();
#endif
    if(view)r.EndRenderTarget();
   }r.EndFrame();r.EndProfilePass(token);double ms=std::chrono::duration<double,std::milli>(Clock::now()-begin).count();host.GetWindow().SwapBuffers();
   if(frame>=0){cpu.push_back(ms);processCpu.push_back(1000.0*double(std::clock()-cpuBegin)/CLOCKS_PER_SEC);auto s=r.Stats();csv<<objects<<','<<lightCount<<','<<views<<','<<frame<<','<<ms<<','<<s.drawCalls<<','<<s.triangles<<'\n';}
  }
  r.FinishForDiagnostics();{ProfileFrame p("drain");r.PollProfileGPU();}std::vector<double> gpu;for(auto& f:profiler.History())for(auto& g:f.gpu)if(!g.pending&&g.pass=="Whole scene")gpu.push_back(g.milliseconds);
  auto name=std::to_string(objects)+"-"+std::to_string(lightCount)+"-"+std::to_string(views);profiler.Export((out/(name+".json")).string(),error);std::vector<unsigned char> pixels;r.CaptureFrame(960,540,pixels);WriteRgbPng((out/(name+".png")).string(),960,540,pixels);
  std::printf("MEASURE objects=%u lights=%u views=%u cpu=%.6f p95=%.6f max=%.6f gpu=%.6f gp95=%.6f draws=%u\n",objects,lightCount,views,Q(cpu,.5),Q(cpu,.95),Q(cpu,1),Q(gpu,.5),Q(gpu,.95),r.Stats().drawCalls);std::printf("CPU_WORK median=%.6f p95=%.6f max=%.6f\n",Q(processCpu,.5),Q(processCpu,.95),Q(processCpu,1));
#ifdef JUDAS_M92
  std::printf("PAYLOAD light_bytes=%zu shadow_bytes=%zu max_instance_upload=%zu\n",r.Stats().lightBytes,r.Stats().shadowBytes,r.Stats().instanceBytes);
#endif
  r.DestroyRenderTarget(target);
 }
 return 0;
}
