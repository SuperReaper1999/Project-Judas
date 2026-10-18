#define main BasicGeometryControls
#include "EpisodeGeometryProbe.cpp"
#undef main
#include <iomanip>
#include <cstring>
uint64_t digest=1469598103934665603ull;template<class T>void Mix(const T& value){const auto* p=reinterpret_cast<const unsigned char*>(&value);for(std::size_t i=0;i<sizeof(value);++i){digest^=p[i];digest*=1099511628211ull;}}
int main(){BasicGeometryControls();auto asset=Cook({{-1,0,-1},{0,0,1},{0,0,-1},{1,.3,-1}},{0,1,2,2,1,3});unsigned states=0;
 for(auto frame:{glm::quat(1,0,0,0),glm::angleAxis(.79f,glm::normalize(glm::vec3(1,2,-3)))})for(unsigned owned=0;owned<4;++owned)for(int x=-6;x<=6;++x)for(int y=-3;y<=3;++y)for(int z=-3;z<=3;++z)for(float angle:{0.f,.5f,1.7f}){
  PrimitivePose query{},mesh{};query.shape=Shape::Box({.1,.06,.15});mesh.shape=Shape::Cooked(asset,"joined");mesh.parentOrientation=frame;query.parentOrientation=frame*glm::angleAxis(angle,glm::normalize(glm::vec3(2,1,3)));query.parentPosition=frame*glm::vec3(x*.2,y*.04,z*.3);MeshContactEpisode episode;if(owned&1)episode.fronts.push_back(0);if(owned&2)episode.fronts.push_back(1);
  const auto features=MeshContactFeatures(query,mesh,&episode);const auto full=EpisodeCookedContacts(query,mesh,0,&episode);const bool truth=MeshEpisodeHasContacts(query,mesh,&episode);Check(truth==(full.count>0),"represented actual Boolean/full response parity");Mix(uint32_t(features.size()));for(auto f:features)Mix(f);Mix(full.count);Mix(truth);for(int i=0;i<full.count;++i){const auto& c=full.points[i];Mix(c.point.x);Mix(c.point.y);Mix(c.point.z);Mix(c.normal.x);Mix(c.normal.y);Mix(c.normal.z);Mix(c.signedSeparation);Mix(c.preciseNormal.x);Mix(c.preciseNormal.y);Mix(c.preciseNormal.z);Mix(c.localAnchorA.x);Mix(c.localAnchorA.y);Mix(c.localAnchorA.z);Mix(c.localAnchorB.x);Mix(c.localAnchorB.y);Mix(c.localAnchorB.z);}++states;
 }
 std::cout<<"states "<<states<<" digest "<<std::hex<<digest<<std::dec<<" checks "<<checks<<" failures "<<failed<<'\n';return failed?1:0;}
