#include "VolumeTerrainGeometry.h"
#include "AssetDatabase.h"
#include "CollisionAsset.h"
#include "SceneFingerprint.h"
#include "../../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <map>
#include <cmath>
#include <algorithm>
namespace fs=std::filesystem;using Json=nlohmann::json;
namespace {
// Bound the DOM before converting grid counts; rejected sources cannot allocate
// arbitrary nested graphs or sample vectors merely because their file is small.
struct VolumeSax : nlohmann::json_sax<Json> {
 size_t events=0,depth=0;bool Next(){return ++events<=650000;}
 bool null()override{return Next();}bool boolean(bool)override{return Next();}
 bool number_integer(number_integer_t)override{return Next();}bool number_unsigned(number_unsigned_t)override{return Next();}
 bool number_float(number_float_t v,const string_t&)override{return Next()&&std::isfinite(v);}
 bool string(string_t& v)override{return Next()&&v.size()<=4096;}bool binary(binary_t&)override{return false;}
 bool key(string_t& v)override{return string(v);}
 bool start_object(std::size_t n)override{return Next()&&++depth<8&&(n==size_t(-1)||n<=300000);}
 bool end_object()override{--depth;return Next();}bool start_array(std::size_t n)override{return start_object(n);}
 bool end_array()override{return end_object();}bool parse_error(std::size_t,const std::string&,const nlohmann::detail::exception&)override{return false;}
};
void Require(bool b,const std::string& e){if(!b)throw std::runtime_error(e);}
bool Finite(glm::vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
std::string Read(const fs::path& p,size_t limit=16*1024*1024){std::ifstream f(p,std::ios::binary|std::ios::ate);Require(bool(f)&&f.tellg()>=0&&uint64_t(f.tellg())<=limit,"missing/bounded volume file: "+p.string());std::string s(size_t(f.tellg()),'\0');f.seekg(0);f.read(s.data(),s.size());Require(bool(f),"volume read failed");return s;}
float Box(glm::vec3 p,glm::vec3 half){auto q=glm::abs(p)-half;return glm::length(glm::max(q,glm::vec3(0)))+std::min(std::max(q.x,std::max(q.y,q.z)),0.f);}
}
bool ValidateVolumeTerrain(const VolumeTerrainSource& t,std::string& e){auto fail=[&](const char* s){e=s;return false;};auto n=t.resolution;
 if(t.version!=1||n.x<3||n.y<3||n.z<3||n.x>129||n.y>129||n.z>129||uint64_t(n.x)*n.y*n.z>300000)return fail("volume version/grid bound: 3..129 per axis, <=300000 samples");
 if(!Finite(t.extent)||glm::any(glm::lessThan(t.extent,glm::vec3(.3f)))||glm::any(glm::greaterThan(t.extent,glm::vec3(1024))))return fail("volume extent .3..1024 local metres");
 if(!IsValidAssetId(t.identity)||!IsValidAssetId(t.meshId)||!IsValidAssetId(t.collisionId)||t.identity==t.meshId||t.identity==t.collisionId||t.meshId==t.collisionId)return fail("volume identities must be valid and distinct");
 if(t.productDirectory.empty()||fs::path(t.productDirectory).is_absolute()||t.productDirectory.find("..")!=std::string::npos)return fail("volume product directory must be owned relative path");
 if(t.density.size()!=size_t(n.x)*n.y*n.z)return fail("volume sample count");
 for(unsigned z=0;z<n.z;++z)for(unsigned y=0;y<n.y;++y)for(unsigned x=0;x<n.x;++x){float d=t.density[t.Index(x,y,z)];if(!std::isfinite(d)||std::abs(d)>4096)return fail("volume density must be finite, bounded metres");if((!x||!y||!z||x==n.x-1||y==n.y-1||z==n.z-1)&&d<=0)return fail("volume boundary must remain air; use separate bounded volumes");}
 e.clear();return true;
}
VolumeTerrainSource CreateVolumeTerrain(glm::uvec3 n,glm::vec3 extent){Require(n.x>=3&&n.y>=3&&n.z>=3&&n.x<=129&&n.y<=129&&n.z<=129&&uint64_t(n.x)*n.y*n.z<=300000,"volume allocation bounds");VolumeTerrainSource t;t.resolution=n;t.extent=extent;t.identity=MintAssetId();t.meshId=MintAssetId();t.collisionId=MintAssetId();t.productDirectory="Assets/Volumes/"+t.identity;t.density.resize(size_t(n.x)*n.y*n.z);auto cell=extent/glm::vec3(n-glm::uvec3(1));auto padding=cell*glm::vec3(n.x<=4?.75f:1.25f,n.y<=4?.75f:1.25f,n.z<=4?.75f:1.25f);for(unsigned z=0;z<n.z;++z)for(unsigned y=0;y<n.y;++y)for(unsigned x=0;x<n.x;++x)t.density[t.Index(x,y,z)]=Box(t.Point(x,y,z),extent*.5f-padding);std::string e;Require(ValidateVolumeTerrain(t,e),e);return t;}
VolumeTerrainSource ForkVolumeTerrain(const VolumeTerrainSource& t){auto out=t;out.identity=MintAssetId();out.meshId=MintAssetId();out.collisionId=MintAssetId();out.productDirectory=(fs::path(t.productDirectory).parent_path()/out.identity).generic_string();return out;}
std::string SerializeVolumeTerrain(const VolumeTerrainSource& t){std::string e;Require(ValidateVolumeTerrain(t,e),e);return Json({{"format","JudasVolumeTerrain"},{"version",1},{"resolution",{t.resolution.x,t.resolution.y,t.resolution.z}},{"extent",{t.extent.x,t.extent.y,t.extent.z}},{"identity",t.identity},{"meshId",t.meshId},{"collisionId",t.collisionId},{"productDirectory",t.productDirectory},{"density",t.density}}).dump()+"\n";}
bool ParseVolumeTerrain(const std::string& s,VolumeTerrainSource& out,std::string& e){try{Require(s.size()<=16*1024*1024,"volume source byte bound");VolumeSax guard;Require(Json::sax_parse(s,&guard),"malformed/bounded volume source");auto j=Json::parse(s,[](int depth,Json::parse_event_t,const Json&){Require(depth<8,"volume nesting bound");return true;});Require(j.at("format")=="JudasVolumeTerrain"&&j.at("version").is_number_integer()&&j.at("version")==1,"volume format/version");Require(j.at("resolution").size()==3&&j.at("extent").size()==3,"volume dimensions");glm::uvec3 n;for(unsigned k=0;k<3;++k){const auto& v=j.at("resolution")[k];Require(v.is_number_integer()&&v.get<int64_t>()>=3&&v.get<int64_t>()<=129,"volume integer resolution bounds");n[k]=v.get<unsigned>();}Require(uint64_t(n.x)*n.y*n.z<=300000&&j.at("density").size()==uint64_t(n.x)*n.y*n.z,"volume grid/sample bounds");VolumeTerrainSource t;t.resolution=n;t.extent={j.at("extent")[0],j.at("extent")[1],j.at("extent")[2]};t.identity=j.at("identity");t.meshId=j.at("meshId");t.collisionId=j.at("collisionId");t.productDirectory=j.at("productDirectory");t.density=j.at("density").get<std::vector<float>>();Require(ValidateVolumeTerrain(t,e),e);out=std::move(t);e.clear();return true;}catch(const std::exception& x){e=x.what();return false;}}
bool LoadVolumeTerrain(const std::string& p,VolumeTerrainSource& t,std::string& e){try{return ParseVolumeTerrain(Read(p),t,e);}catch(const std::exception& x){e=x.what();return false;}}
bool BuildVolumeTerrainMesh(const VolumeTerrainSource& t,MeshData& out,std::string& e,const std::function<bool()>& cancelled){try{Require(ValidateVolumeTerrain(t,e),e);MeshData m;std::map<std::pair<size_t,size_t>,unsigned> edges;auto n=t.resolution;
 auto point=[&](size_t i){unsigned x=i%n.x,y=(i/n.x)%n.y,z=i/(size_t(n.x)*n.y);return t.Point(x,y,z);};
 auto gradient=[&](size_t i){unsigned c[3]={unsigned(i%n.x),unsigned((i/n.x)%n.y),unsigned(i/(size_t(n.x)*n.y))};glm::vec3 g;for(unsigned a=0;a<3;++a){auto lo=glm::uvec3(c[0],c[1],c[2]),hi=lo;lo[a]=c[a]?c[a]-1:0;hi[a]=std::min(c[a]+1,n[a]-1);g[a]=(t.density[t.Index(hi.x,hi.y,hi.z)]-t.density[t.Index(lo.x,lo.y,lo.z)])/(float(hi[a]-lo[a])*t.extent[a]/float(n[a]-1));}return g;};
 auto vertex=[&](size_t a,size_t b){if(a>b)std::swap(a,b);auto key=std::make_pair(a,b);auto it=edges.find(key);if(it!=edges.end())return it->second;float f=t.density[a]/(t.density[a]-t.density[b]);MeshVertex v;v.position=glm::mix(point(a),point(b),f);auto g=glm::mix(gradient(a),gradient(b),f);v.normal=glm::length(g)>1e-8f?glm::normalize(g):glm::vec3(0);v.uv={v.position.x,v.position.z};v.tangent={1,0,0,1};unsigned id=unsigned(m.vertices.size());Require(id<200000,"volume extracted vertex budget exceeded");m.vertices.push_back(v);edges.emplace(key,id);return id;};
 auto triangle=[&](unsigned a,unsigned b,unsigned c,glm::vec3 outward){auto normal=glm::cross(m.vertices[b].position-m.vertices[a].position,m.vertices[c].position-m.vertices[a].position);if(glm::length(normal)<1e-9f)return;if(glm::dot(normal,outward)<0)std::swap(b,c);Require(m.indices.size()<65536*3,"volume extracted triangle budget exceeded; use a smaller/coarser source");m.indices.insert(m.indices.end(),{a,b,c});};
 const unsigned tet[6][4]={{0,1,3,7},{0,3,2,7},{0,2,6,7},{0,6,4,7},{0,4,5,7},{0,5,1,7}};
 for(unsigned z=0;z+1<n.z;++z){Require(!cancelled||!cancelled(),"volume extraction cancelled");for(unsigned y=0;y+1<n.y;++y)for(unsigned x=0;x+1<n.x;++x){size_t cube[8];for(unsigned k=0;k<8;++k)cube[k]=t.Index(x+(k&1),y+((k>>1)&1),z+((k>>2)&1));for(const auto& cell:tet){size_t in[4],air[4];unsigned ni=0,na=0;glm::vec3 pi{0},pa{0};for(auto k:cell){auto id=cube[k];if(t.density[id]<0){in[ni++]=id;pi+=point(id);}else{air[na++]=id;pa+=point(id);}}if(!ni||!na)continue;auto outward=pa/float(na)-pi/float(ni);if(ni==1)triangle(vertex(in[0],air[0]),vertex(in[0],air[1]),vertex(in[0],air[2]),outward);else if(ni==3)triangle(vertex(air[0],in[0]),vertex(air[0],in[1]),vertex(air[0],in[2]),outward);else{auto a=vertex(in[0],air[0]),b=vertex(in[0],air[1]),c=vertex(in[1],air[0]),d=vertex(in[1],air[1]);triangle(a,b,d,outward);triangle(a,d,c,outward);}}}}
 Require(!m.indices.empty(),"volume is empty; remove its scene instance instead of publishing an empty collider");for(size_t i=0;i<m.indices.size();i+=3){auto a=m.indices[i],b=m.indices[i+1],c=m.indices[i+2];auto normal=glm::normalize(glm::cross(m.vertices[b].position-m.vertices[a].position,m.vertices[c].position-m.vertices[a].position));for(auto k:{a,b,c})if(glm::length(m.vertices[k].normal)<1e-8f)m.vertices[k].normal=normal;}m.primitives.emplace_back(0,unsigned(m.indices.size()),-1,"volume");out=std::move(m);e.clear();return true;}catch(const std::exception& x){e=x.what();return false;}}
namespace {
#include "VolumeBrush.inl"
}
bool ApplyVolumeBrush(VolumeTerrainSource& t,glm::vec3 p,const VolumeBrush& b,std::string& e){if(!ValidateVolumeTerrain(t,e))return false;if(!BrushPoint(t,p,b)){e="volume brush bounds";return false;}Dab(t,p,b);e.clear();return true;}
