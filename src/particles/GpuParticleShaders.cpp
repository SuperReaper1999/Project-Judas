#include "GpuParticleShaders.h"
#include "GpuParticleCollision.h"

namespace JudasParticles::GpuDetail {
namespace {
const char* common=R"GLSL(#version 450 core
struct Particle {vec4 positionAge;vec4 velocityLife;uvec4 identity;};
struct Parameters {
 vec4 positionDt;vec4 rotation;vec4 scale;vec4 spreadLife;vec4 velocitySize;vec4 variationEndSize;vec4 accelerationRadius;
 vec4 gravityMagnitude;vec4 gravityCenter;vec4 windRestitution;vec4 color;vec4 endColor;vec4 response;vec4 domainMinimum;vec4 domainMaximum;
 uvec4 control;uvec4 modes;uvec4 query;uvec4 history;
};
struct Impact {vec4 pointAge;vec4 normalSize;vec4 velocityWeight;uvec4 identity;};
layout(std430,binding=0) buffer Particles {Particle particles[];};
layout(std430,binding=1) readonly buffer Settings {Parameters params;};
layout(std430,binding=2) buffer Counters {uint freeCount;uint liveCount;uint contactLo;uint contactHi;uint dropLo;uint dropHi;uint spawnDropped;uint queryFailures;uint simulatedLo;uint simulatedHi;uint birthStart;uint birthAdmitted;uint birthInvalid;};
layout(std430,binding=3) buffer FreeSlots {uint freeSlots[];};
layout(std430,binding=7) buffer Impacts {uvec4 eventHeader;uvec4 eventExtra;uvec4 eventTotals;Impact impacts[];};
void addDropped(uint count){uint old=atomicAdd(dropLo,count);if(old>0xffffffffu-count)atomicAdd(dropHi,1u);}
void retire(uint slot){particles[slot].identity.x=0u;uint free=atomicAdd(freeCount,1u);freeSlots[free]=slot;atomicAdd(liveCount,0xffffffffu);}
)GLSL";
const char* viewCommon=R"GLSL(#version 450 core
struct Particle {vec4 positionAge;vec4 velocityLife;uvec4 identity;};
struct Key {float depth;uint id;uint slot;uint visible;};
struct ViewParameters {mat4 view;mat4 projection;vec4 planes[6];vec4 sizeLife;vec4 color;vec4 endColor;uvec4 control;};
struct Record {vec4 centerSize;vec4 color;};
layout(std430,binding=0) readonly buffer Particles {Particle particles[];};
layout(std430,binding=1) buffer Keys {Key keys[];};
layout(std430,binding=2) readonly buffer ViewSettings {ViewParameters params;};
layout(std430,binding=3) buffer Commands {uint vertices;uint instances;uint first;uint baseInstance;};
layout(std430,binding=4) writeonly buffer Records {Record records[];};
bool lessKey(Key a,Key b){if(a.visible!=b.visible)return a.visible>b.visible;if(a.depth!=b.depth)return a.depth<b.depth;if(a.id!=b.id)return a.id<b.id;return a.slot<b.slot;}
)GLSL";
}
std::string ClearShader(){return std::string(common)+R"GLSL(
layout(local_size_x=256)in;
void main(){uint index=gl_GlobalInvocationID.x;if(index<params.control.x){particles[index].positionAge=vec4(0);particles[index].velocityLife=vec4(0);particles[index].identity=uvec4(0);freeSlots[index]=params.control.x-1u-index;}
 if(index==0u){freeCount=params.control.x;liveCount=contactLo=contactHi=dropLo=dropHi=spawnDropped=queryFailures=simulatedLo=simulatedHi=birthStart=birthAdmitted=birthInvalid=0u;eventHeader=uvec4(0);eventExtra=uvec4(0);eventTotals=uvec4(0);}}
)GLSL";}
std::string ShiftShader(){return std::string(common)+R"GLSL(
layout(local_size_x=256)in;
void main(){uint index=gl_GlobalInvocationID.x;if(index<params.control.x&&particles[index].identity.x!=0u)particles[index].positionAge.xyz=vec3(dvec3(particles[index].positionAge.xyz)-dvec3(packDouble2x32(params.history.xy),packDouble2x32(params.history.zw),packDouble2x32(params.query.xy)));}
)GLSL";}
std::string ResetShader(){return std::string(common)+R"GLSL(
layout(local_size_x=1)in;
void main(){if(params.history.z!=0u)addDropped(eventHeader.w);eventHeader=uvec4(0);eventExtra=uvec4(0);eventTotals=uvec4(0);}
)GLSL";}
std::string FinalizeShader(){return std::string(common)+R"GLSL(
layout(local_size_x=1)in;
layout(std430,binding=4) buffer RejectedBirths {uint rejectedBirthSlots[];};
uniform uvec2 uParameters;
void main(){
 if(uParameters.x==0u){
  // One reservation after deaths are published. No contended retry loop:
  // the following dispatch owns this disjoint interval of the free stack.
  birthAdmitted=min(params.control.y,freeCount);birthStart=freeCount-birthAdmitted;birthInvalid=0u;
  freeCount=birthStart;liveCount+=birthAdmitted;spawnDropped+=params.control.y-birthAdmitted;return;
 }
 // Only rejected numerical births take this exceptional serial recycle path.
 // The normal valid-birth path performs no scan of the admitted population.
 for(uint i=0u;i<birthInvalid;++i)freeSlots[freeCount+i]=rejectedBirthSlots[i];
 freeCount+=birthInvalid;liveCount-=birthInvalid;
 eventHeader=uvec4(liveCount,contactLo,contactHi,min(eventHeader.w,params.modes.w));eventExtra=uvec4(dropLo,dropHi,queryFailures,spawnDropped);eventTotals=uvec4(simulatedLo,simulatedHi,0u,0u);
}
)GLSL";}
std::string SpawnShader(){return std::string(common)+R"GLSL(
layout(local_size_x=256)in;
layout(std430,binding=4) buffer RejectedBirths {uint rejectedBirthSlots[];};
uint hash(uint state){state^=state>>16;state*=0x7feb352du;state^=state>>15;state*=0x846ca68bu;state^=state>>16;return state;}
float random(inout uint seed){seed=seed*1664525u+1013904223u;return float(seed>>8)*(1.0/16777216.0);}
vec3 variation(inout uint seed,vec3 scale){float x=random(seed);float y=random(seed);float z=random(seed);return (vec3(x,y,z)*2.0-1.0)*scale;}
vec3 rotate(vec4 q,vec3 p){return p+2.0*cross(q.xyz,cross(q.xyz,p)+q.w*p);}
void main(){uint index=gl_GlobalInvocationID.x;if(index>=birthAdmitted)return;
 uint slot=freeSlots[birthStart+birthAdmitted-1u-index];
 uint id=params.control.z+index,seed=hash(params.control.w^id);vec3 position=variation(seed,params.spreadLife.xyz)*params.scale.xyz;
 vec3 velocity=params.velocitySize.xyz+variation(seed,params.variationEndSize.xyz);
 position=params.positionDt.xyz+rotate(params.rotation,position);velocity=rotate(params.rotation,velocity);
 if(any(isnan(position))||any(isinf(position))||any(isnan(velocity))||any(isinf(velocity))){uint rejected=atomicAdd(birthInvalid,1u);rejectedBirthSlots[rejected]=slot;particles[slot].identity=uvec4(0u,id,seed,0u);atomicAdd(queryFailures,1u);atomicAdd(spawnDropped,1u);return;}
 particles[slot].positionAge=vec4(position,0);
 particles[slot].velocityLife=vec4(velocity,params.spreadLife.w);
 particles[slot].identity=uvec4(1u,id,seed,0u);
}
)GLSL";}
std::string SimulationShader(bool collision){
 std::string source=std::string(common)+(collision?CollisionShaderSource():"");
 source+=collision?"\n#define COLLISION 1\nlayout(local_size_x=128)in;\n":"\n#define COLLISION 0\nlayout(local_size_x=256)in;\n";
 source+=R"GLSL(
void recordImpact(Particle particle,vec3 point,vec3 normal,uint collider,uint feature){
 uint previous=atomicAdd(contactLo,1u);if(previous==0xffffffffu)atomicAdd(contactHi,1u);
 if(params.modes.w==0u)return;
 uint index=atomicAdd(eventHeader.w,1u);if(index>=params.modes.w){addDropped(1u);return;}
 Impact impact;impact.pointAge=vec4(point,particle.positionAge.w);impact.normalSize=vec4(normal,mix(params.velocitySize.w,params.variationEndSize.w,clamp(particle.positionAge.w/params.spreadLife.w,0.0,1.0)));
 impact.velocityWeight=vec4(particle.velocityLife.xyz,params.response.y);impact.identity=uvec4(collider,particle.identity.y,feature,0u);impacts[index]=impact;
}
shared uint groupSimulated;
void simulate(uint slot){if(slot>=params.control.x)return;Particle particle=particles[slot];if(particle.identity.x==0u)return;atomicAdd(groupSimulated,1u);
 if(params.history.w!=0u)particle.identity.w=0u;
 float oldAge=particle.positionAge.w;float activeDt=max(0.0,min(params.positionDt.w,params.spreadLife.w-oldAge));bool expires=oldAge+params.positionDt.w>=params.spreadLife.w;particle.positionAge.w=oldAge+activeDt;if(activeDt<=0.0&&expires){retire(slot);return;}
 if(particle.identity.w!=0u){if(expires){retire(slot);return;}particles[slot]=particle;return;}
 vec3 acceleration=params.accelerationRadius.xyz;
 if(params.modes.x==1u)acceleration+=params.gravityMagnitude.xyz;
 else if(params.modes.x==2u){vec3 delta=params.gravityCenter.xyz-particle.positionAge.xyz;float distance=length(delta);if(distance>1e-6)acceleration+=delta/distance*params.gravityMagnitude.w;}
 acceleration+=params.windRestitution.xyz-particle.velocityLife.xyz*params.gravityCenter.w;
 particle.velocityLife.xyz+=acceleration*activeDt;
 if(any(isnan(particle.velocityLife))||any(isinf(particle.velocityLife))){atomicAdd(queryFailures,1u);retire(slot);return;}
#if COLLISION
 float remaining=activeDt;uint contacts=0u;
 while(remaining>1e-9&&contacts<params.modes.z){vec3 from=particle.positionAge.xyz,to=from+particle.velocityLife.xyz*remaining;float fraction;Nearest hit;
  bool collided=sweepScene(from,to,fraction,hit);if(hit.failed){atomicAdd(queryFailures,1u);remaining=0;break;}
  if(!collided){particle.positionAge.xyz=to;remaining=0;break;}
  particle.positionAge.xyz=mix(from,to,fraction);Particle contactParticle=particle;contactParticle.positionAge.w=oldAge+activeDt-remaining+remaining*fraction;recordImpact(contactParticle,hit.point,hit.normal,hit.collider,hit.feature);++contacts;
  if(params.modes.y==3u){retire(slot);return;}
  particle.positionAge.xyz+=hit.normal*(max(0.0,-hit.gap)+.0001);
  if(params.modes.y==2u){particle.velocityLife.xyz=vec3(0);particle.identity.w=1u;remaining=0;break;}
  float normalSpeed=dot(particle.velocityLife.xyz,hit.normal);
  if(normalSpeed<0.0){float impulse=-(1.0+(params.modes.y==1u?0.0:params.windRestitution.w))*normalSpeed;particle.velocityLife.xyz+=hit.normal*impulse;vec3 tangent=particle.velocityLife.xyz-hit.normal*dot(particle.velocityLife.xyz,hit.normal);float speed=length(tangent);if(speed>0.0)particle.velocityLife.xyz-=tangent*min(1.0,params.response.x*impulse/speed);}
  remaining*=1.0-fraction;
 }
 if(remaining>1e-9)atomicAdd(queryFailures,1u);
#else
 particle.positionAge.xyz+=particle.velocityLife.xyz*activeDt;
#endif
 if(any(isnan(particle.positionAge))||any(isinf(particle.positionAge))||any(isnan(particle.velocityLife))||any(isinf(particle.velocityLife))){atomicAdd(queryFailures,1u);retire(slot);return;}
 if(expires){retire(slot);return;}particles[slot]=particle;
}
void main(){if(gl_LocalInvocationID.x==0u)groupSimulated=0u;barrier();simulate(gl_GlobalInvocationID.x);barrier();if(gl_LocalInvocationID.x==0u){uint old=atomicAdd(simulatedLo,groupSimulated);if(old>0xffffffffu-groupSimulated)atomicAdd(simulatedHi,1u);}}
)GLSL";return source;
}
std::string ViewKeysShader(){return std::string(viewCommon)+R"GLSL(
layout(local_size_x=256)in;
uniform uvec2 uParameters;
void main(){uint index=gl_GlobalInvocationID.x;if(uParameters.x!=0u){if(index==0u){vertices=6u;instances=0u;first=0u;baseInstance=0u;}return;}
 if(index>=params.control.y)return;Key key;key.depth=0;key.id=0xffffffffu;key.slot=index;key.visible=0u;
 if(index<params.control.x){Particle particle=particles[index];if(particle.identity.x!=0u){
  float size=mix(params.sizeLife.x,params.sizeLife.y,clamp(particle.positionAge.w/params.sizeLife.z,0.0,1.0));bool visible=true;
  for(uint plane=0u;plane<6u;++plane)if(dot(params.planes[plane],vec4(particle.positionAge.xyz,1))< -size*.7071068)visible=false;
  key.depth=(params.view*vec4(particle.positionAge.xyz,1)).z;key.id=particle.identity.y;
  if(visible){key.visible=1u;atomicAdd(instances,1u);}
 }}keys[index]=key;
}
)GLSL";}
std::string SortLocalShader(){return std::string(viewCommon)+R"GLSL(
layout(local_size_x=256)in;
uniform uvec2 uParameters;
shared Key tile[256];
void main(){uint global=gl_GlobalInvocationID.x,local=gl_LocalInvocationID.x;tile[local]=keys[global];barrier();
 if(uParameters.x==0u){for(uint level=2u;level<=256u;level<<=1u)for(uint pass=level>>1u;pass>0u;pass>>=1u){uint partner=local^pass;if(local<partner){Key a=tile[local],b=tile[partner];bool ascending=(global&level)==0u;if(lessKey(b,a)==ascending){tile[local]=b;tile[partner]=a;}}barrier();}}
 else{for(uint pass=128u;pass>0u;pass>>=1u){uint partner=local^pass;if(local<partner){Key a=tile[local],b=tile[partner];bool ascending=(global&uParameters.x)==0u;if(lessKey(b,a)==ascending){tile[local]=b;tile[partner]=a;}}barrier();}}
 keys[global]=tile[local];
}
)GLSL";}
std::string SortMergeShader(){return std::string(viewCommon)+R"GLSL(
layout(local_size_x=256)in;
uniform uvec2 uParameters;
void main(){uint index=gl_GlobalInvocationID.x,partner=index^uParameters.y;if(index<partner){Key a=keys[index],b=keys[partner];bool ascending=(index&uParameters.x)==0u;if(lessKey(b,a)==ascending){keys[index]=b;keys[partner]=a;}}}
)GLSL";}
std::string GatherShader(){return std::string(viewCommon)+R"GLSL(
layout(local_size_x=256)in;
void main(){uint index=gl_GlobalInvocationID.x;if(index>=params.control.y)return;Key key=keys[index];if(key.visible==0u)return;
 Particle particle=particles[key.slot];float age=clamp(particle.positionAge.w/params.sizeLife.z,0.0,1.0);uint destination=params.control.z!=0u?index:atomicAdd(instances,1u);
 records[destination].centerSize=vec4(particle.positionAge.xyz,mix(params.sizeLife.x,params.sizeLife.y,age));records[destination].color=mix(params.color,params.endColor,age);
}
)GLSL";}
} // namespace JudasParticles::GpuDetail
