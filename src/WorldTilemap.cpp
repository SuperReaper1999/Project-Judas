#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "NetworkSession.h"
#include "authoring/Tilemap.h"
#include "PlanarAuthoring.h"

#include <limits>
#include <set>

bool RuntimeWorld::EditTilemapCell(EntityId id,const std::string& layer,glm::ivec2 cell,
                                   const std::string& frame,bool flipX,bool flipY,std::string& error) {
    error.clear();
    if(auto* network=NetworkIfLoaded();network&&network->Active()){
        error="runtime tile editing is unavailable during an online session";
        return false;
    }
    const auto key=TilemapKey(id);
    const auto found=m_settings.authoringRecipes.find(key);
    const auto* root=RuntimeDefinition(id);
    const auto* assets=m_assets?m_assets->Assets():nullptr;
    if(!root||found==m_settings.authoringRecipes.end()||!assets||legacyGameplay||IsTransientEntity(id)){
        error="runtime tile edit requires an authored tilemap and a project-owned world";
        return false;
    }
    if(root->parent||root->transform.scale!=glm::vec3(1)||root->transform.rotation!=glm::quat(1,0,0,0)){
        error="runtime tile edit requires an unparented, unrotated unit-scale grid";
        return false;
    }

    TilemapRecipe recipe;
    if(!ParseTilemap(found->second,recipe,error)||recipe.root!=id){
        if(error.empty())error="tilemap recipe owner mismatch";
        return false;
    }
    if(!PaintTilemap(recipe,layer,cell,cell,frame,frame.empty(),flipX,flipY,error))return false;

    Scene source;
    source.Settings()=m_settings;
    for(const auto& definition:ScriptObjects()){
        const auto* local=m_hierarchy.Find(definition.id);
        auto object=definition;
        if(local)object.transform=local->transform;
        if(!source.InsertObject(object)){
            error="duplicate tilemap scene identity";
            return false;
        }
    }
    if(m_nextRuntimeId>EntityId(std::numeric_limits<int64_t>::max())-kTilemapMaxCells*2){
        error="tilemap edit exceeds runtime entity identity range";
        return false;
    }
    source.SetNextId(m_nextRuntimeId);
    Scene result;
    TilemapRecipe baked;
    if(!BakeTilemap(source,recipe,*assets,result,baked,error,true))return false;
    bool planar=false;
    if(!ValidatePlanarScene(result,planar,error))return false;
    if(result.Objects().size()>8192){
        error="tilemap edit would exceed the save entity limit";
        return false;
    }

    auto replacementRecipe=std::move(result.Settings().authoringRecipes.at(key));
    std::set<EntityId> retained;
    std::vector<const SceneObject*> additions;
    for(const auto& entry:baked.generated){
        retained.insert(entry.entity);
        if(source.Find(entry.entity))continue;
        const auto* object=result.Find(entry.entity);
        const auto& local=object->transform.position;
        if((root->transform.position+local)-root->transform.position!=local){
            error="tile position cannot be restored exactly in this grid frame";
            return false;
        }
        if(!ValidateEntityCreation(*object,error))return false;
        additions.push_back(object);
    }
    // Construct every replacement before retiring anything. This call runs
    // synchronously on the world thread, outside physics/renderer iteration.
    std::vector<EntityId> created;
    created.reserve(additions.size());
    const auto oldNextId=m_nextRuntimeId;
    for(const auto* object:additions){
        const auto createdId=CreateEntity(*object,nullptr,&error);
        if(!createdId){
            for(auto prior:created)RemoveRegionObject(prior);
            m_nextRuntimeId=oldNextId;
            return false;
        }
        created.push_back(createdId);
    }
    for(const auto& entry:recipe.generated)
        if(!retained.count(entry.entity))RemoveRegionObject(entry.entity);
    found->second.swap(replacementRecipe);
    return true;
}
