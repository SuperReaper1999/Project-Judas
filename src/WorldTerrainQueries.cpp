#include "CollisionGeometry.h"
#include "PlanetTiles.h"
#include "RuntimeWorld.h"
#include <limits>

std::vector<BodyHandle> RuntimeWorld::OwnedCollisionBodies(EntityId id) const {
    if (!RuntimeDefinition(id))
        return {};
    if (m_runtimeTerrain) {
        auto generated = m_runtimeTerrain->Bodies(id);
        if (!generated.empty())
            return generated;
    }
    if (auto it = m_planets.find(id); it != m_planets.end())
        return it->second->Bodies();
    auto body = RuntimeBody(id);
    return body.IsValid() ? std::vector<BodyHandle>{body} : std::vector<BodyHandle>{};
}
BodyHandle RuntimeWorld::CollisionBodyNear(EntityId id, glm::vec3 point) const {
    const auto candidates = OwnedCollisionBodies(id);
    if (candidates.size() == 1)
        return candidates.front();
    double nearest = std::numeric_limits<double>::infinity();
    BodyHandle result;
    for (auto body : candidates) {
        Shape shape;
        BodyTransform pose;
        if (!m_physics.GetBodyShape(body, shape, pose))
            continue;
        RigidBody rigid;
        rigid.position = pose.position;
        rigid.orientation = pose.rotation;
        for (int i = 0; i < PrimitiveCount(shape); ++i) {
            auto distance = PointGeometry(glm::dvec3(point), PrimitiveAt(shape, rigid, i), nearest);
            if (distance.valid && distance.gap < nearest) {
                nearest = distance.gap;
                result = body;
            }
        }
    }
    return result;
}
