#include "CollisionGeometry.h"
#include "PlanetTiles.h"
#include "RuntimeWorld.h"
#include <algorithm>
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
    size_t bestOrder = std::numeric_limits<size_t>::max();
    struct Candidate {
        BodyHandle body;
        Shape shape;
        BodyTransform pose;
        double lowerBound;
        size_t order = 0;
    };
    std::vector<Candidate> ordered;
    for (auto body : candidates) {
        Candidate candidate;
        candidate.body = body;
        candidate.order = ordered.size();
        if (!m_physics.GetBodyShape(body, candidate.shape, candidate.pose, false))
            continue;
        const auto bounds =
            ShapeAabb(candidate.shape, candidate.pose.position, candidate.pose.rotation);
        const auto delta = glm::max(glm::max(glm::dvec3(bounds.min) - glm::dvec3(point),
                                             glm::dvec3(point) - glm::dvec3(bounds.max)),
                                    glm::dvec3(0));
        candidate.lowerBound = glm::length(delta);
        ordered.push_back(std::move(candidate));
    }
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const auto &a, const auto &b) { return a.lowerBound < b.lowerBound; });
    for (const auto &candidate : ordered) {
        if (candidate.lowerBound > nearest)
            break;
        const auto &shape = candidate.shape;
        const auto &pose = candidate.pose;
        const auto body = candidate.body;
        RigidBody rigid;
        rigid.position = pose.position;
        rigid.orientation = pose.rotation;
        for (int i = 0; i < PrimitiveCount(shape); ++i) {
            auto distance = PointGeometry(glm::dvec3(point), PrimitiveAt(shape, rigid, i), nearest);
            if (distance.valid && (distance.gap < nearest ||
                                   (distance.gap == nearest && candidate.order < bestOrder))) {
                nearest = distance.gap;
                bestOrder = candidate.order;
                result = body;
            }
        }
    }
    return result;
}
