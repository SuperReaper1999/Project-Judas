#include "BuildIdentity.h"
#include "authoring/TerrainAuthoring.h"
#include "terrain/TerrainMesher.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using Clock = std::chrono::steady_clock;
double Milliseconds(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
struct Candidate {
    TerrainV2::Chunk chunk;
    glm::dvec3 minimum{INFINITY}, maximum{-INFINITY};
};
// Disposable broadphase prototype. The exact triangle routine remains the
// existing authoring picker; these timings do not qualify a production change.
bool Intersects(const Candidate &c, glm::vec3 origin, glm::vec3 direction) {
    double near = 0, far = INFINITY;
    for (unsigned axis = 0; axis < 3; ++axis) {
        if (direction[axis] == 0) {
            if (origin[axis] < c.minimum[axis] || origin[axis] > c.maximum[axis])
                return false;
        } else {
            double a = (c.minimum[axis] - origin[axis]) / direction[axis];
            double b = (c.maximum[axis] - origin[axis]) / direction[axis];
            if (a > b)
                std::swap(a, b);
            near = std::max(near, a);
            far = std::min(far, b);
            if (near > far)
                return false;
        }
    }
    return true;
}
void Report(const char *path, std::vector<double> samples) {
    std::sort(samples.begin(), samples.end());
    std::cout << "{\"path\":\"" << path << "\",\"samples\":" << samples.size()
              << ",\"median_ms\":" << samples[samples.size() / 2]
              << ",\"p95_ms\":" << samples[size_t(std::ceil(samples.size() * .95)) - 1]
              << ",\"worst_ms\":" << samples.back() << "}\n";
}
} // namespace

int main() {
    try {
        auto source = TerrainV2::Create(513, 513, 2);
        for (unsigned z = 0; z < source.samplesZ; ++z)
            for (unsigned x = 0; x < source.samplesX; ++x)
                source.heights[size_t(z) * source.samplesX + x] = 35 * std::sin(x * .017f) +
                                                                  20 * std::sin(z * .023f) +
                                                                  8 * std::sin((x + z) * .04f);
        std::vector<TerrainV2::ChunkKey> keys;
        for (unsigned z = 0; z < source.ChunksZ(); ++z)
            for (unsigned x = 0; x < source.ChunksX(); ++x)
                keys.push_back({x, z});
        std::vector<TerrainV2::Chunk> chunks;
        std::string error;
        if (!TerrainV2::BuildChunks(source, keys, chunks, error))
            throw std::runtime_error(error);
        auto start = Clock::now();
        MeshData merged;
        for (const auto &c : chunks) {
            unsigned offset = unsigned(merged.vertices.size());
            for (auto v : c.mesh.vertices) {
                v.position += c.anchor;
                merged.vertices.push_back(v);
            }
            for (auto i : c.mesh.indices)
                merged.indices.push_back(offset + i);
        }
        const double mergeMs = Milliseconds(start);
        start = Clock::now();
        std::vector<Candidate> candidates;
        for (auto &c : chunks) {
            Candidate candidate;
            for (const auto &v : c.mesh.vertices) {
                glm::dvec3 p(v.position + c.anchor);
                candidate.minimum = glm::min(candidate.minimum, p);
                candidate.maximum = glm::max(candidate.maximum, p);
            }
            candidate.chunk = std::move(c);
            candidates.push_back(std::move(candidate));
        }
        const double boundsMs = Milliseconds(start);
        std::vector<double> before, after;
        double maxDifference = 0;
        size_t triangles = 0, hits = 0;
        for (unsigned i = 0; i < 120; ++i) {
            glm::vec3 origin(-490.f + float((i * 97) % 980), 180, -490.f + float((i * 61) % 980));
            glm::vec3 direction = i % 3 == 0 ? glm::vec3(.13f, -.6f, -.08f)
                                             : glm::vec3(0, i % 11 == 0 ? 1.f : -1.f, 0);
            glm::vec3 reference{}, actual{};
            bool oldHit = false, newHit = false;
            auto old = [&] {
                auto t = Clock::now();
                oldHit = PickTerrain(merged, origin, direction, reference);
                before.push_back(Milliseconds(t));
            };
            auto next = [&] {
                auto t = Clock::now();
                double nearest = INFINITY;
                for (const auto &c : candidates) {
                    if (!Intersects(c, origin, direction))
                        continue;
                    triangles += c.chunk.mesh.indices.size() / 3;
                    glm::vec3 point;
                    if (PickTerrain(c.chunk.mesh, origin - c.chunk.anchor, direction, point)) {
                        point += c.chunk.anchor;
                        auto delta = glm::dvec3(point) - glm::dvec3(origin);
                        double distance = glm::dot(delta, delta);
                        if (distance < nearest) {
                            nearest = distance;
                            actual = point;
                            newHit = true;
                        }
                    }
                }
                after.push_back(Milliseconds(t));
            };
            if (i % 2) {
                next();
                old();
            } else {
                old();
                next();
            }
            if (oldHit != newHit)
                throw std::runtime_error("prototype changes hit classification");
            if (oldHit) {
                ++hits;
                maxDifference = std::max(maxDifference, double(glm::length(actual - reference)));
            }
        }
        Report("existing_merged_hover", before);
        Report("prototype_chunk_bounds", after);
        std::cout << "{\"triangles\":" << merged.indices.size() / 3
                  << ",\"chunks\":" << candidates.size() << ",\"preview_merge_ms\":" << mergeMs
                  << ",\"bounds_setup_ms\":" << boundsMs << ",\"hits\":" << hits
                  << ",\"max_hit_difference_m\":" << maxDifference
                  << ",\"prototype_triangles_total\":" << triangles
                  << ",\"reference_triangles_total\":" << before.size() * merged.indices.size() / 3
                  << "}\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
