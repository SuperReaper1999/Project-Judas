#include "NativeSATContinuity.h"
#include <iostream>
#include <iomanip>
std::shared_ptr<CollisionAsset> Sheet(){
 const int cellsX=2,cellsZ=26;MeshData m;const float half=1.5f,radius=1.6f;
 for(int j=0;j<=cellsZ;++j)for(int i=0;i<=cellsX;++i){MeshVertex v;const float x=-half+2*half*float(i)/float(cellsX),z=-half+2*half*float(j)/float(cellsZ);v.position={x,radius-std::sqrt(radius*radius-z*z),z};m.vertices.push_back(v);}
 const auto at=[&](int i,int j){return std::uint32_t(j*(cellsX+1)+i);};
 for(int j=0;j<cellsZ;++j)for(int i=0;i<cellsX;++i)m.indices.insert(m.indices.end(),{at(i,j),at(i,j+1),at(i+1,j+1),at(i,j),at(i+1,j+1),at(i+1,j)});
 CollisionCookSettings settings;auto asset=std::make_shared<CollisionAsset>();std::string error;if(!CookCollision(m,settings,*asset,error))throw std::runtime_error(error);return asset;
}
int main(){
 std::cout<<std::setprecision(17);PrimitivePose query,mesh;query.shape=Shape::Box({.02f,.01f,.03f});mesh.shape=Shape::Cooked(Sheet(),"exact captured box5 geometry");
 RigidMotionSegment segment;segment.inverseMass=1;segment.begin=0.012716979254868147;segment.end=0.01666666753590107;
 segment.position={-0.70000004768371582,0.017770588397979736,-0.11849060654640198};segment.orientation={1,0,0,0};
 segment.linearVelocity={-2.3657503334106877e-07,-30.766334533691406,26.541725158691406};segment.driftAngularVelocity={964.20758056640625,5.6333967222599313e-05,-7.2237358835991472e-05};
 const double remaining=segment.end-segment.begin,angular=glm::length(glm::dvec3(segment.driftAngularVelocity));
 const double speed=glm::length(glm::dvec3(segment.linearVelocity))+angular*ShapeBoundingRadius(query.shape);
 const auto samples=unsigned(std::max({16.,std::ceil(16.*speed*remaining/double(query.shape.halfExtents.y)),std::ceil(256.*angular*remaining)}));
 const double finish=segment.begin+remaining/double(samples);std::cout<<"CAPTURE first="<<segment.begin<<" finish="<<finish<<" samples="<<samples<<" feature=44\n";
 const auto old=episode_probe::Connected(query,mesh,44,segment,segment.begin,finish);std::cout<<"FIXED_WITNESS result="<<int(old.result)<<" intervals="<<old.intervals<<'\n';

 for(int power=0;power<42;++power){const double t=segment.begin+std::ldexp(finish-segment.begin,-power);auto at=episode_probe::At(query,segment,t);MeshContactEpisode ep{{44}};std::vector<uint32_t> others;for(unsigned i=0;i<mesh.shape.asset->faces.size();++i)if(i!=44)others.push_back(i);const bool native=MeshEpisodeHasContacts(at,mesh,&ep,&others);std::cout<<"MICRO time="<<t<<" dt="<<(t-segment.begin)<<" pose="<<at.parentPosition.y<<','<<at.parentPosition.z<<" qx="<<at.parentOrientation.x<<" native="<<native<<" native_sat_cert="<<episode_probe::NativeOverlapThroughout(query,mesh,44,segment,t,t)<<'\n';}
 MeshContactEpisode inherited{{44}};std::vector<uint32_t> excluded;for(unsigned i=0;i<mesh.shape.asset->faces.size();++i)if(i!=44)excluded.push_back(i);
 const auto truth=[&](double time){return MeshEpisodeHasContacts(episode_probe::At(query,segment,time),mesh,&inherited,&excluded);};

 MeshContactEpisode all;for(unsigned f=0;f<mesh.shape.asset->faces.size();++f)all.fronts.push_back(f);
 for(unsigned i=0;i<=16;++i){const double time=segment.begin+(finish-segment.begin)*double(i)/16.;const auto at=episode_probe::At(query,segment,time);
  const auto owned=MeshContactFeatures(at,mesh,&inherited),geometric=MeshContactFeatures(at,mesh,&all),front=MeshContactFeatures(at,mesh,nullptr);
  std::cout<<"FEATURE_IDS t="<<time<<" owned=";for(auto f:owned)std::cout<<f<<',';std::cout<<" all=";for(auto f:geometric)std::cout<<f<<',';std::cout<<" front=";for(auto f:front)std::cout<<f<<',';std::cout<<'\n';
 }
 const auto& actualFace=mesh.shape.asset->faces[44];std::cout<<"TOPOLOGY44 normal="<<actualFace.normal.x<<','<<actualFace.normal.y<<','<<actualFace.normal.z<<" adjacent=";for(auto f:actualFace.adjacent)std::cout<<f<<',';std::cout<<" vertices=";for(auto v:actualFace.vertices){const auto p=mesh.shape.asset->vertices[v];std::cout<<'('<<p.x<<','<<p.y<<','<<p.z<<')';}std::cout<<'\n';
 unsigned empty=0,uncertifiedPoint=0;double firstEmpty=-1;
 for(unsigned i=0;i<=4096;++i){const double time=segment.begin+(finish-segment.begin)*double(i)/4096.;if(!truth(time)){++empty;if(firstEmpty<0)firstEmpty=time;}if(!episode_probe::NativeOverlapThroughout(query,mesh,44,segment,time,time))++uncertifiedPoint;}
 std::cout<<"DENSE_DIAGNOSTIC exact_truth_empty="<<empty<<" first_empty="<<firstEmpty<<" native_point_uncertified="<<uncertifiedPoint<<'\n';
 unsigned intervals=0,certificates=0;double separated=-1;
 const auto walk=[&](auto&&self,double from,double to,unsigned depth)->episode_probe::Result{
  if(++intervals>1024)return episode_probe::Result::Uncertain;
  if(!truth(from)){separated=from;return episode_probe::Result::Separated;}
  if(!truth(to)){separated=to;return episode_probe::Result::Separated;}
  if(episode_probe::Constant(segment,from,to)||episode_probe::NativeOverlapThroughout(query,mesh,44,segment,from,to)){++certificates;return episode_probe::Result::Connected;}
  const double mid=(from+to)*.5;if(!truth(mid)){separated=mid;return episode_probe::Result::Separated;}
  if(depth>=40||mid<=from||mid>=to)return episode_probe::Result::Uncertain;
  auto left=self(self,from,mid,depth+1);return left==episode_probe::Result::Connected?self(self,mid,to,depth+1):left;
 };
 const auto result=walk(walk,segment.begin,finish,0);std::cout<<"NATIVE_SAT result="<<int(result)<<" intervals="<<intervals<<" certificates="<<certificates<<" separated_at="<<separated<<'\n';
 return 0;
}
