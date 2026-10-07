#include "ContentReferences.h"
#include "NamedAuthoring.h"
#include "SceneSerialization.h"
#include "RuntimeUI.h"
#include "Project.h"
#include "CollisionAsset.h"
#include "ModelLoader.h"
#include "Material.h"
#include "WorldBuilder.h"
#include <fstream>
#include <sstream>
namespace {
void add(std::set<AssetId>& ids,const AssetId& id){if(!id.empty())ids.insert(id);}
void sceneAssets(const Scene& scene,std::set<AssetId>& ids){
    add(ids,scene.Settings().environmentAsset);
    for(const auto& o:scene.Objects()){
        add(ids,o.prefabAsset);
        if(o.render){add(ids,o.render->meshAsset);add(ids,o.render->textureAsset);for(auto& m:o.render->materials)add(ids,m.asset);}
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
}
bool DirectDocumentAssets(const std::string& kind,const std::string& text,std::set<AssetId>& out,std::string& error){
    std::set<AssetId> ids;
    if(kind=="scene"||kind=="prefab"){
        Scene scene;if(!LoadSceneFromString(text,scene,error))return false;sceneAssets(scene,ids);
    }else if(kind=="ui"){
        UIDocument doc;if(!ParseUIDocument(text,doc,error))return false;
        for(auto& e:doc.elements){add(ids,e.texture);add(ids,e.font);}
    }else if(kind=="project"){
        ProjectSettings p;if(!Project::ParseFromString(text,p,error))return false;
        add(ids,p.iconAsset);add(ids,p.worldManifest);
        for(auto& f:p.localization.fonts)add(ids,f);
        for(auto& [_,l]:p.localization.locales){add(ids,l.catalog);for(auto& f:l.fonts)add(ids,f);}
    }else if(kind=="recipe"){
        WorldRecipe r;if(!ParseWorldRecipe(text,r,error))return false;
        for(auto& id:{r.prefabAsset,r.renderAsset,r.proxyAsset,r.collisionAsset})add(ids,id);
    }else if(!ValidateAuthoredDocument(text,kind,error))return false;
    out=std::move(ids);error.clear();return true;
}
bool DirectAssetDependencies(const AssetRecord& record,std::set<AssetId>& out,std::string& error){
    std::set<AssetId> ids;
    if(record.type==AssetType::Prefab||record.type==AssetType::UI){
        std::ifstream f(record.path,std::ios::binary);std::ostringstream text;text<<f.rdbuf();
        if(!DirectDocumentAssets(record.type==AssetType::Prefab?"prefab":"ui",text.str(),ids,error))return false;
    }else if(record.type==AssetType::Material){
        MaterialDefinition material;if(!LoadMaterial(record.path,material,error))return false;
        for(auto& map:material.maps)add(ids,map.asset);
    }else if(record.type==AssetType::Mesh){
        MeshData mesh;if(!LoadModelMesh(record.path,mesh,error))return false;
        for(auto& material:mesh.materials)for(auto& map:material.maps)add(ids,map.asset);
    }else if(record.type==AssetType::Collision){
        CollisionAsset collision;if(!LoadCollisionAsset(record.path,collision,error))return false;add(ids,collision.sourceAsset);
    }
    // Project-owned script strings are not statically linked. M38's all-registered
    // asset policy remains authoritative for dynamic IDs and other cooked types.
    out=std::move(ids);error.clear();return true;
}
