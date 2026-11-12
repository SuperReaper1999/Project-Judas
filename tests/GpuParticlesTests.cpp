#include "GraphicsApi.h"
#include "CollisionAsset.h"
#include "particles/CollisionSnapshot.h"
#include "particles/GpuParticles.h"
#include "particles/GpuParticleShaders.h"
#include "particles/ParticleRandom.h"
#include "render/GpuParticlePass.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <thread>
#include <glm/gtc/matrix_transform.hpp>

using namespace JudasParticles;
namespace {
unsigned checks=0,failures=0;
class GraphicsTestHost {
 SDL_Window* window=nullptr;SDL_GLContext context=nullptr;GraphicsCapabilities caps;bool vulkan=false;
public:
 bool Init(const char* title,int width,int height,bool,std::string& error){
  if(SDL_Init(SDL_INIT_VIDEO)!=0){error=SDL_GetError();return false;}
  if(JudasVulkan::Requested()){
   // JUDAS_GRAPHICS=vulkan: the same production particle owners on the experimental Vulkan device.
   window=SDL_CreateWindow(title,SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,width,height,JudasVulkan::WindowFlag()|SDL_WINDOW_HIDDEN);
   if(!window){error=SDL_GetError();Shutdown();return false;}
   if(!JudasVulkan::Init(window,4,6,error)){Shutdown();return false;}
   vulkan=true;
   if(!gladLoadGL(&JudasVulkan::ProcAddress)){error="Cannot load Vulkan backend entry points";Shutdown();return false;}
   caps=QueryGraphicsCapabilities();if(!ValidateGraphicsCapabilities(caps,error)){Shutdown();return false;}return true;
  }
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,5);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
  window=SDL_CreateWindow(title,SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,width,height,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  if(!window){error=SDL_GetError();Shutdown();return false;}
  context=SDL_GL_CreateContext(window);if(!context||SDL_GL_MakeCurrent(window,context)!=0){error=SDL_GetError();Shutdown();return false;}
  if(!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)){error="Cannot load actual GL entry points";Shutdown();return false;}
  caps=QueryGraphicsCapabilities();if(!ValidateGraphicsCapabilities(caps,error)){Shutdown();return false;}return true;
 }
 const GraphicsCapabilities& GraphicsCaps()const{return caps;}
 void Shutdown(){if(vulkan)JudasVulkan::Shutdown();vulkan=false;if(context)SDL_GL_DeleteContext(context);if(window)SDL_DestroyWindow(window);context=nullptr;window=nullptr;SDL_QuitSubSystem(SDL_INIT_VIDEO);}
 ~GraphicsTestHost(){Shutdown();}
};
void Check(bool pass,const char* name){++checks;failures+=!pass;std::printf("%s %s\n",pass?"PASS":"FAIL",name);std::fflush(stdout);}
bool Near(glm::vec3 a,glm::vec3 b,float tolerance=2e-5f){return glm::length(a-b)<=tolerance;}
std::vector<GpuParticleState> Live(GpuParticles& pool,std::string& error){
 std::vector<GpuParticleState> result;if(!pool.ReadStateForDiagnostics(result,error))return {};
 result.erase(std::remove_if(result.begin(),result.end(),[](const auto& p){return !p.identity[0];}),result.end());
 std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.identity[1]<b.identity[1];});return result;
}
glm::vec3 Position(const GpuParticleState& p){return {p.positionAge[0],p.positionAge[1],p.positionAge[2]};}
glm::vec3 Velocity(const GpuParticleState& p){return {p.velocityLife[0],p.velocityLife[1],p.velocityLife[2]};}
bool Sample(GpuParticles& pool,std::uint64_t tick,std::string& error){
 for(unsigned attempt=0;attempt<500;++attempt){if(!pool.PollEvents(error))return false;if(pool.Diagnostics().sampleTick>=tick)return true;std::this_thread::sleep_for(std::chrono::milliseconds(1));}return false;
}
void Pure(){
 GpuParticles pool;ParticleEmitterSettings settings;settings.backend=ParticleBackend::Gpu;std::string error;
 Check(!pool.Initialize({},settings,{},error)&&!error.empty()&&!pool.Initialized()&&pool.Stats().gpuBytes==0,"portable/no-context compute rejection allocates no GPU particle state");
 settings.impacts=ParticleImpactDelivery::Reliable;
 Check(!pool.Initialize({},settings,{},error)&&error.find("Reliable")!=std::string::npos,"reliable gameplay impacts explicitly require CPU before initialization");
 settings.impacts=ParticleImpactDelivery::Off;settings.localSpace=true;
 Check(!pool.Initialize({},settings,{},error)&&error.find("local-space")!=std::string::npos,"unsupported local-space GPU simulation has an actionable CPU boundary");
 settings.localSpace=false;settings.response=ParticleResponse::Stick;
 Check(!pool.Initialize({},settings,{},error)&&error.find("Stick")!=std::string::npos,"stick anchors select CPU until their continuation can transfer safely");
 Check(pool.Shutdown(error)&&pool.Shutdown(error),"empty GPU particle teardown is idempotent without a context");
 const auto a=SampleParticleBirth(settings,1),b=SampleParticleBirth(settings,1);
 Check(a.position==b.position&&a.velocity==b.velocity&&a.state==b.state,"V2 seed and stable birth id give repeatable explicitly ordered random values");
}
void SmallPools(GraphicsTestHost& host){
 // Query the actual production clear kernel's runtime scalar-array block.
 // Some drivers report a rounded linked minimum larger than one uint.
 const auto source=GpuDetail::ClearShader();const char* text=source.c_str();
 const auto shader=glCreateShader(GL_COMPUTE_SHADER);glShaderSource(shader,1,&text,nullptr);glCompileShader(shader);
 const auto program=glCreateProgram();glAttachShader(program,shader);glLinkProgram(program);
 GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);
 const auto block=linked?glGetProgramResourceIndex(program,GL_SHADER_STORAGE_BLOCK,"FreeSlots"):GL_INVALID_INDEX;
 GLint minimum=0;if(block!=GL_INVALID_INDEX){const GLenum property=GL_BUFFER_DATA_SIZE;glGetProgramResourceiv(program,GL_SHADER_STORAGE_BLOCK,block,1,&property,1,nullptr,&minimum);}
 std::printf("SMALL_POOL_LAYOUT free_slots_linked_minimum=%d scalar_bytes=%zu\n",minimum,sizeof(unsigned));
 Check(linked&&block!=GL_INVALID_INDEX&&minimum>=GLint(sizeof(unsigned)),"production scalar-array storage reports its actual linked minimum");
 glDeleteProgram(program);glDeleteShader(shader);
 for(unsigned capacity=1;capacity<=4;++capacity){
  ParticleEmitterSettings settings;settings.backend=ParticleBackend::Gpu;settings.maxParticles=int(capacity);settings.spread=settings.velocityVariation=settings.velocity=glm::vec3(0);
  GpuParticles pool;std::string error;bool ready=pool.Initialize(host.GraphicsCaps(),settings,{},error);
  if(!ready)std::fprintf(stderr,"SMALL_POOL_INITIALIZE capacity=%u error=%s\n",capacity,error.c_str());
  GpuParticleStepInputs input;input.spawnCount=capacity;input.tick=1;
  if(ready)ready=pool.Step(settings,input,nullptr,error);
  auto state=ready?Live(pool,error):std::vector<GpuParticleState>{};bool exact=ready&&state.size()==capacity;
  for(unsigned index=0;exact&&index<capacity;++index)exact=state[index].identity[1]==index+1;
  const auto operationError=error;const auto sampled=pool.Stats().live;const auto tick=pool.Diagnostics().sampleTick;
  if(!exact&&ready){
   std::vector<GpuParticleState> raw;std::string diagnostic;
   const bool read=pool.ReadStateForDiagnostics(raw,diagnostic);
   std::fprintf(stderr,"SMALL_POOL_RAW read=%u slots=%zu error=%s\n",read?1u:0u,raw.size(),diagnostic.c_str());
   for(const auto& particle:raw)std::fprintf(stderr,"SMALL_POOL_RAW_SLOT live=%u id=%u seed=%u age=%g lifetime=%g\n",particle.identity[0],particle.identity[1],particle.identity[2],particle.positionAge[3],particle.velocityLife[3]);
  }
  const bool retired=pool.Shutdown(error)&&pool.Stats().gpuBytes==0;
  if(!exact||!retired){
   std::fprintf(stderr,"SMALL_POOL_RESULT capacity=%u ready=%u actual_live=%zu sampled_live=%llu sample_tick=%llu retired=%u operation_error=%s shutdown_error=%s\n",capacity,ready?1u:0u,state.size(),static_cast<unsigned long long>(sampled),static_cast<unsigned long long>(tick),retired?1u:0u,operationError.c_str(),error.c_str());
   for(const auto& particle:state)std::fprintf(stderr,"SMALL_POOL_PARTICLE live=%u id=%u seed=%u age=%g lifetime=%g pos=(%g,%g,%g) vel=(%g,%g,%g)\n",particle.identity[0],particle.identity[1],particle.identity[2],particle.positionAge[3],particle.velocityLife[3],particle.positionAge[0],particle.positionAge[1],particle.positionAge[2],particle.velocityLife[0],particle.velocityLife[1],particle.velocityLife[2]);
  }
  char label[128];std::snprintf(label,sizeof(label),"%u-slot resident pool admits its exact capacity and releases all allocations",capacity);
  Check(exact&&retired,label);
 }
}
struct Target {
 unsigned fbo=0,texture=0,white=0;
 bool Init(){glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,32,32,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)return false;
  glGenTextures(1,&white);glBindTexture(GL_TEXTURE_2D,white);const unsigned char pixel[4]={255,255,255,255};glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,pixel);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);return true;}
 void Clear(){glBindFramebuffer(GL_FRAMEBUFFER,fbo);glViewport(0,0,32,32);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);}
 std::array<unsigned char,4> Pixel(){std::array<unsigned char,4> value{};glReadPixels(16,16,1,1,GL_RGBA,GL_UNSIGNED_BYTE,value.data());return value;}
 ~Target(){glBindFramebuffer(GL_FRAMEBUFFER,0);if(fbo)glDeleteFramebuffers(1,&fbo);if(texture)glDeleteTextures(1,&texture);if(white)glDeleteTextures(1,&white);}
};
bool PixelNear(std::array<unsigned char,4> actual,std::array<int,4> expected){for(unsigned i=0;i<4;++i)if(std::abs(int(actual[i])-expected[i])>2)return false;return true;}
void Randomized100k(GraphicsTestHost& host){
 ParticleEmitterSettings settings;settings.backend=ParticleBackend::Gpu;settings.maxParticles=100000;settings.lifetime=10;settings.size=settings.endSize=.005f;settings.spread={.5f,.4f,.3f};settings.velocity={1,2,3};settings.velocityVariation={.1f,.2f,.3f};settings.acceleration={.1f,.2f,0};settings.seed=123;settings.useGravity=settings.useWind=true;
 GpuParticles pool;std::string error;const bool initialized=pool.Initialize(host.GraphicsCaps(),settings,{},error);Check(initialized,"100,000-slot compute pool allocates bounded resident simulation/sort/draw resources");if(!initialized){std::fprintf(stderr,"%s\n",error.c_str());return;}
 GpuParticleStepInputs input;input.spawnCount=100000;input.tick=1;input.position={-.2f,.1f,.3f};input.scale={1,2,1};input.rotation=glm::angleAxis(.2f,glm::vec3(0,0,1));input.gravityKind=GpuGravityKind::Uniform;input.gravity={0,-9.81f,0};input.wind={1,0,0};
 Check(pool.Step(settings,input,nullptr,error),"a single GPU birth batch creates 100,000 particles without CPU state upload");
 auto state=Live(pool,error);bool same=state.size()==100000;
 for(std::size_t i=0;same&&i<state.size();++i){const auto birth=SampleParticleBirth(settings,unsigned(i+1));same=state[i].identity[1]==i+1&&state[i].identity[2]==birth.state&&Near(Position(state[i]),input.position+input.rotation*(birth.position*input.scale))&&Near(Velocity(state[i]),input.rotation*birth.velocity)&&state[i].positionAge[3]==0;}
 Check(same,"all randomized GPU births match CPU seed/id/ordered-axis reference positions, velocities and RNG continuation");
 auto expected=state;input.spawnCount=0;input.dt=1.f/60;
 for(unsigned step=0;step<4;++step){input.tick=step+2;Check(pool.Step(settings,input,nullptr,error),"resident GPU fixed-step update succeeds");for(auto& particle:expected){auto velocity=Velocity(particle);velocity+=(input.rotation*settings.acceleration+input.gravity+input.wind-velocity)*input.dt;const auto position=Position(particle)+velocity*input.dt;particle.positionAge={position.x,position.y,position.z,particle.positionAge[3]+input.dt};particle.velocityLife[0]=velocity.x;particle.velocityLife[1]=velocity.y;particle.velocityLife[2]=velocity.z;}}
 state=Live(pool,error);same=state.size()==expected.size();float maximumPositionError=0,maximumVelocityError=0;
 for(std::size_t i=0;i<std::min(state.size(),expected.size());++i){maximumPositionError=std::max(maximumPositionError,glm::length(Position(state[i])-Position(expected[i])));maximumVelocityError=std::max(maximumVelocityError,glm::length(Velocity(state[i])-Velocity(expected[i])));same&=std::abs(state[i].positionAge[3]-expected[i].positionAge[3])<1e-6;}
 std::printf("100K_PARITY max_position_error=%.9g max_velocity_error=%.9g tolerance=4e-5 chart=near_origin steps=4\n",maximumPositionError,maximumVelocityError);
 same&=maximumPositionError<=4e-5f&&maximumVelocityError<=4e-5f;
 Check(same,"100,000 resident trajectories match independent CPU uniform gravity, authored acceleration and wind drag");
 Check(pool.Stats().stateTransferBytes==0&&pool.Stats().gpuBytes<=64*1024*1024,"ordinary GPU updates upload no particle state and remain inside the declared device budget; full reads are explicit test diagnostics");
 const auto prior=state;Check(pool.ShiftOrigin({2,3,4},error),"origin rebasing updates resident state through a bounded GPU dispatch");state=Live(pool,error);same=state.size()==prior.size();for(std::size_t i=0;same&&i<state.size();++i)same=Near(Position(state[i]),Position(prior[i])-glm::vec3(2,3,4))&&Velocity(state[i])==Velocity(prior[i])&&state[i].positionAge[3]==prior[i].positionAge[3];Check(same,"GPU origin shift preserves velocity, age and particle identities");
 const auto shifted=state;const glm::dvec3 fractionalDelta(.1,.2,.3);bool fractional=pool.ShiftOrigin(fractionalDelta,error);state=Live(pool,error);fractional&=state.size()==shifted.size();for(std::size_t i=0;fractional&&i<state.size();++i)fractional=Position(state[i])==glm::vec3(glm::dvec3(Position(shifted[i]))-fractionalDelta);Check(fractional,"fractional GPU origin shifts match binary64 subtraction before float publication");
 Check(pool.Shutdown(error)&&pool.Stats().gpuBytes==0,"large resident pool explicitly releases every allocation before context teardown");
}
void ViewOrdering(GraphicsTestHost& host,Target& target){
 ParticleEmitterSettings settings;settings.backend=ParticleBackend::Gpu;settings.maxParticles=16;settings.spread=settings.velocityVariation=settings.velocity=glm::vec3(0);settings.color={1,0,0,.5f};settings.endColor={0,0,1,.5f};settings.lifetime=1;settings.size=settings.endSize=1.5f;
 GpuParticles pool;std::string error;if(!pool.Initialize(host.GraphicsCaps(),settings,{},error)){Check(false,"known-colour GPU pool initializes");return;}
 GpuParticleStepInputs input;input.spawnCount=1;input.position.z=-.5f;input.tick=1;bool ready=pool.Step(settings,input,nullptr,error);input.spawnCount=0;input.dt=.25f;ready&=pool.Step(settings,input,nullptr,error);ready&=pool.Step(settings,input,nullptr,error);input.dt=0;input.spawnCount=1;input.position.z=.5f;input.tick=2;ready&=pool.Step(settings,input,nullptr,error);
 Check(ready,"two depth-separated particle generations have independently known ages and colours");
 const auto before=Live(pool,error);JudasRender::GpuParticlePass pass;target.Clear();const bool drawn=pass.Draw(pool,glm::mat4(1),glm::mat4(1),target.white,false,false,error);Check(drawn&&PixelNear(target.Pixel(),{159,0,32,96}),"resident indirect draw sorts red/aged-purple particles to the analytic alpha result");
 const auto reverse=glm::rotate(glm::mat4(1),3.14159265358979323846f,glm::vec3(0,1,0));target.Clear();Check(pass.Draw(pool,reverse,glm::mat4(1),target.white,false,false,error)&&PixelNear(target.Pixel(),{128,0,64,96}),"a second camera gets its own opposite depth ordering without rerunning simulation");
 const auto after=Live(pool,error);Check(before.size()==after.size()&&std::memcmp(before.data(),after.data(),before.size()*sizeof(before[0]))==0&&pool.Diagnostics().draws==2,"multi-camera preparation changes no physical particle state and records actual indirect draws");
 target.Clear();auto outside=glm::translate(glm::mat4(1),glm::vec3(100,0,0));Check(pass.Draw(pool,outside,glm::mat4(1),target.white,false,false,error)&&target.Pixel()==std::array<unsigned char,4>{},"GPU per-view frustum culling generates a zero-instance draw for offscreen particles");
 Check(pass.Shutdown(&error)&&pool.Shutdown(error),"view-specific GPU render resources and simulation owner shut down explicitly");GLint current=0;glGetIntegerv(GL_CURRENT_PROGRAM,&current);Check(current==0,"retiring the current resident draw program unbinds it before later compute dispatch can restore a deleted name");
}
void Collision(GraphicsTestHost& host){
 PhysicsWorld world;world.Init();const auto box=world.CreateStaticBox({0,0,0},{.5f,.5f,.5f},0,0);CollisionSnapshot snapshot;std::string error;
 Check(snapshot.Capture(world,{{-10,-10,-10},{10,10,10}},.25,error,{},[](BodyHandle){return std::uint64_t(77);}),"collision snapshot captures actual static MK2 box geometry once");
 ParticleEmitterSettings settings;settings.backend=ParticleBackend::Gpu;settings.maxParticles=1;settings.collision=true;settings.collisionRadius=.1f;settings.restitution=.5f;settings.friction=.2f;settings.spread=settings.velocityVariation=glm::vec3(0);settings.velocity={2,-4,0};settings.lifetime=10;settings.impacts=ParticleImpactDelivery::Cosmetic;
 GpuParticles pool;if(!pool.Initialize(host.GraphicsCaps(),settings,{},error)){Check(false,"GPU collision pool initializes");std::fprintf(stderr,"%s\n",error.c_str());return;}
 GpuParticleStepInputs input;input.position={0,1,0};input.spawnCount=1;input.tick=1;bool ready=pool.Step(settings,input,&snapshot,error);input.spawnCount=0;input.dt=.25f;input.tick=2;ready&=pool.Step(settings,input,&snapshot,error);
 const auto state=Live(pool,error);Check(ready&&state.size()==1&&Near(Velocity(state[0]),{.8f,2,0},1e-3f)&&Near(Position(state[0]),{.32f,.9001f,0},1e-3f),"resident sphere sweep, rebound, remaining-time motion and Coulomb friction match the independent analytic box oracle");
 Check(Sample(pool,2,error),"bounded nonblocking GPU impact/counter readbacks eventually complete");const auto impacts=pool.TakeImpacts();Check(impacts.size()==1&&impacts[0].receiver==77&&impacts[0].count==1&&std::abs(impacts[0].position.y-.5)<1e-3&&Near(impacts[0].normal,{0,1,0}),"GPU contacts deliver bounded receiver-local cosmetic impact data after asynchronous completion");
 Check(world.GetLinearVelocity(box)==glm::vec3(0)&&world.GetTransform(box).position==glm::vec3(0)&&pool.Stats().queryFailures==0,"GPU visual collision cannot mutate the authoritative MK2 body and reports no hidden query exhaustion");
 auto unsupported=settings;unsupported.impacts=ParticleImpactDelivery::Reliable;const auto before=Live(pool,error);Check(!pool.Step(unsupported,input,&snapshot,error)&&error.find("Reliable")!=std::string::npos,"reliable event mode declines before mutating existing resident state");const auto after=Live(pool,error);Check(before.size()==after.size()&&std::memcmp(before.data(),after.data(),before.size()*sizeof(before[0]))==0,"unsupported step rejection is transactional for an explicit CPU transfer");
 Check(pool.Shutdown(error),"collision GPU resources retire on their context");
}

void StaticFeatureControls(GraphicsTestHost& host){
 ParticleEmitterSettings settings;settings.backend=ParticleBackend::Gpu;settings.maxParticles=1;settings.collision=true;settings.collisionRadius=.1f;settings.restitution=1;settings.friction=0;settings.spread=settings.velocityVariation=glm::vec3(0);settings.lifetime=10;settings.impacts=ParticleImpactDelivery::Cosmetic;
 GpuParticles pool;std::string error;if(!pool.Initialize(host.GraphicsCaps(),settings,{},error)){Check(false,"static-feature GPU pool initializes");std::fprintf(stderr,"%s\n",error.c_str());return;}
 auto run=[&](const CollisionSnapshot& snapshot,glm::vec3 from,glm::vec3 velocity,bool expectedHit,const char* label){
  pool.Clear(error);settings.velocity=velocity;GpuParticleStepInputs input;input.position=from;input.spawnCount=1;input.tick=10;bool ready=pool.Step(settings,input,&snapshot,error);input.spawnCount=0;input.dt=.25f;input.tick=11;ready&=pool.Step(settings,input,&snapshot,error);
  const auto oracle=snapshot.SweepSphere(from,from+velocity*.25f,settings.collisionRadius);
  const auto state=Live(pool,error);bool correct=ready&&state.size()==1&&(oracle.status==CollisionQueryStatus::Hit)==expectedHit;
  if(correct){glm::vec3 expectedPosition=from+velocity*.25f,expectedVelocity=velocity;
   if(expectedHit){expectedVelocity=glm::vec3(CollisionResponse(velocity,oracle,1,0));expectedPosition=glm::vec3(oracle.center+oracle.normal*.0001+glm::dvec3(expectedVelocity)*(.25*(1-oracle.fraction)));}
   const auto pe=glm::length(Position(state[0])-expectedPosition),ve=glm::length(Velocity(state[0])-expectedVelocity);std::printf("STATIC_PARITY position_error=%.9g velocity_error=%.9g tolerance=1e-3 case=%s\n",pe,ve,label);correct=pe<=1e-3f&&ve<=1e-3f&&pool.Stats().queryFailures==0;
  }
  Check(correct,label);if(!correct)std::fprintf(stderr,"static feature diagnostic: %s status=%u live=%zu query=%llu\n",error.c_str(),unsigned(oracle.status),state.size(),(unsigned long long)pool.Stats().queryFailures);
 };
 {
  PhysicsWorld world;world.Init();world.CreateStaticSphere({0,0,0},.5f,0,0);CollisionSnapshot snapshot;snapshot.Capture(world,{{-20,-20,-20},{20,20,20}},.25,error);run(snapshot,{0,1,0},{0,-4,0},true,"static GPU sphere collision matches the independent native sweep and remaining-time rebound");
 }
 {
  PhysicsWorld world;world.Init();world.CreateShape(Shape::Capsule(.25f,.5f),{},false,0,0,0);CollisionSnapshot snapshot;snapshot.Capture(world,{{-20,-20,-20},{20,20,20}},.25,error);run(snapshot,{0,1.2f,0},{0,-4,0},true,"static GPU capsule collision matches the independent native finite capsule oracle");
 }
 std::shared_ptr<CollisionAsset> triangle=std::make_shared<CollisionAsset>();
 {MeshData mesh;mesh.vertices.resize(3);mesh.vertices[0].position={-2,0,-2};mesh.vertices[1].position={0,0,2};mesh.vertices[2].position={2,0,-2};mesh.indices={0,1,2};CollisionCookSettings cook;Check(CookCollision(mesh,cook,*triangle,error),"finite one-sided triangle fixture uses the actual collision cooker");}
 {
  PhysicsWorld world;world.Init();world.CreateShape(Shape::Cooked(triangle,""),{},false,0,0,0);CollisionSnapshot snapshot;snapshot.Capture(world,{{-20,-20,-20},{20,20,20}},.25,error);
  run(snapshot,{0,1,0},{0,-8,0},true,"static GPU cooked-BVH triangle front-face collision matches native timing and response");
  run(snapshot,{0,-1,0},{0,8,0},false,"one-sided GPU triangle rejects back-to-front traversal");
  run(snapshot,{0,-.05f,0},{0,1,0},false,"one-sided GPU triangle rejects a close back-side start rather than inventing front-side contact");
  run(snapshot,{3,1,0},{0,-8,0},false,"finite GPU triangle cannot become an infinite response plane beyond its edge");
  run(snapshot,{2.05f,1,-2},{0,-8,0},true,"rounded finite-vertex GPU contact matches the native sphere/triangle oracle");
 }
 {
  PhysicsWorld world;world.Init();world.CreateStaticBox({0,-.5f,0},{10,.5f,10},0,0);CollisionSnapshot snapshot;snapshot.Capture(world,{{-20,-20,-20},{20,20,20}},.25,error);
  run(snapshot,{0,.10002f,0},{4,0,0},false,"near-tangent floor motion completes without needless distance/speed exhaustion");
  world.CreateStaticBox({.75f,1,0},{.05f,1,1},0,0);snapshot.Capture(world,{{-20,-20,-20},{20,20,20}},.25,error);
  run(snapshot,{0,.10002f,0},{4,0,0},true,"a tangent nearest floor cannot hide the second wall crossed later in the sweep");
 }
 Check(pool.Shutdown(error),"static analytic and cooked-feature GPU resources retire cleanly");
}
void ExpiryAndOverflow(GraphicsTestHost& host){
 PhysicsWorld world;world.Init();world.CreateStaticBox({0,0,0},{.5f,.5f,.5f},0,0);CollisionSnapshot snapshot;std::string error;snapshot.Capture(world,{{-10,-10,-10},{10,10,10}},.25,error,{},[](BodyHandle){return std::uint64_t(91);});
 ParticleEmitterSettings settings;settings.backend=ParticleBackend::Gpu;settings.maxParticles=1;settings.collision=true;settings.collisionRadius=.1f;settings.restitution=1;settings.friction=0;settings.spread=settings.velocityVariation=glm::vec3(0);settings.velocity={0,-4,0};settings.lifetime=.2f;settings.impacts=ParticleImpactDelivery::Cosmetic;
 GpuParticles pool;bool ready=pool.Initialize(host.GraphicsCaps(),settings,{},error);GpuParticleStepInputs input;input.position={0,1,0};input.spawnCount=1;input.tick=1;ready&=pool.Step(settings,input,&snapshot,error);input.spawnCount=0;input.dt=.25f;input.tick=2;ready&=pool.Step(settings,input,&snapshot,error);
 auto state=Live(pool,error);auto impacts=pool.TakeImpacts();Check(ready&&state.empty()&&impacts.size()==1&&impacts[0].count==1&&std::abs(impacts[0].age-.1f)<1e-3f,"a particle expiring mid-step still integrates its remaining lifetime and reports its final physical contact");
 pool.Clear(error);settings.lifetime=.05f;input.dt=0;input.spawnCount=1;ready=pool.Step(settings,input,&snapshot,error);input.spawnCount=0;input.dt=.25f;input.tick=3;ready&=pool.Step(settings,input,&snapshot,error);state=Live(pool,error);impacts=pool.TakeImpacts();Check(ready&&state.empty()&&impacts.empty(),"a particle expiring before the wall does not collide using the discarded remainder of the fixed step");
 pool.Clear(error);settings.collision=false;settings.lifetime=10;settings.velocity={3.3e38f,0,0};settings.acceleration={3.3e38f,0,0};input.dt=0;input.spawnCount=1;input.tick=4;ready=pool.Step(settings,input,nullptr,error);input.spawnCount=0;input.dt=.25f;input.tick=5;ready&=pool.Step(settings,input,nullptr,error);state=Live(pool,error);Check(ready&&state.empty()&&pool.Stats().queryFailures>0&&!pool.Stats().diagnostic.empty(),"finite inputs which overflow GPU integration retire unsafe visuals and report failure instead of publishing NaN/Inf");
 Check(pool.CanMutate(error)&&pool.CanShiftOrigin({.1,.2,.3},error)&&!pool.CanShiftOrigin({std::numeric_limits<double>::infinity(),0,0},error),"control/origin preflight validates ownership and finite coordinate range without mutation");
 bool foreign=false;std::thread thread([&]{std::string reason;foreign=!pool.CanMutate(reason)&&!reason.empty();});thread.join();Check(foreign,"foreign-thread control preflight fails before changing any world or particle state");
 pool.Shutdown(error);
}


void BatchAdmission(GraphicsTestHost& host){
 ParticleEmitterSettings settings;settings.backend=ParticleBackend::Gpu;settings.maxParticles=16;settings.lifetime=1;settings.spread=settings.velocityVariation=settings.velocity=glm::vec3(0);
 GpuParticles pool;std::string error;if(!pool.Initialize(host.GraphicsCaps(),settings,{},error)){Check(false,"batch-admission pool initializes");return;}
 GpuParticleStepInputs input;input.tick=1;input.spawnCount=8;bool ready=pool.Step(settings,input,nullptr,error);
 input.spawnCount=0;input.dt=.25f;for(unsigned i=0;i<3;++i){++input.tick;ready&=pool.Step(settings,input,nullptr,error);}
 input.spawnCount=4;input.dt=0;++input.tick;ready&=pool.Step(settings,input,nullptr,error);
 input.spawnCount=16;input.dt=.25f;++input.tick;ready&=pool.Step(settings,input,nullptr,error);auto state=Live(pool,error);
 bool mixed=ready&&state.size()==16;for(std::size_t i=0;mixed&&i<state.size();++i)mixed=state[i].identity[1]==i+9&&std::abs(state[i].positionAge[3]-(i<4?.25f:0.f))<1e-6f;
 Check(mixed,"one batch reserves only free slots after mixed expiry and keeps admitted birth ids in deterministic order");
 const auto before=state;input.spawnCount=1000;input.dt=0;++input.tick;ready=pool.Step(settings,input,nullptr,error);state=Live(pool,error);
 Check(ready&&state.size()==16&&std::memcmp(before.data(),state.data(),state.size()*sizeof(state[0]))==0,"an excess request against a saturated pool changes no live particle and never overspawns");
 input.spawnCount=0;input.dt=.25f;for(unsigned i=0;i<3;++i){++input.tick;ready&=pool.Step(settings,input,nullptr,error);}
 input.spawnCount=6;input.dt=0;++input.tick;ready&=pool.Step(settings,input,nullptr,error);state=Live(pool,error);
 bool reused=ready&&state.size()==16;for(std::size_t i=0;reused&&i<state.size();++i)reused=state[i].identity[1]==(i<12?unsigned(i+13):unsigned(1029+i-12));
 if(!reused||pool.Stats().live!=16){std::printf("BATCH_REUSE ready=%u actual_live=%zu reported_live=%llu sample_tick=%llu requested_tick=%llu error=%s ids=",ready?1u:0u,state.size(),static_cast<unsigned long long>(pool.Stats().live),static_cast<unsigned long long>(pool.Diagnostics().sampleTick),static_cast<unsigned long long>(input.tick),error.c_str());for(const auto& particle:state)std::printf("%u,",particle.identity[1]);std::puts("");}
 Check(reused&&pool.Stats().live==16&&pool.Diagnostics().sampleTick==input.tick,"recycled expiry slots admit the next bounded interval and an explicit diagnostic drain publishes the latest accounting");
 pool.Clear(error);settings.spread=glm::vec3(1e20f);input.scale=glm::vec3(3.3e38f);input.spawnCount=16;input.dt=0;++input.tick;ready=pool.Step(settings,input,nullptr,error);state=Live(pool,error);
 Check(ready&&state.empty()&&pool.Stats().queryFailures==16,"numerically invalid reserved births retire explicitly without publishing unsafe live state");
 settings.spread=glm::vec3(0);input.scale=glm::vec3(1);input.spawnCount=16;++input.tick;ready=pool.Step(settings,input,nullptr,error);state=Live(pool,error);
 bool recovered=ready&&state.size()==16;for(std::size_t i=0;recovered&&i<state.size();++i)recovered=state[i].identity[1]==i+17;
 Check(recovered&&pool.Stats().live==16,"rejected-birth recycling returns every reserved slot after the parallel readers have completed");
 pool.Shutdown(error);
}

void Native(){GraphicsTestHost host;std::string error;const bool ready=host.Init("Particles V2 GPU proof",64,64,false,error);Check(ready,"native SDL/GLAD context qualifies the actual production GPU owners");if(!ready){std::fprintf(stderr,"%s\n",error.c_str());return;}if(!host.GraphicsCaps().modernTier){std::puts("SKIP native resident GPU proof: selected context is portable");return;}SmallPools(host);Randomized100k(host);{Target target;Check(target.Init(),"independent known-answer colour target initializes");ViewOrdering(host,target);}Collision(host);StaticFeatureControls(host);ExpiryAndOverflow(host);BatchAdmission(host);Check(glGetError()==GL_NO_ERROR,"resident simulation, collision and rendering finish without GL errors");host.Shutdown();}
}
int main(int argc,char** argv){if(argc>2||(argc==2&&std::string(argv[1])!="--native")){std::fprintf(stderr,"usage: GpuParticlesTests [--native]\n");return 2;}Pure();if(argc==2)Native();std::printf("GPU particles: %u checks, %u failures\n",checks,failures);return failures?1:0;}
