#include "MeshUnionContinuity.h"
#include <iostream>
#include <iomanip>
std::shared_ptr<CollisionAsset> Cook(const std::vector<glm::vec3>& points,const std::vector<uint32_t>&ids){
 MeshData m;for(auto p:points){MeshVertex v;v.position=p;m.vertices.push_back(v);}m.indices=ids;
 auto a=std::make_shared<CollisionAsset>();CollisionCookSettings settings;std::string error;if(!CookCollision(m,settings,*a,error))throw std::runtime_error(error);return a;
}
int main(){
 unsigned checks=0,failed=0;std::cout<<std::setprecision(17);
 const auto check=[&](bool yes,const char*message){++checks;if(!yes)++failed;std::cout<<"CONTROL "<<message<<" passed="<<yes<<'\n';};
 PrimitivePose query,mesh;query.shape=Shape::Box({.05,.1,.05});mesh.shape=Shape::Cooked(Cook({{-1,0,-1},{0,0,1},{0,0,-1},{1,0,-1}},{0,1,2,2,1,3}),"joined");
 RigidMotionSegment segment;segment.inverseMass=1;segment.end=1./60;segment.position={-.3,-.02,0};segment.linearVelocity={36,0,0};MeshContactEpisode old{{0}};
 auto traced=episode_probe::TraceEndpoint(query,mesh,1,old,segment,0,segment.end);
 std::cout<<"WELDED result="<<int(traced.measure.result)<<" intervals="<<traced.measure.intervals<<" certificates="<<traced.measure.pointCertificates<<'\n';
 check(traced.measure.result==episode_probe::Result::Connected,"target ancestry crosses actual welded finite edge");
 check(traced.selected.fronts==std::vector<uint32_t>{1},"old finite face retires without losing actual adjacent endpoint");
 // Endpoint body stays behind the plane. It cannot acquire a new front episode
 // from an unrelated ordinary-front interval or native epsilon band.
 query.shape=Shape::Box({.3,.005,.05});segment.position={.15,-.002,0};segment.linearVelocity={};segment.driftAngularVelocity={0,0,24000};
 mesh.shape=Shape::Cooked(Cook({{-.02,0,-.02},{0,0,.02},{.02,0,-.02}},{0,1,2}),"hidden finite exit");
 check(MeshFiniteHasOverlap(episode_probe::At(query,segment,0),mesh)&&MeshFiniteHasOverlap(episode_probe::At(query,segment,segment.end),mesh),"behind-centre high-spin endpoints both genuinely overlap");
 traced=episode_probe::TraceEndpoint(query,mesh,0,old,segment,0,segment.end);
 std::cout<<"HIGHSPIN result="<<int(traced.measure.result)<<" intervals="<<traced.measure.intervals<<'\n';
 check(traced.measure.result==episode_probe::Result::Separated,"intermediate actual finite gap expires target ancestry");
 check(traced.selected.fronts.empty(),"no behind-centre ownership is invented after re-entry");
 // A tiny second triangle around the rotation centre remains inside the box.
 // It is topologically disconnected from the first triangle: persistent
 // whole-pair contact must never certify the first triangle's side history.
 mesh.shape=Shape::Cooked(Cook({{-.02,0,-.02},{0,0,.02},{.02,0,-.02},{.149,0,-.001},{.15,0,.001},{.151,0,-.001}},{0,1,2,3,4,5}),"disconnected persistent patch");
 const auto endpoint=episode_probe::At(query,segment,segment.end);
 check(MeshFiniteComponent(endpoint,mesh,{0})==std::vector<uint32_t>{0},"actual finite topology keeps disconnected endpoint patch isolated");
 const auto unrelated=episode_probe::TraceEndpoint(query,mesh,0,MeshContactEpisode{{1}},segment,0,segment.end);
 check(unrelated.measure.result!=episode_probe::Result::Connected,"unrelated persistent patch cannot supply target ancestry");
 const auto oldForward=episode_probe::ConnectedUnion(query,mesh,MeshContactEpisode{{0,1}},segment,0,segment.end);
 const auto targeted=episode_probe::TraceEndpoint(query,mesh,0,MeshContactEpisode{{0,1}},segment,0,segment.end);
 std::cout<<"DISCONNECTED old_forward="<<int(oldForward.measure.result)<<" old_intervals="<<oldForward.measure.intervals<<" target="<<int(targeted.measure.result)<<" target_intervals="<<targeted.measure.intervals<<'\n';
 check(targeted.measure.result!=episode_probe::Result::Connected,"whole-pair persistence cannot hide transient target separation");
 std::cout<<"checks="<<checks<<" failures="<<failed<<'\n';return failed?1:0;
}
