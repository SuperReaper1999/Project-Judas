// M55 normal project/physics/renderer path. Direct fixture placements probe
// openings/geometry; the operator's project manipulates bodies using forces.
#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "GameSession.h"
#include "Simulation.h"
#include "Project.h"
#include "SceneSerialization.h"
#include "WorldPresentation.h"
#include "ScreenshotWriter.h"
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <cstdio>
#include <algorithm>
int main(int argc,char** argv){
 unsigned checks=0,failed=0;std::string error;
 auto check=[&](bool ok,const char* message){++checks;failed+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",message);};
 auto near=[](double a,double b,double tolerance=1e-8){return std::abs(a-b)<=tolerance;};
 Project project;check(project.Load("projects/liquid_surface_demo/liquid_surface_demo.judasproj",error),"ordinary M55 project and logical input map");
 EngineHost host;check(host.Init("M55 liquid application",640,360,false,error),"normal EngineHost/GL resources");
 if(failed){std::puts(error.c_str());return 1;}
 host.OpenProjectAssets(project.RootDir(),project.AssetsDir());auto& window=host.GetWindow();window.SetTestInputMode(true);window.Input().SetMap(project.Settings().input,error);
 for(const char* name:{"lab","radial"}){
  if(argc>1&&std::string(argv[1])!=name)continue;
  bool radial=std::string(name)=="radial";Scene scene;RuntimeWorld world;world.ConfigureProject(project.Settings());GameSession game;
  check(LoadSceneFromFile(project.RootDir()+"/Scenes/"+name+".judas",scene,error),"registered surface scene loads normally");world.legacyGameplay=false;
  check(world.Build(scene,&host.Resources(),error,&project.Settings().classification)&&game.Begin(world,error),"ordinary resource/world/session build");
  if(!world.IsBuilt()){std::puts(error.c_str());return 1;}
  host.Resources().WaitForAll();
  auto step=[&](unsigned count){for(unsigned i=0;i<count;++i){window.Input().BeginFixedStep();world.UpdateUIScripts(&window.Input(),1.f/60);world.UpdateScripts(&window.Input(),1.f/60);StepPlayedWorld(game,window,1.f/60);world.PresentationScripts(&window.Input(),1.f/60,.5f);window.Input().BeginFrame();}};
  step(2);auto& liquid=world.Liquids();auto pool=liquid.Handle(radial?120:4000),bucket=liquid.Handle(200);
  check(liquid.Errors().empty()&&world.Scripts()->Diagnostics().empty(),"baked resources and ordinary JS callbacks resolve without faults");
  if(!liquid.Get(pool)||!liquid.Get(pool)->dynamicSurface){for(auto& [id,e]:liquid.Errors())std::printf("owner %llu %s\n",(unsigned long long)id,e.c_str());return 1;}
  auto* reservoir=liquid.Get(pool);double initialTotal=liquid.Accounting("water").total;
  check(near(reservoir->dynamicSurface->Volume(),reservoir->volume)&&reservoir->dynamicSurface->cells.size()>=192,"swimming-sized conserved spatial partition, no bulk particles");
  check(bucket.id&&near(liquid.Get(bucket)->volume,0),"ordinary prefab bucket starts dry");
  auto main=liquid.Handle(radial?120:20),storage=liquid.Handle(radial?121:21);double before=liquid.Get(main)->volume;
  window.Input().SetPhysical("key:Q",1);step(1);window.Input().SetPhysical("key:Q",0);
  check(near(liquid.Get(main)->volume,before-.8)&&near(liquid.Get(storage)->volume,.8),"JS drain changes owner and local allocation together");
  window.Input().SetPhysical("key:E",1);step(1);window.Input().SetPhysical("key:E",0);
  check(near(liquid.Get(main)->volume,before),"JS refill restores quantity while preserving surface state");
  glm::vec3 point=radial?glm::vec3(.3f,20.6f,0):glm::vec3(.3f,1.2f,-12);
  auto sample=liquid.Sample(point);check(sample&&glm::length(sample->up)>.99f&&std::isfinite(sample->depth),"normal query reports actual local occupied depth and separate gravity up");
  check(liquid.SurfaceImpulse(pool,point,{100,0,0}),"generic momentum impulse disturbs existing water without creating volume");
  step(60);double low=1e30,high=-1e30;for(auto& cell:reservoir->dynamicSurface->cells)if(cell.volume>0){low=std::min(low,cell.q);high=std::max(high,cell.q);}
  check(high-low>1e-6&&near(liquid.Accounting("water").total,initialTotal,1e-7),"waves persist and owner/parcel ledger stays conserved");
  // Geometry probe: let the real opening exchange, then carry and tilt. No
  // fixture fills the container or writes any cell volume.
  auto pose=world.RuntimeDefinition(200)->transform;pose.position=radial?glm::vec3(0,21,0):glm::vec3(0,1.2f,-12);pose.rotation=glm::quat(1,0,0,0);
  world.SetRuntimeTransform(200,pose);
  for(unsigned i=0;i<40;++i){world.SetRuntimeTransform(200,pose);world.Physics().SetLinearVelocity(world.RuntimeBody(200),{});world.Physics().SetAngularVelocity(world.RuntimeBody(200),{});step(1);if(i%10==0)std::printf("M55 %s scoop step %u volume %.9g\n",name,i,liquid.Get(bucket)->volume);}
  double carried=liquid.Get(bucket)->volume;std::printf("M55 %s acquired %.9f L ledger %.12g\n",name,carried*1000,liquid.Accounting("water").error);
  check(carried>.002&&near(liquid.Accounting("water").total,initialTotal,1e-7),"physical opening acquires real moving-surface water with paired ownership");
  pose.position=radial?glm::vec3(0,24,0):glm::vec3(0,4,0);world.SetRuntimeTransform(200,pose);
  for(unsigned i=0;i<15;++i){world.SetRuntimeTransform(200,pose);world.Physics().SetLinearVelocity(world.RuntimeBody(200),{});world.Physics().SetAngularVelocity(world.RuntimeBody(200),{});step(1);}
  check(near(liquid.Get(bucket)->volume,carried),"removed cup retains real conserved water");
  pose.position=radial?glm::vec3(0,23,0):glm::vec3(1.5f,3.15f,-2.7f);pose.rotation=glm::angleAxis(-glm::half_pi<float>(),glm::vec3(1,0,0));world.SetRuntimeTransform(200,pose);
  for(unsigned i=0;i<180;++i){world.SetRuntimeTransform(200,pose);world.Physics().SetLinearVelocity(world.RuntimeBody(200),{});world.Physics().SetAngularVelocity(world.RuntimeBody(200),{});step(1);}
  check(liquid.Get(bucket)->volume<carried*.7&&near(liquid.Accounting("water").total,initialTotal,1e-7),"pouring moves retained water through conserved parcels");
  auto actor=world.RuntimeDefinition(10)->transform;actor.position=radial?glm::vec3(0,21.2f,-1):glm::vec3(0,1.3f,-13);world.SetRuntimeTransform(10,actor);step(30);
  check(world.RuntimeCharacter(10)&&std::isfinite(glm::length(world.RuntimeCharacter(10)->velocity))&&world.Scripts()->Diagnostics().empty(),"JS liquid response drives normal CharacterMotor under selected gravity");
  std::vector<double> fixed,frames;unsigned draws=0;
  for(unsigned i=0;i<60;++i){auto begin=std::chrono::steady_clock::now();step(1);fixed.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());}
  for(unsigned i=0;i<12;++i){auto begin=std::chrono::steady_clock::now();auto camera=world.view->pose;
   if(i==0){auto eye=radial?glm::vec3(6,26,6):glm::vec3(5,7,2),target=radial?glm::vec3(0,21,0):glm::vec3(0,1,-10);camera.position=eye;camera.rotation=glm::quat_cast(glm::inverse(glm::lookAt(eye,target,glm::vec3(0,1,0))));}
   world.view->pose=camera;
   auto view=glm::mat4_cast(glm::inverse(camera.rotation))*glm::translate(glm::mat4(1),-camera.position),projection=glm::perspective(glm::radians(70.f),640.f/360,.1f,500.f);
   host.GetRenderer().ResetStats();RenderWorldFrame(host.GetRenderer(),640,360,world,&game,view,projection,camera.position,.5f);
   host.GetRenderer().BeginUIFrame(640,360);world.UI().Draw(host.GetRenderer(),640,360);host.GetRenderer().EndUIFrame();
   if(i==0||i==11){std::filesystem::create_directories("build/m55-visual");std::vector<unsigned char> rgb;host.GetRenderer().CaptureFrame(640,360,rgb);WriteRgbPng(std::string("build/m55-visual/")+name+(i==0?"-overview":"")+".png",640,360,rgb);}
   window.SwapBuffers();draws=host.GetRenderer().Stats().drawCalls;if(i>1)frames.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());
  }
  auto print=[&](const char* phase,std::vector<double>& times){std::sort(times.begin(),times.end());std::printf("M55 %s %s median %.4f p95 %.4f max %.4f ms\n",name,phase,times[times.size()/2],times[size_t((times.size()-1)*.95)],times.back());};print("fixed",fixed);print("offscreen render 640x360",frames);
  std::printf("M55 %s cells %zu faces %zu parcels %zu bodies %zu draws %u solve %.5f ms iterations %u residual %.12g partition %.12g\n",name,reservoir->dynamicSurface->cells.size(),reservoir->dynamicSurface->discharge.size(),liquid.Parcels().size(),world.CountLifecycle().physicsBodies,draws,reservoir->dynamicSurface->stats.seconds*1000,reservoir->dynamicSurface->stats.iterations,reservoir->dynamicSurface->stats.residual,reservoir->dynamicSurface->Volume()-reservoir->volume);
  auto phases=liquid.StepTimes();size_t storageTets=0,baseTets=0;
  for(auto& cell:reservoir->dynamicSurface->cells)storageTets+=cell.storage.geometry.cells.size();
  for(auto& cell:reservoir->dynamicSurface->data->cells)baseTets+=cell.storage.geometry.cells.size();
  std::printf("M55 %s post-interaction last liquid %.5f containers %.5f storage %.5f solve %.5f loading %.5f ms; storage tets %zu base %zu\n",name,phases.total*1000,phases.containers*1000,phases.geometry*1000,phases.surface*1000,phases.loading*1000,storageTets,baseTets);
  for(auto& o:world.ScriptObjects())if(o.liquidInteraction&&world.Physics().IsDynamicBody(world.RuntimeBody(o.id))){auto h=world.RuntimeBody(o.id);auto p=world.Physics().GetTransform(h).position;std::printf("M55 body %llu position %.3f %.3f %.3f speed %.3f angular %.3f\n",(unsigned long long)o.id,p.x,p.y,p.z,glm::length(world.Physics().GetLinearVelocity(h)),glm::length(world.Physics().GetAngularVelocity(h)));}
  auto beginQuery=std::chrono::steady_clock::now();for(unsigned i=0;i<100;++i)liquid.Sample(point);std::printf("M55 %s 100 sample queries %.6f ms\n",name,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-beginQuery).count());
  auto beginPaths=std::chrono::steady_clock::now();liquid.OpticalPaths(host.GetRenderer().ViewMatrix(),host.GetRenderer().ProjectionMatrix(),32,18,.5f);std::printf("M55 %s optical paths %.6f ms\n",name,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-beginPaths).count());
  auto beginMesh=std::chrono::steady_clock::now();auto measuredMesh=reservoir->dynamicSurface->Mesh(.5f);std::printf("M55 %s mesh %zu vertices %.6f ms\n",name,measuredMesh.vertices.size(),std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-beginMesh).count());
  auto accounting=liquid.Accounting("water");check(std::abs(accounting.error)<=accounting.tolerance,"complete ledger closes within M54 floating-point tolerance");
  check(liquid.Errors().empty()&&world.Scripts()->Diagnostics().empty(),"continued interaction has no retained solve/script failures");
  auto stale=pool;game.ResetToAuthoredState();world.UpdateLiquids(1./60);
  check(!world.Liquids().Get(stale)&&world.Liquids().Get(world.Liquids().Handle(radial?120:4000))->dynamicSurface,"reload reconstructs surface state and rejects old handles");
  game.End();world.Destroy();check(world.Liquids().States().empty()&&world.Liquids().Parcels().empty(),"Stop clears surfaces, containers and parcels");
 }
 host.Shutdown();std::printf("M55 application %u checks %u failures\n",checks,failed);return failed?1:0;
}
