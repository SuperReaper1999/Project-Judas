#include "GameWaterRuntimeInternal.h"
#include "RuntimeWorld.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace JudasWater {
BasicWaterSample WorldSystem::Sample(glm::vec3 center, glm::vec3 up, float halfHeight,
                                     float radius) const {
    BasicWaterSample result;
    if (!std::isfinite(glm::dot(center, center)) || !std::isfinite(glm::dot(up, up)) ||
        glm::length(up) < 1e-6f || !std::isfinite(halfHeight) || halfHeight < 0 ||
        !std::isfinite(radius) || radius <= 0)
        return result;
    up = glm::normalize(up);
    const auto reference = std::abs(up.y) < .9f ? glm::vec3(0, 1, 0) : glm::vec3(0, 0, 1);
    const auto right = glm::normalize(glm::cross(up, reference));
    SolidBox capsule;
    capsule.center = center;
    capsule.radius = radius;
    capsule.halfHeight = halfHeight;
    capsule.half = {radius, radius + halfHeight, radius};
    capsule.kind = SolidBox::Kind::Capsule;
    capsule.rotation = glm::quat_cast(glm::mat3(right, up, glm::cross(right, up)));
    for (const auto &[id, region] : regions_) {
        if (!world_.IsPublished(id) || !world_.RuntimeDefinition(id))
            continue;
        glm::vec3 immersedCentre;
        const float immersion =
            region->grid.ImmersedBox(capsule, immersedCentre) / SolidVolume(capsule);
        if (immersion > result.immersion)
            result = {immersion, region->grid.Configuration().density,
                      region->grid.Velocity(center)};
    }
    return result;
}
std::vector<glm::vec2> WorldSystem::OpticalPaths(const glm::mat4 &view, const glm::mat4 &projection,
                                                 unsigned columns, unsigned rows) const {
    if (!columns || !rows || columns > 256 || rows > 256 || regions_.empty())
        return {};
    std::vector<glm::vec2> result(std::size_t(columns) * rows, glm::vec2(0));
    const auto inverse = glm::inverse(projection * view);
    for (unsigned y = 0; y < rows; ++y)
        for (unsigned x = 0; x < columns; ++x) {
            const glm::vec2 uv((x + .5f) / columns, (y + .5f) / rows);
            const auto a = inverse * glm::vec4(uv * 2.f - 1.f, -1, 1),
                       b = inverse * glm::vec4(uv * 2.f - 1.f, 1, 1);
            if (std::abs(a.w) < 1e-12f || std::abs(b.w) < 1e-12f)
                continue;
            const auto start = glm::vec3(a) / a.w, end = glm::vec3(b) / b.w;
            const auto direction = end - start;
            const float length = glm::length(direction);
            if (length <= 0)
                continue;
            float first = length, last = 0;
            for (const auto &[id, region] : regions_) {
                if (!world_.IsPublished(id) || !world_.RuntimeDefinition(id))
                    continue;
                const auto &grid = region->grid;
                const auto &c = grid.Configuration();
                const auto local = grid.ToLocal(start),
                           ray = glm::conjugate(c.rotation) * (direction / length);
                float lo = 0, hi = length;
                for (int axis = 0; axis < 3; ++axis) {
                    if (std::abs(ray[axis]) < 1e-8f) {
                        if (local[axis] < 0 || local[axis] > c.cells[axis] * c.cellSize) {
                            hi = -1;
                            break;
                        }
                        continue;
                    }
                    auto aa = -local[axis] / ray[axis],
                         bb = (c.cells[axis] * c.cellSize - local[axis]) / ray[axis];
                    lo = std::max(lo, std::min(aa, bb));
                    hi = std::min(hi, std::max(aa, bb));
                }
                if (lo >= hi)
                    continue;
                const unsigned steps =
                    std::max(1u, unsigned(std::ceil((hi - lo) / (c.cellSize * .5f))));
                for (unsigned i = 0; i < steps; ++i) {
                    const float t = lo + (hi - lo) * (i + .5f) / steps;
                    if (grid.WetPoint(start + direction * (t / length))) {
                        first = std::min(first, lo + (hi - lo) * i / steps);
                        last = std::max(last, lo + (hi - lo) * (i + 1) / steps);
                    }
                }
            }
            if (last > first)
                result[y * columns + x] = {first, last};
        }
    return result;
}
} // namespace JudasWater
