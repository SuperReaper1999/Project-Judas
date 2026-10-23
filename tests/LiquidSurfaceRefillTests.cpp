// Human-reproduced M55 drain/refill regression through ordinary authored JS
// actions, ownership transactions, fixed steps and collision geometry.
#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "GameSession.h"
#include "Simulation.h"
#include "Project.h"
#include "SceneSerialization.h"
#include <algorithm>
#include <cstdio>
#include <string>
#include <array>

int main(int argc,char** argv){
 unsigned checks=0,failed=0;std::string error;
 auto check=[&](bool ok,const char* message){++checks;failed+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",message);};
 Project project;EngineHost host;
 if(!project.Load("projects/liquid_surface_demo/liquid_surface_demo.judasproj",error)||!host.Init("M55 refill regression",320,180,false,error)){std::puts(error.c_str());return 1;}
 host.OpenProjectAssets(project.RootDir(),project.AssetsDir());auto& window=host.GetWindow();window.SetTestInputMode(true);window.Input().SetMap(project.Settings().input,error);
 for(const char* name:{"lab","radial"}){
  if(argc>1&&std::string(argv[1])!=name)continue;
  bool radial=std::string(name)=="radial",baseline=argc>2&&std::string(argv[2])=="--baseline";Scene scene;RuntimeWorld world;world.ConfigureProject(project.Settings());GameSession game;world.legacyGameplay=false;
  if(!LoadSceneFromFile(project.RootDir()+"/Scenes/"+name+".judas",scene,error)||!world.Build(scene,&host.Resources(),error,&project.Settings().classification)||!game.Begin(world,error)){std::puts(error.c_str());return 1;}
  world.legacyGameplay=false;host.Resources().WaitForAll();
  auto step=[&](){window.Input().BeginFixedStep();world.UpdateUIScripts(&window.Input(),1.f/60);world.UpdateScripts(&window.Input(),1.f/60);StepPlayedWorld(game,window,1.f/60);world.PresentationScripts(&window.Input(),1.f/60,.5f);window.Input().BeginFrame();};
  for(unsigned i=0;i<30;++i)step();
  auto& liquid=world.Liquids();auto h=liquid.Handle(radial?120:4000),storage=liquid.Handle(radial?121:4001);auto* owner=liquid.Get(h);
  if(!owner||!owner->dynamicSurface){std::puts("missing loaded surface");return 1;}
  double initial=owner->volume,total=liquid.Accounting("water").total;
  struct Measurement {unsigned steps=0,iterations=0,maximumIterations=0,retries=0;double seconds=0,maximumSeconds=0,maximumResidual=0;};
  std::array<Measurement,4> measurements{};unsigned phase=0;
  auto measuredStep=[&](){step();auto& m=measurements[phase];auto& s=owner->dynamicSurface->stats;++m.steps;m.iterations+=s.iterations;m.maximumIterations=std::max(m.maximumIterations,s.iterations);m.retries+=s.retries;m.seconds+=s.seconds;m.maximumSeconds=std::max(m.maximumSeconds,s.seconds);m.maximumResidual=std::max(m.maximumResidual,s.residual);};
  auto report=[&](const char* phase,unsigned cycle,unsigned press){unsigned wet=0;double minimum=1e30,maximum=-1e30;auto& s=*owner->dynamicSurface;
   for(auto& c:s.cells){wet+=c.volume>1e-10;minimum=std::min(minimum,c.q);maximum=std::max(maximum,c.q);}
   std::printf("M55 %s %s cycle %u press %u pool %.12g storage %.12g wet %u/%zu q %.9g..%.9g iterations %u retries %u residual %.12g solve %.6f ms ledger %.12g partition %.12g\n",name,phase,cycle,press,owner->volume,liquid.Get(storage)->volume,wet,s.cells.size(),minimum,maximum,s.stats.iterations,s.stats.retries,s.stats.residual,s.stats.seconds*1000,liquid.Accounting("water").error,s.Volume()-owner->volume);
   for(auto& [id,e]:liquid.Errors())std::printf("M55 error %llu: %s\n",(unsigned long long)id,e.c_str());
   std::fflush(stdout);
  };
  auto valid=[&](){auto a=liquid.Accounting("water");bool ok=liquid.Errors().empty()&&world.Scripts()->Diagnostics().empty()&&std::abs(a.total-total)<=a.tolerance&&std::abs(owner->dynamicSurface->Volume()-owner->volume)<1e-8;
   for(auto& c:owner->dynamicSurface->cells)ok&=std::isfinite(c.volume)&&c.volume>=0;
   return ok;
  };
  for(unsigned i=0;i<30;++i)measuredStep();
  report("ordinary",0,0);check(valid(),"ordinary filled operation is conservative and solves");
  bool cycles=true;
  for(unsigned cycle=0;cycle<5&&cycles;++cycle){
   phase=1;
   unsigned presses=0;
   while(owner->volume>1e-8&&presses<32){window.Input().SetPhysical("key:Z",1);measuredStep();window.Input().SetPhysical("key:Z",0);measuredStep();report("drain",cycle,++presses);if(!valid()&&!(baseline&&owner->volume<1e-8)){cycles=false;break;}}
   check(cycles&&owner->volume<initial*.01,"large pool substantially drains through authored Z action");
   if(!cycles)break;
   presses=0;phase=2;
   while(liquid.Get(storage)->volume>1e-8&&presses<64){window.Input().SetPhysical("key:X",1);measuredStep();report("refill",cycle,++presses);if(!valid()){cycles=false;break;}window.Input().SetPhysical("key:X",0);measuredStep();if(!valid()){report("refill-release",cycle,presses);cycles=false;break;}}
   check(cycles&&std::abs(owner->volume-initial)<.1,"large pool refills through authored X action within normal solve budget");
   phase=3;for(unsigned i=0;i<120&&cycles;++i){measuredStep();cycles&=valid();if(!cycles)report("settle-failure",cycle,i);}
   check(cycles,"refilled state remains nonnegative with coherent owner/partition/ledger");
  }
  check(cycles,"five consecutive full drain/refill cycles remain valid");
  for(unsigned p=0;p<4;++p){auto& m=measurements[p];std::printf("MEASURE %s %s steps %u iterations mean %.3f max %u retries %u residual max %.12g solve mean %.6f max %.6f ms\n",name,std::array<const char*,4>{"ordinary","drain","refill","settle"}[p],m.steps,m.steps?double(m.iterations)/m.steps:0,m.maximumIterations,m.retries,m.maximumResidual,m.steps?m.seconds*1000/m.steps:0,m.maximumSeconds*1000);}
  game.End();world.Destroy();
 }
 host.Shutdown();std::printf("M55 refill %u checks %u failures\n",checks,failed);return failed?1:0;
}
