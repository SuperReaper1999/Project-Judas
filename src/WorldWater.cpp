#include "RuntimeWorld.h"
#include "Project.h"
#include <stdexcept>

void RuntimeWorld::ConfigureProject(const ProjectSettings& project) {
    if (IsBuilt()) throw std::logic_error("Project settings require a fresh world; restart Play");
    m_advancedFluids=project.advancedFluidDynamics;
    m_basicWaterEnabled=project.basicWater;
    m_waterSwimming=project.waterSwimming;
    legacyGameplay=project.legacyGameplay;
    audioGroups=project.audio;
}
std::vector<BasicWaterInstance> RuntimeWorld::BasicWaters(float alpha) const {
    std::vector<BasicWaterInstance> result;
    if (!m_basicWaterEnabled) return result;
    result.reserve(m_basicWaterOwners.size());
    for (auto id:m_basicWaterOwners) {
        const auto* definition=RuntimeDefinition(id);
        if (!definition || !definition->basicWater || !definition->basicWater->enabled || !IsPublished(id)) continue;
        const auto pose=PresentedTransform(id,definition->transform,alpha);
        result.push_back({*definition->basicWater,pose.position,pose.rotation,id});
    }
    return result;
}
BasicWaterSample RuntimeWorld::SampleWater(glm::vec3 center,glm::vec3 up,float halfHeight,float radius) const {
    if (!m_waterSwimming || m_basicWaterOwners.empty()) return {};
    return SampleBasicWater(BasicWaters(),center,up,halfHeight,radius);
}
std::vector<glm::vec2> RuntimeWorld::WaterOpticalPaths(const glm::mat4& view,const glm::mat4& projection,
                                                    unsigned columns,unsigned rows,float alpha) const {
    const auto waters=BasicWaters(alpha);
    auto result=waters.empty()?std::vector<glm::vec2>{}:BasicWaterOpticalPaths(waters,view,projection,columns,rows);
    if (m_advancedFluids && !m_liquid->States().empty()) {
        auto advanced=m_liquid->OpticalPaths(view,projection,columns,rows,alpha);
        if (result.empty()) return advanced;
        for (size_t i=0;i<result.size()&&i<advanced.size();++i)
            if (advanced[i].y>advanced[i].x && (result[i].y<=result[i].x || advanced[i].x<result[i].x)) result[i]=advanced[i];
    }
    return result;
}
