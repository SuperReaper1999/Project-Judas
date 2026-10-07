#include "ModelArchive.h"
#include "SkeletalAnimation.h"
#include "../third_party/nlohmann/json.hpp"
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <set>
#include <filesystem>
#include <fstream>
#include "SceneFingerprint.h"
#include "AsyncFile.h"
#include "TextureLoader.h"
using Json=nlohmann::json;
namespace {
// SAX preflight rejects nesting/container bombs before constructing the DOM.
struct BoundedModelSax : nlohmann::json_sax<Json> {
 size_t events=0,depth=0;bool Next(){return ++events<=24000000;}
 bool null() override{return Next();} bool boolean(bool) override{return Next();}
 bool number_integer(number_integer_t) override{return Next();} bool number_unsigned(number_unsigned_t) override{return Next();}
 bool number_float(number_float_t,const string_t&) override{return Next();}
 bool string(string_t& value) override{return Next()&&value.size()<=1048576;}
 bool binary(binary_t& value) override{return Next()&&value.size()<=128*1024*1024;}
 bool key(string_t& value) override{return string(value);}
 bool start_object(std::size_t n) override{return Next()&&++depth<=64&&(n==size_t(-1)||n<=6000000);}
 bool end_object() override{--depth;return Next();}
 bool start_array(std::size_t n) override{return start_object(n);}bool end_array() override{return end_object();}
 bool parse_error(std::size_t,const std::string&,const nlohmann::detail::exception&) override{return false;}
};
struct MetadataSax final:BoundedModelSax {
 std::string keyName,format,record;unsigned version=0;std::vector<std::string> definitions;
 bool key(string_t& value) override {keyName=value;return Next()&&value.size()<=1048576;}
 bool string(string_t& value) override {
  if(depth==1&&keyName=="format")format=value;
  if(depth==1&&keyName=="importRecord")record=value;
  if(depth==3&&keyName=="definition")definitions.push_back(value);
  keyName.clear();return BoundedModelSax::string(value);
 }
 bool number_unsigned(number_unsigned_t value) override {
  if(depth==1&&keyName=="version")version=unsigned(value);
  keyName.clear();return BoundedModelSax::number_unsigned(value);
 }
};
void Float(std::vector<uint8_t>& b,float f){uint32_t u;std::memcpy(&u,&f,4);for(int k=0;k<4;++k)b.push_back(uint8_t(u>>(k*8)));}
float Float(const std::vector<uint8_t>& b,size_t& i){if(i+4>b.size())throw std::runtime_error("truncated vertex stream");uint32_t u=0;for(int k=0;k<4;++k)u|=uint32_t(b[i++])<<(k*8);float f;std::memcpy(&f,&u,4);if(!std::isfinite(f))throw std::runtime_error("nonfinite model value");return f;}
Json Vec(glm::vec4 v){return Json::array({v.x,v.y,v.z,v.w});}
glm::vec4 ReadVec(const Json& j){if(!j.is_array()||j.size()!=4)throw std::runtime_error("vector dimensions");glm::vec4 v;for(int k=0;k<4;++k){v[k]=j[k].get<float>();if(!std::isfinite(v[k]))throw std::runtime_error("nonfinite model value");}return v;}
void Bound(size_t n,size_t max,const char* what){if(n>max)throw std::runtime_error(std::string("model bound: ")+what);}
}
bool EncodeModelArchive(const MeshData& mesh,std::vector<uint8_t>& bytes,std::string& error){try{
 Json j={{"format","JudasModel"},{"version",1}};std::vector<uint8_t> vertices;
 for(size_t i=0;i<mesh.vertices.size();++i){const auto& v=mesh.vertices[i];const float values[]={v.position.x,v.position.y,v.position.z,v.normal.x,v.normal.y,v.normal.z,v.uv.x,v.uv.y,v.tangent.x,v.tangent.y,v.tangent.z,v.tangent.w,v.uv1.x,v.uv1.y};for(unsigned k=0;k<14;++k)if(!std::isfinite(values[k]))throw std::runtime_error("nonfinite vertex "+std::to_string(i)+" field "+std::to_string(k)+" normal "+std::to_string(v.normal.x)+","+std::to_string(v.normal.y)+","+std::to_string(v.normal.z));}
 for(const auto& v:mesh.vertices){for(auto x:{v.position.x,v.position.y,v.position.z,v.normal.x,v.normal.y,v.normal.z,v.uv.x,v.uv.y,v.tangent.x,v.tangent.y,v.tangent.z,v.tangent.w,v.uv1.x,v.uv1.y})Float(vertices,x);}
 j["materialKeys"]=mesh.materialKeys;j["sourceNodes"]=mesh.sourceNodes;j["vertexLocations"]=Json::array();j["faceLocations"]=Json::array();for(auto p:mesh.vertexLocations)j["vertexLocations"].push_back({p.node,p.element});for(auto p:mesh.faceLocations)j["faceLocations"].push_back({p.node,p.element});
 j["vertices"]=Json::binary(std::move(vertices));j["indices"]=mesh.indices;j["sourceVertices"]=mesh.sourceVertexIds;j["warnings"]=mesh.importWarnings;j["importRecord"]=mesh.importRecord;
 j["parts"]=Json::array();for(const auto& p:mesh.primitives)j["parts"].push_back({{"first",p.first},{"count",p.count},{"material",p.material},{"identity",p.part},{"node",p.node}});
 j["materials"]=Json::array();for(const auto& m:mesh.materials){Json material={{"definition",SerializeMaterial(m)},{"maps",Json::array()}};for(const auto& map:m.maps)material["maps"].push_back({{"encoded",Json::binary(map.encodedImage)},{"pixels",Json::binary(map.encodedImage.empty()?map.embedded.pixels:std::vector<uint8_t>{})},{"width",map.embedded.width},{"height",map.embedded.height},{"uv",map.uvSet},{"scale",Vec({map.scale,0,0})},{"offset",Vec({map.offset,0,0})},{"rotation",map.rotation},{"sampler",Json::array({map.sampler.wrapS,map.sampler.wrapT,map.sampler.minFilter,map.sampler.magFilter})}});j["materials"].push_back(material);}
 if(mesh.skeletal){const auto& s=mesh.skeletal->skeleton;Json rig={{"motionRoot",s.motionRoot},{"names",s.names},{"parents",s.parents},{"order",s.order},{"skinNodes",s.skinNodes},{"inverseBind",Json::array()},{"rest",Json::array()},{"clips",Json::array()}};
 rig["affine"]=Json::array();for(auto& m:s.affine){Json a=Json::array();for(int c=0;c<4;++c)a.push_back(Vec(m[c]));rig["affine"].push_back(a);}
 for(auto& p:s.rest.local)rig["rest"].push_back(Json::array({Vec({p.translation,0}),Vec({p.rotation.x,p.rotation.y,p.rotation.z,p.rotation.w}),Vec({p.scale,0})}));
 for(auto& m:s.inverseBind){Json a=Json::array();for(int c=0;c<4;++c)a.push_back(Vec(m[c]));rig["inverseBind"].push_back(a);}
 for(auto& c:mesh.skeletal->clips){Json clip={{"name",c.name},{"duration",c.duration},{"loop",c.loop},{"motionTimes",c.motionTimes},{"motion",Json::array()},{"tracks",Json::array()}};for(auto& p:c.motion)clip["motion"].push_back(Json::array({Vec({p.translation,0}),Vec({p.rotation.x,p.rotation.y,p.rotation.z,p.rotation.w})}));for(auto& t:c.tracks){Json track={{"node",t.node},{"path",int(t.path)},{"interpolation",int(t.interpolation)},{"times",t.times},{"values",Json::array()}};for(auto v:t.values)track["values"].push_back(Vec(v));clip["tracks"].push_back(track);}rig["clips"].push_back(clip);}j["rig"]=rig;
 j["weights"]=Json::array();for(auto& v:mesh.skinVertices)j["weights"].push_back(Json::array({Json::array({v.joints.x,v.joints.y,v.joints.z,v.joints.w}),Vec(v.weights),Json::array({v.joints1.x,v.joints1.y,v.joints1.z,v.joints1.w}),Vec(v.weights1)}));}
 auto output=Json::to_cbor(j);j=Json();MeshData check;if(!DecodeModelArchive(output.data(),output.size(),check,error))return false;bytes=std::move(output);error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}}
bool DecodeModelArchive(const void* data,size_t size,MeshData& result,std::string& error){try{
 Bound(size,256*1024*1024,"bytes");const auto* b=static_cast<const uint8_t*>(data);BoundedModelSax guard;if(!Json::sax_parse(b,b+size,&guard,Json::input_format_t::cbor))throw std::runtime_error("malformed/bounded CBOR model");auto j=Json::from_cbor(b,b+size);if(j.at("format")!="JudasModel"||j.at("version")!=1)throw std::runtime_error("model header/version");MeshData m;
 const auto& stream=j.at("vertices").get_binary();if(stream.size()%56)throw std::runtime_error("vertex stream dimensions");Bound(stream.size()/56,2000000,"vertices");for(size_t i=0;i<stream.size();){MeshVertex v;v.position={Float(stream,i),Float(stream,i),Float(stream,i)};v.normal={Float(stream,i),Float(stream,i),Float(stream,i)};v.uv={Float(stream,i),Float(stream,i)};v.tangent={Float(stream,i),Float(stream,i),Float(stream,i),Float(stream,i)};v.uv1={Float(stream,i),Float(stream,i)};m.vertices.push_back(v);}
 Bound(j.at("indices").size(),6000000,"indices");m.indices=j.at("indices").get<std::vector<uint32_t>>();for(auto i:m.indices)if(i>=m.vertices.size())throw std::runtime_error("model vertex index");if(m.indices.size()%3)throw std::runtime_error("triangle index count");m.sourceVertexIds=j.at("sourceVertices").get<std::vector<uint32_t>>();if(!m.sourceVertexIds.empty()&&m.sourceVertexIds.size()!=m.vertices.size())throw std::runtime_error("source vertex count");m.importWarnings=j.at("warnings").get<std::vector<std::string>>();m.importRecord=j.value("importRecord",std::string());
 if(j.contains("sourceNodes")){Bound(j.at("sourceNodes").size(),kModelNodeLimit,"source nodes");m.sourceNodes=j.at("sourceNodes").get<std::vector<std::string>>();auto locations=[&](const char* key,size_t count,std::vector<ModelSourceLocation>& out){if(!j.contains(key))return;auto& values=j.at(key);if(!values.empty()&&values.size()!=count)throw std::runtime_error("source provenance count");for(auto& v:values){ModelSourceLocation p{v.at(0),v.at(1)};if(p.node>=m.sourceNodes.size())throw std::runtime_error("source provenance node");out.push_back(p);}};locations("vertexLocations",m.vertices.size(),m.vertexLocations);locations("faceLocations",m.indices.size()/3,m.faceLocations);}
 Bound(j.at("materials").size(),4096,"materials");for(auto& x:j.at("materials")){MaterialDefinition d;if(!ParseMaterial(x.at("definition"),d,error))throw std::runtime_error(error);if(x.at("maps").size()!=5)throw std::runtime_error("material map count");for(int k=0;k<5;++k){auto& map=d.maps[k];auto& src=x["maps"][k];map.embedded.width=src.at("width");map.embedded.height=src.at("height");map.embedded.pixels=std::move(src.at("pixels").get_binary());if(src.contains("encoded")){map.encodedImage=std::move(src.at("encoded").get_binary());Bound(map.encodedImage.size(),64*1024*1024,"encoded image");if(!map.encodedImage.empty()){TextureData decoded;if(!DecodeTextureFromMemory(map.encodedImage.data(),map.encodedImage.size(),"cooked image",decoded,error))throw std::runtime_error(error);if(decoded.width!=map.embedded.width||decoded.height!=map.embedded.height)throw std::runtime_error("encoded image dimensions disagree");map.embedded=std::move(decoded);}}auto n=uint64_t(map.embedded.width)*uint64_t(map.embedded.height)*4;if(map.embedded.width<0||map.embedded.height<0||n>64*1024*1024||n!=map.embedded.pixels.size())throw std::runtime_error("model image bounds");map.uvSet=src.at("uv");if(map.uvSet<0||map.uvSet>1)throw std::runtime_error("model UV set");map.scale=glm::vec2(ReadVec(src.at("scale")));map.offset=glm::vec2(ReadVec(src.at("offset")));map.rotation=src.at("rotation");auto& t=src.at("sampler");map.sampler={t.at(0),t.at(1),t.at(2),t.at(3)};}if(!ValidateMaterial(d,error))throw std::runtime_error(error);m.materials.push_back(std::move(d));}
 m.materialKeys=j.value("materialKeys",std::vector<std::string>{});if(!m.materialKeys.empty()&&m.materialKeys.size()!=m.materials.size())throw std::runtime_error("material identity count");
 Bound(j.at("parts").size(),16384,"parts");std::set<std::string> parts;for(auto& p:j.at("parts")){MeshPrimitive x{p.at("first"),p.at("count"),p.at("material"),p.at("identity"),p.at("node")};if(size_t(x.first)+x.count>m.indices.size()||x.count%3||x.material< -1||x.material>=int(m.materials.size()))throw std::runtime_error("model part range");if(!x.part.empty()&&!parts.insert(x.part).second)throw std::runtime_error("ambiguous model part identity");m.primitives.push_back(std::move(x));}
 if(j.contains("rig")){auto asset=std::make_shared<SkeletalAsset>();auto& s=asset->skeleton;auto& r=j["rig"];Bound(r.at("names").size(),kModelNodeLimit,"hierarchy nodes");s.names=r.at("names").get<std::vector<std::string>>();s.parents=r.at("parents").get<std::vector<int>>();s.order=r.at("order").get<std::vector<int>>();auto n=s.names.size();s.motionRoot=r.value("motionRoot",-1);if(s.motionRoot< -1||s.motionRoot>=int(n))throw std::runtime_error("motion root index");if(s.parents.size()!=n||s.order.size()!=n||r.at("rest").size()!=n)throw std::runtime_error("hierarchy counts");std::set<int> visited;for(int i:s.order){if(i<0||size_t(i)>=n||visited.count(i)||s.parents[i]< -1||(s.parents[i]>=0&&!visited.count(s.parents[i])))throw std::runtime_error("hierarchy order/cycle");visited.insert(i);}for(auto& p:r.at("rest")){JointTransform t;t.translation=glm::vec3(ReadVec(p.at(0)));auto q=ReadVec(p.at(1));t.rotation={q.w,q.x,q.y,q.z};if(std::abs(glm::length(t.rotation)-1)>1e-4)throw std::runtime_error("model rotation normalization");t.scale=glm::vec3(ReadVec(p.at(2)));s.rest.local.push_back(t);}
 if(r.contains("affine")&&!r.at("affine").empty()){if(r.at("affine").size()!=n)throw std::runtime_error("affine node count");for(auto& x:r.at("affine")){glm::mat4 matrix;for(int c=0;c<4;++c)matrix[c]=ReadVec(x.at(c));if(matrix[0][3]!=0||matrix[1][3]!=0||matrix[2][3]!=0||matrix[3][3]!=1||std::abs(glm::determinant(glm::mat3(matrix)))<1e-12f)throw std::runtime_error("invalid affine node matrix");s.affine.push_back(matrix);}}
 Bound(r.at("skinNodes").size(),kModelPaletteLimit,"palette");s.skinNodes=r.at("skinNodes").get<std::vector<int>>();if(r.at("inverseBind").size()!=s.skinNodes.size())throw std::runtime_error("palette bind count");for(auto i:s.skinNodes)if(i<0||size_t(i)>=n)throw std::runtime_error("palette node");for(auto& x:r.at("inverseBind")){glm::mat4 matrix;for(int c=0;c<4;++c)matrix[c]=ReadVec(x.at(c));if(matrix[0][3]!=0||matrix[1][3]!=0||matrix[2][3]!=0||matrix[3][3]!=1||std::abs(glm::determinant(glm::mat3(matrix)))<1e-12f)throw std::runtime_error("invalid inverse bind matrix");s.inverseBind.push_back(matrix);}
 Bound(r.at("clips").size(),256,"clips");std::set<std::string> names;size_t keys=0;for(auto& c:r.at("clips")){AnimationClip clip;clip.name=c.at("name");clip.duration=c.at("duration");clip.loop=c.value("loop",false);if(c.contains("motion")){clip.motionTimes=c.at("motionTimes").get<std::vector<float>>();if(c.at("motion").size()!=clip.motionTimes.size())throw std::runtime_error("root motion count");Bound(clip.motionTimes.size(),1000000,"root motion keys");float previous=-1;for(auto time:clip.motionTimes){if(!std::isfinite(time)||time<0||time<=previous||time>clip.duration)throw std::runtime_error("root motion times");previous=time;}for(auto& p:c.at("motion")){JointTransform t;t.translation=glm::vec3(ReadVec(p.at(0)));auto q=ReadVec(p.at(1));t.rotation={q.w,q.x,q.y,q.z};if(std::abs(glm::length(t.rotation)-1)>1e-4)throw std::runtime_error("root motion quaternion");clip.motion.push_back(t);}}if(!std::isfinite(clip.duration)||clip.duration<0||!names.insert(clip.name).second)throw std::runtime_error("clip identity/duration");Bound(c.at("tracks").size(),n*3,"tracks");std::set<std::pair<int,int>> tracks;for(auto& x:c.at("tracks")){AnimationTrack t;t.node=x.at("node");int path=x.at("path"),interp=x.at("interpolation");if(t.node<0||size_t(t.node)>=n||path<0||path>2||interp<0||interp>2||!tracks.insert({t.node,path}).second)throw std::runtime_error("clip track");t.path=TrackPath(path);t.interpolation=TrackInterpolation(interp);keys+=x.at("times").size();Bound(keys,4000000,"animation keys");t.times=x.at("times").get<std::vector<float>>();float previous=-1;for(auto time:t.times){if(!std::isfinite(time)||time<0||time<=previous||time>clip.duration)throw std::runtime_error("track times");previous=time;}if(x.at("values").size()!=t.times.size()*(interp==2?3:1))throw std::runtime_error("track values");for(auto& v:x.at("values"))t.values.push_back(ReadVec(v));if(t.path==TrackPath::Rotation)for(size_t i=0;i<t.values.size();++i)if((interp!=2||i%3==1)&&glm::length(t.values[i])<1e-6f)throw std::runtime_error("zero rotation key");clip.tracks.push_back(std::move(t));}asset->clips.push_back(std::move(clip));}
 if(j.at("weights").size()!=m.vertices.size())throw std::runtime_error("skin vertex count");
 for(auto& x:j.at("weights")){MeshSkinVertex v;v.weights=ReadVec(x.at(1));v.weights1=ReadVec(x.at(3));float sum=0;for(int k=0;k<4;++k){v.joints[k]=x.at(0).at(k);v.joints1[k]=x.at(2).at(k);if(v.joints[k]>=s.skinNodes.size()||v.joints1[k]>=s.skinNodes.size()||v.weights[k]<0||v.weights1[k]<0)throw std::runtime_error("skin index/weight");sum+=v.weights[k]+v.weights1[k];}if(std::abs(sum-1)>1e-4)throw std::runtime_error("skin weight normalization");m.skinVertices.push_back(v);}m.skeletal=std::move(asset);}
 if(m.vertices.empty())throw std::runtime_error("empty model");
 result=std::move(m);error.clear();return true;
 }catch(const std::exception& e){error=std::string("JudasModel: ")+e.what();return false;}}

bool ReadModelArchiveMetadata(const void* data,size_t size,std::string& record,std::vector<MaterialDefinition>& materials,std::string& error){try{
 Bound(size,256*1024*1024,"bytes");const auto* b=static_cast<const uint8_t*>(data);MetadataSax sax;
 if(!Json::sax_parse(b,b+size,&sax,Json::input_format_t::cbor)||sax.format!="JudasModel"||sax.version!=1)throw std::runtime_error("malformed/bounded model metadata");
 Bound(sax.definitions.size(),4096,"materials");std::vector<MaterialDefinition> found;
 for(const auto& text:sax.definitions){MaterialDefinition m;if(!ParseMaterial(text,m,error))return false;found.push_back(std::move(m));}
 record=std::move(sax.record);materials=std::move(found);error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}}

bool VerifyImportedModelFresh(const std::string& root,const std::string& path,std::string& error){try{
 std::vector<uint8_t> bytes;if(!ReadWholeFile(path,bytes,error))return false;std::string record;std::vector<MaterialDefinition> materials;if(!ReadModelArchiveMetadata(bytes.data(),bytes.size(),record,materials,error))return false;if(record.empty())return true;
 auto j=Json::parse(record);auto base=std::filesystem::weakly_canonical(root);
 auto owned=[&](const std::string& p){auto file=std::filesystem::weakly_canonical(base/p);auto relative=file.lexically_relative(base);if(relative.empty()||relative.is_absolute()||*relative.begin()=="..")throw std::runtime_error("import record outside project");return file;};
 std::ifstream recipe(owned(j.at("recipe")));if(!recipe)throw std::runtime_error("missing import recipe; restore author-owned recipe before export");if(std::filesystem::file_size(owned(j.at("recipe")))>4*1024*1024)throw std::runtime_error("recipe exceeds 4 MiB");auto current=Json::parse(recipe,[](int depth,Json::parse_event_t,const Json&){if(depth>=48)throw std::runtime_error("recipe nesting bound");return true;});if(SceneFingerprintSha256(current.dump()+j.at("revision").get<std::string>())!=j.at("digest"))throw std::runtime_error("import settings changed; reimport before export");
 for(auto it=j.at("inputs").begin();it!=j.at("inputs").end();++it){std::string digest;if(!SceneFingerprintSha256File(owned(it.key()).string(),digest,error)||digest!=it.value())throw std::runtime_error("stale imported source/dependency: "+it.key()+"; reimport before export");}error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}}
