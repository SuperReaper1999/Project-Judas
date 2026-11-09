#include "/home/conner/Documents/GitHub/Project-Judas/tests/support/VehicleFixture.h"
#include "PerformanceProfiler.h"
#include "BuildIdentity.h"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <map>
#include <vector>
#include <string>
using Clock=std::chrono::steady_clock;
void Summary(const std::string& name,std::vector<double> v){std::sort(v.begin(),v.end());auto q=[&](double p){return v[size_t(p*(v.size()-1))];};std::cout<<name<<" n="<<v.size()<<" p50_ms="<<q(.5)<<" p95_ms="<<q(.95)<<" p99_ms="<<q(.99)<<" worst_ms="<<v.back()<<'\n';}
void Run(unsigned count,bool drive,bool rough,bool impact=false){
 auto scene=VehicleFixture::Create();const auto first=scene.Objects();
 for(unsigned n=1;n<count;++n)for(auto object:first)if(object.id>=3){object.id+=n*10;if(object.parent)object.parent+=n*10;if(object.wheel)object.wheel->chassis+=n*10;else object.transform.position+=glm::vec3(float(n%5)*4,0,float(n/5)*6);scene.Objects().push_back(object);}
 scene.SetNextId(count*10+8);
 if(rough)for(int z=-200;z<25;z+=2){auto& strip=scene.CreateObject("Rough strip");strip.body=SceneBodyComponent{};strip.body->halfExtents={30,.04f,.3f};strip.transform.position={8,.04f,float(z)};}
 for(unsigned n=0;n<count;++n){
  PowertrainSettings graph;
  for(unsigned id=4;id<=7;++id){PowertrainShaft shaft;shaft.name="wheel_"+std::to_string(id);shaft.kind=PowertrainShaftKind::Wheel;shaft.target=id+n*10;graph.shafts.push_back(shaft);
   PowertrainSource source;source.name="motor_"+std::to_string(id);source.kind=PowertrainSourceKind::Electric;source.shaft=id-4;source.maxTorque=35;source.maxPower=10000;graph.sources.push_back(source);}
  scene.Find(3+n*10)->powertrain=graph;
 }
 RuntimeWorld world;std::string error;if(!world.Build(scene,nullptr,error))throw std::runtime_error(error);
 VehicleFixture::Tick(world,300);
 if(impact){
  world.Physics().ResetBody(world.RuntimeBody(3),{0,.8f,-3},{1,0,0,0});
  world.Physics().ResetBody(world.RuntimeBody(13),{0,.8f,3},{1,0,0,0});
  world.Physics().SetLinearVelocity(world.RuntimeBody(3),{0,0,8});
  world.Physics().SetLinearVelocity(world.RuntimeBody(13),{0,0,-8});
  for(unsigned id=4;id<=7;++id){world.RuntimeWheel(id)->state.angularVelocity=-8/.35f;world.RuntimeWheel(id+10)->state.angularVelocity=8/.35f;}
 }
 const auto initialPosition=world.Physics().GetTransform(world.RuntimeBody(3)).position;
 float minimumY=1000,peakAngular=0;
 if(drive)for(unsigned n=0;n<count;++n)for(auto& source:world.RuntimePowertrain(3+n*10)->command.sources)source.input=1;
 auto& profiler=PerformanceProfiler::Get();profiler.Enable(true);unsigned incomplete=0;std::map<std::string,std::vector<double>> samples;
 for(unsigned frame=0;frame<1200;++frame){auto start=Clock::now();{ProfileFrame capture("vehicle-benchmark");VehicleFixture::Tick(world);}auto end=Clock::now();
  for(unsigned n=0;n<count;++n){auto body=world.RuntimeBody(3+n*10);minimumY=std::min(minimumY,world.Physics().GetTransform(body).position.y);peakAngular=std::max(peakAngular,glm::length(world.Physics().GetAngularVelocity(body)));}
  if(frame<120&&!impact)continue;
  samples["whole_step"].push_back(std::chrono::duration<double,std::milli>(end-start).count());
  const auto snap=profiler.Snapshot(profiler.FrameId());incomplete+=snap.incomplete;std::map<std::string,double> times;
  for(const auto& scope:snap.scopes){const std::string name(scope.name.c_str());if(name=="Vehicle contact preparation"||name=="Vehicle suspension and tyres"||name=="Rigid physics"||name=="Mechanical powertrains")times[name]+=double(scope.end-scope.start)*1e-6;}
  for(const auto& [name,value]:times)samples[name].push_back(value);
 }
 profiler.Enable(false);std::cout<<"CAPTURE incomplete_frames="<<incomplete<<'\n';std::cout<<"CASE powered=1 vehicles="<<count<<" driven="<<drive<<" rough="<<rough<<" impact="<<impact<<'\n';
 for(auto& [name,v]:samples)Summary(name,v);
 unsigned sleepers=0,contacts=0;for(unsigned n=0;n<count;++n){sleepers+=world.Physics().IsSleeping(world.RuntimeBody(3+n*10));for(unsigned j=4;j<=7;++j)contacts+=world.RuntimeWheel(j+n*10)->state.contact;}
 std::cout<<"OUTCOME sleepers="<<sleepers<<" contacts="<<contacts<<" min_y_m="<<minimumY<<" peak_angular_rad_s="<<peakAngular<<" primary_distance_m="<<glm::distance(initialPosition,world.Physics().GetTransform(world.RuntimeBody(3)).position)<<" wheels="<<4*count<<'\n';
}
int main(){try{std::cout<<"BUILD "<<BuildIdentityJson()<<'\n';std::cout<<std::setprecision(8);Run(1,false,false);Run(1,true,false);Run(25,false,false);Run(6,true,true);Run(2,false,false,true);}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
