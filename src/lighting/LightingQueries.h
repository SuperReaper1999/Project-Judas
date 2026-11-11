#pragma once
#include "../PhysicsWorld.h"
#include <optional>
#include <string>
class RuntimeWorld;
namespace JudasLighting {
struct ExposureOptions {
    unsigned maxSources=1024,maxLights=32,maxOcclusionQueries=16;
    float sunOcclusionDistance=128;
    std::optional<glm::vec3> normal;
    PhysicsQueryFilter filter;
};
struct ExposureContributor {
    std::uint64_t source=0; // zero is directional sun
    glm::vec3 potential{0},visible{0};
    bool occlusionKnown=false,occluded=false;
};
struct ExposureSample {
    glm::vec3 color{0},upperColor{0};
    float relativeIlluminance=0,upperIlluminance=0;
    bool complete=true,upperBoundKnown=true,stale=false;
    double sampledAtSeconds=0,ageSeconds=0;
    unsigned sourcesExamined=0,queries=0,unresolvedSources=0;
    std::vector<ExposureContributor> contributors;
};
// Current simulation geometry, no GPU readback or renderer-quality dependency.
// A query-count budget is not a hard wall-time guarantee for arbitrary meshes.
// Unknown occlusion contributes only to the upper estimate, never 'visible'.
bool SampleExposure(const RuntimeWorld&,glm::vec3,const ExposureOptions&,ExposureSample&,std::string& error);
}
