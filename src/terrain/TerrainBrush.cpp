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
    const double reach = BrushReach(source, brush.radius);
    if (double(point.x) - reach > halfX || double(point.x) + reach < -halfX ||
        double(point.z) - reach > halfZ || double(point.z) + reach < -halfZ) {
        error = "brush outside terrain";
        return false;
    }
    auto lower = [&](float at, float half) {
        return source.chart ? int(std::floor((double(at) - reach + half) / source.spacing))
                            : int(std::floor((at - brush.radius + half) / source.spacing));
    };
    auto upper = [&](float at, float half) {
        return source.chart ? int(std::ceil((double(at) + reach + half) / source.spacing))
                            : int(std::ceil((at + brush.radius + half) / source.spacing));
    };
    const int x0 = std::max(0, lower(point.x, halfX)), z0 = std::max(0, lower(point.z, halfZ));
    const int x1 = std::min(int(source.samplesX) - 1, upper(point.x, halfX)),
              z1 = std::min(int(source.samplesZ) - 1, upper(point.z, halfZ));
    bool changed = false;
    for (int z = z0; z <= z1; ++z)
        for (int x = x0; x <= x1; ++x) {
            const auto i = size_t(z) * source.samplesX + x;
            const float distance =
                source.chart ? float(glm::length(source.chart->LocalPoint(
                                                     {x * source.spacing - halfX, source.heights[i],
                                                      z * source.spacing - halfZ}) -
                                                 source.chart->LocalPoint(point)))
                             : glm::length(glm::vec2(x * source.spacing - halfX - point.x,
                                                     z * source.spacing - halfZ - point.z));
            if (distance >= brush.radius)
                continue;
            const float t = 1 - distance / brush.radius, falloff = t * t * (3 - 2 * t);
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
            if (source.chart && value != source.heights[i] &&
                (x == 0 || z == 0 || x == int(source.samplesX) - 1 ||
                 z == int(source.samplesZ) - 1)) {
                error =
                    "planet surface brush reaches its protected rim; use a larger editable patch";
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
