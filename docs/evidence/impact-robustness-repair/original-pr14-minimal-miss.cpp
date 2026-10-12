#include "CollisionGeometry.h"
#include "CollisionAsset.h"
#include "MeshData.h"
#include <iostream>
int main(){MeshData m;for(auto p:{glm::vec3(-1,0,-1),glm::vec3(1,0,-1),glm::vec3(-1,0,1)}){MeshVertex v;v.position=p;m.vertices.push_back(v);}m.indices={0,2,1};CollisionCookSettings settings;auto asset=std::make_shared<CollisionAsset>();std::string error;if(!CookCollision(m,settings,*asset,error))return 2;PrimitivePose mesh;mesh.shape=Shape::Cooked(asset,"finite triangle");PrimitivePose box;box.shape=Shape::Box({.2f,.015f,.3f});box.parentPosition={.738190f,.0436529f,.993021f};box.parentOrientation=glm::normalize(glm::quat(.997289f,-.0196676f,-.0393352f,-.0590029f));
// Every triangle point satisfies x+z<=0. Every box point satisfies x+z>0:
// the centre sum exceeds its rotation-independent bounding-sphere projection.
double analyticLowerGap=(double(box.parentPosition.x)+box.parentPosition.z)/std::sqrt(2.)-glm::length(glm::dvec3(box.shape.halfExtents));auto exact=PolyGeometry(box,mesh);auto contacts=CookedContacts(box,mesh,0);bool any=CookedHasContacts(box,mesh,0);std::cout<<"independent_separation_lower_bound="<<analyticLowerGap<<" exact_gap="<<exact.gap<<" contacts="<<contacts.count<<" existence="<<any<<"\n";return analyticLowerGap>0&&exact.gap>0&&!contacts.count&&!any?0:1;}
