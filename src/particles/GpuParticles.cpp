#include "GpuParticles.h"
#include "GpuParticleCollision.h"
#include "GpuParticleData.h"
#include "GpuParticleShaders.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <limits>
#include <glm/gtc/type_ptr.hpp>

namespace JudasParticles {
namespace {
using namespace JudasRender;
using namespace GpuDetail;
constexpr std::size_t EventHeaderBytes=48;
constexpr unsigned MaxCosmeticRecords=1024;
constexpr auto StorageBarrier=ComputeBarrier::Storage;
std::array<float,4> V(glm::vec3 value,float w=0){return {value.x,value.y,value.z,w};}
std::array<float,4> V(glm::vec4 value){return {value.x,value.y,value.z,value.w};}
bool Finite(const glm::vec3& v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool Finite(const glm::mat4& matrix){const auto p=glm::value_ptr(matrix);return std::all_of(p,p+16,[](auto value){return std::isfinite(value);});}
std::uint64_t Wide(unsigned low,unsigned high){return std::uint64_t(low)|(std::uint64_t(high)<<32);}
bool SupportedSettings(const ParticleEmitterSettings& settings,std::string& error){
 if(!ValidParticleSettings(settings)){error="GPU particle settings are invalid.";return false;}
 if(settings.localSpace){error="GPU particle simulation currently requires world-space particles; local-space emitters use CPU.";return false;}
 if(settings.impacts==ParticleImpactDelivery::Reliable){error="Reliable gameplay particle impacts require the CPU backend; GPU delivery is explicitly cosmetic and delayed.";return false;}
 if(settings.response==ParticleResponse::Stick){error="GPU stick anchors cannot yet transfer safely to moving-body CPU continuation; select CPU for Stick response.";return false;}
 return true;
}
struct Receiver {
 std::uint64_t entity=0,revision=0;unsigned child=0;
 glm::vec3 position{0};glm::quat rotation{1,0,0,0};
};
}

struct GpuParticles::Storage {
 ComputeDevice device;
 ComputeBuffer states,params,counters,freeSlots,events,colliders,nodes,triangles,keys,viewParams,records,commands;
 ComputeKernel reset,clear,shift,spawn,simulate,collide,finalize,viewKeys,sortLocal,sortMerge,gather;
 Params parameters;
 ParticleEmitterSettings settings;
 ParticleRuntimeStats stats;
 GpuParticleDiagnostics diagnostics;
 ParticleImpacts impacts;
 std::uint64_t geometryFingerprint=0;
 std::vector<std::shared_ptr<const CollisionAsset>> retainedAssets;
 unsigned topRoot=0,colliderCount=0,impactCapacity=0,nextBirth=1;
 std::uint64_t gpuDropped=0,invalidImpactDropped=0;
 bool previousReported=true,viewReady=false;
 struct Pending {
  ComputeReadback handle;
  std::vector<unsigned char> bytes;
  std::vector<Receiver> receivers;
  glm::dvec3 origin{0};
  ParticleEmitterSettings settings;
  std::uint64_t tick=0;
 };
 std::deque<Pending> pending;
 // Only the latest uncopied batch still exists in the source event buffer.
 // Ordinary ticks may overwrite it with explicit cosmetic-drop accounting;
 // an explicit blocking CPU transition must drain it before teardown.
 std::unique_ptr<Pending> unreported;

 void Publish(Pending& pending){
  std::array<unsigned,12> header{};std::memcpy(header.data(),pending.bytes.data(),EventHeaderBytes);
  stats.live=header[0];stats.contacts=Wide(header[1],header[2]);stats.queryFailures=header[6];
  stats.simulated=Wide(header[8],header[9]);gpuDropped=Wide(header[4],header[5]);
  diagnostics.sampleTick=pending.tick;diagnostics.readbackBytes+=pending.bytes.size();
  const auto count=std::min(header[3],unsigned((pending.bytes.size()-EventHeaderBytes)/sizeof(ImpactRecord)));
  for(unsigned index=0;index<count;++index){ImpactRecord record;std::memcpy(&record,pending.bytes.data()+EventHeaderBytes+index*sizeof(record),sizeof(record));
   if(record.identity[0]>=pending.receivers.size()){++invalidImpactDropped;continue;}
   const auto& receiver=pending.receivers[record.identity[0]];ParticleImpact impact;impact.tick=pending.tick;impact.receiver=receiver.entity;impact.geometryRevision=receiver.revision;impact.childKey=receiver.child;impact.feature=record.identity[2];
   const glm::vec3 point(record.pointAge[0],record.pointAge[1],record.pointAge[2]),normal(record.normalSize[0],record.normalSize[1],record.normalSize[2]);
   impact.position=pending.origin+glm::dvec3(point);impact.localPosition=glm::dvec3(glm::inverse(receiver.rotation)*(point-receiver.position));impact.normal=normal;impact.localNormal=glm::inverse(receiver.rotation)*normal;
   impact.velocity={record.velocityWeight[0],record.velocityWeight[1],record.velocityWeight[2]};impact.size=record.normalSize[3];impact.age=record.pointAge[3];impact.count=1;impact.weight=record.velocityWeight[3];impact.color=glm::mix(pending.settings.color,pending.settings.endColor,glm::clamp(impact.age/pending.settings.lifetime,0.f,1.f));
   if(!impacts.Add(impact,pending.settings))invalidImpactDropped+=impact.count;
  }
 }

 bool Dispatch(ComputeKernel kernel,unsigned count,unsigned groupSize,std::string& error){
  if(!device.Dispatch(kernel,{(count+groupSize-1)/groupSize,1,1},error))return false;
  ++diagnostics.dispatches;return device.Barrier(StorageBarrier,error);
 }
 bool BindSimulation(std::string& error){
  return device.BindStorage(states,0,ComputeAccess::ReadWrite,error)&&
   device.BindStorage(params,1,ComputeAccess::ReadOnly,error)&&
   device.BindStorage(counters,2,ComputeAccess::ReadWrite,error)&&
   device.BindStorage(freeSlots,3,ComputeAccess::ReadWrite,error)&&
   device.BindStorage(colliders,4,ComputeAccess::ReadOnly,error)&&
   device.BindStorage(nodes,5,ComputeAccess::ReadOnly,error)&&
   device.BindStorage(triangles,6,ComputeAccess::ReadOnly,error)&&
   device.BindStorage(events,7,ComputeAccess::ReadWrite,error);
 }
 bool UploadParams(std::string& error){if(!device.UpdateBuffer(params,0,&parameters,sizeof(parameters),error))return false;diagnostics.parameterUploadBytes+=sizeof(parameters);return true;}
 bool UploadGeometry(const CollisionSnapshot& snapshot,std::uint64_t fingerprint,std::string& error){
  if(fingerprint==geometryFingerprint)return true;
  PackedGeometry geometry;
  if(!PackGeometry(snapshot,geometry,error))return false;
  if(geometry.colliders.empty())geometry.colliders.emplace_back();
  if(geometry.nodes.empty())geometry.nodes.emplace_back();
  if(geometry.triangles.empty())geometry.triangles.emplace_back();
  ComputeBuffer replacements[3];
  replacements[0]=device.CreateBuffer(geometry.colliders.size()*sizeof(Collider),ComputeBufferUse::Storage,geometry.colliders.data(),error);
  if(replacements[0].IsValid())replacements[1]=device.CreateBuffer(geometry.nodes.size()*sizeof(Node),ComputeBufferUse::Storage,geometry.nodes.data(),error);
  if(replacements[1].IsValid())replacements[2]=device.CreateBuffer(geometry.triangles.size()*sizeof(Triangle),ComputeBufferUse::Storage,geometry.triangles.data(),error);
  if(!replacements[2].IsValid()){
   const auto cause=error;std::string ignored;for(auto buffer:replacements)if(buffer.IsValid())device.DestroyBuffer(buffer,ignored);error=cause;return false;
  }
  std::string ignored;
  for(auto buffer:{colliders,nodes,triangles})if(buffer.IsValid())device.DestroyBuffer(buffer,ignored);
  colliders=replacements[0];nodes=replacements[1];triangles=replacements[2];
  retainedAssets.clear();for(const auto& collider:snapshot.Colliders())if(collider.asset)retainedAssets.push_back(collider.asset);
  geometryFingerprint=fingerprint;topRoot=geometry.root;colliderCount=unsigned(snapshot.Colliders().size());
  diagnostics.geometryUploadBytes+=geometry.colliders.size()*sizeof(Collider)+geometry.nodes.size()*sizeof(Node)+geometry.triangles.size()*sizeof(Triangle);
  ++stats.geometryUploads;stats.snapshotBodies=snapshot.Bodies().size();stats.snapshotColliders=colliderCount;stats.snapshotTriangles=snapshot.Stats().uniqueTriangles;
  return true;
 }
};

GpuParticles::GpuParticles():storage(std::make_unique<Storage>()){}
GpuParticles::~GpuParticles(){std::string error;if(!Shutdown(error))std::fprintf(stderr,"[Particles GPU] %s\n",error.c_str());}
bool GpuParticles::Initialized()const{return storage->device.Supported();}
const ParticleRuntimeStats& GpuParticles::Stats()const{return storage->stats;}
GpuParticleDiagnostics GpuParticles::Diagnostics()const{return storage->diagnostics;}

bool GpuParticles::Initialize(const GraphicsCapabilities& caps,const ParticleEmitterSettings& settings,
                              ComputeBudget budget,std::string& error){
 error.clear();auto& s=*storage;
 s.stats.requested=settings.backend;
 if(Initialized()){error="GPU particle owner is already initialized.";return false;}
 if(!SupportedSettings(settings,error)){s.stats.diagnostic=error;return false;}
 if(!s.device.Initialize(caps,budget,error)){s.stats.diagnostic=error;return false;}
 s.settings=settings;s.stats.requested=settings.backend;s.stats.active=ParticleBackend::Gpu;
 s.diagnostics.capacity=unsigned(settings.maxParticles);s.diagnostics.sortCapacity=256;
 while(s.diagnostics.sortCapacity<s.diagnostics.capacity)s.diagnostics.sortCapacity*=2;
 s.impactCapacity=std::min(settings.maxImpactCells,MaxCosmeticRecords);
 auto buffer=[&](std::size_t bytes,ComputeBufferUse use,const void* initial=nullptr){return s.device.CreateBuffer(bytes,use,initial,error);};
 s.states=buffer(s.diagnostics.capacity*sizeof(GpuParticleState),ComputeBufferUse::Storage);
 s.params=buffer(sizeof(Params),ComputeBufferUse::Storage);
 s.counters=buffer(16*sizeof(unsigned),ComputeBufferUse::Storage);
 s.freeSlots=buffer(s.diagnostics.capacity*sizeof(unsigned),ComputeBufferUse::Storage);
 s.events=buffer(EventHeaderBytes+s.impactCapacity*sizeof(ImpactRecord),ComputeBufferUse::Storage);
 s.colliders=buffer(sizeof(Collider),ComputeBufferUse::Storage);
 s.nodes=buffer(sizeof(Node),ComputeBufferUse::Storage);
 s.triangles=buffer(sizeof(Triangle),ComputeBufferUse::Storage);
 s.keys=buffer(s.diagnostics.sortCapacity*sizeof(Key),ComputeBufferUse::Storage);
 s.viewParams=buffer(sizeof(ViewParams),ComputeBufferUse::Storage);
 s.records=buffer(s.diagnostics.capacity*32,ComputeBufferUse::Storage|ComputeBufferUse::Vertex);
 s.commands=buffer(4*sizeof(unsigned),ComputeBufferUse::Storage|ComputeBufferUse::Indirect);
 const auto buffers={s.states,s.params,s.counters,s.freeSlots,s.events,s.colliders,s.nodes,s.triangles,s.keys,s.viewParams,s.records,s.commands};
 bool success=std::all_of(buffers.begin(),buffers.end(),[](auto handle){return handle.IsValid();});
 auto kernel=[&](std::string source,const char* label){return s.device.CreateKernel(source.c_str(),label,error);};
 if(success){
  s.reset=kernel(ResetShader(),"ParticlesV2/reset-impacts");s.clear=kernel(ClearShader(),"ParticlesV2/clear-pool");
  s.shift=kernel(ShiftShader(),"ParticlesV2/origin-shift");s.spawn=kernel(SpawnShader(),"ParticlesV2/spawn");
  s.simulate=kernel(SimulationShader(false),"ParticlesV2/simulate");s.finalize=kernel(FinalizeShader(),"ParticlesV2/finalize");
  s.viewKeys=kernel(ViewKeysShader(),"ParticlesV2/view-keys");s.sortLocal=kernel(SortLocalShader(),"ParticlesV2/sort-local");
  s.sortMerge=kernel(SortMergeShader(),"ParticlesV2/sort-merge");s.gather=kernel(GatherShader(),"ParticlesV2/gather");
  for(auto handle:{s.reset,s.clear,s.shift,s.spawn,s.simulate,s.finalize,s.viewKeys,s.sortLocal,s.sortMerge,s.gather})success&=handle.IsValid();
 }
 if(!success){const auto cause=error;std::string ignored;Shutdown(ignored);error=cause.empty()?"Cannot allocate resident GPU particle resources.":cause;storage->stats.diagnostic=error;return false;}
 s.parameters.control[0]=s.diagnostics.capacity;
 if(!Clear(error)){const auto cause=error;std::string ignored;Shutdown(ignored);error=cause;return false;}
 s.stats.gpuBytes=s.device.AllocatedBytes();s.stats.diagnostic="Resident desktop compute particles; cosmetic counters/events are asynchronous.";
 return true;
}

bool GpuParticles::Step(const ParticleEmitterSettings& settings,const GpuParticleStepInputs& input,
                        const CollisionSnapshot* snapshot,std::string& error){
 error.clear();auto& s=*storage;
 // Every unsupported combination is rejected before simulation, spawning or
 // random-id mutation, so a deliberate one-time CPU transfer remains possible.
 if(!Initialized()){error="GPU particle owner is not initialized.";return false;}
 if(!SupportedSettings(settings,error))return false;
 if(unsigned(settings.maxParticles)!=s.diagnostics.capacity){error="Changing resident GPU particle capacity requires an explicit clear/rebuild or CPU transition.";return false;}
 if(!std::isfinite(input.dt)||input.dt<0||input.dt>.25f||input.spawnCount>100000||
    !Finite(input.position)||!Finite(input.scale)||!Finite(input.gravity)||!Finite(input.gravityCenter)||!Finite(input.wind)||
    !std::isfinite(input.gravityMagnitude)||input.gravityMagnitude<0||
    !std::isfinite(glm::dot(input.rotation,input.rotation))||glm::dot(input.rotation,input.rotation)<1e-12f||
    unsigned(input.gravityKind)>unsigned(GpuGravityKind::Radial)){
  error="GPU particle step needs bounded fixed dt, finite transforms/fields and a supported gravity descriptor.";return false;
 }
 if(input.spawnCount>std::numeric_limits<unsigned>::max()-s.nextBirth){error="GPU particle birth identities exhausted; explicitly clear or transition the emitter.";return false;}
 const unsigned births=settings.enabled?input.spawnCount:0;
 std::uint64_t fingerprint=0;
 if(settings.collision){
  if(!snapshot){error="GPU collision requires a completed immutable collision snapshot.";return false;}
  fingerprint=GeometryFingerprint(*snapshot,error);if(!error.empty())return false;
 }
 if(!PollEvents(error))return false;
 const bool changed=settings.collision&&fingerprint!=s.geometryFingerprint;
 if(settings.collision&&!s.UploadGeometry(*snapshot,fingerprint,error))return false;
 if(settings.collision&&!s.collide.IsValid()){
  const auto source=SimulationShader(true);s.collide=s.device.CreateKernel(source.c_str(),"ParticlesV2/static-BVH-collision",error);
  if(!s.collide.IsValid())return false;
 }
 const auto rotation=glm::normalize(input.rotation);
 Params params;params.positionDt=V(input.position,input.dt);params.rotation={rotation.x,rotation.y,rotation.z,rotation.w};params.scale=V(input.scale);
 params.spreadLife=V(settings.spread,settings.lifetime);params.velocitySize=V(settings.velocity,settings.size);params.variationEndSize=V(settings.velocityVariation,settings.endSize);
 params.accelerationRadius=V(rotation*settings.acceleration,settings.collisionRadius);
 params.gravityMagnitude=V(input.gravity,input.gravityMagnitude);params.gravityCenter=V(input.gravityCenter,settings.useWind?1.f:0.f);
 params.windRestitution=V(settings.useWind?input.wind:glm::vec3(0),settings.restitution);params.color=V(settings.color);params.endColor=V(settings.endColor);
 params.response={settings.friction,settings.impactWeight,settings.impactCellSize,0};params.control={s.diagnostics.capacity,births,s.nextBirth,settings.seed};
 if(settings.collision){params.domainMinimum=V(snapshot->Domain().min);params.domainMaximum=V(snapshot->Domain().max);}
 params.modes={settings.useGravity?unsigned(input.gravityKind):0u,unsigned(settings.response),settings.maxContacts,settings.impacts==ParticleImpactDelivery::Cosmetic?std::min(settings.maxImpactCells,s.impactCapacity):0u};
 params.query={s.topRoot,settings.collision?s.colliderCount:0u,0,0};params.history={unsigned(input.tick),unsigned(input.tick>>32),s.previousReported?0u:1u,changed?1u:0u};
 s.parameters=params;
 if(!s.UploadParams(error)||!s.BindSimulation(error)||!s.Dispatch(s.reset,1,1,error)||
    !s.Dispatch(settings.collision?s.collide:s.simulate,s.diagnostics.capacity,settings.collision?128:256,error))return false;
 // The draw-record allocation is idle scratch until the next view prepares
 // it. Reuse a bounded prefix for exceptional rejected-birth slot indices.
 if(!s.device.BindStorage(s.records,4,ComputeAccess::ReadWrite,error)||
    !s.device.SetKernelUInt2(s.finalize,"uParameters",{0,0},error)||!s.Dispatch(s.finalize,1,1,error))return false;
 if(births&&!s.Dispatch(s.spawn,births,256,error))return false;
 if(!s.device.SetKernelUInt2(s.finalize,"uParameters",{1,0},error)||!s.Dispatch(s.finalize,1,1,error)||!s.device.Barrier(ComputeBarrier::BufferUpdate,error))return false;
 s.nextBirth+=births;s.settings=settings;s.viewReady=false;s.previousReported=false;
 s.diagnostics.emitted+=births;
 const auto readbackBytes=EventHeaderBytes+params.modes[3]*sizeof(ImpactRecord);
 Storage::Pending pending;pending.bytes.resize(readbackBytes);pending.tick=input.tick;pending.origin=input.worldOrigin;pending.settings=settings;
 if(settings.collision&&params.modes[3])for(const auto& collider:snapshot->Colliders()){
  const auto& body=snapshot->Bodies()[collider.bodyIndex];pending.receivers.push_back({body.entity,collider.geometryRevision,collider.childKey,body.pose.position,body.pose.rotation});
 }
 pending.handle=s.device.RequestReadback(s.events,0,readbackBytes,error);
 s.unreported.reset();
 if(pending.handle.IsValid()){
  s.pending.push_back(std::move(pending));s.previousReported=true;
 }else{++s.diagnostics.skippedReadbacks;s.unreported=std::make_unique<Storage::Pending>(std::move(pending));error.clear();}
 s.stats.gpuBytes=s.device.AllocatedBytes();return true;
}

bool GpuParticles::PollEvents(std::string& error){
 error.clear();auto& s=*storage;if(!Initialized())return true;
 while(!s.pending.empty()){
  auto& pending=s.pending.front();const auto status=s.device.PollReadback(pending.handle,pending.bytes.data(),pending.bytes.size(),error);
  if(status==ComputeReadbackStatus::Pending)break;
  if(status==ComputeReadbackStatus::Failed){s.pending.pop_front();return false;}
  s.Publish(pending);
  s.pending.pop_front();
 }
 s.stats.cosmeticDropped=s.gpuDropped+s.invalidImpactDropped+s.impacts.Dropped();if(s.stats.queryFailures)s.stats.diagnostic="GPU collision/numerical query failure: unsafe visual motion was held or retired; inspect counters or select CPU.";s.stats.queuedImpacts=s.impacts.Pending().size();s.stats.deliveredImpacts=s.impacts.Delivered();s.stats.gpuBytes=s.device.AllocatedBytes();return true;
}
std::vector<ParticleImpact> GpuParticles::TakeImpacts(){auto result=storage->impacts.Take();storage->stats.queuedImpacts=0;storage->stats.deliveredImpacts=storage->impacts.Delivered();return result;}

bool GpuParticles::PrepareView(const glm::mat4& view,const glm::mat4& projection,bool sorted,std::string& error){
 auto& s=*storage;error.clear();if(!Initialized()){error="GPU particles are not initialized.";return false;}
 if(!Finite(view)||!Finite(projection)||!Finite(glm::inverse(view))){error="GPU particle view matrices must be finite and invertible.";return false;}
 ViewParams parameters;std::memcpy(parameters.view.data(),glm::value_ptr(view),64);std::memcpy(parameters.projection.data(),glm::value_ptr(projection),64);
 const auto vp=projection*view;glm::vec4 row[4];for(unsigned r=0;r<4;++r)row[r]={vp[0][r],vp[1][r],vp[2][r],vp[3][r]};
 for(unsigned axis=0;axis<3;++axis)for(unsigned side=0;side<2;++side){auto plane=row[3]+(side?row[axis]:-row[axis]);const auto length=glm::length(glm::vec3(plane));if(length<1e-12f){error="Degenerate GPU particle frustum.";return false;}parameters.planes[axis*2+side]=V(plane/length);}
 parameters.sizeLife={s.settings.size,s.settings.endSize,s.settings.lifetime,0};parameters.color=V(s.settings.color);parameters.endColor=V(s.settings.endColor);parameters.control={s.diagnostics.capacity,s.diagnostics.sortCapacity,sorted?1u:0u,0};
 if(!s.device.UpdateBuffer(s.viewParams,0,&parameters,sizeof(parameters),error)||
    !s.device.BindStorage(s.states,0,ComputeAccess::ReadOnly,error)||!s.device.BindStorage(s.keys,1,ComputeAccess::ReadWrite,error)||
    !s.device.BindStorage(s.viewParams,2,ComputeAccess::ReadOnly,error)||!s.device.BindStorage(s.commands,3,ComputeAccess::ReadWrite,error)||!s.device.BindStorage(s.records,4,ComputeAccess::WriteOnly,error))return false;
 s.diagnostics.parameterUploadBytes+=sizeof(parameters);
 auto dispatch=[&](ComputeKernel kernel,unsigned x,unsigned y){return s.device.SetKernelUInt2(kernel,"uParameters",{x,y},error)&&s.Dispatch(kernel,s.diagnostics.sortCapacity,256,error);};
 auto resetDraw=[&]{return s.device.SetKernelUInt2(s.viewKeys,"uParameters",{1,0},error)&&s.Dispatch(s.viewKeys,1,256,error);};
 if(!resetDraw()||!dispatch(s.viewKeys,0,0))return false;
 if(sorted){
  if(!dispatch(s.sortLocal,0,0))return false;
  for(unsigned level=512;level<=s.diagnostics.sortCapacity;level*=2){
   for(unsigned pass=level/2;pass>=256;pass/=2)if(!dispatch(s.sortMerge,level,pass))return false;
   if(!dispatch(s.sortLocal,level,0))return false;
  }
 }else if(!resetDraw())return false;
 if(!s.Dispatch(s.gather,s.diagnostics.sortCapacity,256,error)||!s.device.Barrier(ComputeBarrier::Vertex|ComputeBarrier::Indirect,error))return false;
 s.viewReady=true;return true;
}
bool GpuParticles::BindDrawBuffers(std::string& error){auto& s=*storage;if(!s.viewReady){error="GPU particle rendering requires preparation for this view.";return false;}return s.device.BindVertex(s.records,error)&&s.device.BindIndirect(s.commands,error);}
void GpuParticles::RecordDraw(){++storage->diagnostics.draws;}
bool GpuParticles::CanMutate(std::string& error)const{return storage->device.ValidateOwner(error);}
bool GpuParticles::CanShiftOrigin(glm::dvec3 delta,std::string& error)const{
 if(!CanMutate(error))return false;
 for(unsigned axis=0;axis<3;++axis)if(!std::isfinite(delta[axis])||std::abs(delta[axis])>std::numeric_limits<float>::max()){
  error="GPU particle origin delta must be finite and fit the local-coordinate range.";return false;
 }
 return true;
}

bool GpuParticles::Clear(std::string& error){
 auto& s=*storage;if(!Initialized()){error="GPU particles are not initialized.";return false;}
 for(auto& pending:s.pending){std::string ignored;s.device.CancelReadback(pending.handle,ignored);}s.pending.clear();s.unreported.reset();
 if(!s.UploadParams(error)||!s.BindSimulation(error)||!s.Dispatch(s.clear,s.diagnostics.capacity,256,error))return false;
 s.nextBirth=1;s.diagnostics.emitted=0;s.diagnostics.sampleTick=0;s.previousReported=true;s.viewReady=false;s.impacts=ParticleImpacts{};s.gpuDropped=s.invalidImpactDropped=0;s.stats.live=s.stats.contacts=s.stats.simulated=s.stats.queuedImpacts=s.stats.cosmeticDropped=s.stats.queryFailures=0;return true;
}
bool GpuParticles::ShiftOrigin(glm::dvec3 delta,std::string& error){
 if(!CanShiftOrigin(delta,error))return false;
 auto& s=*storage;
 // Origin changes are infrequent. Preserve the existing binary64 subtraction
 // contract here rather than rounding the delta before every float position.
 // These inactive integer parameters transport the exact IEEE double bits.
 std::array<unsigned,6> words{};
 for(unsigned axis=0;axis<3;++axis){std::uint64_t bits;std::memcpy(&bits,&delta[axis],8);words[axis*2]=unsigned(bits);words[axis*2+1]=unsigned(bits>>32);}
 s.parameters.history={words[0],words[1],words[2],words[3]};s.parameters.query[0]=words[4];s.parameters.query[1]=words[5];
 if(!s.UploadParams(error)||!s.BindSimulation(error)||!s.Dispatch(s.shift,s.diagnostics.capacity,256,error))return false;
 s.geometryFingerprint=0;s.viewReady=false;return true;
}
bool GpuParticles::ReadStateForDiagnostics(std::vector<GpuParticleState>& output,std::string& error){
 auto& s=*storage;if(!Initialized()){error="GPU particles are not initialized.";return false;}
 std::vector<GpuParticleState> state(s.diagnostics.capacity);
 // This explicit blocking endpoint is also the exceptional CPU transition.
 // Finish prior copies and drain their cosmetic observations before retiring
 // the GPU owner, rather than silently cancelling still-pending batches.
 if(!s.device.Barrier(ComputeBarrier::BufferUpdate,error)||!s.device.FinishForDiagnostics(error)||!PollEvents(error))return false;
 if(s.unreported){
  auto& last=*s.unreported;
  if(!s.device.ReadForDiagnostics(s.events,0,last.bytes.data(),last.bytes.size(),error))return false;
  s.Publish(last);s.unreported.reset();s.previousReported=true;
  s.stats.cosmeticDropped=s.gpuDropped+s.invalidImpactDropped+s.impacts.Dropped();
  s.stats.queuedImpacts=s.impacts.Pending().size();
  if(s.stats.queryFailures)s.stats.diagnostic="GPU collision/numerical query failure: unsafe visual motion was held or retired; inspect counters or select CPU.";
 }
 if(!s.device.ReadForDiagnostics(s.states,0,state.data(),state.size()*sizeof(state[0]),error))return false;
 output=std::move(state);return true;
}
bool GpuParticles::Shutdown(std::string& error){
 auto& s=*storage;if(!s.device.Shutdown(error))return false;
 const auto requested=s.stats.requested;
 for(auto* buffer:{&s.states,&s.params,&s.counters,&s.freeSlots,&s.events,&s.colliders,&s.nodes,&s.triangles,&s.keys,&s.viewParams,&s.records,&s.commands})*buffer={};
 for(auto* kernel:{&s.reset,&s.clear,&s.shift,&s.spawn,&s.simulate,&s.collide,&s.finalize,&s.viewKeys,&s.sortLocal,&s.sortMerge,&s.gather})*kernel={};
 s.parameters={};s.settings={};s.stats={};s.stats.requested=requested;s.diagnostics={};s.impacts=ParticleImpacts{};s.pending.clear();s.unreported.reset();
 s.retainedAssets.clear();s.geometryFingerprint=0;s.gpuDropped=s.invalidImpactDropped=0;s.topRoot=s.colliderCount=s.impactCapacity=0;s.nextBirth=1;s.previousReported=true;s.viewReady=false;return true;
}
} // namespace JudasParticles
