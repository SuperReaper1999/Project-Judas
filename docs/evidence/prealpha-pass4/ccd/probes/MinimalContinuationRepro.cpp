// Independent reduction of the unchanged curved mesh fixture. Source rows,
// body volume, launch, gravity, fixed step and 5 cm acceptance remain unchanged.
#include <cstring>
#define main PreservedImpactMeshCostMain
#include "ImpactMeshCostTests.cpp"
#undef main
int main(){
 std::cout<<std::setprecision(17);
 const auto full=Sheet(2,26,true);MeshData clipped;
 for(const auto v:full->vertices){MeshVertex vertex;vertex.position=glm::vec3(v);clipped.vertices.push_back(vertex);}
 for(const auto& face:full->faces){bool selected=true;for(const auto index:face.vertices)selected&=full->vertices[index].z>=-1.04&&full->vertices[index].z<=-.57;if(selected)clipped.indices.insert(clipped.indices.end(),face.vertices.begin(),face.vertices.end());}
 auto reduced=std::make_shared<CollisionAsset>();CollisionCookSettings settings;std::string error;
 if(!CookCollision(clipped,settings,*reduced,error))throw std::runtime_error(error);
 const SheetOracle oracle(*full);std::vector<BodyTransform> reference;bool anyCrossed=false;
 for(const auto& ground:{full,reduced}){
  PhysicsWorld world;world.Init();world.CreateShape(Shape::Cooked(ground,"front continuation witness"),{},false,1,.6f,0);
  auto box=world.CreateDynamicBox({-1.2f,1.2f,-1.f},{.02f,.01f,.03f},.2f,.4f,0);world.SetLinearVelocity(box,{0,-40,0});
  bool crossed=false;
  for(int step=0;step<3;++step){world.ApplyLinearAcceleration(box,{0,-9.81f,0},1.f/60.f);world.Step(1.f/60.f);auto pose=world.GetTransform(box);const auto v=world.GetLinearVelocity(box);const auto stats=world.LastStepStats();
   bool same=true;if(ground==full)reference.push_back(pose);else same=std::memcmp(&pose.position,&reference[step].position,sizeof(pose.position))==0&&std::memcmp(&pose.rotation,&reference[step].rotation,sizeof(pose.rotation))==0;
   // This endpoint remains inside the actual retained triangle strip. The
   // original graph oracle therefore measures the same physical surface.
   if(pose.position.z< -1.038462f||pose.position.z>-.576922f)throw std::runtime_error("endpoint escaped retained physical footprint");
   const auto depth=oracle.Depth(pose.position);crossed|=depth>.05;
   std::cout<<"MINIMAL faces="<<ground->faces.size()<<" step="<<step<<" position="<<pose.position.x<<","<<pose.position.y<<","<<pose.position.z<<" velocity="<<v.x<<","<<v.y<<","<<v.z<<" depth="<<depth<<" queries="<<stats.impactQueries<<" events="<<stats.impactEvents<<" identical_full_pose="<<same<<"\n";
  }
  std::cout<<"MINIMAL_RESULT faces="<<ground->faces.size()<<" crossed="<<crossed<<"\n";anyCrossed|=crossed;
 }
 return anyCrossed?1:0;
}
