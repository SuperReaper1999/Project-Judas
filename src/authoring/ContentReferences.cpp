#include "ContentReferences.h"
#include "AssetDependencies.h"
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

}
bool DirectDocumentAssets(const std::string& kind,const std::string& text,std::set<AssetId>& out,std::string& error){
    std::set<AssetId> ids;
    if(kind=="scene"||kind=="prefab"){
        Scene scene;if(!LoadSceneFromString(text,scene,error))return false;CollectSceneAssetReferences(scene,ids);
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
 std::set<std::string> auxiliary;return CollectAssetDependencies(record,out,auxiliary,error);
}
