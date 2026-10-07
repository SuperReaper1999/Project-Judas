#include "EngineHost.h"
#include "Project.h"
#include "SceneSerialization.h"
#include "PerformanceProfiler.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
namespace fs=std::filesystem;using Clock=std::chrono::steady_clock;using J=nlohmann::json;
int main(int argc,char** argv){
 if(argc!=2){std::cerr<<"usage: judas_resource_handoff_tests NEW-owned-output\n";return 2;}
 unsigned checks=0;auto check=[&](bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);std::cout<<"PASS "<<label<<'\n';};
 try{
  auto out=fs::absolute(argv[1]);check(!fs::exists(out),"fresh owned output required");fs::create_directories(out);
  auto& profiler=PerformanceProfiler::Get();profiler.Enable(true);profiler.RegisterThread("Resource handoff owner");
  Project project;std::string error;check(project.Load("projects/import_lab/import_lab.judasproj",error),"ordinary import lab project");Scene scene;check(LoadSceneFromFile(project.StartupScenePath(),scene,error),"authored original multipart scene");auto id=scene.Find(10)->render->meshAsset;
  EngineHost host;check(host.Init("M68 private resource handoff",640,480,false,error),"actual owner-thread GL context");host.OpenProjectAssets(project.RootDir(),project.AssetsDir());std::cout<<"GL renderer: "<<reinterpret_cast<const char*>(glGetString(GL_RENDERER))<<" vendor: "<<reinterpret_cast<const char*>(glGetString(GL_VENDOR))<<'\n';auto& resources=host.Resources();resources.AddRef(id);resources.RequestMesh(id);
  auto start=Clock::now();std::vector<double> pumps;unsigned privateFrames=0;bool cancelled=false,admissionBounded=true,publicationPrivate=true;
  while(resources.StateOf(id)!=ResourceState::Ready){
   if(Clock::now()-start>=std::chrono::seconds(30))throw std::runtime_error("bounded ready progress");
   {ProfileFrame f("M68 resource admission/upload");auto t=Clock::now();resources.Pump();pumps.push_back(std::chrono::duration<double,std::milli>(Clock::now()-t).count());}
   auto stats=resources.Stats();admissionBounded &= stats.admittedRequests<=3;
   if(resources.StateOf(id)==ResourceState::CpuReady){++privateFrames;publicationPrivate &= !resources.TryGetMesh(id).IsValid();if(!cancelled){resources.ReleaseRef(id);resources.Pump();check(!resources.TryGetMesh(id).IsValid(),"cancelled private upload cannot publish");resources.AddRef(id);resources.RequestMesh(id);cancelled=true;}}
   if(resources.StateOf(id)==ResourceState::Failed)throw std::runtime_error(resources.ErrorOf(id));
   std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  check(resources.TryGetMesh(id).IsValid()&&resources.TryGetSkeletal(id)->skeleton.skinNodes.size()==109,"complete full-quality original asset publishes normally");
  check(admissionBounded&&publicationPrivate,"bounded admission and private publication throughout all pumps");
  check(privateFrames>0&&cancelled,"real staged cancellation exercised");
  std::sort(pumps.begin(),pumps.end());double ready=std::chrono::duration<double,std::milli>(Clock::now()-start).count();auto stats=resources.Stats();
  profiler.Export((out/"m56-profile.json").string(),error);J result{{"checks",checks},{"readyIncludingCancellationMs",ready},{"pumpMs",{{"median",pumps[pumps.size()/2]},{"p95",pumps[(pumps.size()-1)*95/100]},{"maximum",pumps.back()}}},{"privateFrames",privateFrames},{"residentBytes",stats.bytesResident},{"samples",pumps.size()}};
  resources.ReleaseRef(id);resources.Release(id);resources.Pump();check(resources.Stats().loading==0&&resources.Stats().bytesResident==0,"retirement returns CPU/GPU residency to zero");result["checks"]=checks;result["postUnloadBytes"]=resources.Stats().bytesResident;result["failures"]=0;std::ofstream(out/"results.json")<<result.dump(2)<<'\n';host.Shutdown();std::cout<<"SUMMARY "<<checks<<" checks\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
