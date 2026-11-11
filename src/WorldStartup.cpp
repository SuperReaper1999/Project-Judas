#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "WorldPresentation.h"
#include "CameraProjection.h"
#include "PerformanceProfiler.h"
#include <glm/gtc/matrix_transform.hpp>

bool RuntimeWorld::PrepareStartup(Renderer& renderer,int width,int height,bool& ready,std::string& error){
    JUDAS_PROFILE_SCOPE("Startup resource preparation");
    ready=false;error.clear();
    viewportWidth=width;viewportHeight=height;
    PrepareDeformableMechanics();
    UpdateAnimations(0);
    for(auto id:m_deformableOwners)if(IsPublished(id)){
        std::string problem;
        if(!RuntimeDeformable(id,problem)){
            if(!problem.empty()&&problem!="loading"){error="Startup deformable "+std::to_string(id)+": "+problem;return false;}
            return true;
        }
        if(!problem.empty()){error="Startup deformable "+std::to_string(id)+": "+problem;return false;}
    }
    // Submit one hidden, zero-clock view through the real presentation path.
    // This discovers streamed meshes, material maps, camera targets and derived
    // skins without advancing simulation or inventing a second dependency list.
    glm::mat4 view(1);
    if(this->view){const auto& pose=this->view->pose;view=glm::lookAt(pose.position,
        pose.position+pose.rotation*glm::vec3(0,0,-1),pose.rotation*glm::vec3(0,1,0));}
    const auto camera=BuildCameraFrame(glm::vec3(0),glm::quat(1,0,0,0),60,.1f,1000,CameraLens{},width,height);
    RenderWorldFrame(renderer,width,height,*this,view,camera.projection,glm::vec3(glm::inverse(view)[3]),1,0);
    if(!m_assets){ready=true;return true;}
    const auto required=[&](const AssetId& id){
        const auto state=m_assets->StateOf(id);
        const auto* record=m_assets->Assets()?m_assets->Assets()->Find(id):nullptr;
        // Disabled/streamed audio can retain ownership without requesting a
        // buffered decode. It is not an unfinished visual dependency.
        if(state==ResourceState::Unloaded&&record&&record->type==AssetType::Audio)return true;
        if(state==ResourceState::Failed||state==ResourceState::Cancelled){
            error="Startup asset failed: "+(record?record->relativePath:id);return false;
        }
        return state==ResourceState::Ready;
    };
    bool complete=true;
    for(const auto& id:m_referencedAssets)complete=required(id)&&complete;
    for(const auto& entry:m_renderAssetDemand)for(const auto& id:entry.second)complete=required(id)&&complete;
    if(!error.empty())return false;
    ready=complete&&m_assets->Stats().loading==0;
    return true;
}
