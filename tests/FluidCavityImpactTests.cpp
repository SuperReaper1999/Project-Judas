#include "Project.h"
// FTFT9 direct production-path contained-particle impulse budget witness.
#include <cmath>
#include <iomanip>
#include <iostream>
#include <glm/gtc/quaternion.hpp>
#include "Scene.h"
#include "RuntimeWorld.h"
#include "GameSession.h"
#include "Simulation.h"
#include "ProductionFluidCoupling.h"
#include "Window.h"
int Run(float bodyMass,const glm::quat& rotation,const glm::vec3& offset,float boost=0) {
 Scene scene;scene.Settings().name="contained transfer budget";scene.Settings().fluidHydrostaticDragRate=0;
 auto& observer=scene.CreateObject("observer");observer.playerStart=ScenePlayerStartComponent{};observer.transform.position={10,10,10};
 auto& gravity=scene.CreateObject("zero acceleration");gravity.gravity=SceneGravityComponent{};gravity.gravity->kind=SceneGravityKind::Uniform;gravity.gravity->magnitude=0;gravity.gravity->regionShape=SceneRegionShape::Box;gravity.gravity->regionHalfExtents=glm::vec3(100);
 auto& wall=scene.CreateObject("boundary");const auto id=wall.id;wall.body=SceneBodyComponent{};wall.body->motion=SceneBodyMotion::Dynamic;wall.body->mass=bodyMass;wall.body->initialLinearVelocity=rotation*glm::vec3(boost,0,0);wall.body->halfExtents={.05f,1,1};wall.body->friction=0;wall.body->restitution=0;wall.body->fluidCavities.push_back({{-.55f,0,0},{.5f,.5f,.5f}});wall.transform.position=offset+rotation*glm::vec3(.55f,0,0);wall.transform.rotation=rotation;
 auto& volume=scene.CreateObject("liquid");volume.fluidVolume=SceneFluidVolumeComponent{};volume.fluidVolume->countX=1;volume.fluidVolume->countY=1;volume.fluidVolume->countZ=1;volume.fluidVolume->spacing=std::cbrt(.1f);volume.transform.position={.47f,0,0};
 RuntimeWorld world;ProjectSettings experimental;experimental.advancedFluidDynamics=true;world.ConfigureProject(experimental);GameSession session;Window window;window.SetTestInputMode(true);std::string error;
 if(!world.Build(scene,nullptr,error)||!session.Begin(world,error)){std::cerr<<error<<'\n';return 2;}
 // Public runtime API sets the prescribed initial fluid velocity. There is no
 // alternative integration/contact path: the ordinary StepPlayedWorld runs.
 world.Fluid().Clear();world.Fluid().AddParticle(offset+rotation*glm::vec3(.47f,0,0),rotation*glm::vec3(1+boost,0,0),100);
 const auto handle=world.DynamicBodies()[world.FindEntity(id)->slot].Handle();
 auto budget=[&](){const auto fd=world.Fluid().GetDiagnostics();const auto v=world.Physics().GetLinearVelocity(handle),w=world.Physics().GetAngularVelocity(handle);const double ke=fd.kineticEnergy+.5*world.Physics().GetMass(handle)*glm::dot(v,v)+.5*glm::dot(w,world.Physics().GetInertiaWorld(handle)*w);return std::pair<double,glm::vec3>{ke,fd.totalMomentum+world.Physics().GetMass(handle)*v};};
 const auto before=budget();StepPlayedWorld(session,window,1.f/60);const auto after=budget();
 const auto v=world.Physics().GetLinearVelocity(handle),w=world.Physics().GetAngularVelocity(handle);const auto& particle=world.Fluid().Particles().front();const auto& measurement=world.FluidCoupling().Measurements();
 std::cout<<std::setprecision(17)<<"particle_mass=100 body_mass="<<bodyMass<<" common_boost="<<boost<<" rotation_angle="<<glm::angle(rotation)<<" dt="<<1.f/60<<" initial_energy="<<before.first<<" final_energy="<<after.first<<" energy_change="<<after.first-before.first<<" initial_momentum="<<before.second.x<<","<<before.second.y<<","<<before.second.z<<" final_momentum="<<after.second.x<<","<<after.second.y<<","<<after.second.z<<" momentum_residual="<<glm::length(after.second-before.second)<<" body_v="<<v.x<<","<<v.y<<","<<v.z<<" body_w="<<w.x<<","<<w.y<<","<<w.z<<" particle_v="<<particle.velocity.x<<","<<particle.velocity.y<<","<<particle.velocity.z<<" contained_impulse="<<measurement.bodies.front().containedImpulse.x<<","<<measurement.bodies.front().containedImpulse.y<<","<<measurement.bodies.front().containedImpulse.z<<'\n';
 const bool energy=after.first<=before.first+1e-3;const bool momentum=glm::length(after.second-before.second)<=1e-3;
 // Independent perfectly inelastic two-mass collision: common velocity is
 // total initial momentum / total mass; kinetic-energy loss follows from it.
 const glm::vec3 expected=rotation*glm::vec3(boost+100.f/(100.f+bodyMass),0,0);
 const double expectedEnergy=.5*(100.0+bodyMass)*std::pow(boost+100.0/(100.0+bodyMass),2);
 const bool mechanics=glm::length(v-expected)<2e-5f && glm::length(particle.velocity-expected)<2e-5f && glm::length(w)<2e-5f && std::abs(after.first-expectedEnergy)<1e-3;
 std::cout<<"ORACLE common_speed="<<boost+100.0/(100+bodyMass)<<" expected_energy="<<expectedEnergy<<" mechanics="<<mechanics<<'\n';
 std::cout<<"CHECK no_unexplained_energy_creation="<<energy<<" momentum_closed="<<momentum<<'\n';return energy&&momentum&&mechanics?0:1;
}

int main() {
 int failed=0;
 const glm::quat rotation=glm::angleAxis(.91f,glm::normalize(glm::vec3(1,-2,3)));
 for(float mass:{1.f,80.f,1000.f}) { failed+=Run(mass,glm::quat(1,0,0,0),glm::vec3(0)); failed+=Run(mass,rotation,{2,-3,1}); }
 failed+=Run(80.f,glm::quat(1,0,0,0),glm::vec3(0),3.f);
 std::cout<<"SUMMARY cases=7 failures="<<failed<<'\n';return failed?1:0;
}
