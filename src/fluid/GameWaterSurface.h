#pragma once
#include "GameWater.h"
#include "MeshData.h"
namespace JudasWater {
// Render-only intervals. They continue water across moving-solid exclusions;
// they never change the solver's quantities, collision or buoyancy witnesses.
struct SurfaceField {
    glm::ivec3 cells{0};
    float spacing = 1;
    glm::vec3 origin{0};
    glm::quat rotation{1,0,0,0};
    // Lower/upper boundary, valid flag, speed; one interval per resolved run.
    std::vector<glm::vec4> intervals;
    std::vector<glm::vec2> flow;
    struct Sample {float height=0,bottom=0,coverage=0;glm::vec2 gradient{0},velocity{0};};
    int Index(int x,int y,int z) const;
    Sample At(float x,float z,float referenceHeight) const;
    bool SameTopology(const SurfaceField&) const;
};
SurfaceField BuildSurfaceField(const Grid &);
MeshData BuildPoolSurface(const SurfaceField &, unsigned subdivisions=4);
MeshData BuildPoolSurface(const Grid &);
} // namespace JudasWater
