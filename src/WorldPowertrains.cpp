#include "RuntimeWorld.h"
#include "PowertrainAuthoring.h"
#include "PerformanceProfiler.h"
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>
#include <cmath>
#include <map>

namespace {
struct BodyRow {BodyHandle body;glm::vec3 axis{0};};
struct ShaftRow {std::map<uint64_t,BodyRow> bodies;WheelInstance* wheel=nullptr;double ownInverse=0;glm::vec3 axis{1,0,0};double velocity=0;bool available=false;};
void Add(ShaftRow& row,BodyHandle body,glm::vec3 axis){if(!body.IsValid())return;auto& value=row.bodies[body.id];value.body=body;value.axis+=axis;}
double AxialSpeed(PhysicsWorld& physics,BodyHandle body,const glm::vec3& axis,float dt){
 if(dt>0&&physics.IsKinematicBody(body)){
  // Reuse prescribed motion's current-interval preview. Projecting the
  // velocity of a perpendicular unit tangent extracts angular speed without
  // advancing the bearing pose, its target phase, or its velocity command.
  const auto center=physics.GetCenterOfMass(body);
  const auto tangent=glm::normalize(glm::cross(axis,std::abs(axis.x)<.8f?glm::vec3(1,0,0):glm::vec3(0,1,0)));
  const auto delta=physics.GetStepPointVelocity(body,center+tangent,dt)-physics.GetStepPointVelocity(body,center,dt);
  return glm::dot(glm::cross(tangent,delta),axis);
 }
 return glm::dot(physics.GetAngularVelocity(body),axis);
}
std::vector<ShaftRow> Rows(RuntimeWorld& world,PowertrainInstance& train,float dt=0){
 auto& physics=world.Physics();std::vector<ShaftRow> rows(train.settings.shafts.size());
 for(size_t i=0;i<rows.size();++i){auto& row=rows[i];const auto& shaft=train.settings.shafts[i];
  const auto bearing=world.RuntimeBody(shaft.bearing);const bool bearingPresent=!shaft.bearing||physics.IsBodyEnabled(bearing);
  row.axis=glm::normalize(shaft.axis);if(shaft.bearing&&bearingPresent)row.axis=physics.GetTransform(bearing).rotation*row.axis;
  if(shaft.kind==PowertrainShaftKind::Wheel){row.wheel=world.RuntimeWheel(shaft.target);if(!row.wheel)continue;const auto chassis=world.RuntimeBody(row.wheel->settings.chassis);row.available=row.wheel->settings.enabled&&physics.IsBodyEnabled(chassis)&&physics.IsDynamicBody(chassis);if(!row.available)continue;
   row.axis=glm::normalize(physics.GetTransform(chassis).rotation*row.wheel->settings.orientation*glm::angleAxis(row.wheel->command.steeringAngle,glm::vec3(0,1,0)))*glm::vec3(-1,0,0);
   row.ownInverse=1.0/row.wheel->settings.spinInertia;row.velocity=row.wheel->state.angularVelocity-glm::dot(physics.GetAngularVelocity(chassis),row.axis);Add(row,chassis,-row.axis);
  }else if(shaft.kind==PowertrainShaftKind::Body){const auto rotor=world.RuntimeBody(shaft.target);row.available=bearingPresent&&physics.IsBodyEnabled(rotor)&&physics.IsDynamicBody(rotor);if(!row.available)continue;row.velocity=glm::dot(physics.GetAngularVelocity(rotor),row.axis)-AxialSpeed(physics,bearing,row.axis,dt);Add(row,rotor,row.axis);Add(row,bearing,-row.axis);
  }else{row.available=bearingPresent;if(!row.available)continue;const double bearingSpeed=AxialSpeed(physics,bearing,row.axis,dt);if(!train.internalInitialized[i]){train.absoluteSpin[i]+=bearingSpeed;train.internalInitialized[i]=true;}row.ownInverse=1.0/shaft.inertia;row.velocity=train.absoluteSpin[i]-bearingSpeed;Add(row,bearing,-row.axis);}
 }
 return rows;
}
glm::dmat3 InverseInertia(PhysicsWorld& physics,BodyHandle body){if(!physics.IsDynamicBody(body))return glm::dmat3(0);const auto inverse=glm::inverse(glm::dmat3(physics.GetInertiaWorld(body)));return .5*(inverse+glm::transpose(inverse));}
}
PowertrainInstance* RuntimeWorld::RuntimePowertrain(EntityId id){const auto* definition=RuntimeDefinition(id);if(!definition||!definition->powertrain)return nullptr;auto [it,added]=m_powertrains.try_emplace(id);if(added){it->second.Reset(*definition->powertrain);const auto rows=Rows(*this,it->second);for(size_t i=0;i<rows.size();++i){auto& shaft=it->second.state.shafts[i];shaft.angularVelocity=rows[i].velocity;shaft.available=it->second.settings.enabled&&rows[i].available;it->second.state.available|=shaft.available;}}return &it->second;}
const PowertrainInstance* RuntimeWorld::RuntimePowertrain(EntityId id)const{const auto* definition=RuntimeDefinition(id);if(!definition||!definition->powertrain)return nullptr;const auto found=m_powertrains.find(id);return found==m_powertrains.end()?nullptr:&found->second;}
bool RuntimeWorld::SetPowertrainSettings(EntityId id,const PowertrainSettings& settings,std::string& error){
 auto* train=RuntimePowertrain(id);if(!train||!ValidatePowertrainSettings(settings,error))return false;
 auto previous=train->settings;previous.enabled=settings.enabled;
 if(!PowertrainSettingsEqual(previous,settings)){error="runtime powertrain topology/settings are authored; only enabled may change";return false;}
 // Toggling a mechanical contribution changes neither endpoint identity nor
 // durable momentum. Lost/disabled terminals remain safely unavailable.
 train->settings.enabled=settings.enabled;m_scriptDefinitions.at(id).powertrain=settings;
 if(!settings.enabled){train->state.available=false;for(auto& shaft:train->state.shafts)shaft.available=false;}

 return true;
}
void RuntimeWorld::RefreshPowertrainParticipants(){m_powertrainParticipants.clear();for(auto owner:m_powertrainOwners){const auto definition=m_scriptDefinitions.find(owner);if(definition!=m_scriptDefinitions.end()&&definition->second.powertrain)for(auto target:PowertrainReferences(*definition->second.powertrain))m_powertrainParticipants.insert(target);}}
void RuntimeWorld::StepPowertrains(float dt,bool sources){
 JUDAS_PROFILE_SCOPE("Mechanical powertrains");JUDAS_PROFILE_COUNTER("Powertrain assemblies",double(m_powertrainOwners.size()),ProfileCounterMode::Latest);
 for(auto id:m_powertrainOwners){auto* train=RuntimePowertrain(id);if(!train)continue;if(!sources&&!train->state.converged)continue;auto rows=Rows(*this,*train,dt);const auto n=rows.size();std::vector<double> velocities(n),mass(n*n);std::vector<bool> available(n);
  for(size_t i=0;i<n;++i){velocities[i]=rows[i].velocity;available[i]=rows[i].available;mass[i*n+i]=rows[i].ownInverse;for(size_t j=0;j<n;++j)for(const auto& [body,a]:rows[i].bodies){auto other=rows[j].bodies.find(body);if(other!=rows[j].bodies.end())mass[i*n+j]+=glm::dot(glm::dvec3(a.axis),InverseInertia(m_physics,a.body)*glm::dvec3(other->second.axis));}}
  PowertrainState prior;if(!sources)prior=train->state;const double previousEnergy=train->state.kineticEnergy;auto impulses=train->Step(dt,velocities,mass,available,sources);if(impulses.size()!=n)continue;
  std::map<uint64_t,BodyRow> physical;bool finite=true;
  for(size_t i=0;i<n;++i)if(available[i]){
   const double impulse=impulses[i];finite&=std::isfinite(impulse);
   if(rows[i].wheel)finite&=std::isfinite(float(double(rows[i].wheel->state.angularVelocity)+impulse*rows[i].ownInverse));
   else if(train->settings.shafts[i].kind==PowertrainShaftKind::Internal)finite&=std::isfinite(train->absoluteSpin[i]+impulse*rows[i].ownInverse);
   for(auto [key,body]:rows[i].bodies){auto& sum=physical[key];sum.body=body.body;sum.axis+=body.axis*float(impulse);}
  }
  for(const auto& [_,impulse]:physical){const auto velocity=m_physics.GetAngularVelocity(impulse.body)+glm::vec3(InverseInertia(m_physics,impulse.body)*glm::dvec3(impulse.axis));for(unsigned k=0;k<3;++k)finite&=std::isfinite(impulse.axis[k])&&std::isfinite(velocity[k]);}
  if(!finite){if(!sources)train->state=std::move(prior);else{train->state.sourceTorque.assign(train->settings.sources.size(),0);train->state.brakeTorque.assign(train->settings.brakes.size(),0);train->state.links.assign(train->settings.links.size(),{});train->state.kineticEnergy=previousEnergy;train->state.constraintEnergyChange=0;}train->state.converged=false;train->state.available=false;train->state.diagnostic="mechanical impulse exceeds physical numeric representation";for(size_t i=0;i<n;++i){train->state.shafts[i].angularVelocity=rows[i].velocity;if(sources)train->state.shafts[i].torque=0;train->state.shafts[i].available=false;}continue;}
  // Preflight the complete assembly before publishing any physical impulse.
  // No clamped torque or partially driven endpoint is substituted on failure.
  for(size_t i=0;i<n;++i)if(available[i]){if(rows[i].wheel)rows[i].wheel->state.angularVelocity=float(double(rows[i].wheel->state.angularVelocity)+impulses[i]*rows[i].ownInverse);else if(train->settings.shafts[i].kind==PowertrainShaftKind::Internal)train->absoluteSpin[i]+=impulses[i]*rows[i].ownInverse;}
  for(const auto& [_,impulse]:physical)if(glm::length(impulse.axis)>1e-8f)m_physics.ApplyAngularImpulse(impulse.body,impulse.axis);
 }
}
void RuntimeWorld::UpdatePowertrains(float dt){StepPowertrains(dt,true);}
void RuntimeWorld::ResolvePowertrainLoads(float dt){StepPowertrains(dt,false);}
void RuntimeWorld::PublishPowertrains(float dt){
 for(auto id:m_powertrainOwners){auto* train=RuntimePowertrain(id);if(!train)continue;const auto rows=Rows(*this,*train);train->state.available=false;
  for(size_t i=0;i<rows.size();++i){train->state.shafts[i].angularVelocity=rows[i].velocity;train->state.shafts[i].available=train->settings.enabled&&train->state.converged&&rows[i].available;train->state.available|=train->state.shafts[i].available;}
  for(size_t i=0;i<train->state.links.size();++i){auto& state=train->state.links[i];if(!state.active||!train->settings.enabled||!train->state.converged||i>=train->command.links.size()){state.slip=0;continue;}const auto& link=train->settings.links[i];const auto& command=train->command.links[i];const auto ratio=command.overrideRatio?command.ratio:link.ratio;state.slip=rows[link.output].velocity-ratio*rows[link.input].velocity;if(link.kind==PowertrainLinkKind::Differential)state.slip+=rows[link.outputB].velocity-ratio*rows[link.input].velocity;}
  if(train->settings.enabled&&dt>0)train->AdvanceAngles(dt);
 }
}
