#include "CollisionGeometry.h"
#include "CollisionAsset.h"
#include "MeshData.h"
#include <random>
#include <iostream>
int main(){MeshData m;for(auto p:{glm::vec3(-1,0,-1),glm::vec3(1,0,-1),glm::vec3(-1,0,1)}){MeshVertex v;v.position=p;m.vertices.push_back(v);}m.indices={0,2,1};CollisionCookSettings s;s.twoSided=true;auto a=std::make_shared<CollisionAsset>();std::string err;if(!CookCollision(m,s,*a,err))return 2;PrimitivePose target;target.shape=Shape::Cooked(a,"triangle");std::mt19937 g(1234);std::uniform_real_distribution<float> p(-1.5,1.5),y(-.1,.1),ang(-3.14,3.14);unsigned hits=0,falseHits=0;for(int i=0;i<50000;++i){PrimitivePose q;q.shape=Shape::Box({.2,.015,.3});q.parentPosition={p(g),y(g),p(g)};q.parentOrientation=glm::angleAxis(ang(g),glm::normalize(glm::vec3(1,2,3)));auto d=PolyGeometry(q,target);auto c=CookedContacts(q,target,0);if(c.count){++hits;if(d.valid&&d.gap>1e-7){if(falseHits++<10)std::cout<<"FALSE "<<i<<" gap="<<d.gap<<" position="<<q.parentPosition.x<<","<<q.parentPosition.y<<","<<q.parentPosition.z<<"\n";}}}std::cout<<"hits="<<hits<<" falseHits="<<falseHits<<"\n";return falseHits?1:0;}
