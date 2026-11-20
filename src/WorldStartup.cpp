#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "WorldGraphicsState.h"
#include "PlanetTiles.h"
#include "WorldPresentation.h"
#include "CameraProjection.h"
#include "PerformanceProfiler.h"
#include "fluid/GameWaterRuntime.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

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
    bool uiReady=true;
    if(m_ui&&!m_ui->PrepareStartup(width,height,uiReady,error))return false;
    if(!m_assets){ready=uiReady;return true;}
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
    ready=complete&&uiReady&&m_assets->Stats().loading==0;
    return true;
}


bool RuntimeWorld::CaptureGraphicsState(WorldGraphicsState& output, std::string& error) {
    WorldGraphicsState captured;
    for (auto& emitter : m_particleEmitters) {
        if (!emitter.gpu) continue;
        WorldGraphicsState::Emitter snapshot{emitter.id, {}};
        if (!emitter.gpu->CaptureDeviceState(snapshot.state, error)) {
            error = "Particle emitter " + std::to_string(emitter.id) + ": " + error;
            return false;
        }
        captured.m_emitters.push_back(std::move(snapshot));
    }
    output = std::move(captured);
    return true;
}

bool RuntimeWorld::ReleaseWorldGraphics(Renderer& renderer, std::string& error) {
    for (const auto& emitter : m_particleEmitters)
        if (emitter.gpu && !emitter.gpu->CanMutate(error)) return false;
    for (auto& emitter : m_particleEmitters) {
        if (!emitter.gpu) continue;
        if (!emitter.gpu->Shutdown(error)) return false;
        emitter.gpu.reset();
    }
    ReleasePresentationTargets();
    if (m_gridWater) m_gridWater->ReleaseGraphics(renderer);
    if (m_runtimeTerrain) m_runtimeTerrain->ReleaseGraphics();
    for (auto& [id, planet] : m_planets) planet->ReleaseGraphics();
    for (auto& [id, deformable] : m_deformables) {
        renderer.DestroyMesh(deformable.mesh);
        deformable.mesh = {};
        deformable.mappedRevision = ~std::uint64_t(0);
        deformable.mappedAlpha = -1;
    }
    return true;
}

bool RuntimeWorld::RestoreGraphicsState(const WorldGraphicsState& state,
                                       const GraphicsCapabilities& capabilities, std::string& error) {
    using Pool = JudasParticles::GpuParticles;
    std::vector<std::pair<ParticleEmitter*, std::shared_ptr<Pool>>> restored;
    restored.reserve(state.m_emitters.size());
    for (const auto& saved : state.m_emitters) {
        const auto emitter = std::find_if(m_particleEmitters.begin(), m_particleEmitters.end(),
            [&](const auto& candidate) { return candidate.id == saved.id; });
        if (emitter == m_particleEmitters.end() || emitter->gpu) {
            error = "Particle owner changed during graphics transition: " + std::to_string(saved.id);
            return false;
        }
        auto pool = std::make_shared<Pool>();
        if (!pool->RestoreDeviceState(capabilities, saved.state, {}, error)) {
            error = "Particle emitter " + std::to_string(saved.id) + ": " + error;
            return false;
        }
        restored.emplace_back(&*emitter, std::move(pool));
    }
    for (auto& [emitter, pool] : restored) emitter->gpu = std::move(pool);
    return true;
}
