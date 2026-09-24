#include "FluidSurface.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace {
struct GridPoint {
    int x = 0, y = 0, z = 0;
    bool operator==(const GridPoint& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct GridHash {
    std::size_t operator()(const GridPoint& p) const {
        const std::size_t x = static_cast<std::size_t>(static_cast<unsigned int>(p.x));
        const std::size_t y = static_cast<std::size_t>(static_cast<unsigned int>(p.y));
        const std::size_t z = static_cast<std::size_t>(static_cast<unsigned int>(p.z));
        return (x * 73856093u) ^ (y * 19349663u) ^ (z * 83492791u);
    }
};

using Field = std::unordered_map<GridPoint, float, GridHash>;

float Value(const Field& field, GridPoint p) {
    const auto found = field.find(p);
    return found == field.end() ? 0.0f : found->second;
}

glm::vec3 Position(GridPoint p, float spacing) {
    return glm::vec3(p.x * spacing, p.y * spacing, p.z * spacing);
}

glm::vec3 OutwardNormal(const Field& field, GridPoint p) {
    // The scalar field grows toward water; its negative gradient points
    // outward. Differencing lattice samples avoids one particle's center
    // imposing its own surface normal on the whole neighboring volume.
    const glm::vec3 gradient(
        Value(field, {p.x - 1, p.y, p.z}) - Value(field, {p.x + 1, p.y, p.z}),
        Value(field, {p.x, p.y - 1, p.z}) - Value(field, {p.x, p.y + 1, p.z}),
        Value(field, {p.x, p.y, p.z - 1}) - Value(field, {p.x, p.y, p.z + 1}));
    const float length = glm::length(gradient);
    return length > 1.0e-6f ? gradient / length : glm::vec3(0.0f);
}

struct Sample {
    glm::vec3 position;
    glm::vec3 normal;
    float field = 0.0f;
};

MeshVertex Interpolate(const Sample& a, const Sample& b, float iso) {
    const float difference = b.field - a.field;
    const float t = std::abs(difference) > 1.0e-8f
                        ? std::clamp((iso - a.field) / difference, 0.0f, 1.0f)
                        : 0.5f;
    MeshVertex result;
    result.position = glm::mix(a.position, b.position, t);
    const glm::vec3 normal = glm::mix(a.normal, b.normal, t);
    const float normalLength = glm::length(normal);
    result.normal = normalLength > 1.0e-6f ? normal / normalLength : glm::vec3(0.0f);
    return result;
}

void EmitTriangle(std::vector<MeshVertex>& vertices, MeshVertex a, MeshVertex b, MeshVertex c) {
    const glm::vec3 geometricNormal = glm::cross(b.position - a.position, c.position - a.position);
    const float length = glm::length(geometricNormal);
    if (length < 1.0e-9f) return;
    const glm::vec3 average = a.normal + b.normal + c.normal;
    if (glm::dot(geometricNormal, average) < 0.0f) std::swap(b, c);
    const glm::vec3 fallback = glm::normalize(glm::cross(b.position - a.position,
                                                         c.position - a.position));
    if (glm::length(a.normal) < 1.0e-6f) a.normal = fallback;
    if (glm::length(b.normal) < 1.0e-6f) b.normal = fallback;
    if (glm::length(c.normal) < 1.0e-6f) c.normal = fallback;
    vertices.push_back(a);
    vertices.push_back(b);
    vertices.push_back(c);
}

void EmitTetra(std::vector<MeshVertex>& vertices, const std::array<Sample, 8>& samples,
               const std::array<int, 4>& tetra, float iso) {
    int inside[4], outside[4], insideCount = 0, outsideCount = 0;
    for (const int i : tetra) {
        if (samples[i].field >= iso) inside[insideCount++] = i;
        else outside[outsideCount++] = i;
    }
    if (insideCount == 0 || insideCount == 4) return;
    if (insideCount == 1) {
        EmitTriangle(vertices,
                     Interpolate(samples[inside[0]], samples[outside[0]], iso),
                     Interpolate(samples[inside[0]], samples[outside[1]], iso),
                     Interpolate(samples[inside[0]], samples[outside[2]], iso));
    } else if (insideCount == 3) {
        EmitTriangle(vertices,
                     Interpolate(samples[outside[0]], samples[inside[0]], iso),
                     Interpolate(samples[outside[0]], samples[inside[1]], iso),
                     Interpolate(samples[outside[0]], samples[inside[2]], iso));
    } else {
        const MeshVertex a = Interpolate(samples[inside[0]], samples[outside[0]], iso);
        const MeshVertex b = Interpolate(samples[inside[0]], samples[outside[1]], iso);
        const MeshVertex c = Interpolate(samples[inside[1]], samples[outside[0]], iso);
        const MeshVertex d = Interpolate(samples[inside[1]], samples[outside[1]], iso);
        EmitTriangle(vertices, a, b, c);
        EmitTriangle(vertices, b, d, c);
    }
}
}  // namespace

std::vector<MeshVertex> BuildFluidSurface(const std::vector<glm::vec3>& positions,
                                          float kernelRadius, float gridSpacing, float isoValue) {
    std::vector<MeshVertex> vertices;
    if (positions.empty() || kernelRadius <= 0.0f || gridSpacing <= 0.0f || isoValue <= 0.0f)
        return vertices;

    Field field;
    std::unordered_set<GridPoint, GridHash> cells;
    const float radiusSquared = kernelRadius * kernelRadius;
    for (const glm::vec3& particle : positions) {
        const glm::vec3 minimum = (particle - glm::vec3(kernelRadius)) / gridSpacing;
        const glm::vec3 maximum = (particle + glm::vec3(kernelRadius)) / gridSpacing;
        const int lowX = static_cast<int>(std::floor(minimum.x));
        const int lowY = static_cast<int>(std::floor(minimum.y));
        const int lowZ = static_cast<int>(std::floor(minimum.z));
        const int highX = static_cast<int>(std::ceil(maximum.x));
        const int highY = static_cast<int>(std::ceil(maximum.y));
        const int highZ = static_cast<int>(std::ceil(maximum.z));
        for (int z = lowZ; z <= highZ; ++z) {
            for (int y = lowY; y <= highY; ++y) {
                for (int x = lowX; x <= highX; ++x) {
                    const GridPoint key{x, y, z};
                    const glm::vec3 offset = Position(key, gridSpacing) - particle;
                    const float distanceSquared = glm::dot(offset, offset);
                    if (distanceSquared >= radiusSquared) continue;
                    const float t = 1.0f - distanceSquared / radiusSquared;
                    field[key] += t * t * t;
                    for (int dz = -1; dz <= 0; ++dz)
                        for (int dy = -1; dy <= 0; ++dy)
                            for (int dx = -1; dx <= 0; ++dx)
                                cells.insert({x + dx, y + dy, z + dz});
                }
            }
        }
    }

    constexpr std::array<GridPoint, 8> offsets{{{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
                                                {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}};
    constexpr std::array<std::array<int, 4>, 6> tetrahedra{{{{0, 5, 1, 6}}, {{0, 1, 2, 6}},
                                                              {{0, 2, 3, 6}}, {{0, 3, 7, 6}},
                                                              {{0, 7, 4, 6}}, {{0, 4, 5, 6}}}};
    for (const GridPoint& cell : cells) {
        std::array<Sample, 8> corners;
        int insideCount = 0;
        for (std::size_t i = 0; i < offsets.size(); ++i) {
            const GridPoint point{cell.x + offsets[i].x, cell.y + offsets[i].y,
                                  cell.z + offsets[i].z};
            corners[i] = {Position(point, gridSpacing), OutwardNormal(field, point),
                          Value(field, point)};
            if (corners[i].field >= isoValue) ++insideCount;
        }
        if (insideCount == 0 || insideCount == 8) continue;
        for (const auto& tetra : tetrahedra) EmitTetra(vertices, corners, tetra, isoValue);
    }
    return vertices;
}
