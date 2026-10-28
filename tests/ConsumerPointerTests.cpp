#include "AssetDatabase.h"
#include "InputSystem.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "../third_party/nlohmann/json.hpp"
#include <cstdio>
#include <filesystem>

namespace {
using J=nlohmann::json;unsigned checks=0,failures=0;
void Check(bool ok,const char* text){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",text);}
J State(RuntimeWorld& w){for(const auto& row:w.Scripts()->Capture())if(row.entity==1)return J::parse(row.json);return J::object();}
}
int main(int argc,char** argv){namespace fs=std::filesystem;const auto output=fs::absolute(argc>1?argv[1]:"build/consumer-pointer");const auto fixtures=fs::absolute(argc>2?argv[2]:"tests/fixtures/pointer-taps");fs::create_directories(output/"Assets");std::string error;
 for(const char* name:{"original","fixed"}){
  fs::copy_file(fixtures/(std::string(name)+".js"),output/"Assets"/(std::string(name)+".js"),fs::copy_options::overwrite_existing);AssetDatabase db;db.Scan(output.string(),(output/"Assets").string());AssetRecord script;const auto* existing=db.FindByRelativePath("Assets/"+std::string(name)+".js");const bool tracked=existing?(script=*existing,true):db.Track((output/"Assets"/(std::string(name)+".js")).string(),script,error);Check(tracked,"real consumer script fixture registered");if(failures){std::puts(error.c_str());return 1;}ResourceManager resources(nullptr,&db);
  for(unsigned scenario=0;scenario<7;++scenario){RuntimeWorld world;world.legacyGameplay=false;Scene scene;auto& owner=scene.CreateObject("Consumer tap detector");owner.id=1;owner.scripts={{1,script.id,true,"{}"}};scene.SetNextId(2);Check(world.Build(scene,&resources,error),"ordinary script runtime builds");if(failures){std::puts(error.c_str());return 1;}world.viewportWidth=640;world.viewportHeight=360;InputSystem input;auto map=InputMap::Defaults();map.Add("pointer_press",false);map.AddBinding("pointer_press",{"mouse:Left"});Check(input.SetMap(map,error),"project-owned pointer action installed");world.UpdateScripts(&input,0);
   auto tick=[&](){input.BeginFixedStep();world.FixedScripts(&input,1.f/60);};auto pointer=[&](float x,float y,bool down){input.SetPointer(x,y,down);input.SetPhysical("mouse:Left",down?1:0);};
   if(scenario==0||scenario==1||scenario==5||scenario==6){input.BeginFrame();pointer(.5f,scenario==5?.05f:.5f,true);pointer(.5f,scenario==5?.05f:.5f,false);if(scenario==1)input.BeginFrame();if(scenario==6)input.ConsumeBindings({"ui_click"});tick();}
   if(scenario==2){input.BeginFrame();pointer(.5f,.5f,true);tick();input.BeginFrame();pointer(.5f,.5f,false);tick();}
   if(scenario==3){input.BeginFrame();pointer(.5f,.5f,true);tick();input.BeginFrame();pointer(.8f,.5f,true);tick();input.BeginFrame();pointer(.8f,.5f,false);tick();}
   if(scenario==4){input.BeginFrame();pointer(.5f,.5f,true);for(unsigned step=0;step<22;++step){tick();input.BeginFrame();}pointer(.5f,.5f,false);tick();}
   Check(world.Scripts()->Diagnostics().empty(),"consumer callback remains healthy");auto state=State(world);unsigned expected=scenario==2||(std::string(name)=="fixed"&&(scenario==0||scenario==1))?1:0;Check(state.value("taps",999u)==expected,"native logical edges preserve short taps while drag/hold/HUD/UI-consumed inputs do not fire");if(scenario==3||scenario==4)Check(state.value("drags",0)>0,"negative control actually exercises aiming/long-press path");std::printf("CONSUMER_POINTER version=%s scenario=%u taps=%u expected=%u\n",name,scenario,state.value("taps",999u),expected);
  }
 }
 std::printf("CONSUMER_POINTER checks=%u failures=%u\n",checks,failures);return failures?1:0;
}
