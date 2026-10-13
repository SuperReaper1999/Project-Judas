#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "WorldPersistence.h"
#include "SaveService.h"
#include "SaveStorage.h"
#include "SaveArchive.h"
#include "ScreenshotWriter.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <thread>

// This is a new end-to-end contract, not a relaxation of the earlier four
// bearing fixture. Only ordinary logical input drives the authored vehicle.
namespace {
unsigned checks=0,failures=0;
void Check(bool ok,const char* text){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",text);std::fflush(stdout);}
std::string MaterialBytes(DeformableMaterial material){SaveArchive archive;material.Save(archive);return archive.bytes;}
std::string StrengthNormalizedCook(const DeformableAsset& original){
 auto copy=original;
 // The immutable assets remain untouched. Normalize ONLY authored strength
 // capacities, retaining every other cooked physical/render-binding field.
 copy.fracture=std::make_shared<FractureCook>(*original.fracture);
 copy.fracture->tension=copy.fracture->shear=copy.fracture->compression=1;
 for(auto& bond:copy.fracture->bonds)bond.tension=bond.shear=bond.compression=1;
 return EncodeDeformableAsset(copy);
}
struct Observation {
 EntityId root=0,nose=0,material=0,wall=0;
 glm::vec3 initial{0},forward{0};glm::dvec3 initialSpan{0},cutRelativeNose{0};
 std::vector<glm::dmat3> initialPlastic{};
 std::vector<glm::dvec3> initialShape{};
 std::vector<std::array<double,6>> initialEdges{};
 double distance=0,approachSpeed=0,wallImpulse=0,plasticBefore=0,plasticImpact=0,shapeBefore=0,shapeImpact=0;
 double nonrigidBefore=0,nonrigidBeforeCut=0,nonrigidImpact=0,plasticBeforeCut=0;
 double intactPlastic=0,intactNonrigid=0;
 double compression=0,separation=0,tension=0,shear=0,peakForce=0;
 double noseMass=0;unsigned contactTick=0,firstFractureTick=0,cutTick=0,physicalBonds=0,scriptBonds=0;
 bool unhealthy=false,attachmentReleased=false,frontCommandHeld=true,rearPathSurvives=true;
};
glm::dvec3 GroupCenter(const DeformableInstance& material,const char* name){
 const auto found=material.asset->groups.find(name);if(found==material.asset->groups.end()||found->second.empty())return {};
 glm::dvec3 point{0};for(auto node:found->second)point+=material.positions[node];return point/double(found->second.size());
}
bool PatchesConnected(const DeformableInstance& material){
 const auto* cook=material.asset->fracture.get();if(!cook)return false;
 std::vector<unsigned> rearComponents,frontComponents;
 auto components=[&](const char* group,std::vector<unsigned>& values){
  auto found=material.asset->groups.find(group);if(found==material.asset->groups.end())return;
  for(auto node:found->second){const auto part=cook->nodePart[node];if(!material.fracture.removed[part])values.push_back(material.fracture.component[part]);}
 };
 components("rear",rearComponents);components("front",frontComponents);
 if(rearComponents.empty()||frontComponents.empty())return false;
 for(auto rear:rearComponents)for(auto front:frontComponents)if(rear==front)return true;
 return false;
}
double PlasticChange(const DeformableInstance& material,const Observation& observation){
 double result=0;for(size_t element=0;element<material.plasticRest.size()&&element<observation.initialPlastic.size();++element)
  for(unsigned column=0;column<3;++column)result=std::max(result,glm::length(material.plasticRest[element][column]-observation.initialPlastic[element][column]));
 return result;
}
double ShapeChange(const DeformableInstance& material,const BodyTransform& root,const Observation& observation){
 double result=0;for(size_t n=0;n<material.positions.size()&&n<observation.initialShape.size();++n){
  const auto local=glm::inverse(glm::dquat(root.rotation))*(material.positions[n]-glm::dvec3(root.position));
  result=std::max(result,glm::length(local-observation.initialShape[n]));
 }return result;
}
std::array<double,6> TetEdges(const DeformableInstance& material,const glm::uvec4& nodes){
 std::array<double,6> lengths{};unsigned edge=0;
 for(unsigned a=0;a<4;++a)for(unsigned b=a+1;b<4;++b)lengths[edge++]=glm::length(material.positions[nodes[a]]-material.positions[nodes[b]]);
 return lengths;
}
double NonrigidChange(const DeformableInstance& material,const Observation& observation){
 double result=0;for(size_t element=0;element<material.asset->solids.size()&&element<observation.initialEdges.size();++element){
  const auto edges=TetEdges(material,material.asset->solids[element].nodes);
  for(unsigned edge=0;edge<6;++edge)result=std::max(result,std::abs(edges[edge]-observation.initialEdges[element][edge]));
 }return result;
}
bool Contact(RuntimeWorld& world,EntityId body,EntityId other,double& peak){
 std::vector<PhysicsWorld::BodyContactSample> contacts;bool actual=false;
 if(world.Physics().ReadBodyContactSample(world.RuntimeBody(body),contacts))for(const auto& contact:contacts)
  if(world.EntityIdOfBody(contact.other)==other){peak=std::max(peak,double(contact.normalImpulse));actual|=contact.normalImpulse>0;}
 return actual;
}
bool LiveNose(RuntimeWorld& world,const Observation& observation){
 const auto body=world.RuntimeBody(observation.nose);Shape shape;BodyTransform pose;
 return world.Physics().IsBodyEnabled(body)&&world.Physics().IsDynamicBody(body)&&
  std::abs(world.Physics().GetMass(body)-observation.noseMass)<1e-6&&world.Physics().GetBodyShape(body,shape,pose)&&
  world.RuntimeWheel(observation.root+1)&&world.RuntimeWheel(observation.root+2)&&
  world.RuntimeWheel(observation.root+1)->settings.chassis==observation.nose&&
  world.RuntimeWheel(observation.root+2)->settings.chassis==observation.nose&&
  world.RuntimeWheel(observation.root+1)->state.available&&world.RuntimeWheel(observation.root+2)->state.available;
}
bool FrontCut(const PowertrainInstance& train,EntityId material){
 unsigned front=0;for(size_t i=0;i<train.settings.links.size();++i)if(train.settings.links[i].connection.owner==material){
  ++front;if(train.state.links[i].connected||train.state.links[i].active||std::abs(train.state.links[i].torque)>1e-9||!train.command.links[i].enabled)return false;
 }return front==2;
}
bool RearIntact(const PowertrainInstance& train){
 unsigned rear=0;for(size_t i=0;i<train.settings.links.size();++i)if(!train.settings.links[i].connection.owner){
  ++rear;if(!train.state.links[i].connected||!train.command.links[i].enabled)return false;
 }return rear==2;
}
struct SpawnObservation {EntityId root=0,nose=0,material=0;glm::vec3 start{0},forward{0};};
std::vector<SpawnObservation> SpawnedCars(RuntimeWorld& world){
 std::vector<SpawnObservation> cars;std::string error;
 for(const auto& definition:world.ScriptObjects(false))if(definition.id>=kRuntimeEntityIdBase&&definition.powertrain){
  const auto* train=world.RuntimePowertrain(definition.id);if(!train||!train->state.available)continue;
  SpawnObservation car;car.root=definition.id;bool valid=true;unsigned front=0,rear=0;
  for(size_t i=0;i<train->settings.links.size();++i)if(train->settings.links[i].connection.owner){
   const auto& c=train->settings.links[i].connection;if(car.material&&car.material!=c.owner)valid=false;car.material=c.owner;
   valid&=c.owner>=kRuntimeEntityIdBase&&c.attachment=="front"&&c.rootAttachment=="rear"&&world.PowertrainConnectionIntact(c)&&train->state.links[i].connected;
  }
  auto* material=world.RuntimeDeformable(car.material,error);
  if(!material||!material->error.empty()||std::any_of(material->fracture.broken.begin(),material->fracture.broken.end(),[](auto b){return b!=0;}))continue;
  for(const auto& a:material->settings.attachments){valid&=a.kind==DeformableAttachment::Kind::Body;
   if(a.group=="rear")valid&=a.target==car.root;
   if(a.group=="front")car.nose=a.target;}
  valid&=car.nose>=kRuntimeEntityIdBase&&car.nose!=car.root&&world.Physics().IsDynamicBody(world.RuntimeBody(car.nose));
  for(const auto& shaft:train->settings.shafts)if(shaft.kind==PowertrainShaftKind::Wheel){const auto* wheel=world.RuntimeWheel(shaft.target);
   valid&=wheel&&wheel->state.available&&shaft.target>=kRuntimeEntityIdBase;
   if(wheel){front+=wheel->settings.chassis==car.nose;rear+=wheel->settings.chassis==car.root;}}
  const auto root=world.RuntimeBody(car.root);valid&=front==2&&rear==2&&world.Physics().IsDynamicBody(root);
  if(valid){const auto pose=world.Physics().GetTransform(root);car.start=pose.position;car.forward=pose.rotation*glm::vec3(0,0,-1);cars.push_back(car);}
 }return cars;
}
void Dump(const std::filesystem::path& path,const std::string& bytes){std::ofstream file(path,std::ios::binary);file.write(bytes.data(),bytes.size());}
void ReadDiagnostic(RuntimeWorld& world,unsigned frame,const char* stage,size_t fixedSteps,bool paused,glm::vec3 initial,
 std::ofstream& trace,std::ofstream& contactTrace,std::ofstream& tyreTrace){
 const auto body=world.RuntimeBody(910);
 const auto pose=world.Physics().GetTransform(body);
 const auto velocity=world.Physics().GetLinearVelocity(body),angular=world.Physics().GetAngularVelocity(body),gravity=world.Gravity().Sample(pose.position);
 SaveArchive encoded;world.Physics().PersistBodyForces(encoded,body);SaveArchive forces(encoded.bytes);glm::vec3 force{0},torque{0};forces(force,torque);forces.Finish();
 std::string identity;float friction=0,restitution=0;world.Physics().GetPhysicalMaterial(body,identity,friction,restitution);
 std::vector<PhysicsWorld::BodyContactSample> contacts;const bool contactAvailable=world.Physics().ReadBodyContactSample(body,contacts);
 double normalImpulse=0,tyreLongitudinal=0,tyreLateral=0,tyreLoad=0;
 for(const auto& contact:contacts){normalImpulse+=contact.normalImpulse;std::string otherIdentity;float otherFriction=0,otherRestitution=0;
  world.Physics().GetPhysicalMaterial(contact.other,otherIdentity,otherFriction,otherRestitution);
  contactTrace<<frame<<','<<stage<<','<<world.EntityIdOfBody(contact.other)<<','<<contact.point.x<<','<<contact.point.y<<','<<contact.point.z<<','
   <<contact.normal.x<<','<<contact.normal.y<<','<<contact.normal.z<<','<<contact.normalImpulse<<','<<contact.separation<<','<<contact.sleeping<<','
   <<friction<<','<<otherFriction<<','<<std::sqrt(double(friction)*otherFriction)*contact.normalImpulse<<'\n';
 }
 for(EntityId wheelId:{901,902})if(const auto* wheel=world.RuntimeWheel(wheelId)){const auto& s=wheel->state;
  tyreLongitudinal+=s.longitudinalImpulse;tyreLateral+=s.lateralImpulse;tyreLoad+=s.normalForce;
  tyreTrace<<frame<<','<<stage<<','<<wheelId<<','<<s.available<<','<<s.contact<<','<<s.normalForce<<','<<s.normal.x<<','<<s.normal.y<<','<<s.normal.z<<','
   <<s.longitudinalSlip<<','<<s.lateralSlip<<','<<s.longitudinalImpulse<<','<<s.lateralImpulse<<','<<wheel->command.driveTorque<<','
   <<wheel->command.brakeTorque<<','<<wheel->settings.friction<<'\n';
 }
 const auto* train=world.RuntimePowertrain(900);std::string error;const auto* material=world.RuntimeDeformable(920,error);glm::dvec3 attachmentForce{0};
 if(material)for(size_t a=0;a<material->settings.attachments.size()&&a<material->attachmentState.size();++a)
  if(material->settings.attachments[a].target==910)attachmentForce+=material->attachmentState[a].force;
 trace<<frame<<','<<stage<<','<<fixedSteps<<','<<paused<<','<<body.id<<','<<body.world<<','
  <<pose.position.x<<','<<pose.position.y<<','<<pose.position.z<<','<<pose.rotation.w<<','<<pose.rotation.x<<','<<pose.rotation.y<<','<<pose.rotation.z<<','
  <<velocity.x<<','<<velocity.y<<','<<velocity.z<<','<<angular.x<<','<<angular.y<<','<<angular.z<<','<<world.Physics().GetMass(body)<<','
  <<world.Physics().IsSleeping(body)<<','<<glm::length(pose.position-initial)<<','<<gravity.x<<','<<gravity.y<<','<<gravity.z<<','
  <<force.x<<','<<force.y<<','<<force.z<<','<<torque.x<<','<<torque.y<<','<<torque.z<<','<<contactAvailable<<','<<contacts.size()<<','<<normalImpulse<<','
  <<tyreLoad<<','<<tyreLongitudinal<<','<<tyreLateral<<','<<attachmentForce.x<<','<<attachmentForce.y<<','<<attachmentForce.z<<','
  <<(train&&FrontCut(*train,920))<<','<<(material&&PatchesConnected(*material))<<'\n';
}
}

int main(int argc,char** argv){
 if(argc!=4)return 2;
 const std::string scene=argv[1],mode=argv[2];const bool probe=mode=="probe",watch=mode=="watch",lifecycle=mode=="lifecycle",drive=mode=="drive"||probe||mode=="write"||watch,read=mode=="read";
 const unsigned endTick=watch?2100:1200; // Allow the ordinary tour's own 30 s stop policy to complete.
 if(!drive&&!read&&!lifecycle&&mode!="rest")return 2;
 const auto output=std::filesystem::absolute(argv[3]);std::filesystem::create_directories(output);
 std::ofstream trace(output/"vehicle-trace.csv");trace<<std::setprecision(12)
  <<"tick,root,distance_m,approach_speed_mps,current_forward_mps,root_x,root_y,root_z,root_up_x,root_up_y,root_up_z,nose_x,nose_y,nose_z,nose_speed_mps,nose_angular_radps,nose_wall_step,nose_wall_peak_ns,source_input,source_torque_nm,transmission_torque_nm,plastic_rest_change_m,shape_change_m,nonrigid_edge_change_m,compression_m,separation_m,physical_bonds,script_bonds,patches_connected,front_connected,rear_connected,max_strain,min_jacobian,patch_peak_n,tensile_peak_pa,shear_peak_pa,material_error\n";
 std::ofstream wheelTrace(output/"wheel-trace.csv");wheelTrace<<std::setprecision(12)
  <<"tick,root,wheel,available,contact,length_m,normal_force_n,rpm,longitudinal_slip_mps,longitudinal_impulse_ns,lateral_impulse_ns\n";
 std::ofstream readTrace(output/"cold-read-trace.csv");readTrace<<std::setprecision(12)
  <<"frame,stage,fixed_steps,paused,body_id,body_world,x,y,z,qw,qx,qy,qz,vx,vy,vz,wx,wy,wz,mass_kg,sleeping,displacement_m,gx,gy,gz,force_x,force_y,force_z,torque_x,torque_y,torque_z,contact_available,contacts,normal_impulse_sum_ns,tyre_load_n,tyre_longitudinal_ns,tyre_lateral_ns,attachment_force_x,attachment_force_y,attachment_force_z,front_drive_cut,material_path_connected\n";
 std::ofstream readContacts(output/"cold-read-contacts.csv");readContacts<<std::setprecision(12)
  <<"frame,stage,other,px,py,pz,nx,ny,nz,normal_impulse_ns,separation_m,sleeping,friction,other_friction,geometric_mean_mu_impulse_bound_ns\n";
 std::ofstream readTyres(output/"cold-read-tyres.csv");readTyres<<std::setprecision(12)
  <<"frame,stage,wheel,available,contact,normal_force_n,nx,ny,nz,longitudinal_slip_mps,lateral_slip_mps,longitudinal_impulse_ns,lateral_impulse_ns,drive_torque_nm,brake_torque_nm,friction\n";
 std::array<Observation,2> observations{{{900,910,920,1500},{1100,1110,1120,1501}}};
 Window* window=nullptr;RuntimeWorld* original=nullptr;GameSnapshot snapshot;
 unsigned frame=0,ready=0,phase=0;bool complete=false,frozen=false,equalInput=true,approachPicture=false,cutPicture=false;
 bool crumplePicture=false,crumpleArmed=false;unsigned phaseFrame=0;size_t pausedSteps=0;
 RuntimeWorld* priorWorld=nullptr;std::vector<SpawnObservation> spawned;std::array<glm::vec3,2> pausedPositions{};
 std::vector<double> fixedTimes,frameTimes;auto frameStarted=std::chrono::steady_clock::now();
 auto key=[&](const char* name,bool down){window->QueueTestPhysical(std::string("key:")+name,down?1.f:0.f);};
 ApplicationControl control;control.hidden=true;control.frameSeconds=[&](float){return frozen?0.f:1.f/60;};
 control.hostReady=[&](EngineHost& host){window=&host.GetWindow();window->SetTestInputMode(true);SDL_GL_SetSwapInterval(0);std::string ignored;host.Audio().Init(ignored,true);};
 control.worldReady=[&](EngineHost&,RuntimeWorld& world,InteractivePlay&){original=&world;};
 control.beforeFrame=[&](EngineHost&,RuntimeWorld& world,InteractivePlay&){
  frameStarted=std::chrono::steady_clock::now();
  if(read&&phase==1){key("F8",true);phase=2;}else if(read&&phase==2)key("F8",false);
  if(read&&phase==2&&&world!=original){frozen=true;phase=3;}
 };
 control.afterFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){
  ++frame;std::string error;bool available=world.Scripts()&&world.Scripts()->Diagnostics().empty();
  for(const auto& o:observations){const auto* train=world.RuntimePowertrain(o.root);auto* material=world.RuntimeDeformable(o.material,error);
   available&=train&&train->state.available&&material&&material->asset->fracture&&
    world.Physics().IsBodyEnabled(world.RuntimeBody(o.root))&&world.Physics().IsBodyEnabled(world.RuntimeBody(o.nose));
  }
  auto witness=[&](const char* name){std::vector<unsigned char> pixels;host.GetRenderer().CaptureFrame(window->Width(),window->Height(),pixels);
   return WriteRgbPng((output/name).string(),window->Width(),window->Height(),pixels);};
  if(!ready&&available){
   ready=frame;
   for(auto& o:observations){auto* material=world.RuntimeDeformable(o.material,error);const auto root=world.Physics().GetTransform(world.RuntimeBody(o.root));
    o.initial=root.position;o.forward=root.rotation*glm::vec3(0,0,-1);o.initialPlastic=material->plasticRest;
    o.initialSpan=glm::inverse(glm::dquat(root.rotation))*(GroupCenter(*material,"front")-GroupCenter(*material,"rear"));
    for(auto point:material->positions)o.initialShape.push_back(glm::inverse(glm::dquat(root.rotation))*(point-glm::dvec3(root.position)));
    for(const auto& tet:material->asset->solids)o.initialEdges.push_back(TetEdges(*material,tet.nodes));
    o.noseMass=world.Physics().GetMass(world.RuntimeBody(o.nose));
   }
   const auto* weak=world.RuntimeDeformable(920,error);const auto* strong=world.RuntimeDeformable(1120,error);
   Check(StrengthNormalizedCook(*weak->asset)==StrengthNormalizedCook(*strong->asset)&&
    MaterialBytes(weak->settings.material)==MaterialBytes(strong->settings.material)&&
    weak->settings.substeps==strong->settings.substeps&&weak->settings.iterations==strong->settings.iterations&&
    world.Physics().GetMass(world.RuntimeBody(900))==world.Physics().GetMass(world.RuntimeBody(1100))&&
    observations[0].noseMass==observations[1].noseMass&&
    weak->asset->fracture->compression>0&&weak->asset->fracture->compression<strong->asset->fracture->compression,
    "same actual vehicle geometry and plastic material compare independently authored cohesive strength");
   Check(weak->asset->fracture->structural&&!weak->asset->fracture->rigid&&weak->asset->fracture->parts.size()>2&&
    std::all_of(weak->settings.attachments.begin(),weak->settings.attachments.end(),[](const auto& a){return a.kind==DeformableAttachment::Kind::Body&&a.forceLimit==0&&a.torqueLimit==0;}),
    "load-bearing crumple region uses real plastic tetrahedra and interior cohesive failure rather than finite mount release");
   bool ownership=true;for(const auto& o:observations)for(unsigned i=0;i<4;++i){const auto* wheel=world.RuntimeWheel(o.root+1+i);
    ownership&=wheel&&wheel->settings.chassis==(i<2?o.nose:o.root);}
   Check(ownership&&observations[0].noseMass>=20&&observations[1].noseMass>=20,
    "two front wheels belong to a substantial physical nose/axle module and rear wheels to the cabin body");
   if(read){auto* service=world.SceneControl()->Saves(host.Resources());SaveStorage storage(service->Root());bool recovered=false;
    Check(storage.Read("damage-current",snapshot,recovered,error),"fresh process reads the ordinary actual crashed-vehicle save");phase=1;}
  }
  if(ready&&!read&&!lifecycle){
   const unsigned tick=frame-ready;
   if(tick>20){fixedTimes.push_back(play.LastFixedStepMilliseconds());frameTimes.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frameStarted).count());}
   for(auto& o:observations){auto* material=world.RuntimeDeformable(o.material,error);auto* train=world.RuntimePowertrain(o.root);
    const auto root=world.Physics().GetTransform(world.RuntimeBody(o.root)),nose=world.Physics().GetTransform(world.RuntimeBody(o.nose));
    const auto velocity=world.Physics().GetLinearVelocity(world.RuntimeBody(o.root)),noseVelocity=world.Physics().GetLinearVelocity(world.RuntimeBody(o.nose));
    const auto localNose=glm::inverse(glm::dquat(root.rotation))*glm::dvec3(nose.position-root.position);
    o.distance=std::max(o.distance,double(glm::dot(root.position-o.initial,o.forward)));
    const bool actualContact=Contact(world,o.nose,o.wall,o.wallImpulse);
    if(actualContact&&!o.contactTick)o.contactTick=tick;
    if(tick>80&&!o.contactTick)o.approachSpeed=std::max(o.approachSpeed,double(glm::dot(velocity,o.forward)));
    const double plastic=PlasticChange(*material,o),shape=ShapeChange(*material,root,o),nonrigid=NonrigidChange(*material,o);
    const auto span=glm::inverse(glm::dquat(root.rotation))*(GroupCenter(*material,"front")-GroupCenter(*material,"rear"));
    const double compressed=glm::length(o.initialSpan)-glm::dot(span,glm::normalize(o.initialSpan));
    const bool path=PatchesConnected(*material);o.physicalBonds=o.scriptBonds=0;
    for(auto cause:material->fracture.broken){o.physicalBonds+=cause==1;o.scriptBonds+=cause==2;}
    o.attachmentReleased|=std::any_of(material->released.begin(),material->released.end(),[](auto r){return r!=0;});
    if(o.physicalBonds&&!o.firstFractureTick)o.firstFractureTick=tick;
    if(!o.contactTick){o.plasticBefore=std::max(o.plasticBefore,plastic);o.shapeBefore=std::max(o.shapeBefore,shape);o.nonrigidBefore=std::max(o.nonrigidBefore,nonrigid);}
    // Edge lengths are invariant under a fragment's rigid translation and
    // rotation. Impact-driven yielding after a cohesive cut still counts as
    // true nonrigid deformation, while freely moving intact debris cannot.
    if(o.contactTick){o.plasticImpact=std::max(o.plasticImpact,plastic);o.nonrigidImpact=std::max(o.nonrigidImpact,nonrigid);}
    // The forming witness must occur before ANY fracture, while the physical
    // section still carries the complete axle. Post-cut sag/debris cannot pass.
    if(o.contactTick&&path&&!o.physicalBonds&&!o.scriptBonds&&!o.attachmentReleased){
     o.intactPlastic=std::max(o.intactPlastic,plastic);o.intactNonrigid=std::max(o.intactNonrigid,nonrigid);}
    if(o.contactTick&&!o.cutTick){o.plasticBeforeCut=std::max(o.plasticBeforeCut,plastic);o.nonrigidBeforeCut=std::max(o.nonrigidBeforeCut,nonrigid);
     o.shapeImpact=std::max(o.shapeImpact,shape);o.compression=std::max(o.compression,compressed);}
    if(!path&&!o.cutTick){o.cutTick=tick;o.cutRelativeNose=localNose;}
    if(o.cutTick)o.separation=std::max(o.separation,glm::length(localNose-o.cutRelativeNose));
    double transmitted=0;unsigned frontConnected=0,rearConnected=0;
    for(size_t i=0;i<train->settings.links.size();++i){transmitted+=std::abs(train->state.links[i].torque);
     if(train->settings.links[i].connection.owner){frontConnected+=train->state.links[i].connected;o.frontCommandHeld&=train->command.links[i].enabled;}
     else{rearConnected+=train->state.links[i].connected;o.rearPathSurvives&=train->state.links[i].connected&&train->command.links[i].enabled;}
    }
    for(const auto& a:material->attachmentState)o.peakForce=std::max(o.peakForce,a.peakForce);
    for(auto value:material->fracture.tensionDemand)o.tension=std::max(o.tension,value);
    for(auto value:material->fracture.shearDemand)o.shear=std::max(o.shear,value);
    o.unhealthy|=!material->error.empty()||!material->fracture.error.empty();
    const auto up=root.rotation*glm::vec3(0,1,0);
    trace<<tick<<','<<o.root<<','<<o.distance<<','<<o.approachSpeed<<','<<glm::dot(velocity,o.forward)<<','<<root.position.x<<','<<root.position.y<<','<<root.position.z<<','
     <<up.x<<','<<up.y<<','<<up.z<<','<<nose.position.x<<','<<nose.position.y<<','<<nose.position.z<<','<<glm::length(noseVelocity)<<','
     <<glm::length(world.Physics().GetAngularVelocity(world.RuntimeBody(o.nose)))<<','<<actualContact<<','<<o.wallImpulse<<','
     <<train->command.sources[0].input<<','<<train->state.sourceTorque[0]<<','<<transmitted<<','<<plastic<<','<<shape<<','<<nonrigid<<','<<compressed<<','<<o.separation<<','
     <<o.physicalBonds<<','<<o.scriptBonds<<','<<path<<','<<frontConnected<<','<<rearConnected<<','<<material->stats.maximumStrain<<','<<material->stats.minimumJacobian<<','
     <<o.peakForce<<','<<o.tension<<','<<o.shear<<','<<material->error<<'\n';
    for(unsigned i=0;i<4;++i)if(auto* wheel=world.RuntimeWheel(o.root+1+i)){const auto& s=wheel->state;
     wheelTrace<<tick<<','<<o.root<<','<<o.root+1+i<<','<<s.available<<','<<s.contact<<','<<s.length<<','<<s.normalForce<<','
      <<s.angularVelocity*60/(2*3.14159265358979323846)<<','<<s.longitudinalSlip<<','<<s.longitudinalImpulse<<','<<s.lateralImpulse<<'\n';}
   }
   const auto* weakTrain=world.RuntimePowertrain(900);const auto* strongTrain=world.RuntimePowertrain(1100);
   equalInput&=weakTrain->command.sources[0].input==strongTrain->command.sources[0].input;
   if(drive&&!crumplePicture&&crumpleArmed&&!observations[0].physicalBonds&&!observations[0].attachmentReleased&&!observations[0].cutTick)
    crumplePicture=witness("intact-crumple.png");
   crumpleArmed|=observations[0].intactPlastic>observations[0].plasticBefore+.005&&observations[0].intactNonrigid>observations[0].nonrigidBefore+.03;
   if(drive&&!approachPicture&&observations[0].distance>8&&!observations[0].contactTick)approachPicture=witness("approach.png");
   // The first topology boundary is earlier than a visibly separated module
   // and the next UI/presentation publication. Capture the physical result,
   // rather than labelling the preceding intact rendered frame as damage.
   if(drive&&!cutPicture&&observations[0].separation>.2&&observations[0].cutTick&&tick>observations[0].cutTick+2)
    cutPicture=witness("structural-separation.png");
   if(tick==80){
    size_t wheels=0,trains=0,materials=0,nodes=0,tets=0;for(const auto& d:world.ScriptObjects(false)){
     wheels+=d.wheel.has_value();trains+=d.powertrain.has_value();if(d.deformable)if(auto* m=world.RuntimeDeformable(d.id,error)){++materials;nodes+=m->positions.size();tets+=m->asset->solids.size();}}
    std::printf("WORKLOAD bodies=%zu dynamic_bodies=%zu wheels=%zu powertrains=%zu materials=%zu nodes=%zu tets=%zu material_substeps=%u iterations=%u fixed_hz=60\n",
     world.Physics().AliveBodyCount(),world.Physics().DynamicBodyCount(),wheels,trains,materials,nodes,tets,
     world.RuntimeDeformable(920,error)->settings.substeps,world.RuntimeDeformable(920,error)->settings.iterations);
    bool supported=true,intact=true;for(const auto& o:observations){unsigned contact=0;for(unsigned i=0;i<4;++i)contact+=world.RuntimeWheel(o.root+1+i)->state.contact;
     supported&=contact>=3;intact&=!o.cutTick&&!o.physicalBonds&&!o.attachmentReleased&&!o.unhealthy;}
    Check(supported&&intact,"both real complete vehicles support their ordinary pre-drive load without structural failure");
    if(watch)key("V",true);
    else if(drive)key("W",true);
   }
   if(watch&&tick==81)key("V",false);
   if(tick==endTick-60)key("Tab",true);
   if(tick==endTick-59)key("Tab",false);
   if(tick==endTick){
    key("W",false);
    for(const auto& o:observations)std::printf("VEHICLE root=%llu distance_m=%.9g approach_speed_mps=%.9g nose_wall_ns=%.9g prewall_plastic_m=%.9g impact_plastic_m=%.9g precut_plastic_m=%.9g intact_plastic_m=%.9g prewall_nonrigid_m=%.9g impact_nonrigid_m=%.9g precut_nonrigid_m=%.9g intact_nonrigid_m=%.9g prewall_shape_m=%.9g impact_shape_m=%.9g compression_m=%.9g module_separation_m=%.9g nose_mass_kg=%.9g physical_bonds=%u scripted_bonds=%u contact_tick=%u first_fracture_tick=%u cut_tick=%u patch_peak_n=%.9g tensile_pa=%.9g shear_pa=%.9g error=%d\n",
     static_cast<unsigned long long>(o.root),o.distance,o.approachSpeed,o.wallImpulse,o.plasticBefore,o.plasticImpact,o.plasticBeforeCut,o.intactPlastic,o.nonrigidBefore,o.nonrigidImpact,o.nonrigidBeforeCut,o.intactNonrigid,o.shapeBefore,o.shapeImpact,o.compression,o.separation,
     o.noseMass,o.physicalBonds,o.scriptBonds,o.contactTick,o.firstFractureTick,o.cutTick,o.peakForce,o.tension,o.shear,o.unhealthy);
    const auto& weak=observations[0];const auto& strong=observations[1];
    Check(available&&!weak.unhealthy&&!strong.unhealthy,"ordinary public-API vehicle content completes without script or physical-solve failure");
    Check(LiveNose(world,weak)&&LiveNose(world,strong),"substantial front modules retain original dynamic mass, collision and actual front wheels");
    if(drive){
     Check(equalInput&&(watch||weakTrain->command.sources[0].input>0),
      watch?"ordinary V watch action controls both physical sources through the authored tour policy":"equal ordinary logical input supplies the same held source policy to both vehicles");
     if(watch){bool finished=false;for(const auto& state:world.Scripts()->Capture())if(state.entity==10)
       finished=state.json.find("\"automatic\":false")!=std::string::npos;
      Check(finished&&weakTrain->command.sources[0].input==0&&strongTrain->command.sources[0].input==0,
       "ordinary watch policy finishes and releases throttle after its actual physical crash tour");}
     Check(weak.distance>8&&strong.distance>8&&weak.approachSpeed>2&&strong.approachSpeed>2,"actual powertrain and tyre forces produce a real forward wall approach");
     Check(weak.wallImpulse>0&&strong.wallImpulse>0,"actual physical front-module wall contacts carry nonzero solved impact impulses");
     if(!probe){
      Check(weak.intactPlastic>weak.plasticBefore+.005&&weak.intactNonrigid>weak.nonrigidBefore+.03,
       "wall loading permanently forms the actual intact section before any fracture beyond ordinary ride deformation");
      Check(weak.cutTick>=weak.contactTick&&weak.contactTick&&weak.physicalBonds>0&&weak.scriptBonds==0&&!weak.attachmentReleased&&
       !PatchesConnected(*world.RuntimeDeformable(920,error)),"physical interior cohesive fracture severs the front/rear load path after impact without scripted or mount release");
      Check(weak.separation>.2&&LiveNose(world,weak),"a substantial complete front axle/nose module with both wheels separates through real physics");
      Check(FrontCut(*weakTrain,920)&&weak.frontCommandHeld&&RearIntact(*weakTrain)&&weak.rearPathSurvives,
       "actual topology loss removes front drive with commands still held while the independent rear drivetrain survives");
      Check(!strong.cutTick&&!strong.physicalBonds&&!strong.scriptBonds&&!strong.attachmentReleased&&PatchesConnected(*world.RuntimeDeformable(1120,error)),
       "otherwise identical stronger same-input vehicle retains its actual structural and mechanical load path");
      Check(PlasticChange(*world.RuntimeDeformable(920,error),weak)>.005,"permanent physical material rest remains changed after the damaging encounter");
     }
    }else{
     Check(weak.distance<.5&&strong.distance<.5&&weakTrain->command.sources[0].input==0&&strongTrain->command.sources[0].input==0,
      "same-duration no-drive control receives no hidden launch or scripted crash");
     Check(!weak.physicalBonds&&!strong.physicalBonds&&!weak.cutTick&&!strong.cutTick&&!weak.attachmentReleased&&!strong.attachmentReleased,
      "ordinary stationary load leaves both complete structural paths intact");
    }
    witness("strong-comparison.png");
    if(mode=="write"){key("F5",true);phase=6;}else{complete=true;world.UI().RequestQuit();}
   }
   if(tick==1201)key("F5",false);
   if(mode=="write"&&phase==6){auto* service=world.SceneControl()->Saves(host.Resources());
    for(uint64_t id=1;id<8;++id)if(const auto* status=service->Status(id);status&&status->operation=="save"){
     if(status->state=="completed"){Check(true,"ordinary save commits actual crashed material, physical endpoints and drive state");complete=true;phase=9;world.UI().RequestQuit();}
     if(status->state=="failed"){std::printf("SAVE %s\n",status->error.c_str());Check(false,"actual crashed-vehicle save completes");complete=true;phase=9;world.UI().RequestQuit();}
    }
   }
  }
  if(ready&&lifecycle){
   if(phase==0&&frame>=ready+80){key("P",true);phase=1;phaseFrame=frame;}
   if((phase==1||phase==2)&&frame==phaseFrame+1)key("P",false);
   if(phase==1&&frame>phaseFrame+100){const auto cars=SpawnedCars(world);
    Check(cars.size()==1,"ordinary P creates a complete fresh vehicle with remapped physical nose, material, wheels and drive path");
    key("P",true);phase=2;phaseFrame=frame;}
   if(phase==2&&frame>phaseFrame+140){spawned=SpawnedCars(world);
    bool distinct=spawned.size()==2,calm=distinct;
    if(distinct){distinct=spawned[0].root!=spawned[1].root&&spawned[0].nose!=spawned[1].nose&&spawned[0].material!=spawned[1].material&&
      world.RuntimeDeformable(spawned[0].material,error)!=world.RuntimeDeformable(spawned[1].material,error);
     for(const auto& car:spawned)calm&=glm::length(world.Physics().GetLinearVelocity(world.RuntimeBody(car.root)))<.5f&&
      world.RuntimePowertrain(car.root)->command.sources[0].input==0;
    }
    Check(distinct&&calm,"two ordinary prefab instances settle intact with independent finite bodies/material state and no hidden launch");
    key("W",true);phase=3;phaseFrame=frame;}
   if(phase==3&&frame>phaseFrame+80){bool moved=spawned.size()==2;
    for(const auto& car:spawned)moved&=glm::dot(world.Physics().GetTransform(world.RuntimeBody(car.root)).position-car.start,car.forward)>.5f&&
      world.RuntimePowertrain(car.root)->command.sources[0].input>0&&world.PowertrainConnectionIntact(world.RuntimePowertrain(car.root)->settings.links[0].connection);
    Check(moved,"ordinary drive moves both separately constructed physical vehicles through their own intact tyres and powertrains");
    key("W",false);key("Escape",true);phase=4;phaseFrame=frame;}
   if(phase==4&&frame==phaseFrame+1){key("Escape",false);pausedSteps=play.FixedStepsSinceReset();
    for(size_t i=0;i<spawned.size()&&i<pausedPositions.size();++i)pausedPositions[i]=world.Physics().GetTransform(world.RuntimeBody(spawned[i].root)).position;}
   if(phase==4&&frame>phaseFrame+12){bool unchanged=spawned.size()==2;
    for(size_t i=0;i<spawned.size()&&i<pausedPositions.size();++i)unchanged&=world.Physics().GetTransform(world.RuntimeBody(spawned[i].root)).position==pausedPositions[i];
    Check(play.IsPaused()&&play.FixedStepsSinceReset()==pausedSteps&&unchanged,"ordinary pause holds the actual spawned physical endpoints without hidden fixed simulation");
    key("Escape",true);phase=5;phaseFrame=frame;}
   if(phase==5&&frame==phaseFrame+1)key("Escape",false);
   if(phase==5&&frame>phaseFrame+12){Check(!play.IsPaused()&&play.FixedStepsSinceReset()>pausedSteps,"ordinary resume returns to authoritative physical stepping");
    priorWorld=&world;key("R",true);phase=6;phaseFrame=frame;}
   if(phase==6&&frame==phaseFrame+1)key("R",false);
   if(phase==6&&&world!=priorWorld&&available){bool retired=true;for(const auto& car:spawned)retired&=!world.RuntimeDefinition(car.root)&&!world.RuntimeDefinition(car.nose)&&!world.RuntimeDefinition(car.material);
    Check(retired&&PatchesConnected(*world.RuntimeDeformable(920,error))&&world.Scripts()->Diagnostics().empty(),
     "ordinary reload replaces the world, retires spawned mechanics and reconstructs healthy intact authored vehicles");
    priorWorld=&world;key("2",true);phase=7;phaseFrame=frame;}
   if(phase==7&&frame==phaseFrame+1)key("2",false);
   if(phase==7&&&world!=priorWorld&&world.SceneControl()->Current()=="Scenes/flat.judas"&&world.RuntimePowertrain(100)){
    Check(!world.RuntimeDefinition(900)&&world.RuntimeDeformable(102,error)&&world.Scripts()->Diagnostics().empty(),
     "ordinary scene selection replaces vehicle material ownership with the retained bearing lab");
    priorWorld=&world;key("1",true);phase=8;phaseFrame=frame;}
   if(phase==8&&frame==phaseFrame+1)key("1",false);
   if(phase==8&&&world!=priorWorld&&world.SceneControl()->Current()=="Scenes/vehicle_crash.judas"&&available){
    Check(PatchesConnected(*world.RuntimeDeformable(920,error))&&SpawnedCars(world).empty()&&world.Scripts()->Diagnostics().empty(),
     "returning normally reconstructs current vehicle assets without stale prefab, material or script ownership");
    witness("lifecycle.png");complete=true;world.UI().RequestQuit();}
  }
  if(read&&phase==3){std::map<std::string,SaveChunk> chunks;
   if(WorldPersistence::Capture(world,chunks,error)){
    for(const char* name:{"entities","contacts","sleep"}){
     if(auto found=snapshot.participants.find(name);found!=snapshot.participants.end())Dump(output/(std::string("expected-")+name+".bin"),found->second.data);
     if(auto found=chunks.find(name);found!=chunks.end())Dump(output/(std::string("actual-")+name+".bin"),found->second.data);
    }
    Check(play.FixedStepsSinceReset()==0,"new process observes restored physical endpoint before any resumed solver step");
    for(const char* name:{"entities","deformables","wheels","powertrains"}){
     const auto found=chunks.find(name),expected=snapshot.participants.find(name);
     Check(found!=chunks.end()&&expected!=snapshot.participants.end()&&found->second.data==expected->second.data,
      (std::string("cold restoration retains exact authoritative ")+name+" rather than cached damage labels").c_str());
     if(found!=chunks.end())Dump(output/(std::string("restored-")+name+".bin"),found->second.data);
    }
    auto* weak=world.RuntimeDeformable(920,error);auto* train=world.RuntimePowertrain(900);
    Check(weak&&!PatchesConnected(*weak)&&FrontCut(*train,920)&&RearIntact(*train)&&LiveNose(world,observations[0]),
     "cold restored actual fractured body, wheels and derived front-drive cut preserve physical ownership");
    observations[0].initial=world.Physics().GetTransform(world.RuntimeBody(910)).position;
    const auto lateral=world.Physics().GetTransform(world.RuntimeBody(900)).rotation*glm::vec3(1,0,0);
    ReadDiagnostic(world,frame,"restored",play.FixedStepsSinceReset(),play.IsPaused(),observations[0].initial,readTrace,readContacts,readTyres);
    std::printf("COLD IMPULSE vector_ns=(%.12g,%.12g,%.12g) fixed_dt=%.12g unchanged_displacement_gate_m=.01 unchanged_observation_steps=45\n",
     double(lateral.x*20),double(lateral.y*20),double(lateral.z*20),double(1.f/60));
    world.Physics().ApplyLinearImpulse(world.RuntimeBody(910),lateral*20.f);
    ReadDiagnostic(world,frame,"impulse-immediate",play.FixedStepsSinceReset(),play.IsPaused(),observations[0].initial,readTrace,readContacts,readTyres);
    frozen=false;phase=4;ready=frame;
   }else if(error!="loading"&&error.find("required script instance not initialized")==std::string::npos){
    std::printf("RESTORE %s\n",error.c_str());Check(false,"cold restored actual vehicle becomes ready");complete=true;world.UI().RequestQuit();}
  }
  if(read&&phase==4&&frame>ready)ReadDiagnostic(world,frame,"resumed",play.FixedStepsSinceReset(),play.IsPaused(),observations[0].initial,readTrace,readContacts,readTyres);
  if(read&&phase==4&&frame>ready+45){auto* weak=world.RuntimeDeformable(920,error);const auto current=world.Physics().GetTransform(world.RuntimeBody(910)).position;
   std::printf("COLD ENDPOINT displacement_m=%.12g fixed_steps=%zu paused=%d pose=(%.12g,%.12g,%.12g) velocity=(%.12g,%.12g,%.12g)\n",
    double(glm::length(current-observations[0].initial)),play.FixedStepsSinceReset(),play.IsPaused(),double(current.x),double(current.y),double(current.z),
    double(world.Physics().GetLinearVelocity(world.RuntimeBody(910)).x),double(world.Physics().GetLinearVelocity(world.RuntimeBody(910)).y),double(world.Physics().GetLinearVelocity(world.RuntimeBody(910)).z));
   Check(glm::length(current-observations[0].initial)>.01f&&weak&&!PatchesConnected(*weak)&&FrontCut(*world.RuntimePowertrain(900),920),
    "restored front module physically responds to a new ordinary impulse while its topology and drive stay disconnected");
   complete=true;world.UI().RequestQuit();}
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  if(frame>2600){Check(false,"bounded physical vehicle application deadline");world.UI().RequestQuit();}
 };
 Application application;std::string argument=scene;char name[]="judas";char* args[]={name,argument.data()};
 Check(application.Run(2,args,&control)==0&&complete,"ordinary registered vehicle scene starts and shuts down normally");
 if(!fixedTimes.empty()){std::sort(fixedTimes.begin(),fixedTimes.end());std::sort(frameTimes.begin(),frameTimes.end());
  std::printf("PERF samples=%zu fixed_median_ms=%.6f fixed_p95_ms=%.6f fixed_max_ms=%.6f frame_median_ms=%.6f frame_p95_ms=%.6f frame_max_ms=%.6f\n",
   fixedTimes.size(),fixedTimes[fixedTimes.size()/2],fixedTimes[size_t(.95*(fixedTimes.size()-1))],fixedTimes.back(),frameTimes[frameTimes.size()/2],frameTimes[size_t(.95*(frameTimes.size()-1))],frameTimes.back());}
 std::printf("M83 physical vehicle %s checks=%u failures=%u\n",mode.c_str(),checks,failures);return failures?1:0;
}
