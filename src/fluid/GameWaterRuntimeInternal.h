#pragma once
#include "GameWaterRuntime.h"
#include "Narrowphase.h"
#include "render/RenderTypes.h"

namespace JudasWater {
struct WorldSystem::Region {
    std::uint64_t owner = 0, revision = 1;
    SceneFluidVolumeComponent authored;
    Grid grid, staging;
    bool paused = false;
    std::string error;
    double runtimeMs = 0, preparationMs = 0, couplingMs = 0;
    unsigned geometryBakes = 0;
    std::vector<glm::vec3> gravitySignature;
    bool RefreshGravity(RuntimeWorld &, Grid &, std::string &);
    struct StaticGeometry {
        BodyHandle handle;
        Shape shape;
        BodyTransform pose;
    };
    std::vector<StaticGeometry> statics;
    std::vector<StaticGeometry> pendingStatics;
    bool PrepareGeometry(RuntimeWorld &, Grid &, std::vector<SolidBox> &, std::vector<BodyHandle> &,
                         std::string &);
    mutable MeshHandle mesh;
    mutable std::uint64_t meshRevision = 0;
};
} // namespace JudasWater
