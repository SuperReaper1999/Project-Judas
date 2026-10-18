#include "MeshUnionContinuity.h"
#include <iostream>
#include <iomanip>
std::shared_ptr<CollisionAsset> Sheet(){
 const int cellsX=2,cellsZ=26;MeshData m;const float half=1.5f,radius=1.6f;
 for(int j=0;j<=cellsZ;++j)for(int i=0;i<=cellsX;++i){MeshVertex v;const float x=-half+2*half*float(i)/float(cellsX),z=-half+2*half*float(j)/float(cellsZ);v.position={x,radius-std::sqrt(radius*radius-z*z),z};m.vertices.push_back(v);}
 const auto at=[&](int i,int j){return std::uint32_t(j*(cellsX+1)+i);};
 for(int j=0;j<cellsZ;++j)for(int i=0;i<cellsX;++i)m.indices.insert(m.indices.end(),{at(i,j),at(i,j+1),at(i+1,j+1),at(i,j),at(i+1,j+1),at(i+1,j)});
 CollisionCookSettings settings;auto asset=std::make_shared<CollisionAsset>();std::string error;if(!CookCollision(m,settings,*asset,error))throw std::runtime_error(error);return asset;
}
double NativeSide(const PrimitivePose&query,const PrimitivePose&mesh,unsigned feature){
 glm::dvec3 center(0);const auto r=PrimitiveRotation(query);const auto c=PrimitiveCenter(query)-PrimitiveCenter(mesh);const glm::dvec3 h(query.shape.halfExtents);
 for(int z:{-1,1})for(int y:{-1,1})for(int x:{-1,1})center+=c+r*glm::dvec3(x*h.x,y*h.y,z*h.z);center/=8.;
 const auto fixed=PrimitiveRotation(mesh);const auto&face=mesh.shape.asset->faces[feature];return glm::dot(fixed*face.normal,center-fixed*mesh.shape.asset->vertices[face.vertices[0]]);
}
int main(){
 std::cout<<std::setprecision(17);unsigned checks=0,failed=0;
 const auto check=[&](bool yes,const char*message){++checks;if(!yes)++failed;std::cout<<"CONTROL "<<message<<" passed="<<yes<<'\n';};
 PrimitivePose query,mesh;query.shape=Shape::Box({.02f,.01f,.03f});mesh.shape=Shape::Cooked(Sheet(),"exact originalbox5step6 capture");
 RigidMotionSegment segment;segment.inverseMass=1;segment.begin=0.010362875706079655;segment.end=0.01666666753590107;
 segment.position={-0.72217696905136108,0.22075329720973969,0.80937790870666504};segment.orientation={0.77646172046661377,-0.1877262145280838,0.5829806923866272,-0.14832216501235962};
 segment.linearVelocity={-0.24034340679645538,5.0112495422363281,8.5181331634521484};segment.driftAngularVelocity={-20.477052688598633,6.0451889038085938,-9.4317626953125};
 const double target=0.013023191524169427;MeshContactEpisode old{{76,81}};
 const auto original=episode_probe::ConnectedUnion(query,mesh,old,segment,segment.begin,target);
 std::cout<<"OLD_ANCHOR result="<<int(original.measure.result)<<" intervals="<<original.measure.intervals<<" certificates="<<original.measure.pointCertificates<<'\n';
 check(original.measure.result==episode_probe::Result::Uncertain,"preserved original causal interval remains uncertified");
 const auto at=[&](double t){return episode_probe::At(query,segment,t);};
 double lo=segment.begin,hi=target;std::cout<<"SIDES start="<<NativeSide(at(lo),mesh,81)<<" end="<<NativeSide(at(hi),mesh,81)<<'\n';
 check(NativeSide(at(lo),mesh,81)>=0&&NativeSide(at(hi),mesh,81)<0,"actual targetface crosses ordinary front reference");
 for(unsigned i=0;i<32;++i){const auto mid=(lo+hi)*.5;if(NativeSide(at(mid),mesh,81)>=0)lo=mid;else hi=mid;}
 auto native=MeshContactFeatures(at(lo),mesh,nullptr);native.erase(std::remove_if(native.begin(),native.end(),[&](unsigned f){return NativeSide(at(lo),mesh,f)<0;}),native.end());
 std::cout<<"NATIVE_ACQUISITION time="<<lo<<" side="<<NativeSide(at(lo),mesh,81)<<" features=";for(auto f:native)std::cout<<f<<',';std::cout<<'\n';
 check(std::binary_search(native.begin(),native.end(),81),"later ordinary finite front witness really exists");
 MeshContactEpisode acquisition{native};const auto continued=episode_probe::ConnectedUnion(query,mesh,acquisition,segment,lo,target);
 std::cout<<"NEW_ANCHOR result="<<int(continued.measure.result)<<" intervals="<<continued.measure.intervals<<" certificates="<<continued.measure.pointCertificates<<" endfeatures=";for(auto f:continued.selected.fronts)std::cout<<f<<',';std::cout<<'\n';
 check(continued.measure.result==episode_probe::Result::Connected,"later independent acquisition has certified finite continuation");
 const auto expected=MeshContactFeatures(at(target),mesh,&old);bool complete=true;for(auto f:expected)if(NativeSide(at(target),mesh,f)<0&&!std::binary_search(continued.selected.fronts.begin(),continued.selected.fronts.end(),f))complete=false;
 check(complete,"every endpoint history-sensitive feature is covered");
 const auto nativeEnd=CookedContacts(at(target),mesh,0),selectedEnd=EpisodeCookedContacts(at(target),mesh,0,&continued.selected);
 check(nativeEnd.count==0&&selectedEnd.count>0,"genuine behind-center response retained using new acquisition");
 std::cout<<"checks="<<checks<<" failures="<<failed<<'\n';return failed?1:0;
}
