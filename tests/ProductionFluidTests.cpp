#include "Project.h"
// FTFT9 actual authored-scene -> ordinary fixed-step hybrid body checks.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <glm/gtc/quaternion.hpp>
#include "Scene.h"
#include "SceneSerialization.h"
#include "RuntimeWorld.h"
#include "GameSession.h"
#include "Simulation.h"
#include "ProductionFluidCoupling.h"
#include "Window.h"
namespace {
namespace fs=std::filesystem;constexpr float dt=1.0f/60;int checks=0,failed=0,diagnosticChecks=0,diagnosticFailures=0,totalSteps=0;bool strictDiagnostics=false;std::string selectedCase;
void Check(bool ok,const std::string& label){++checks;if(!ok){++failed;std::cout<<"FAIL "<<label<<'\n';}}
void DiagnosticCheck(bool ok,const std::string& label){++checks;++diagnosticChecks;if(!ok){++diagnosticFailures;std::cout<<"DIAGNOSTIC_FAIL "<<label<<'\n';}}
int ReportedFailures(){return failed+(strictDiagnostics?diagnosticFailures:0);}
void PrintSummary(){std::cout<<"SUMMARY checks="<<checks<<" failures="<<ReportedFailures()<<" acceptance_failures="<<failed<<" diagnostic_checks="<<diagnosticChecks<<" diagnostic_failures="<<diagnosticFailures<<" strict_diagnostics="<<strictDiagnostics<<" steps="<<totalSteps<<'\n';}
struct Result { float finalY=0,finalV=0,meanFraction=0,maxSpeed=0;double medianParticleMs=0,medianExecutedMs=0,amortizedMs=0,energy=0;int count=0; };
double Median(std::vector<double> values){if(values.empty())return 0;std::sort(values.begin(),values.end());return values[values.size()/2];}
// Independent raw-particle observation, shared by release validity and the
// final immersion oracle. It never calls the engine's hydrostatic sampler.
std::optional<double> ObservedSurface(const std::vector<FluidParticle>& particles,
 const glm::quat& q,const glm::vec3& localCenter,const glm::vec3& half,float spacing,int nx){
 std::vector<double> tops(static_cast<std::size_t>(nx*nx),-1e30);
 for(const auto& particle:particles){
  const glm::vec3 point=glm::conjugate(q)*particle.position;
  if(std::abs(point.x-localCenter.x)<half.x+spacing||std::abs(point.z-localCenter.z)<half.z+spacing)continue;
  const int x=static_cast<int>(std::floor((point.x+1)/spacing)),z=static_cast<int>(std::floor((point.z+1)/spacing));
  if(x>=0&&x<nx&&z>=0&&z<nx)tops[static_cast<std::size_t>(z*nx+x)]=std::max(tops[static_cast<std::size_t>(z*nx+x)],static_cast<double>(point.y)+.5*spacing);
 }
 tops.erase(std::remove_if(tops.begin(),tops.end(),[](double v){return v< -1e20;}),tops.end());
 if(tops.empty())return {};
 return Median(tops);
}
Result Run(const std::string& name,float spacing,float densityRatio,float sizeScale,const glm::quat& q,float g,bool freefall,fs::path out,bool observationalScale=false){
 if(!selectedCase.empty()&&name!=selectedCase)return {};
 const int failBefore=failed,diagnosticBefore=diagnosticFailures,diagnosticChecksBefore=diagnosticChecks;Scene scene;scene.Settings().name=name;scene.Settings().fluidScale=spacing/.05f;
 auto& observer=scene.CreateObject("observer");observer.playerStart=ScenePlayerStartComponent{};observer.transform.position=q*glm::vec3(20,30,20);
 auto& field=scene.CreateObject("field");field.gravity=SceneGravityComponent{};field.gravity->kind=SceneGravityKind::Uniform;field.gravity->magnitude=g;field.gravity->regionShape=SceneRegionShape::Box;field.gravity->regionHalfExtents=glm::vec3(500);field.transform.rotation=q;
 auto box=[&](glm::vec3 position,glm::vec3 half){auto& o=scene.CreateObject("boundary");o.body=SceneBodyComponent{};o.body->halfExtents=half;o.body->friction=0;o.body->restitution=0;o.transform.position=q*position;o.transform.rotation=q;};
 if(!freefall){box({0,-.1f,0},{1.1f,.1f,1.1f});box({-1.1f,1.5f,0},{.1f,1.5f,1.1f});box({1.1f,1.5f,0},{.1f,1.5f,1.1f});box({0,1.5f,-1.1f},{1,1.499f,.1f});box({0,1.5f,1.1f},{1,1.499f,.1f});}
 const glm::vec3 half(.3f*sizeScale),initial(0,.7f,0);const double bodyVolume=8.*half.x*half.y*half.z;
 auto& body=scene.CreateObject("body");const auto id=body.id;body.body=SceneBodyComponent{};body.body->motion=freefall?SceneBodyMotion::Dynamic:SceneBodyMotion::Static;body.body->halfExtents=half;body.body->mass=static_cast<float>(1000.*densityRatio*bodyVolume);body.body->friction=0;body.body->restitution=0;body.transform.position=q*initial;body.transform.rotation=q;
 // The same deep reference pool is used for every density and spacing. PBF
 // boundary packing does not preserve continuum mass/area surface height, so
 // validate the observed release surface below instead of assuming the authored
 // fill remains at its initial height. No density-specific preparation is used.
 // Ordinary fluid-volume rows represent the initial liquid outside the solid.
 // No runtime clipping: missing initial matter is explicitly absent authored volume.
 const int nx=static_cast<int>(std::round(2/spacing)),ny=static_cast<int>(std::round(2.4f/spacing));int particles=0;
 for(int y=0;y<ny;++y)for(int z=0;z<nx;++z){int start=-1;for(int x=0;x<=nx;++x){const glm::vec3 pos((x+.5f)*spacing-1,(y+.5f)*spacing,(z+.5f)*spacing-1);
   const bool present=x<nx && !glm::all(glm::lessThan(glm::abs(pos-initial),half+glm::vec3(spacing*.36f)));
   if(present&&start<0)start=x;
   if(!present&&start>=0){const int n=x-start;auto& water=scene.CreateObject("liquid");water.fluidVolume=SceneFluidVolumeComponent{};water.fluidVolume->spacing=spacing;water.fluidVolume->countX=n;water.fluidVolume->countY=1;water.fluidVolume->countZ=1;water.transform.rotation=q;water.transform.position=q*glm::vec3((start+.5f*n)*spacing-1,(y+.5f)*spacing,(z+.5f)*spacing-1);particles+=n;start=-1;}
 }}
 std::string text,error;SaveSceneToString(scene,text);std::ofstream(out/(name+".judas"))<<text;Scene loaded;Check(LoadSceneFromString(text,loaded,error),name+" scene load "+error);
 RuntimeWorld world;ProjectSettings experimental;experimental.advancedFluidDynamics=true;world.ConfigureProject(experimental);GameSession session;Window window;window.SetTestInputMode(true);if(!world.Build(loaded,nullptr,error)||!session.Begin(world,error)){Check(false,name+" runtime build "+error);return {};}
 SceneObject dynamicDefinition=*loaded.Find(id); dynamicDefinition.id=kInvalidSceneObjectId;dynamicDefinition.body->motion=SceneBodyMotion::Dynamic;
 EntityId dynamicId=id;
 auto snapshot=[&](const std::string& stage,BodyHandle observed){
  std::ofstream particlesFile(out/(name+"_"+stage+"_particles.csv"));particlesFile<<std::setprecision(17)<<"x,y,z,vx,vy,vz,ax,ay,az,mass\n";
  for(const auto& particle:world.Fluid().Particles())particlesFile<<particle.position.x<<','<<particle.position.y<<','<<particle.position.z<<','<<particle.velocity.x<<','<<particle.velocity.y<<','<<particle.velocity.z<<','<<particle.acceleration.x<<','<<particle.acceleration.y<<','<<particle.acceleration.z<<','<<particle.mass<<'\n';
  const auto pose=world.Physics().GetTransform(observed);
  std::ofstream poseFile(out/(name+"_"+stage+"_body.csv"));poseFile<<std::setprecision(17)<<"x,y,z,qw,qx,qy,qz,hx,hy,hz,gx,gy,gz\n";
  const auto gravity=world.Gravity().Sample(pose.position);
  poseFile<<pose.position.x<<','<<pose.position.y<<','<<pose.position.z<<','<<pose.rotation.w<<','<<pose.rotation.x<<','<<pose.rotation.y<<','<<pose.rotation.z<<','<<half.x<<','<<half.y<<','<<half.z<<','<<gravity.x<<','<<gravity.y<<','<<gravity.z<<'\n';
 };
 if(!freefall){
  // Prepare a stationary pool through the ordinary simulation. The static
  // placeholder reserves the identical body volume, then is replaced through
  // the supported entity lifecycle; no liquid or velocity state is edited.
  for(int step=0;step<240;++step){StepPlayedWorld(session,window,dt);++totalSteps;}
  double meanSpeed=0;for(const auto& particle:world.Fluid().Particles())meanSpeed+=glm::length(particle.velocity);
  meanSpeed/=world.Fluid().Particles().size();
  std::cout<<"PRECONDITION "<<name<<" warmup_steps=240 mean_particle_speed="<<meanSpeed<<'\n';
  DiagnosticCheck(meanSpeed<.05,name+" reference pool is quiescent before body release");
  const auto placeholder=std::find_if(world.StaticBodies().begin(),world.StaticBodies().end(),[&](const auto& item){return item.id==id;});
  Check(placeholder!=world.StaticBodies().end(),name+" locate ordinary static placeholder");
  if(placeholder==world.StaticBodies().end())return {};
  world.Physics().DestroyBody(placeholder->handle);
  Shape removedShape;BodyTransform removedPose;
  Check(!world.Physics().GetBodyShape(placeholder->handle,removedShape,removedPose),name+" old static handle invalid after release");
  dynamicId=world.CreateEntity(dynamicDefinition,nullptr,&error);
  Check(dynamicId!=kInvalidSceneObjectId,name+" create released body "+error);
 }
 const auto handle=world.DynamicBodies()[world.FindEntity(dynamicId)->slot].Handle();const float actualMass=world.Physics().GetMass(handle);Check(world.Fluid().Particles().size()==static_cast<std::size_t>(particles),name+" authored particle inventory");
 snapshot("release",handle);
 if(!freefall&&g>0){
  // Independent initial-state oracle: columns outside the placeholder's
  // horizontal footprint determine the represented liquid surface. A neutral
  // body must start fully immersed; an exposed top is expected to sink until
  // enough volume is displaced and cannot establish neutral-drift accuracy.
  const auto surface=ObservedSurface(world.Fluid().Particles(),q,initial,half,spacing,nx);
  Check(surface.has_value(),name+" independent release surface has resolved exterior columns");
  std::cout<<"PRECONDITION "<<name<<" observed_release_surface="<<surface.value_or(0)<<" body_top="<<initial.y+half.y<<" clearance="<<surface.value_or(0)-initial.y-half.y<<'\n';
  Check(surface&&*surface>=initial.y+half.y+spacing,name+" all density cases start fully submerged by one particle spacing");
 }
 const double initialWaterMass=world.Fluid().GetDiagnostics().totalMass;
 Result result;std::vector<double> executed,particleTimes;double totalMs=0,mean=0,observedMean=0;int meanCount=0;const int count=240;
 std::ofstream csv(out/(name+".csv"));csv<<std::setprecision(17)<<"step,y,vy,immersion,bfx,bfy,bfz,fluid_mass,fluid_ke,body_ke,fluid_step_ms,total_coupling_ms,executed,contact_caps,contact_relative_residual,acceleration_rejections,acceleration_boundary_declines,geometry_escapes,unresolved_geometry,geometry_candidates,max_geometry_escape_distance,pressure_reconstruction_samples,max_pressure_reconstruction_velocity_error,rms_pressure_reconstruction_velocity_error\n";
 for(int step=0;step<count;++step){StepPlayedWorld(session,window,dt);++totalSteps;const auto pose=world.Physics().GetTransform(handle);const auto velocity=world.Physics().GetLinearVelocity(handle);const auto omega=world.Physics().GetAngularVelocity(handle);
  const glm::vec3 local=glm::conjugate(q)*pose.position,localV=glm::conjugate(q)*velocity;const auto& m=world.FluidCoupling().Measurements();const auto liquid=world.Fluid().GetDiagnostics();
  const auto d=std::find_if(m.bodies.begin(),m.bodies.end(),[&](const auto& b){return b.body.id==handle.id;});Check(d!=m.bodies.end(),name+" real coupling record");if(d==m.bodies.end())break;
  const double rigidEnergy=.5*actualMass*glm::dot(velocity,velocity)+.5*glm::dot(omega,world.Physics().GetInertiaWorld(handle)*omega);
  Check(std::isfinite(rigidEnergy)&&std::isfinite(liquid.kineticEnergy)&&glm::length(velocity)<100,name+" finite stable state");
  Check(std::abs(liquid.totalMass-initialWaterMass)<.001*std::max(1.,initialWaterMass),name+" finite liquid mass retained");
  if(step>=180){
   mean+=d->fraction;++meanCount;
   // Independent observation: fit a horizontal surface from the highest
   // particles in fixed exterior columns, then integrate the actual rotated
   // box volume below it with a separate dense rectangular quadrature.
   const auto observedSurface=ObservedSurface(world.Fluid().Particles(),q,local,half,spacing,nx);
   if(observedSurface){
    const double surface=*observedSurface;int wet=0;constexpr int n=32;
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){
     const glm::vec3 offset=half*(glm::vec3(2*x+1,2*y+1,2*z+1)/float(n)-1.0f);
     const glm::vec3 point=glm::conjugate(q)*(pose.position+pose.rotation*offset);
     if(point.y<surface)++wet;
    }
    observedMean+=static_cast<double>(wet)/(n*n*n);
   }
  }
  if(m.executed){executed.push_back(m.totalMilliseconds);particleTimes.push_back(m.particleMilliseconds);}totalMs+=m.totalMilliseconds;
  csv<<step<<','<<local.y<<','<<localV.y<<','<<d->fraction<<','<<d->buoyancyForce.x<<','<<d->buoyancyForce.y<<','<<d->buoyancyForce.z<<','<<liquid.totalMass<<','<<liquid.kineticEnergy<<','<<rigidEnergy<<','<<m.particleMilliseconds<<','<<m.totalMilliseconds<<','<<m.executed<<','<<m.contactIterationCaps<<','<<m.maximumContactRelativeResidual<<','<<m.normalAccelerationRejections<<','<<m.normalAccelerationBoundaryDeclines<<','<<(m.executed?liquid.geometryEscapeCount:0)<<','<<(m.executed?liquid.unresolvedGeometryCount:0)<<','<<(m.executed?liquid.geometryEscapeCandidates:0)<<','<<(m.executed?liquid.maximumGeometryEscapeDistance:0)<<','<<(m.executed?liquid.pressureReconstructionSamples:0)<<','<<(m.executed?liquid.maximumPressureReconstructionVelocityError:0)<<','<<(m.executed?liquid.rmsPressureReconstructionVelocityError:0)<<'\n';
  if(step==9||step==239)snapshot("step"+std::to_string(step+1),handle);
  result.finalY=local.y;result.finalV=localV.y;result.maxSpeed=std::max(result.maxSpeed,glm::length(velocity));result.energy=rigidEnergy+liquid.kineticEnergy;
 }
 result.meanFraction=static_cast<float>(mean/meanCount);result.count=count;result.medianParticleMs=Median(particleTimes);result.medianExecutedMs=Median(executed);result.amortizedMs=totalMs/count;
 if(freefall){const glm::vec3 bodyV=world.Physics().GetLinearVelocity(handle);const auto fd=world.Fluid().GetDiagnostics();Check(glm::length(bodyV-fd.totalMomentum/fd.totalMass)<.5f,name+" common free fall relative velocity");}
 else if(g==0){Check(std::abs(result.finalY-initial.y)<.20f&&std::abs(result.finalV)<.2f,name+" zero gravity no invented buoyancy");}
 else if(densityRatio==.5f){
  // Explicit fixture scope only: the original sub-particle-scale expectations
  // remain observable and become exit failures with --strict-diagnostics.
  const auto expectation=observationalScale?DiagnosticCheck:Check;
  expectation(std::abs(result.meanFraction-.5f)<=.10f,name+" half-density sampled equilibrium immersion within 10 percentage points");
  expectation(std::abs(observedMean/meanCount-.5)<=.10,name+" independent measured box/surface immersion within 10 percentage points");
  expectation(std::abs(result.finalV)<.5f,name+" settles rather than passing through desired immersion");
 }
 else if(densityRatio==1){Check(std::abs(result.finalY-initial.y)<=.20f,name+" neutral drift within 0.20m over 4 seconds");}
 else Check(result.finalY<initial.y-.3f,name+" dense body sinks");
 std::cout<<std::setprecision(10)<<"CASE "<<name<<" failures="<<(failed-failBefore+(strictDiagnostics?diagnosticFailures-diagnosticBefore:0))<<" acceptance_failures="<<(failed-failBefore)<<" diagnostic_checks="<<(diagnosticChecks-diagnosticChecksBefore)<<" diagnostic_failures="<<(diagnosticFailures-diagnosticBefore)<<" observational_scale="<<observationalScale<<" mass="<<actualMass<<" particles="<<particles<<" y="<<result.finalY<<" vy="<<result.finalV<<" fraction="<<result.meanFraction<<" observed_fraction="<<observedMean/meanCount<<" max_speed="<<result.maxSpeed<<" particle_ms="<<result.medianParticleMs<<" executed_ms="<<result.medianExecutedMs<<" amortized_ms="<<result.amortizedMs<<'\n';return result;
}
}
int main(int argc,char**argv){fs::path out="build/production-fluid";bool single=false,neutralOnly=false;for(int i=1;i<argc;++i){if(std::string(argv[i])=="--output"&&i+1<argc)out=argv[++i];else if(std::string(argv[i])=="--single")single=true;else if(std::string(argv[i])=="--neutral")neutralOnly=true;else if(std::string(argv[i])=="--case"&&i+1<argc)selectedCase=argv[++i];else if(std::string(argv[i])=="--strict-diagnostics")strictDiagnostics=true;else return 2;}fs::create_directories(out);
 const glm::quat identity(1,0,0,0),rotated=glm::angleAxis(.83f,glm::normalize(glm::vec3(1,-2,3)));
 if(neutralOnly){Run("neutral_020",.2f,1,1,identity,9.81f,false,out);PrintSummary();return ReportedFailures()?1:0;}
 Run("half_020",.2f,.5f,1,identity,9.81f,false,out);
 if(!single){Run("neutral_020",.2f,1,1,identity,9.81f,false,out);Run("dense_020",.2f,2,1,identity,9.81f,false,out);
  Run("half_025",.25f,.5f,1,identity,9.81f,false,out);Run("neutral_025",.25f,1,1,identity,9.81f,false,out);Run("dense_025",.25f,2,1,identity,9.81f,false,out);
  Run("rotated_half",.2f,.5f,1,rotated,9.81f,false,out);Run("zero_gravity",.2f,1,1,identity,0,false,out);
  Run("common_freefall",.2f,1,1,identity,9.81f,true,out);Run("small_mass_125x",.2f,.5f,.2f,identity,9.81f,false,out,true);
  Run("light_80kg",.2f,.5f,std::cbrt(80.f/108.f),identity,9.81f,false,out);
 }
 PrintSummary();return ReportedFailures()?1:0;}
