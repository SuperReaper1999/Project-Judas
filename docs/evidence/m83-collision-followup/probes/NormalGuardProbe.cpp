#include "../../../../src/CollisionGeometry.cpp"
#include <iostream>
#include <iomanip>
#include <glm/gtc/quaternion.hpp>
int main(){
 MeshData mesh;for(auto v:{glm::vec3(0,-1,-1),glm::vec3(0,-1,1),glm::vec3(0,1,-1)}){MeshVertex x;x.position=v;mesh.vertices.push_back(x);}mesh.indices={0,1,2};CollisionCookSettings cs;CollisionAsset asset;std::string error;if(!CookCollision(mesh,cs,asset,error)){std::cerr<<error;return 1;}auto shared=std::make_shared<CollisionAsset>(asset);PrimitivePose wall;wall.shape=Shape::Cooked(shared,"finite wall");
 for(double angle:{0.,-.002,-.05,-.1,-.4,-.6,-.8,-1.1,-1.5}){
  PrimitivePose box;box.shape=Shape::Box({.23f,.4f,.1f});box.parentPosition={-.01f,-.5f,-1.01f};box.parentOrientation=glm::angleAxis(float(angle),glm::vec3(0,1,0));
  const auto pa=poly(box),pb=triangle(wall,0,{0,0,0});const auto separation=sat(pa,pb);const auto contact=CookedContacts(box,wall,0);
  std::cout<<std::setprecision(12)<<"ANGLE "<<angle<<" SAT gap "<<separation.gap<<" normal "<<separation.normal.x<<","<<separation.normal.y<<","<<separation.normal.z<<" edge "<<separation.edge<<" dot "<<glm::dot(separation.normal,pb.normals[0])<<" contacts "<<contact.count;
  if(contact.count)std::cout<<" actual gap "<<contact.points[0].signedSeparation<<" normal "<<contact.points[0].preciseNormal.x<<","<<contact.points[0].preciseNormal.y<<","<<contact.points[0].preciseNormal.z;
  std::cout<<"\n";
 }
}
