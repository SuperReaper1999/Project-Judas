#include "TerrainBrush.h"
#include <algorithm>
#include <cmath>

namespace TerrainV2 {
bool ApplySurfaceBrush(Source &source, glm::vec3 point, const SurfaceBrush &brush,
                       std::vector<ChunkKey> &dirty, std::string &error) {
    if (!Validate(source, error))
        return false;
    if (!std::isfinite(point.x + point.y + point.z) || !std::isfinite(brush.radius) ||
        brush.radius < source.spacing || brush.radius > 128 || !std::isfinite(brush.strength) ||
        brush.strength < 0 || brush.strength > 10 || !std::isfinite(brush.height) ||
        brush.layer > 3) {
        error = "invalid terrain surface brush";
        return false;
    }
    auto next = source;
    const float halfX = (source.samplesX - 1) * source.spacing * .5f,
                halfZ = (source.samplesZ - 1) * source.spacing * .5f;
    if (double(point.x) - brush.radius > halfX || double(point.x) + brush.radius < -halfX ||
        double(point.z) - brush.radius > halfZ || double(point.z) + brush.radius < -halfZ) {
        error = "brush outside terrain";
        return false;
    }
    const int x0 = std::max(0, int(std::floor((point.x - brush.radius + halfX) / source.spacing))),
              z0 = std::max(0, int(std::floor((point.z - brush.radius + halfZ) / source.spacing)));
    const int x1 = std::min(int(source.samplesX) - 1,
                            int(std::ceil((point.x + brush.radius + halfX) / source.spacing))),
              z1 = std::min(int(source.samplesZ) - 1,
                            int(std::ceil((point.z + brush.radius + halfZ) / source.spacing)));
    bool changed = false;
    for (int z = z0; z <= z1; ++z)
        for (int x = x0; x <= x1; ++x) {
            const float distance = glm::length(glm::vec2(x * source.spacing - halfX - point.x,
                                                         z * source.spacing - halfZ - point.z));
            if (distance >= brush.radius)
                continue;
            const float t = 1 - distance / brush.radius, falloff = t * t * (3 - 2 * t);
            const size_t i = size_t(z) * source.samplesX + x;
            if (brush.kind == SurfaceBrush::Kind::Paint) {
                const float blend = std::clamp(brush.strength * falloff, 0.f, 1.f);
                glm::vec4 target(0);
                target[brush.layer] = 1;
                next.weights[i] = glm::mix(source.weights[i], target, blend);
                changed |= next.weights[i] != source.weights[i];
                continue;
            }
            float value = source.heights[i];
            switch (brush.kind) {
            case SurfaceBrush::Kind::Raise:
                value += brush.strength * falloff;
                break;
            case SurfaceBrush::Kind::Lower:
                value -= brush.strength * falloff;
                break;
            case SurfaceBrush::Kind::Flatten:
                value =
                    glm::mix(value, brush.height, std::clamp(brush.strength * falloff, 0.f, 1.f));
                break;
            case SurfaceBrush::Kind::Smooth: {
                float sum = 0;
                unsigned count = 0;
                for (int dz = -1; dz <= 1; ++dz)
                    for (int dx = -1; dx <= 1; ++dx) {
                        int xx = x + dx, zz = z + dz;
                        if (xx >= 0 && zz >= 0 && xx < int(source.samplesX) &&
                            zz < int(source.samplesZ)) {
                            sum += source.heights[size_t(zz) * source.samplesX + xx];
                            ++count;
                        }
                    }
                value =
                    glm::mix(value, sum / count, std::clamp(brush.strength * falloff, 0.f, 1.f));
                break;
            }
            case SurfaceBrush::Kind::Paint:
                break;
            }
            if (value <= source.bottom || value >= source.top) {
                error = "brush exceeds terrain vertical bounds; source unchanged";
                return false;
            }
            next.heights[i] = value;
            changed |= value != source.heights[i];
        }
    if (!changed) {
        error = "brush does not change terrain";
        return false;
    }
    dirty = brush.kind == SurfaceBrush::Kind::Paint
                ? std::vector<ChunkKey>{}
                : Affected(source, {Edit::Kind::Remove, point, brush.radius});
    source = std::move(next);
    error.clear();
    return true;
}
} // namespace TerrainV2
