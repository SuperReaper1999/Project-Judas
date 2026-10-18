#include "MeshUnionContinuity.h"
#include <iostream>

std::shared_ptr<CollisionAsset> Cook(const std::vector<glm::vec3>& points,const std::vector<uint32_t>& ids){
 MeshData data;for(auto p:points){MeshVertex v;v.position=p;data.vertices.push_back(v);}data.indices=ids;
 auto asset=std::make_shared<CollisionAsset>();CollisionCookSettings settings;std::string error;
 if(!CookCollision(data,settings,*asset,error))throw std::runtime_error(error);return asset;
}
int main(){
 unsigned checks=0,failed=0;
 const auto check=[&](bool yes,const char* text){++checks;std::cout<<"CONTROL "<<text<<" passed="<<yes<<'\n';if(!yes)++failed;};
 PrimitivePose query,mesh;query.shape=Shape::Box({.05,.1,.05});
 mesh.shape=Shape::Cooked(Cook({{-1,0,-1},{0,0,1},{0,0,-1},{1,0,-1}},{0,1,2,2,1,3}),"joined");
 RigidMotionSegment segment;segment.inverseMass=1;segment.end=1./60;segment.position={-.3,-.02,0};segment.linearVelocity={36,0,0};
 MeshContactEpisode initial{{0}};
 auto result=episode_probe::ConnectedUnion(query,mesh,initial,segment,0,segment.end);
 std::cout<<"HANDOFF result="<<int(result.measure.result)<<" intervals="<<result.measure.intervals<<" witnesses="<<result.measure.pointCertificates<<" finalfeatures="<<result.selected.fronts.size()<<'\n';
 check(result.measure.result==episode_probe::Result::Connected,"chronological finite welded-edge handoff certified");
 check(result.selected.fronts==std::vector<uint32_t>{1},"departed finite face retires while actual adjacent feature owns front");
 mesh.shape=Shape::Cooked(Cook({{-.02,0,-.02},{0,0,.02},{.02,0,-.02}},{0,1,2}),"transient");
 query.shape=Shape::Box({.3,.005,.05});segment.position={.15,0,0};segment.linearVelocity={};segment.driftAngularVelocity={0,0,24000};
 const auto planes=episode_probe::Planes(query.shape);
 check(!episode_probe::Intersection(episode_probe::At(query,segment,0),mesh,0,planes).empty()&&!episode_probe::Intersection(episode_probe::At(query,segment,segment.end),mesh,0,planes).empty(),"hidden-exit control endpoints both overlap");
 result=episode_probe::ConnectedUnion(query,mesh,initial,segment,0,segment.end);
 check(result.measure.result==episode_probe::Result::Separated,"actual intermediate finite gap expires inherited side");
 check(result.selected.fronts.empty(),"return after finite gap receives no inherited front");
 result=episode_probe::ConnectedUnion(query,mesh,initial,segment,0,segment.end,0);
 check(result.measure.result==episode_probe::Result::Uncertain,"zero certificate budget explicitly uncertified");
 std::cout<<"checks="<<checks<<" failures="<<failed<<'\n';return failed?1:0;
}
