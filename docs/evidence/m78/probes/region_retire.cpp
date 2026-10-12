// Minimal read-only mechanism investigation, separate from the failed M68 suite.
#include "WorldStreaming.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include <filesystem>
#include <iostream>
#include <thread>
#include <chrono>
int main(int argc,char**argv){if(argc!=2)return 2;namespace fs=std::filesystem;fs::path base=argv[1];fs::create_directories(base);bool controls=true;
 for(int count:{10,100})for(size_t budget:{4*1024*1024,16*1024*1024}){
  auto out=base/(std::to_string(count)+"-"+std::to_string(budget));Project project;std::string error;if(!Project::CreateNew(out.string(),"M78 retirement",project,error))return 3;
  Scene region;for(int i=0;i<count;++i)region.CreateObject("inert");if(!SaveSceneToFile(region,project.Resolve("Scenes/part.judas"),error))return 4;
  AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());JobSystem jobs(2);ResourceManager resources(nullptr,&assets,&jobs);RuntimeWorld world;Scene empty;if(!world.Build(empty,&resources,error))return 5;
  WorldManifest manifest;manifest.retainedBytes=budget;for(int i=0;i<4;++i){WorldRegion r;r.id="r"+std::to_string(i);r.scene="Scenes/part.judas";manifest.regions[r.id]=r;}
  WorldStreaming stream(world,resources,project,manifest);std::vector<uint64_t> tokens;for(int i=0;i<4;++i)tokens.push_back(stream.Request("r"+std::to_string(i),false,error));
  auto pump=[&](auto done){for(int i=0;i<2500;++i){resources.Pump();stream.Advance(false);if(done())return true;std::this_thread::sleep_for(std::chrono::milliseconds(1));}return false;};
  bool ready=pump([&]{return stream.Stats().active==4;});for(auto token:tokens)stream.Release(token);bool retired=pump([&]{return stream.Stats().active==0&&stream.Stats().pending==0;});controls&=ready;
  std::cout<<"CASE objects_per_region="<<count<<" budget="<<budget<<" ready="<<ready<<" retired="<<retired<<" active="<<stream.Stats().active<<" retained="<<stream.Stats().retainedBytes<<" scene_object_size="<<sizeof(SceneObject)<<'\n';
  for(auto& r:stream.Regions()){std::cout<<r.id<<" state="<<r.state<<" error="<<r.error<<" bytes="<<r.retained;for(auto& pin:r.pins)std::cout<<" PIN="<<pin;std::cout<<'\n';}
  controls&=retired; // Baseline zero-component regions have no documented safety pin.
 }
 std::cout<<"all_retired="<<controls<<'\n';return controls?0:1;
}
