#include "RuntimeWorld.h"
#include "Project.h"
#include "fluid/GameWaterRuntime.h"
#include <stdexcept>

void RuntimeWorld::ConfigureProject(const ProjectSettings& project) {
    if (IsBuilt()) throw std::logic_error("Project settings require a fresh world; restart Play");
    m_advancedFluids=project.advancedFluidDynamics;
    m_basicWaterEnabled=project.basicWater;
    m_waterSwimming=project.waterSwimming;
    audioGroups=project.audio;
}
bool RuntimeWorld::SetBasicWater(EntityId id,const BasicWaterSettings& settings){
    const auto* definition=RuntimeDefinition(id);
    std::string error;
    if(!definition||!definition->basicWater||!ValidateBasicWater(settings,error))return false;
    m_scriptDefinitions.at(id).basicWater=settings;
    if(auto* record=FindEntity(id);record&&record->definition.basicWater)
        record->definition.basicWater=settings;
    return true;
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
    if (!m_waterSwimming) return {};
    auto result=SampleBasicWater(BasicWaters(),center,up,halfHeight,radius);
    if(m_gridWater){auto grid=m_gridWater->Sample(center,up,halfHeight,radius);
        if(grid.immersion>result.immersion)result=grid;}
    return result;
}
