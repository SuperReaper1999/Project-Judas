#pragma once
#include "Scene.h"

// A chart translation changes positions only. Directions, velocities, local
// attachment frames, immutable assets and absolute planetary/weather data stay put.
inline void TranslateOriginPoint(glm::vec3& point,glm::dvec3 delta) {
    point=glm::vec3(glm::dvec3(point)-delta);
}
inline void TranslateOriginMatrix(glm::mat4& matrix,glm::dvec3 delta) {
    auto point=glm::vec3(matrix[3]);TranslateOriginPoint(point,delta);matrix[3]=glm::vec4(point,matrix[3].w);
}
inline void TranslateOriginMatrix(glm::dmat4& matrix,glm::dvec3 delta) {
    matrix[3]-=glm::dvec4(delta,0);
}
inline void TranslateIKTargets(std::vector<FullBodyIKTarget>& targets,glm::dvec3 delta) {
    for(auto& target:targets)if(target.space==IKTargetSpace::World)target.position-=delta;
}
inline void TranslateOriginIntent(SceneObject& object,glm::dvec3 delta) {
    if(object.animation){
        for(auto& limb:object.animation->limbs){TranslateOriginPoint(limb.target,delta);TranslateOriginPoint(limb.pole,delta);}
        if(object.animation->fullBodyIK)TranslateIKTargets(object.animation->fullBodyIK->targets,delta);
    }
}
