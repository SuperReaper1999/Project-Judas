#include "GameWaterRuntimeInternal.h"
#include "Renderer.h"
#include "RuntimeWorld.h"
#include "PerformanceProfiler.h"

namespace JudasWater {
void WorldSystem::Draw(Renderer &renderer) const {
    const auto eye=glm::vec3(glm::inverse(renderer.ViewMatrix())[3]);
    for (const auto &[id, region] : regions_) {
        if (!world_.IsPublished(id) || !world_.RuntimeDefinition(id) || !world_.RenderVisible(id))continue;
        renderer.SetRenderLayer(world_.RenderLayerOf(id));
        if (!renderer.AllowsLayer(world_.RenderLayerOf(id)))continue;
        const auto &c=region->grid.Configuration();
        VisualBounds bounds;
        for(int z:{0,1})for(int y:{0,1})for(int x:{0,1}){
            const auto p=region->grid.ToWorld(glm::vec3(c.cells)*c.cellSize*glm::vec3(x,y,z));
            bounds.Include(p+c.rotation*glm::vec3(0,.02f,0));
            bounds.Include(p-c.rotation*glm::vec3(0,.02f,0));
        }
        if(!renderer.IsVisible(bounds))continue;
        // No field reconstruction, tessellation, upload or fine wave work for
        // an unseen region. Cameras share immutable revision caches and LODs.
        const float distance=glm::length(eye-region->grid.ToWorld(glm::vec3(c.cells)*c.cellSize*.5f));
        const unsigned lod=distance<35?0:distance<100?1:2;
        const unsigned subdivision=lod==0?4:lod==1?2:1;
        if(region->surfaceRevision!=region->revision){
            JUDAS_PROFILE_SCOPE("Grid water visual field");
            region->surface=BuildSurfaceField(region->grid);region->surfaceRevision=region->revision;
            if(!region->surface.SameTopology(region->surfaceReference)){
                region->surfaceReference=region->surface;
                ++region->surfaceTopology;
            }
        }
        if(!region->meshes[lod].IsValid()||region->meshRevisions[lod]!=region->surfaceTopology){
            JUDAS_PROFILE_SCOPE("Grid water fine surface");
            auto mesh=BuildPoolSurface(region->surface,subdivision);
            if(!region->meshes[lod].IsValid())region->meshes[lod]=renderer.CreateMesh(mesh);
            else renderer.UpdateMeshVertices(region->meshes[lod],mesh.vertices);
            region->meshRevisions[lod]=region->surfaceTopology;
            JUDAS_PROFILE_COUNTER("Grid water visible surface vertices",double(mesh.vertices.size()),ProfileCounterMode::Sum);
        }
        BasicWaterSettings style;style.color=region->authored.color;style.opacity=region->authored.opacity;
        renderer.DrawGridWater(region->meshes[lod],region->surface,style,world_.SimulationTimeSeconds(),region->revision);
    }
    renderer.SetRenderLayer(0);
}
} // namespace JudasWater
