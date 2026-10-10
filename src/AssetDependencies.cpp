#include "AssetDependencies.h"
#include "RuntimeUI.h"
#include "CollisionAsset.h"
#include "Deformable.h"
#include "Material.h"
#include "ModelLoader.h"
#include "ModelArchive.h"
#include "AsyncFile.h"
#include "SceneSerialization.h"
#include "GltfLoader.h"
#include <filesystem>
#include "../third_party/nlohmann/json.hpp"
#include <memory>
namespace {void add(std::set<AssetId>& ids,const AssetId& id){if(!id.empty())ids.insert(id);}}
void CollectSceneAssetReferences(const Scene& scene,std::set<AssetId>& ids){
 add(ids,scene.Settings().environmentAsset);
 if(!scene.Settings().appearanceResetState.empty()){auto baseline=scene.Settings();std::string error;if(DecodeAppearanceState(baseline.appearanceResetState,baseline,error))add(ids,baseline.environmentAsset);}
 for(const auto& o:scene.Objects()){
  add(ids,o.prefabAsset);
 if(o.render){
   add(ids,o.render->meshAsset);add(ids,o.render->textureAsset);
   auto overrides=[&](const MaterialOverride& value){for(const auto& texture:value.textures)if(texture)add(ids,*texture);};
   overrides(o.render->instanceOverrides);
   for(const auto& slot:o.render->materials){add(ids,slot.asset);overrides(slot.overrides);}
   for(const auto* bindings:{&o.render->partMaterials,&o.render->runtimeMaterials})for(const auto& [_,slot]:*bindings){add(ids,slot.asset);overrides(slot.overrides);}
 }
  if(o.body){add(ids,o.body->collisionAsset);add(ids,o.body->physicalMaterial);for(auto& b:o.body->compoundBoxes)add(ids,b.assetId);}
  for(auto& s:o.scripts)add(ids,s.asset);
  if(o.ui)add(ids,o.ui->asset);
  if(o.particleEmitter)add(ids,o.particleEmitter->textureAsset);
  if(o.audioEmitter)add(ids,o.audioEmitter->asset);
  if(o.audioZone)add(ids,o.audioZone->asset);
  if(o.navigationSurface)add(ids,o.navigationSurface->asset);
  if(o.deformable)add(ids,o.deformable->asset);
  if(o.liquidBasin){add(ids,o.liquidBasin->geometry);add(ids,o.liquidBasin->asset);}
  if(o.liquidContainer)add(ids,o.liquidContainer->geometry);
 }
}
bool CollectAssetDependencies(const AssetRecord& record,std::set<AssetId>& out,std::set<std::string>& auxiliary,std::string& error){
 std::set<AssetId> ids;
 if(record.type==AssetType::Prefab){Scene s;if(!LoadSceneFromFile(record.path,s,error))return false;CollectSceneAssetReferences(s,ids);}
 else if(record.type==AssetType::UI){UIDocument d;if(!LoadUIDocument(record.path,d,error))return false;for(auto& e:d.elements){add(ids,e.texture);add(ids,e.font);}}
 else if(record.type==AssetType::Material){MaterialDefinition m;if(!LoadMaterial(record.path,m,error))return false;for(auto& map:m.maps)add(ids,map.asset);}
 else if(record.type==AssetType::Mesh){std::vector<MaterialDefinition> materials;
  if(std::filesystem::path(record.path).extension()==".judasmodel"){std::vector<uint8_t> bytes;std::string provenance;if(!ReadWholeFile(record.path,bytes,error)||!ReadModelArchiveMetadata(bytes.data(),bytes.size(),provenance,materials,error))return false;if(!provenance.empty()){try{auto p=nlohmann::json::parse(provenance);if(p.value("kind",std::string())=="terrain"){add(ids,p.at("appearance"));for(auto& id:p.at("layers"))add(ids,id);}}catch(const std::exception& x){error=x.what();return false;}}}
  else{MeshData m;if(!LoadModelMesh(record.path,m,error))return false;materials=std::move(m.materials);}
  for(auto& m:materials)for(auto& map:m.maps)add(ids,map.asset);
  auto ext=std::filesystem::path(record.path).extension();
  if(ext==".gltf"||ext==".glb"){
   std::vector<std::string> paths;
   if(!GatherGltfDependencies(record.path,paths,error))return false;
   auxiliary.insert(paths.begin(),paths.end());
  }
 }else if(record.type==AssetType::Collision){CollisionAsset c;if(!LoadCollisionAsset(record.path,c,error))return false;add(ids,c.sourceAsset);}
 else if(record.type==AssetType::Deformable){std::vector<uint8_t> b;DeformableAsset d;if(!ReadWholeFile(record.path,b,error)||!DecodeDeformableAsset(b,d,error))return false;add(ids,d.sourceAsset);}
 out=std::move(ids);error.clear();return true;
}
