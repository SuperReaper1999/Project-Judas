#define main PreservedImpactMeshCostMain
#include "ImpactMeshCostTests.cpp"
#undef main
#include "MeshUnionContinuity.h"
int main(){
 std::cout<<std::setprecision(17);
 const auto ground=Sheet(2,26,true);PhysicsWorld world;world.Init();
 world.CreateShape(Shape::Cooked(ground,"impact cost sheet"),{},false,1,.6f,0);
 const int i=5;const float along=-1.2f+.1f*float(i),grazing=float(i%6)*6.f;
 auto h=world.CreateDynamicBox({along,1.2f,-1.f},{.02f,.01f,.03f},.2f,.4f,0);
 world.SetLinearVelocity(h,{0,-40.f,grazing});
 try {for(int step=0;step<90;++step){world.ApplyLinearAcceleration(h,{0,-9.81f,0},1.f/60.f);world.Step(1.f/60.f);}}
 catch(const std::runtime_error& error){
  std::cout<<"FAILURE "<<error.what()<<'\n';
  unsigned n=0;for(const auto& seg:world.GetBodyMotionSegments(h)){
   const auto v=[&](glm::vec3 x){std::cout<<x.x<<','<<x.y<<','<<x.z;};
   std::cout<<"SEGMENT "<<n++<<" begin="<<seg.begin<<" end="<<seg.end<<" position=";v(seg.start.position);
   std::cout<<" rotation="<<seg.start.rotation.w<<','<<seg.start.rotation.x<<','<<seg.start.rotation.y<<','<<seg.start.rotation.z;
   std::cout<<" linear=";v(seg.linearVelocity);std::cout<<" generator=";v(seg.angularVelocity);std::cout<<'\n';
   PrimitivePose query,mesh;query.shape=Shape::Box({.02f,.01f,.03f});query.parentPosition=seg.start.position;query.parentOrientation=seg.start.rotation;mesh.shape=Shape::Cooked(ground,"captured");
   MeshContactEpisode all;for(unsigned k=0;k<ground->faces.size();++k)all.fronts.push_back(k);
   const auto actual=MeshContactFeatures(query,mesh,&all);std::cout<<"GEOMETRIC_FEATURES";for(auto f:actual)std::cout<<' '<<f;std::cout<<'\n';
  }
  return 1;
 }
 std::cout<<"NO_FAILURE\n";return 0;
}
