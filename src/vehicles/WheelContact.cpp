#include "WheelContact.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace JudasVehicles {
PhysicsCastHit FindSupport(PhysicsWorld& world, BodyHandle chassis, const WheelSettings& settings,
                          glm::vec3 anchor, glm::vec3 down, glm::vec3 axle,
                          std::vector<BodyHandle>& candidates) {
    PhysicsQueryFilter filter;
    filter.includeLayers = settings.collisionMask;
    filter.requiredTags = settings.requiredTags;
    filter.excludedTags = settings.excludedTags;
    filter.ignoredBodies.push_back(chassis);
    filter.normalAxis = -down;
    filter.minimumNormalDot = 1e-4f;

    const auto endpoint = anchor + down * settings.maxLength;
    const glm::vec3 extent(settings.radius + settings.width * .5f);
    world.QueryBodiesInAabbInto(glm::min(anchor, endpoint) - extent,
                               glm::max(anchor, endpoint) + extent, candidates, filter);
    for (const auto body : candidates) {
        unsigned layer = 0;
        CategoryMask mask = 0;
        if (world.GetCollisionFilter(body, layer, mask) &&
            !(mask & (CategoryMask(1) << settings.collisionLayer)))
            filter.ignoredBodies.push_back(body);
    }

    // Three tracks across the width, each with five rounded tread samples.
    // Their union is deliberately an approximation, not claimed cylinder CCD.
    // Rounded probes avoid the zero-width rays that miss small raised edges.
    const float probeRadius = std::min(settings.radius * .2f, settings.width * .5f);
    const float track = settings.width * .5f - probeRadius;
    const auto forward = glm::normalize(glm::cross(axle, -down));
    constexpr std::array<float, 5> angles{{-1.047197551f, -.523598776f, 0,
                                          .523598776f, 1.047197551f}};
    PhysicsCastHit best;
    glm::dvec3 points{0}, normals{0};
    unsigned witnesses=0;
    for (const float lateral : {-track, 0.f, track}) {
        for (const float angle : angles) {
            const auto offset = axle * lateral + (settings.radius - probeRadius) *
                (down * std::cos(angle) + forward * std::sin(angle));
            auto hit = world.SphereCast(anchor + offset, probeRadius, down,
                                              settings.maxLength, filter);
            if(hit.hit) {
                // Probes discover finite surface patches; the circular tread's
                // contact with each patch plane is analytic. Leaving the probe
                // witness on a sampled rim lobe produces a false axle torque
                // even on a flat road when the chassis pitches slightly.
                const auto radial=hit.normal-axle*glm::dot(hit.normal,axle);
                const float radialLength=glm::length(radial);
                if(radialLength<1e-5f)continue;
                const float lateralPoint=std::clamp(glm::dot(hit.point-(anchor+down*hit.distance),axle),
                                                     -settings.width*.5f,settings.width*.5f);
                const auto contactOffset=axle*lateralPoint-settings.radius*radial/radialLength;
                const float travel=glm::dot(hit.normal,hit.point-anchor-contactOffset)/glm::dot(hit.normal,down);
                if(travel>settings.maxLength || std::abs(travel-hit.distance)>probeRadius)continue;
                hit.initialOverlap=travel<0;
                hit.distance=std::max(0.f,travel);
                hit.point=anchor+down*hit.distance+contactOffset;
            }
            if (hit.hit && (!best.hit || hit.distance < best.distance-1e-5f ||
                           (std::abs(hit.distance-best.distance)<=1e-5f && hit.body.id < best.body.id))) {
                best = hit;
                points=glm::dvec3(hit.point);normals=glm::dvec3(hit.normal);witnesses=1;
            } else if(hit.hit && hit.body.id==best.body.id &&
                      std::abs(hit.distance-best.distance)<=1e-5f) {
                points+=glm::dvec3(hit.point);normals+=glm::dvec3(hit.normal);++witnesses;
            }
        }
    }
    if(witnesses) {
        // Equal-height tread contacts form one resultant patch. Picking the
        // first edge would create a spurious steering torque on a flat road.
        best.point=glm::vec3(points/double(witnesses));
        best.normal=glm::vec3(glm::normalize(normals));
    }
    return best;
}
}
