#include "RuntimeTerrain.h"
#include "terrain/TerrainRuntime.h"
#include "terrain/TerrainAsset.h"
#include "terrain/TerrainSamples.h"
#include "terrain/TerrainPersistence.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include <algorithm>
#include <chrono>
#include <cmath>
namespace {
constexpr size_t MaxVolumes=8;
}
RuntimeTerrain::RuntimeTerrain(RuntimeWorld& w):world(w){}
RuntimeTerrain::~RuntimeTerrain(){while(!records.empty())Remove(records.begin()->first);}
bool RuntimeTerrain::Has(uint64_t id)const{auto i=records.find(id);return i!=records.end()&&i->second.landscape->State().ready;}
bool RuntimeTerrain::Wait(uint64_t id,std::string& error){auto it=records.find(id);if(it==records.end()||!it->second.landscape){error="missing landscape preparation";return false;}it->second.landscape->Wait();error=it->second.landscape->State().error;return error.empty()&&it->second.landscape->State().ready;}
const RuntimeTerrainStatus* RuntimeTerrain::Status(uint64_t id)const{auto i=records.find(id);return i==records.end()?nullptr:&i->second.landscape->State();}
size_t RuntimeTerrain::LandscapePayloadBytes(uint64_t id)const{auto i=records.find(id);return i==records.end()||!i->second.landscape?0:i->second.landscape->PayloadBytes();}
bool RuntimeTerrain::ReadyToSave()const{for(auto& [id,r]:records)if(r.landscape->State().busy)return false;return true;}
std::vector<uint64_t> RuntimeTerrain::Owners()const{std::vector<uint64_t> out;for(auto& [id,r]:records)out.push_back(id);return out;}
uint64_t RuntimeTerrain::OwnerOf(BodyHandle h)const {
 for(const auto& [id,r]:records)if(r.landscape&&r.landscape->Owns(h))return id;
 return 0;
}
bool RuntimeTerrain::OwnsGenerated(BodyHandle h)const {return OwnerOf(h)!=0;}
bool RuntimeTerrain::IsLandscape(uint64_t id)const {auto it=records.find(id);return it!=records.end()&&bool(it->second.landscape);}
std::vector<BodyHandle> RuntimeTerrain::Bodies(uint64_t id)const {auto it=records.find(id);return it!=records.end()&&it->second.landscape?it->second.landscape->Bodies():std::vector<BodyHandle>{};}
std::vector<TerrainV2::SolidSnapshot> RuntimeTerrain::Solids(glm::vec3 minimum,glm::vec3 maximum)const{
 std::vector<TerrainV2::SolidSnapshot> result;
 for(const auto& [_,record]:records)if(record.landscape){auto sample=record.landscape->Solid();if(sample.Intersects(minimum,maximum))result.push_back(std::move(sample));}
 return result;
}
void RuntimeTerrain::Synchronize(){for(auto& [_,r]:records)if(r.landscape)r.landscape->Synchronize();}
bool RuntimeTerrain::HasLandscapes()const{for(const auto& [_,r]:records)if(r.landscape)return true;return false;}
void RuntimeTerrain::PrepareViews(const std::vector<PlanetView>& views,glm::dvec3 origin,float seconds){for(auto& [_,r]:records)if(r.landscape)r.landscape->PrepareViews(views,origin,seconds);}
bool RuntimeTerrain::CreateLandscape(uint64_t id,TerrainV2::Source source,std::string& error){
 if(auto it=records.find(id);it!=records.end()){
  if(!it->second.landscape||it->second.landscape->State().ready||it->second.landscape->State().busy){error="terrain owner already initialized";return false;}
  records.erase(it); // A failed first preparation owns no published support; permit a corrected retry.
 }
 if(records.size()>=MaxVolumes){error="terrain owner budget exceeded";return false;}
 auto runtime=std::make_unique<TerrainV2::Runtime>(world,id);
 if(!runtime->Create(std::move(source),error))return false;
 records[id].landscape=std::move(runtime);return true;
}
bool RuntimeTerrain::CreateAsset(uint64_t id,const std::string& assetId,std::string& error){
 const auto* assets=world.Resources()?world.Resources()->Assets():nullptr;
 const auto* record=assets?assets->Find(assetId):nullptr;
 if(!record||record->missing||record->type!=AssetType::Terrain){error="registered native terrain asset required";return false;}
 TerrainV2::AssetPages pages;TerrainV2::Source source;
 if(!pages.Open(record->path,error)||!TerrainV2::SampleStorage::Open(record->path,8*1024*1024,source,error))return false;
 const auto fingerprint=pages.Index().fingerprint;
 if(!CreateLandscape(id,std::move(source),error))return false;
 records.at(id).landscape->BindAsset(assetId,fingerprint);return true;
}
bool RuntimeTerrain::InstallPrepared(uint64_t id,TerrainV2::PreparedAsset& prepared,std::string& error){
 const auto definition=world.m_scriptDefinitions.find(id);
 if(definition==world.m_scriptDefinitions.end()||!definition->second.landscape||definition->second.landscape->asset!=prepared.asset||records.count(id)){
  error="prepared terrain source does not match its staged owner";return false;
 }
 auto runtime=std::make_unique<TerrainV2::Runtime>(world,id);
 if(!runtime->Install(prepared,error))return false;
 records[id].landscape=std::move(runtime);return true;
}
bool RuntimeTerrain::Brush(uint64_t id,glm::vec3 point,const TerrainV2::Edit& brush,std::string& error){
 const auto i=records.find(id);
 if(i==records.end()){error="terrain not initialized";return false;}
 return i->second.landscape->Brush(point,brush,error);
}
bool RuntimeTerrain::Surface(uint64_t id,glm::vec3 point,const TerrainV2::SurfaceBrush& brush,std::string& error){
 const auto i=records.find(id);
 if(i==records.end()||!i->second.landscape){error="surface brushes require an initialized V2 landscape";return false;}
 return i->second.landscape->Surface(point,brush,error);
}
void RuntimeTerrain::Advance(){for(auto& [_,r]:records)r.landscape->Advance();}
void RuntimeTerrain::Remove(uint64_t id){records.erase(id);}
void RuntimeTerrain::Draw(Renderer& renderer)const{for(auto& [_,r]:records)r.landscape->Draw(renderer);}
bool RuntimeTerrain::Capture(uint64_t id,std::string& out,std::string& error)const{
 out.clear();const auto i=records.find(id);
 return i==records.end()||i->second.landscape->Capture(out,error);
}
bool RuntimeTerrain::Restore(uint64_t id,const std::string& bytes,std::string& error,bool asynchronous){
 if(bytes.size()>TerrainV2::MaximumSnapshotBytes){error="terrain snapshot exceeds the 8 MiB continuation budget";return false;}
 if(bytes.empty())return true;
 if(const auto i=records.find(id);i!=records.end())return i->second.landscape->Restore(bytes,error,asynchronous);
 auto runtime=std::make_unique<TerrainV2::Runtime>(world,id);
 if(!runtime->Restore(bytes,error,asynchronous))return false;
 records[id].landscape=std::move(runtime);return true;
}
