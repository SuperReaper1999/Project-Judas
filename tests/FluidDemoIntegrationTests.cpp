// Exercise the shipped scenes through ordinary fixed steps. Test manipulation
// supplies hand targets through the existing force/torque API, never moves water.
#include "SceneSerialization.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "GameSession.h"
#include "Window.h"
#include "Simulation.h"
#include "ProductionFluidCoupling.h"
#include "FluidHydrostatics.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <cmath>
using namespace std;
int checks=0,failures=0;
void Check(bool v,const string& s){++checks;if(!v){++failures;cerr<<"FAIL "<<s<<'\n';}}
bool Finite(glm::vec3 v){return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z);}
void Run(const string& kind,const filesystem::path& out){
 Scene scene;string error;Check(LoadSceneFromFile("projects/fluid_demo/Scenes/"+kind+".judas",scene,error),kind+" load "+error);
 glm::quat q(1,0,0,0);EntityId cup=0,floating=0,sinking=0;
 for(auto& o:scene.Objects()){
  if(o.fluidVolume)q=o.transform.rotation;
  if(o.body&&o.body->motion==SceneBodyMotion::Dynamic){if(!o.body->fluidCavities.empty())cup=o.id;else if(o.body->mass<500)floating=o.id;else sinking=o.id;}
  // Render-only script/UI dependencies require a host; core scene physics
  // below is unchanged. Application/export checks retain them normally.
  o.scripts.clear();o.ui.reset();
 }
 glm::vec3 origin=kind=="planet"?q*glm::vec3(0,30,0):glm::vec3(0);
 auto Point=[&](glm::vec3 p){return origin+q*p;};auto Local=[&](glm::vec3 p){return glm::inverse(q)*(p-origin);};
 RuntimeWorld world;ProjectSettings experimental;experimental.advancedFluidDynamics=true;world.ConfigureProject(experimental);GameSession session;Window window;window.SetTestInputMode(true);
 Check(world.Build(scene,nullptr,error)&&session.Begin(world,error),kind+" construct "+error);if(!session.IsActive())return;
 auto Handle=[&](EntityId id){return world.DynamicBodies().at(world.FindEntity(id)->slot).Handle();};
 const auto quadrature=MakeCompoundFluidVolume(world.FindEntity(cup)->definition.body->compoundBoxes);cout<<kind<<" cup_solid_partitions="<<quadrature.solidBoxes.size()<<" cup_volume_samples="<<quadrature.samples.size()<<'\n';
 const auto cupHandle=Handle(cup);const auto cavity=world.FindEntity(cup)->definition.body->fluidCavities[0];
 auto Inside=[&](){set<size_t> ids;auto pose=world.Physics().GetTransform(cupHandle);size_t i=0;for(auto& p:world.Fluid().Particles()){auto l=glm::inverse(pose.rotation)*(p.position-pose.position)-cavity.localCenter;if(abs(l.x)<cavity.halfExtents.x&&abs(l.y)<cavity.halfExtents.y&&abs(l.z)<cavity.halfExtents.z)ids.insert(i);++i;}return ids;};
 const size_t count=world.Fluid().Particles().size();double mass=0;for(auto& p:world.Fluid().Particles())mass+=p.mass;
 ofstream log(out/(kind+".csv"));log<<"step,phase,step_ms,fluid_ms,hydrostatic_ms,player_ms,cup_x,cup_y,cup_z,contained_particles,player_x,player_y,player_z,immersion,unresolved_geometry\n";
 int step=0;bool finite=true,geometryValid=true;float maximumParticleSpeed=0;ObjectManipulation hands({cupHandle});
 auto Tick=[&](const string& phase,int n,glm::vec3 target,glm::quat attitude,bool held){
  for(int j=0;j<n;++j){if(held){hands.ApplyCarryForce(world.Physics(),Point(target),glm::vec3(0));hands.ApplyCarryOrientationTorque(world.Physics(),attitude);}
   session.HandleFrameInput(window,false,false,false,false,false);auto begin=chrono::steady_clock::now();StepPlayedWorld(session,window,1.f/60);
   double ms=chrono::duration<double,milli>(chrono::steady_clock::now()-begin).count();auto pose=world.Physics().GetTransform(cupHandle);auto c=Local(pose.position),p=Local(session.Player().GetPosition());
   for(auto& particle:world.Fluid().Particles()){finite&=Finite(particle.position)&&Finite(particle.velocity);if(glm::length(particle.velocity)>maximumParticleSpeed){maximumParticleSpeed=glm::length(particle.velocity);if(maximumParticleSpeed>40)cout<<kind<<" transient_peak phase="<<phase<<" step="<<step<<" speed="<<maximumParticleSpeed<<" distance="<<glm::length(particle.position-origin)<<" physical_escape="<<world.Fluid().GetDiagnostics().maximumGeometryEscapeDistance<<'\n';}}
   geometryValid&=world.Fluid().GetDiagnostics().unresolvedGeometryCount==0;
   log<<step++<<','<<phase<<','<<ms<<','<<world.FluidCoupling().Measurements().particleMilliseconds<<','<<world.FluidCoupling().Measurements().hydrostaticMilliseconds<<','<<world.FluidCoupling().Measurements().playerMilliseconds<<','<<c.x<<','<<c.y<<','<<c.z<<','<<Inside().size()<<','<<p.x<<','<<p.y<<','<<p.z<<','<<session.Player().GetFluidSample().immersion<<','<<world.Fluid().GetDiagnostics().unresolvedGeometryCount<<'\n';
  }
 };
 Tick("rest",120,{},q,false);Check(Inside().empty(),kind+" dry cup does not fill");
 int outside=0;for(auto& p:world.Fluid().Particles()){auto l=Local(p.position);outside+=abs(l.x)>3.6||abs(l.z)>3.6;}
 Check(outside==0,kind+" resting water contained");
 auto g=world.Gravity().Sample(Point({0,2,0}));Check(glm::dot(glm::normalize(g),q*glm::vec3(0,-1,0))>.99f,kind+" actual local gravity");
 if(kind=="planet")Check(glm::length(glm::normalize(world.Gravity().Sample(Point({3,1,0})))-glm::normalize(g))>.05f,"planet gravity varies spatially");
 // Walk from the authored deck into the actual field; then turn back up stairs.
 window.SetTestActionState(Action::MoveForward,true);Tick("enter",75,{},q,false);window.SetTestActionState(Action::MoveForward,false);
 float immersion=session.Player().GetFluidSample().immersion;cout<<kind<<" entry immersion="<<immersion<<" position="<<Local(session.Player().GetPosition()).z<<'\n';// Entry is observed during the following normal drift into deeper water.
 window.RequestTestJump();Tick("swim",30,{},q,false);Check(session.Player().GetFluidSample().immersion>.5f,kind+" player enters and swims in water");
 window.SetTestActionState(Action::MoveBackward,true);Tick("exit",150,{},q,false);window.SetTestActionState(Action::MoveBackward,false);
 cout<<kind<<" exit immersion="<<session.Player().GetFluidSample().immersion<<" z="<<Local(session.Player().GetPosition()).z<<'\n';Check(session.Player().GetFluidSample().immersion==0,kind+" player leaves water");
 // Launch both existing props toward the basin using the ordinary throw API.
 for(auto id:{floating,sinking}){ObjectManipulation prop({Handle(id)});Check(prop.TryPickUp(Handle(id),world.Physics()),kind+" pickup prop");prop.Throw(world.Physics(),q*glm::vec3(0,.3f,-1),4);}
 Tick("props",180,{},q,false);
 Check(Local(world.Physics().GetTransform(Handle(floating)).position).y>Local(world.Physics().GetTransform(Handle(sinking)).position).y+.4f,kind+" thrown props retain float/sink ordering");
 cout<<kind<<" prop heights float="<<Local(world.Physics().GetTransform(Handle(floating)).position).y<<" sink="<<Local(world.Physics().GetTransform(Handle(sinking)).position).y<<'\n';
 Check(hands.TryPickUp(cupHandle,world.Physics()),kind+" cup pickup");
 Tick("over_pool",150,{1.6f,4,-1.5f},q,true);Tick("dip",210,{1.6f,1.1f,-1.5f},q*glm::angleAxis(1.5707963f,glm::vec3(1,0,0)),true);Tick("upright",90,{1.6f,1.1f,-1.5f},q,true);
 const auto acquired=Inside();cout<<kind<<" acquired="<<acquired.size()<<'\n';Check(!acquired.empty(),kind+" acquires actual pool particles");
 Tick("lift",180,{1.6f,4.4f,-1.5f},q,true);Tick("carry",180,{1.6f,4.4f,5},q,true);
 const auto carried=Inside();size_t matching=0;for(auto i:carried)matching+=acquired.count(i);cout<<kind<<" carried="<<carried.size()<<" same="<<matching<<'\n';Check(matching>0,kind+" carries same acquired particles away");
 Tick("return",180,{1.6f,4.4f,-1.5f},q,true);Tick("pour",240,{1.6f,4.4f,-1.5f},q*glm::angleAxis(2.2f,glm::vec3(1,0,0)),true);
 auto remaining=Inside();cout<<kind<<" remaining="<<remaining.size()<<'\n';Check(remaining.size()<carried.size(),kind+" pours acquired water out");hands.Drop();
 double finalMass=0;float farthest=0,finalMaxSpeed=0;for(auto& p:world.Fluid().Particles()){finalMass+=p.mass;farthest=std::max(farthest,glm::length(p.position-origin));finalMaxSpeed=std::max(finalMaxSpeed,glm::length(p.velocity));}cout<<kind<<" final_farthest_from_basin="<<farthest<<" final_max_particle_speed="<<finalMaxSpeed<<'\n';Check(world.Fluid().Particles().size()==count&&finalMass==mass,kind+" particle inventory/mass exact");Check(finite,kind+" all particle states finite");Check(geometryValid,kind+" no unresolved physical geometry");cout<<kind<<" maximum_particle_speed="<<maximumParticleSpeed<<" initial_mass="<<mass<<" final_mass="<<finalMass<<'\n';
 Check(world.DestroyEntity(cup,&error),kind+" destroy cup");Shape staleShape;BodyTransform stalePose;Check(!world.Physics().GetBodyShape(cupHandle,staleShape,stalePose),kind+" destroyed cup handle invalid");Tick("destroyed",10,{},q,false);
 session.End();world.Destroy();Check(world.Build(scene,nullptr,error)&&session.Begin(world,error),kind+" reconstruct");Check(world.Fluid().Particles().size()==count,kind+" reload fresh inventory");Check(world.FluidSurfaceDirty(),kind+" fresh world surface invalidated");world.MarkFluidSurfaceUploaded();Check(!world.FluidSurfaceDirty(),kind+" unchanged surface reusable");StepPlayedWorld(session,window,1.f/60);Check(world.FluidSurfaceDirty(),kind+" executed liquid step invalidates surface");
}
int main(int argc,char** argv){filesystem::path out="build/fluid-demo-checks";string kind;for(int i=1;i<argc;++i){string arg=argv[i];if(arg=="--output"&&i+1<argc)out=argv[++i];else if(arg=="--scene"&&i+1<argc)kind=argv[++i];else if(i==1)out=arg;else kind=arg;}filesystem::create_directories(out);if(kind.empty()||kind=="pool")Run("pool",out);if(kind.empty()||kind=="planet")Run("planet",out);cout<<"SUMMARY checks="<<checks<<" failures="<<failures<<'\n';return failures?1:0;}
