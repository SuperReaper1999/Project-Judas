#include "VolumeTerrain.h"
#include "AuthoringDocument.h"
#include "CollisionAsset.h"
#include "ModelArchive.h"
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
fs::path Owned(const fs::path& root,const fs::path& p){auto base=fs::weakly_canonical(root),file=fs::weakly_canonical(p.is_absolute()?p:base/p),rel=file.lexically_relative(base);Require(!rel.empty()&&!rel.is_absolute()&&*rel.begin()!="..","volume path escapes project");return file;}

}
namespace {
Json Metadata(const VolumeTerrainSource& t){return Json({{"format","JudasVolumeTerrain"},{"version",t.version},{"resolution",{t.resolution.x,t.resolution.y,t.resolution.z}},{"extent",{t.extent.x,t.extent.y,t.extent.z}},{"identity",t.identity},{"meshId",t.meshId},{"collisionId",t.collisionId},{"productDirectory",t.productDirectory}});}
#include "../VolumeBrush.inl"
}
void VolumeTerrainDraft::Push(std::vector<std::vector<float>>& list,std::vector<float> v){size_t bound=std::min(size_t(32),std::max(size_t(1),size_t(32*1024*1024)/(v.size()*sizeof(float))));if(list.size()>=bound)list.erase(list.begin());list.push_back(std::move(v));}
bool VolumeTerrainDraft::Open(const std::string& p,std::string& e){try{auto bytes=Read(p);VolumeTerrainSource t;if(!ParseVolumeTerrain(bytes,t,e))return false;source=std::move(t);m_path=p;m_disk=bytes;m_saved=Metadata(source).dump();m_savedDensity=source.density;m_dirty=false;m_active=false;m_undo.clear();m_redo.clear();++m_revision;return true;}catch(const std::exception& x){e=x.what();return false;}}
bool VolumeTerrainDraft::ExternalChanged()const{try{return Read(m_path)!=m_disk;}catch(...){return true;}}
bool VolumeTerrainDraft::Save(std::string& e){try{Require(!m_active,"finish volume gesture before saving");auto bytes=SerializeVolumeTerrain(source);bool exists=fs::exists(m_path);Require(!exists||Read(m_path)==m_disk,"volume source changed externally; reopen or fork");fs::create_directories(fs::absolute(m_path).parent_path());Require(PublishAuthoringOutputs(fs::absolute(m_path).parent_path().string(),{{m_path,bytes,m_disk,exists}},e),e);m_disk=bytes;m_saved=Metadata(source).dump();m_savedDensity=source.density;m_dirty=false;return true;}catch(const std::exception& x){e=x.what();return false;}}
bool VolumeTerrainDraft::Begin(glm::vec3 p,const VolumeBrush& b,std::string& e){if(m_active){e="volume stroke already active";return false;}auto before=source.density;if(!ApplyVolumeBrush(source,p,b,e))return false;m_before=std::move(before);m_brush=b;m_last=p;m_remainder=0;m_active=true;++m_revision;return true;}
bool VolumeTerrainDraft::Continue(glm::vec3 p,std::string& e){if(!m_active||!BrushPoint(source,p,m_brush)||source.density.size()!=uint64_t(source.resolution.x)*source.resolution.y*source.resolution.z||source.resolution.x<3||source.resolution.y<3||source.resolution.z<3||source.resolution.x>129||source.resolution.y>129||source.resolution.z>129||!Finite(source.extent)||glm::any(glm::lessThanEqual(source.extent,glm::vec3(0)))){e="invalid volume stroke sample";return false;}auto delta=p-m_last;float length=glm::length(delta),spacing=m_brush.radius*.2f;if(length/spacing>4096){e="volume stroke segment exceeds bounded dab count";return false;}for(float next=spacing-m_remainder;next<=length;next+=spacing){Dab(source,m_last+delta*(next/length),m_brush);++m_revision;}m_remainder=std::fmod(m_remainder+length,spacing);m_last=p;return true;}
void VolumeTerrainDraft::End(){if(!m_active)return;m_active=false;if(source.density!=m_before){Push(m_undo,std::move(m_before));m_redo.clear();}m_dirty=source.density!=m_savedDensity||Metadata(source).dump()!=m_saved;}
void VolumeTerrainDraft::Cancel(){if(!m_active)return;source.density=std::move(m_before);m_active=false;++m_revision;m_dirty=source.density!=m_savedDensity||Metadata(source).dump()!=m_saved;}
bool VolumeTerrainDraft::Undo(){if(m_active||m_undo.empty())return false;Push(m_redo,std::move(source.density));source.density=std::move(m_undo.back());m_undo.pop_back();++m_revision;m_dirty=source.density!=m_savedDensity||Metadata(source).dump()!=m_saved;return true;}
bool VolumeTerrainDraft::Redo(){if(m_active||m_redo.empty())return false;Push(m_undo,std::move(source.density));source.density=std::move(m_redo.back());m_redo.pop_back();++m_revision;m_dirty=source.density!=m_savedDensity||Metadata(source).dump()!=m_saved;return true;}
bool CookVolumeTerrain(const VolumeTerrainSource& t,const std::string& path,const std::string& root,const AssetDatabase& assets,TerrainCookResult& result,std::string& e,const std::function<bool()>& cancelled){auto start=std::chrono::steady_clock::now();try{
 Require(ValidateVolumeTerrain(t,e),e);auto source=Owned(root,path),dir=Owned(root,t.productDirectory),assetRoot=Owned(root,assets.AssetsDir());auto rel=dir.lexically_relative(assetRoot);Require(!rel.empty()&&!rel.is_absolute()&&*rel.begin()!="..","volume products must belong to Assets");auto sourceBytes=Read(source);VolumeTerrainSource disk;Require(ParseVolumeTerrain(sourceBytes,disk,e)&&SerializeVolumeTerrain(disk)==SerializeVolumeTerrain(t),"save volume source before cooking");
 std::vector<AuthoringOutput> outputs;for(const char* name:{"volume.judasmodel","volume.judascollision","volume.judasmodel.judasmeta","volume.judascollision.judasmeta","volume.receipt.json"}){auto p=Owned(root,dir/name);outputs.push_back({p.string(),{},fs::exists(p)?Read(p,64*1024*1024):std::string{},fs::exists(p)});}
 auto modelPath=dir/"volume.judasmodel",collisionPath=dir/"volume.judascollision",receiptPath=dir/"volume.receipt.json";
 for(auto pair:{std::make_pair(t.meshId,modelPath),std::make_pair(t.collisionId,collisionPath)}){auto* a=assets.Find(pair.first);Require(!a||fs::weakly_canonical(a->path)==fs::weakly_canonical(pair.second),"volume identity belongs to another product; fork first");auto* destination=assets.FindByRelativePath(pair.second.lexically_relative(fs::weakly_canonical(root)).generic_string());Require(!destination||destination->id==pair.first,"volume destination belongs to another asset; fork first");}
 MeshData mesh;Require(BuildVolumeTerrainMesh(t,mesh,e,cancelled),e);mesh.importRecord=Json({{"kind","volume-terrain"},{"source",source.lexically_relative(fs::weakly_canonical(root)).generic_string()},{"receipt",receiptPath.lexically_relative(fs::weakly_canonical(root)).generic_string()}}).dump();std::vector<uint8_t> model;Require(EncodeModelArchive(mesh,model,e),e);outputs[0].bytes.assign(model.begin(),model.end());CollisionAsset collision;CollisionCookSettings settings;Require(CookCollision(mesh,settings,collision,e),e);collision.sourceAsset=t.meshId;collision.sourceFingerprint=SceneFingerprintSha256(outputs[0].bytes);collision.settingsFingerprint=CollisionSettingsFingerprint(settings);
 fs::create_directories(dir);auto temporary=dir/(".volume-"+MintAssetId()+".tmp");if(!SaveCollisionAsset(temporary.string(),collision,e)){fs::remove(temporary);throw std::runtime_error(e);}try{outputs[1].bytes=Read(temporary,64*1024*1024);}catch(...){fs::remove(temporary);throw;}fs::remove(temporary);
 auto meta=[](const std::string& id,const char* type){return "JudasAssetMeta 1\nid \""+id+"\"\ntype "+type+"\nsource \"volume-terrain-authoring\"\n";};outputs[2].bytes=meta(t.meshId,"mesh");outputs[3].bytes=meta(t.collisionId,"collision");Json inputs=Json::object(),products=Json::object();inputs[source.lexically_relative(fs::weakly_canonical(root)).generic_string()]=SceneFingerprintSha256(sourceBytes);for(unsigned i=0;i<2;++i)products[fs::path(outputs[i].path).lexically_relative(fs::weakly_canonical(root)).generic_string()]=SceneFingerprintSha256(outputs[i].bytes);outputs[4].bytes=Json({{"format","JudasVolumeTerrainCook"},{"version",1},{"revision","M84-1"},{"inputs",inputs},{"products",products}}).dump(2)+"\n";
 Require(!cancelled||!cancelled(),"volume cook cancelled; last good products retained");Require(Read(source)==sourceBytes,"volume source changed during cook");for(const auto& o:outputs)Require(fs::exists(o.path)==o.existed&&(!o.existed||Read(o.path,64*1024*1024)==o.expected),"volume product changed during cook");bool changed=outputs[0].bytes!=outputs[0].expected;Require(PublishAuthoringOutputs(dir.string(),outputs,e),e);result={};result.geometryChanged=changed;for(auto& o:outputs)result.bytes+=o.bytes.size();result.milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();e.clear();return true;
 }catch(const std::exception& x){e=x.what();return false;}}
