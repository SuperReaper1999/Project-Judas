#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <vector>
#include "CollisionShapes.h"
#include "Classification.h"
#include "JointTypes.h"
// Cache-only passive diagnostic visibility. No production headers/API altered.
#define private public
#include "PhysicsWorld.h"
#undef private
#include "physics/WorldInternal.h"
#define main PreservedImpactMeshCostMain
#include "ImpactMeshCostTests.cpp"
#undef main

int main(){
 std::cout<<std::setprecision(17);const auto ground=Sheet(2,26,true);
 for(int i=0;i<24;++i){PhysicsWorld world;world.Init();world.CreateShape(Shape::Cooked(ground,"impact cost sheet"),{},false,1,.6f,0);
 const float along=-1.2f+.1f*float(i),grazing=float(i%6)*6.f;
 auto h=world.CreateDynamicBox({along,1.2f,-1.f},{.02f,.01f,.03f},.2f,.4f,0);world.SetLinearVelocity(h,{0,-40.f,grazing});
 int step=0;
 try {for(;step<90;++step){world.ApplyLinearAcceleration(h,{0,-9.81f,0},1.f/60.f);world.Step(1.f/60.f);}}
 catch(const std::runtime_error&error){
  auto&w=*world.m_impl;const auto slot=h.id&PhysicsWorld::Impl::kSlotMask;
  const std::string message(error.what());const auto finishPosition=message.find(" finish=");const auto finishEnd=message.find(' ',finishPosition+8);const auto printed=finishPosition==std::string::npos?std::string{}:message.substr(finishPosition+8,finishEnd-finishPosition-8);
  const auto rendered=[](double value){std::ostringstream text;text<<value;return text.str();};
  const auto&ordinary=w.impactPoseSamples[slot];const auto&anchor=w.impactAnchorSamples[slot];
  std::cout<<"CACHE_TIMES ordinary="<<ordinary.time<<" anchor="<<anchor.time<<" printed_failure_finish="<<printed<<" ordinary_matches="<<(rendered(ordinary.time)==printed)<<" anchor_matches="<<(rendered(anchor.time)==printed)<<'\n';
  const bool useAnchor=rendered(anchor.time)==printed&&rendered(ordinary.time)!=printed;
  const auto&sample=useAnchor?anchor:ordinary;const double time=sample.time;
  std::cout<<"FAILURE box="<<i<<" step="<<step<<" exact_time="<<time<<" query_anchor="<<w.impactQueryAnchorTime<<" detail="<<error.what()<<'\n';
  const auto key=std::array<unsigned,4>{0,slot,0,0};const auto&record=w.meshEpisodes.at(key);
  auto query=PrimitiveAt(w.bodies[slot].shape,sample.pose,0,&*sample.orientation),mesh=PrimitiveAt(w.bodies[0].shape,w.bodies[0].rigidBody,0);
  std::cout<<"OWNER anchor="<<record.anchorTime<<" ids=";for(auto id:record.episode.fronts)std::cout<<id<<',';std::cout<<'\n';
  MeshContactEpisode all;for(unsigned k=0;k<ground->faces.size();++k)all.fronts.push_back(k);
  glm::dvec3 nativeCenter(0);const auto r=PrimitiveRotation(query);const auto c=PrimitiveCenter(query)-PrimitiveCenter(mesh);const glm::dvec3 half(query.shape.halfExtents);
  for(int z:{-1,1})for(int y:{-1,1})for(int x:{-1,1})nativeCenter+=c+r*glm::dvec3(x*half.x,y*half.y,z*half.z);nativeCenter/=8.;

  for(const auto&kind:{std::string("geometric"),std::string("episode"),std::string("native")}){
   const auto selected=MeshContactFeatures(query,mesh,kind=="geometric"?&all:kind=="episode"?&record.episode:nullptr);
   std::cout<<"ENDPOINT "<<kind<<" features=";
   for(auto id:selected){const auto&face=ground->faces[id];const double side=glm::dot(face.normal,PrimitiveCenter(query)-ground->vertices[face.vertices[0]]);std::cout<<id<<':'<<side<<"/native="<<glm::dot(face.normal,nativeCenter-ground->vertices[face.vertices[0]])<<"[";for(auto n:face.adjacent)std::cout<<n<<',';std::cout<<"],";}std::cout<<'\n';
  }
  const auto native=CookedContacts(query,mesh,0),owned=EpisodeCookedContacts(query,mesh,0,&record.episode);
  std::cout<<"MANIFOLDS native="<<native.count<<" episode="<<owned.count<<" finite="<<MeshFiniteHasOverlap(query,mesh)<<" native_policy_gate="<<MeshEndpointUsesNativeSelection(query,mesh)<<'\n';
  for(const auto&entry:w.bodies[slot].motion.Segments()){
   const auto v=[&](glm::vec3 p){std::cout<<p.x<<','<<p.y<<','<<p.z;};
   std::cout<<"SEGMENT begin="<<entry.begin<<" end="<<entry.end<<" position=";v(entry.position);std::cout<<" rotation="<<entry.orientation.w<<','<<entry.orientation.x<<','<<entry.orientation.y<<','<<entry.orientation.z<<" linear=";v(entry.linearVelocity);std::cout<<" generator=";v(entry.driftAngularVelocity);std::cout<<'\n';
  }
  return 1;
 }
 }
 std::cout<<"NO_FAILURE\n";return 0;
}
