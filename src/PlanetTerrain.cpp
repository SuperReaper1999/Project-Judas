#include "PlanetTerrain.h"
#include "../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <queue>
#include <set>
#include <stdexcept>
#include <tuple>
namespace {
using V=glm::dvec3;using J=nlohmann::ordered_json;
bool finite(V p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
V unit(V p){const double n=glm::length(p);if(!finite(p)||n<1e-30)throw std::invalid_argument("undefined planetary direction");return p/n;}
const V axes[6][3]={{{1,0,0},{0,0,-1},{0,1,0}},{{-1,0,0},{0,0,1},{0,1,0}},{{0,1,0},{1,0,0},{0,0,-1}},{{0,-1,0},{1,0,0},{0,0,1}},{{0,0,1},{1,0,0},{0,1,0}},{{0,0,-1},{-1,0,0},{0,1,0}}};
void require(bool b,const char* s){if(!b)throw std::invalid_argument(s);}
void validKey(PlanetTileKey k){require(k.face<6&&k.level<=26&&k.x<(1u<<k.level)&&k.y<(1u<<k.level),"invalid planetary tile identity");}
void basis(V n,V& x,V& y){x=unit(glm::cross(std::abs(n.y)<.8?V(0,1,0):V(1,0,0),n));y=glm::cross(n,x);}
double patchMax(const PlanetHeightPatch& p){double m=0;for(auto v:p.heights)m=std::max(m,std::abs(v));return m;}
V jsonVector(const J& j){require(j.is_array()&&j.size()==3,"planet vector requires three numbers");V p;for(unsigned i=0;i<3;++i){require(j[i].is_number(),"planet vector requires numbers");p[i]=j[i].get<double>();}require(finite(p),"nonfinite planet vector");return p;}
J array(V p){return J::array({p.x,p.y,p.z});}
}
bool PlanetTileKey::operator<(const PlanetTileKey& b)const{return std::tie(face,level,x,y)<std::tie(b.face,b.level,b.x,b.y);}
bool PlanetTileKey::operator==(const PlanetTileKey& b)const{return face==b.face&&level==b.level&&x==b.x&&y==b.y;}
PlanetTileKey PlanetTileKey::Parent()const{return level?PlanetTileKey{face,level-1,x/2,y/2}:*this;}
bool ValidatePlanetSettings(const PlanetSettings& s,std::string& error){try{
 require(std::isfinite(s.radius)&&s.radius>=1&&s.radius<=1e10&&finite(s.center)&&glm::all(glm::lessThanEqual(glm::abs(s.center),V(1e12))),"planet radius/absolute centre outside precision contract");
 require(std::isfinite(glm::length(s.rotation))&&glm::length(s.rotation)>1e-12,"invalid planet orientation");
 require(s.segments>=4&&s.segments<=64&&(s.segments&(s.segments-1))==0&&s.maxLevel<=26,"planet grid requires power-of-two 4..64 segments and depth <=26");
 require(double(s.maxResident)*(s.segments+1)*(s.segments+1)*4*sizeof(glm::dvec3)+double(s.maxResident)*s.segments*s.segments*6*sizeof(MeshVertex)<=256.*1024*1024,"planet visual CPU/GPU estimate exceeds 256 MiB budget");
 require(s.maxTiles>=6&&s.maxTiles<=4096&&s.maxResident>=s.maxTiles*2&&s.maxResident<=8192&&s.buildsPerFrame>=4&&s.buildsPerFrame<=32,"invalid planetary residency budget");
 require(std::isfinite(s.pixelError)&&s.pixelError>=.25&&s.pixelError<=64&&std::isfinite(s.morphSeconds)&&s.morphSeconds>=0&&s.morphSeconds<=5,"invalid planetary LOD tolerance");
 require(std::isfinite(s.collisionSpacing)&&s.collisionSpacing>=.02&&s.collisionSpacing<=10&&std::isfinite(s.collisionRadius)&&s.collisionRadius>=1&&s.collisionRadius<=1000&&s.maxCollisionTiles>=4&&s.maxCollisionTiles<=256,"invalid planetary collision budget");
 require(s.bands.size()<=16&&s.patches.size()<=64,"planet source count exceeds bound");double bound=0;size_t samples=0;
 for(const auto& b:s.bands){require(std::isfinite(b.amplitude)&&finite(b.wave)&&glm::length(b.wave)<=1000,"invalid planetary spectrum");bound+=std::abs(b.amplitude);}
 for(const auto& p:s.patches){require(finite(p.direction)&&glm::length(p.direction)>1e-12&&std::isfinite(p.extent)&&p.extent>0&&p.extent<s.radius&&p.resolution>=2&&p.resolution<=257&&p.heights.size()==size_t(p.resolution)*p.resolution,"invalid planetary height patch");samples+=p.heights.size();for(double v:p.heights)require(std::isfinite(v),"nonfinite planetary height sample");bound+=patchMax(p);}
 require(samples<=262144&&bound<s.radius*.25,"planet sample/height bound exceeded");error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}}
std::string EncodePlanetSettings(const PlanetSettings& s){J j={{"version",1},{"radius",s.radius},{"center",array(s.center)},{"rotation",{s.rotation.w,s.rotation.x,s.rotation.y,s.rotation.z}},{"segments",s.segments},{"maxLevel",s.maxLevel},{"maxTiles",s.maxTiles},{"maxResident",s.maxResident},{"buildsPerFrame",s.buildsPerFrame},{"pixelError",s.pixelError},{"morphSeconds",s.morphSeconds},{"collisionSpacing",s.collisionSpacing},{"collisionRadius",s.collisionRadius},{"maxCollisionTiles",s.maxCollisionTiles},{"bands",J::array()},{"patches",J::array()}};
 for(auto& b:s.bands)j["bands"].push_back({{"amplitude",b.amplitude},{"wave",array(b.wave)}});
 for(auto& p:s.patches)j["patches"].push_back({{"direction",array(p.direction)},{"extent",p.extent},{"resolution",p.resolution},{"heights",p.heights}});
 return j.dump();}
bool DecodePlanetSettings(const std::string& text,PlanetSettings& out,std::string& error){try{require(text.size()<=16*1024*1024,"planet descriptor exceeds text budget");auto j=J::parse(text);PlanetSettings s;require(j.is_object()&&j.at("version")==1,"unknown planet descriptor version");const auto prototype=J::parse(EncodePlanetSettings(s));for(auto it=j.begin();it!=j.end();++it)require(prototype.contains(it.key()),"unknown planet descriptor field");
 for(auto name:{"radius","pixelError","morphSeconds","collisionSpacing","collisionRadius"})if(j.contains(name))require(j[name].is_number(),"planet scalar requires number");
 s.radius=j.value("radius",s.radius);s.pixelError=j.value("pixelError",s.pixelError);s.morphSeconds=j.value("morphSeconds",s.morphSeconds);s.collisionSpacing=j.value("collisionSpacing",s.collisionSpacing);s.collisionRadius=j.value("collisionRadius",s.collisionRadius);
 if(j.contains("center"))s.center=jsonVector(j["center"]);
 if(j.contains("rotation")){auto q=j["rotation"];require(q.is_array()&&q.size()==4,"invalid planet quaternion");s.rotation={q.at(0).get<double>(),q.at(1).get<double>(),q.at(2).get<double>(),q.at(3).get<double>()};}
 auto integer=[&](const char* n,unsigned& value){if(j.contains(n)){require(j[n].is_number_unsigned()||j[n].is_number_integer(),"planet budget requires integer");auto v=j[n].get<int64_t>();require(v>=0&&v<=8192,"planet integer outside bound");value=unsigned(v);}};
 integer("segments",s.segments);integer("maxLevel",s.maxLevel);integer("maxTiles",s.maxTiles);integer("maxResident",s.maxResident);integer("buildsPerFrame",s.buildsPerFrame);integer("maxCollisionTiles",s.maxCollisionTiles);
 if(j.contains("bands")){require(j["bands"].is_array()&&j["bands"].size()<=16,"invalid planet bands");for(auto& b:j["bands"]){require(b.is_object()&&b.size()==2,"invalid planet band fields");s.bands.push_back({b.at("amplitude").get<double>(),jsonVector(b.at("wave"))});}}
 if(j.contains("patches")){require(j["patches"].is_array()&&j["patches"].size()<=64,"invalid planet patches");for(auto& v:j["patches"]){require(v.is_object()&&v.size()==4,"invalid planet patch fields");PlanetHeightPatch p;p.direction=jsonVector(v.at("direction"));p.extent=v.at("extent").get<double>();require(v.at("resolution").is_number_integer(),"planet patch resolution requires integer");auto n=v.at("resolution").get<int>();require(n>=2&&n<=257,"planet patch resolution outside bound");p.resolution=unsigned(n);require(v.at("heights").is_array()&&v.at("heights").size()==size_t(n)*n,"planet patch data count differs");p.heights=v.at("heights").get<std::vector<double>>();s.patches.push_back(std::move(p));}}
 if(!ValidatePlanetSettings(s,error))throw std::invalid_argument(error);
 s.rotation=glm::normalize(s.rotation);out=std::move(s);error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}}
PlanetSurface::PlanetSurface(PlanetSettings s):settings(std::move(s)){std::string error;if(!ValidatePlanetSettings(settings,error))throw std::invalid_argument(error);for(auto& b:settings.bands){m_heightBound+=std::abs(b.amplitude);m_curvature+=4*std::abs(b.amplitude)*glm::dot(b.wave,b.wave);}for(auto& p:settings.patches)m_heightBound+=patchMax(p);}
PlanetSurface::PlanetSurface(PlanetSettings s,Height h,double bound,double curvature):PlanetSurface(std::move(s)){require(bool(h)&&std::isfinite(bound)&&bound>=0&&bound<settings.radius*.25&&std::isfinite(curvature)&&curvature>=0,"invalid custom planet source bounds");m_source=std::move(h);m_heightBound=bound;m_curvature=curvature;}
double PlanetSurface::HeightAt(V d)const{d=unit(d);double h=0;if(m_source)h=m_source(d);else {
 for(auto& b:settings.bands)h+=b.amplitude*std::sin(glm::dot(d*settings.radius,b.wave));
 for(auto& p:settings.patches){V n=unit(p.direction),x,y;basis(n,x,y);const double facing=glm::dot(d,n);if(facing<=.5)continue;const double u=glm::dot(d,x)*settings.radius/(facing*p.extent),v=glm::dot(d,y)*settings.radius/(facing*p.extent);if(std::abs(u)>=1||std::abs(v)>=1)continue;const double sx=(u+1)*.5*(p.resolution-1),sy=(v+1)*.5*(p.resolution-1);const unsigned ix=std::min(unsigned(sx),p.resolution-2),iy=std::min(unsigned(sy),p.resolution-2);auto at=[&](unsigned a,unsigned b){return p.heights[size_t(b)*p.resolution+a];};const double value=glm::mix(glm::mix(at(ix,iy),at(ix+1,iy),sx-ix),glm::mix(at(ix,iy+1),at(ix+1,iy+1),sx-ix),sy-iy);h+=value*std::pow(1-u*u,2)*std::pow(1-v*v,2);}}
 require(std::isfinite(h)&&std::abs(h)<=m_heightBound+1e-9,"planet source violated height certificate");return h;}
V PlanetSurface::Point(V d)const{d=unit(d);return settings.center+glm::normalize(settings.rotation)*(d*(settings.radius+HeightAt(d)));}
PlanetSample PlanetSurface::Sample(V p,double normalStep)const{require(finite(p),"nonfinite planet query");const auto q=glm::normalize(settings.rotation);V d=glm::inverse(q)*(p-settings.center);if(glm::length(d)<1e-20)d={0,1,0};d=unit(d);V x,y;basis(d,x,y);double step=normalStep>0?std::min(normalStep,settings.radius*.05):.02;
 if(normalStep<=0)for(auto& b:settings.bands)if(glm::length(b.wave)>0)step=std::min(step,1/(64*glm::length(b.wave)));
 if(normalStep<=0)for(auto& patch:settings.patches)step=std::min(step,patch.extent/(8*(patch.resolution-1)));
 step=std::max(step,1e-5);const double angle=step/settings.radius;
 const double dx=(HeightAt(unit(d+x*angle))-HeightAt(unit(d-x*angle)))/(2*angle),dy=(HeightAt(unit(d+y*angle))-HeightAt(unit(d-y*angle)))/(2*angle),h=HeightAt(d);
 const V normal=q*unit(d-(x*dx+y*dy)/(settings.radius+h));const V point=Point(d);return {point,normal,h,glm::dot(p-point,normal)};}
V PlanetSurface::Direction(unsigned face,double u,double v){require(face<6&&std::isfinite(u)&&std::isfinite(v),"invalid cube face coordinate");return unit(axes[face][0]+axes[face][1]*u+axes[face][2]*v);}
glm::dvec2 PlanetSurface::FaceCoordinates(unsigned f,V d){require(f<6,"invalid cube face");const double w=glm::dot(d,axes[f][0]);require(w>0,"direction outside cube hemisphere");return {glm::dot(d,axes[f][1])/w,glm::dot(d,axes[f][2])/w};}
PlanetTileKey PlanetSurface::Locate(V d,unsigned level){require(finite(d)&&glm::length(d)>0&&level<=26,"invalid planet tile query");unsigned axis=0;if(std::abs(d.y)>std::abs(d.x))axis=1;if(std::abs(d.z)>std::abs(d[axis]))axis=2;const unsigned face=axis==0?(d.x>=0?0:1):axis==1?(d.y>=0?2:3):(d.z>=0?4:5);auto uv=FaceCoordinates(face,d);const unsigned count=1u<<level;auto index=[&](double v){return unsigned(std::clamp(std::floor((v+1)*.5*count),0.,double(count-1)));};return {face,level,index(uv.x),index(uv.y)};}
V PlanetSurface::GridPoint(PlanetTileKey k,unsigned x,unsigned y,unsigned n)const{validKey(k);require(n>0&&x<=n&&y<=n,"invalid planet grid coordinate");const double cells=double(1u<<k.level)*n;return Point(Direction(k.face,2*(double(k.x)*n+x)/cells-1,2*(double(k.y)*n+y)/cells-1));}
V PlanetSurface::TileCenter(PlanetTileKey k)const{validKey(k);const double n=1u<<k.level;return Point(Direction(k.face,2*(k.x+.5)/n-1,2*(k.y+.5)/n-1));}
double PlanetSurface::TileRadius(PlanetTileKey k)const{
 const auto center=glm::normalize(glm::conjugate(glm::normalize(settings.rotation))*(TileCenter(k)-settings.center));
 double chord=0;for(unsigned x:{0u,1u})for(unsigned y:{0u,1u}){const double tiles=double(1u<<k.level);auto d=Direction(k.face,2*(k.x+x)/tiles-1,2*(k.y+y)/tiles-1);chord=std::max(chord,glm::length(d-center));}
 const double arc=2*std::asin(std::min(1.,chord*.5))*settings.radius;
 double variation=m_source?2*m_heightBound:0;
 if(!m_source){for(auto& band:settings.bands)variation+=std::min(2*std::abs(band.amplitude),std::abs(band.amplitude)*glm::length(band.wave)*arc);for(auto& patch:settings.patches)variation+=std::min(2*patchMax(patch),8*patchMax(patch)*(patch.resolution-1)/(2*patch.extent)*arc);}
 return (settings.radius+m_heightBound)*chord+variation+.001;
}
double PlanetSurface::Error(PlanetTileKey k)const{
 validKey(k);const double cell=2*settings.radius/(double(1u<<k.level)*settings.segments);double error=cell*cell/settings.radius;
 if(m_source)error+=std::min(2*m_heightBound,m_curvature*cell*cell);
 else {for(auto& b:settings.bands)error+=std::min(2*std::abs(b.amplitude),4*std::abs(b.amplitude)*glm::dot(b.wave,b.wave)*cell*cell);
  const double count=1u<<k.level;const auto center=Direction(k.face,2*(k.x+.5)/count-1,2*(k.y+.5)/count-1);double cap=0;for(unsigned x:{0u,1u})for(unsigned y:{0u,1u})cap=std::max(cap,std::acos(std::clamp(glm::dot(center,Direction(k.face,2*(k.x+x)/count-1,2*(k.y+y)/count-1)),-1.,1.)));
  for(auto& p:settings.patches){const auto patchCap=std::atan(std::sqrt(2.)*p.extent/settings.radius);if(std::acos(std::clamp(glm::dot(center,unit(p.direction)),-1.,1.))<=cap+patchCap+1e-8)error+=std::min(2*patchMax(p),8*patchMax(p)*cell*(p.resolution-1)/(2*p.extent));}}
 return error;
}
V PlanetTrianglePoint(const PlanetSurface& s,PlanetTileKey k,V d,unsigned n){validKey(k);auto uv=PlanetSurface::FaceCoordinates(k.face,d);const double tiles=1u<<k.level,sx=std::clamp(((uv.x+1)*.5*tiles-k.x)*n,0.,double(n)),sy=std::clamp(((uv.y+1)*.5*tiles-k.y)*n,0.,double(n));const unsigned x=std::min(unsigned(sx),n-1),y=std::min(unsigned(sy),n-1);const double u=sx-x,v=sy-y;V a=s.GridPoint(k,x,y,n),c=s.GridPoint(k,x+1,y+1,n);return v<=u?a*(1-u)+s.GridPoint(k,x+1,y,n)*(u-v)+c*v:a*(1-v)+c*u+s.GridPoint(k,x,y+1,n)*(v-u);}
PlanetTileData BuildPlanetTile(const PlanetSurface& s,PlanetTileKey k,unsigned n,bool presentation){validKey(k);require(n>=2&&n<=256,"planet mesh resolution outside bound");PlanetTileData t;t.key=k;t.origin=s.TileCenter(k);t.points.reserve(size_t(n+1)*(n+1));for(unsigned y=0;y<=n;++y)for(unsigned x=0;x<=n;++x){V p=s.GridPoint(k,x,y,n);const double count=double(1u<<k.level);const V d=PlanetSurface::Direction(k.face,2*(k.x+double(x)/n)/count-1,2*(k.y+double(y)/n)/count-1);t.points.push_back(p);t.normals.push_back(presentation?s.Sample(p,s.settings.radius/(double(1u<<k.level)*n)).normal:glm::normalize(p-s.settings.center));t.parentPoints.push_back(presentation&&k.level?PlanetTrianglePoint(s,k.Parent(),d,n):p);}return t;}
MeshData PlanetTileData::Mesh(double morph,double skirt)const{require(points.size()==normals.size()&&points.size()==parentPoints.size()&&!points.empty(),"incomplete planetary tile");const unsigned stride=unsigned(std::lround(std::sqrt(double(points.size())))),n=stride-1;require(size_t(stride)*stride==points.size()&&n>0&&std::isfinite(morph)&&std::isfinite(skirt)&&skirt>=0,"invalid planetary mesh state");MeshData m;m.vertices.reserve(size_t(n)*n*6+24*n);morph=std::clamp(morph,0.,1.);auto pos=[&](unsigned i){return parentPoints[i]+(points[i]-parentPoints[i])*morph;};auto add=[&](unsigned i,bool lower=false){const V p=pos(i)-(lower?normals[i]*skirt:V(0));MeshVertex v;v.position=glm::vec3(p-origin);v.normal=glm::vec3(normals[i]);v.uv={float(i%stride)/n,float(i/stride)/n};m.vertices.push_back(v);};for(unsigned y=0;y<n;++y)for(unsigned x=0;x<n;++x){unsigned a=y*stride+x,b=a+1,c=a+stride+1,d=a+stride;for(auto i:{a,b,c,a,c,d})add(i);}
 if(skirt>0)for(unsigned edge=0;edge<4;++edge)for(unsigned i=0;i<n;++i){unsigned a=0,b=0;if(edge==0){a=i;b=i+1;}if(edge==1){a=i*stride+n;b=(i+1)*stride+n;}if(edge==2){a=n*stride+n-i;b=a-1;}if(edge==3){a=(n-i)*stride;b=a-stride;}add(a);add(a,true);add(b);add(b);add(a,true);add(b,true);}
 return m;}
PlanetSelection SelectPlanetTiles(const PlanetSurface& s,V eye,double fov,unsigned pixels){require(finite(eye)&&std::isfinite(fov)&&fov>.01&&fov<3.13&&pixels>0,"invalid planet view");const double focal=pixels/(2*std::tan(fov*.5));const V relative=eye-s.settings.center;const double distance=glm::length(relative),minimum=s.settings.radius-s.MaximumHeight();const V direction=distance>0?relative/distance:V(0,1,0);const double horizon=distance>minimum?std::acos(std::clamp(minimum/distance,0.,1.)):3.141592653589793;
 auto priority=[&](PlanetTileKey k){V c=s.TileCenter(k),normal=unit(c-s.settings.center);double cap=0;for(unsigned x:{0u,1u})for(unsigned y:{0u,1u})cap=std::max(cap,std::acos(std::clamp(glm::dot(normal,unit(s.GridPoint(k,x,y,1)-s.settings.center)),-1.,1.)));const double angle=std::acos(std::clamp(glm::dot(normal,direction),-1.,1.));if(angle>horizon+cap+.01)return 0.;return s.Error(k)*focal/std::max(1.,glm::length(eye-c)-s.TileRadius(k));};
 struct Item{double error;PlanetTileKey key;bool operator<(const Item& b)const{return error!=b.error?error<b.error:b.key<key;}};std::priority_queue<Item> q;std::set<PlanetTileKey> leaves;for(unsigned f=0;f<6;++f){PlanetTileKey k{f,0,0,0};leaves.insert(k);q.push({priority(k),k});}bool limited=false;
 while(!q.empty()){auto item=q.top();q.pop();if(item.error<=s.settings.pixelError)break;if(item.key.level==s.settings.maxLevel){limited=true;continue;}if(leaves.size()+3>s.settings.maxTiles){limited=true;break;}leaves.erase(item.key);for(unsigned y=0;y<2;++y)for(unsigned x=0;x<2;++x){PlanetTileKey k{item.key.face,item.key.level+1,item.key.x*2+x,item.key.y*2+y};leaves.insert(k);q.push({priority(k),k});}}
 PlanetSelection out;out.leaves.assign(leaves.begin(),leaves.end());out.budgetLimited=limited;for(auto k:out.leaves)out.maximumPixelError=std::max(out.maximumPixelError,priority(k));return out;}
