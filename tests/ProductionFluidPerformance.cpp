#include "Project.h"
// Real authored demo scenes, ordinary fixed-step simulation, no renderer timing.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include "GameSession.h"
#include "ProductionFluidCoupling.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "Simulation.h"
#include "Window.h"
namespace {
using Clock=std::chrono::steady_clock;
double Ms(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
double Median(std::vector<double> v){std::sort(v.begin(),v.end());return v.size()%2?v[v.size()/2]:.5*(v[v.size()/2-1]+v[v.size()/2]);}
}
int main(int argc,char**argv){
 if(argc!=3||std::string(argv[1])!="--output")return 2;
 const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);int failed=0;
 for(const std::string name:{"classic","terrain"}){
  Scene scene;std::string error;const auto started=Clock::now();
  if(!LoadSceneFromFile("assets/scenes/"+name+".judas",scene,error)){std::cerr<<error<<'\n';return 2;}
  RuntimeWorld world;ProjectSettings experimental;experimental.advancedFluidDynamics=true;world.ConfigureProject(experimental);GameSession session;Window window;window.SetTestInputMode(true);
  if(!world.Build(scene,nullptr,error)||!session.Begin(world,error)){std::cerr<<error<<'\n';return 2;}
  const double construction=Ms(started);std::vector<double> particle,executed,total;double amortized=0;int frames=360;
  std::size_t batches=0,iterations=0,products=0,accelerations=0,caps=0,breakdowns=0;float residual=0;
  std::ofstream csv(out/(name+".csv"));csv<<std::setprecision(17)<<"step,fluid_executed,fluid_dt,particles,particle_ms,hybrid_ms,whole_fixed_step_ms,contact_batches,contact_iterations,normal_products,normal_accelerations,contact_caps,normal_breakdowns,maximum_contact_relative_residual\n";
  for(int step=0;step<frames;++step){const auto begin=Clock::now();StepPlayedWorld(session,window,1.f/60);const double ms=Ms(begin);
   const auto& m=world.FluidCoupling().Measurements();total.push_back(ms);amortized+=m.totalMilliseconds;
   batches+=m.contactBatches;iterations+=m.contactIterations;products+=m.normalMatrixProducts;accelerations+=m.normalAccelerations;caps+=m.contactIterationCaps;breakdowns+=m.normalBreakdowns;residual=std::max(residual,m.maximumContactRelativeResidual);
   if(m.executed){particle.push_back(m.particleMilliseconds);executed.push_back(m.totalMilliseconds);}
   csv<<step<<','<<m.executed<<','<<(m.executed?m.lastFluidDeltaTime:0)<<','<<world.Fluid().Particles().size()<<','<<m.particleMilliseconds<<','<<m.totalMilliseconds<<','<<ms<<','<<m.contactBatches<<','<<m.contactIterations<<','<<m.normalMatrixProducts<<','<<m.normalAccelerations<<','<<m.contactIterationCaps<<','<<m.normalBreakdowns<<','<<m.maximumContactRelativeResidual<<'\n';
  }
  const double median=Median(particle),hybrid=Median(executed),mean=amortized/frames;
  const bool pass=median<=15&&mean<=8&&*std::max_element(particle.begin(),particle.end())<200;
  failed+=!pass;
  std::cout<<std::setprecision(12)<<"CASE "<<name<<" pass="<<pass<<" construction_ms="<<construction<<" steps="<<frames<<" executed="<<particle.size()<<" particles="<<world.Fluid().Particles().size()<<" particle_median_ms="<<median<<" particle_max_ms="<<*std::max_element(particle.begin(),particle.end())<<" hybrid_executed_median_ms="<<hybrid<<" hybrid_amortized_ms="<<mean<<" whole_step_median_ms="<<Median(total)<<" first_step_ms="<<total.front()<<" contact_batches="<<batches<<" contact_iterations="<<iterations<<" normal_products="<<products<<" normal_accelerations="<<accelerations<<" contact_caps="<<caps<<" normal_breakdowns="<<breakdowns<<" maximum_contact_relative_residual="<<residual<<'\n';
 }
 return failed?1:0;
}
