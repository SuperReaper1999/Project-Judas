#include "GameWaterRuntimeInternal.h"
#include "GameWaterSurface.h"
#include "Renderer.h"
#include "RuntimeWorld.h"

namespace JudasWater {
void WorldSystem::Draw(Renderer &renderer) const {
    for (const auto &[id, region] : regions_) {
        if (!world_.IsPublished(id) || !world_.RuntimeDefinition(id) || !world_.RenderVisible(id))
            continue;
        renderer.SetRenderLayer(world_.RenderLayerOf(id));
        if (!renderer.AllowsLayer(world_.RenderLayerOf(id)))
            continue;
        const auto &c = region->grid.Configuration();
        VisualBounds bounds;
        for (int z : {0, 1})
            for (int y : {0, 1})
                for (int x : {0, 1})
                    bounds.Include(
                        region->grid.ToWorld(glm::vec3(c.cells) * c.cellSize * glm::vec3(x, y, z)));
        if (!renderer.IsVisible(bounds))
            continue;
        if (!region->mesh.IsValid() || region->meshRevision != region->revision) {
            auto mesh = BuildPoolSurface(region->grid);
            if (!region->mesh.IsValid())
                region->mesh = renderer.CreateMesh(mesh);
            else
                renderer.UpdateMeshVertices(region->mesh, mesh.vertices);
            region->meshRevision = region->revision;
        }
        BasicWaterSettings style;
        style.color = region->authored.color;
        style.opacity = region->authored.opacity;
        renderer.DrawWaterSurface(region->mesh, style, c.rotation * glm::vec3(0, 1, 0));
    }
    renderer.SetRenderLayer(0);
}
} // namespace JudasWater
