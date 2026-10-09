#pragma once
#include "GravityVolume.h"
#include <glm/gtc/quaternion.hpp>

// Initial root scenes and streamed regions share the same spatial predicate.
class OrientedGravityVolume final:public GravityVolume {
    glm::vec3 m_position,m_halfExtents;
    glm::quat m_inverse;
public:
    OrientedGravityVolume(glm::vec3 position,glm::quat rotation,glm::vec3 halfExtents)
        :m_position(position),m_halfExtents(halfExtents),m_inverse(glm::inverse(glm::normalize(rotation))){}
    bool Contains(const glm::vec3& point)const override {
        auto p=glm::abs(m_inverse*(point-m_position));
        return p.x<=m_halfExtents.x&&p.y<=m_halfExtents.y&&p.z<=m_halfExtents.z;
    }
};
