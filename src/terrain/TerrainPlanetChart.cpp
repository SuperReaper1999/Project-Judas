#include "TerrainPlanetChart.h"
#include "SceneFingerprint.h"
#include "TerrainSource.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace TerrainV2 {
namespace {
struct Frame {
    glm::dvec3 axis, u, v, direction;
    double count, uc, vc;
};
Frame Coordinates(const PlanetChart &chart) {
    Frame f;
    f.count = double(uint64_t(1) << chart.tile.level);
    f.uc = 2 * (chart.tile.x + .5) / f.count - 1;
    f.vc = 2 * (chart.tile.y + .5) / f.count - 1;
    f.axis = PlanetSurface::Direction(chart.tile.face, 0, 0);
    f.u = PlanetSurface::Direction(chart.tile.face, 1, 0) * std::sqrt(2.) - f.axis;
    f.v = PlanetSurface::Direction(chart.tile.face, 0, 1) * std::sqrt(2.) - f.axis;
    f.direction = PlanetSurface::Direction(chart.tile.face, f.uc, f.vc);
    return f;
}
glm::dmat3 Rotation(const Frame &f) {
    const auto x = glm::normalize(f.u - f.direction * glm::dot(f.direction, f.u));
    return {x, f.direction, glm::cross(x, f.direction)};
}
} // namespace
glm::dvec3 PlanetChart::ModelOrigin() const {
    return Coordinates(*this).direction * (radius + originHeight);
}
glm::dquat PlanetChart::ModelRotation() const {
    return glm::normalize(glm::quat_cast(Rotation(Coordinates(*this))));
}
glm::dvec3 PlanetChart::ModelPoint(glm::dvec3 p) const {
    const auto f = Coordinates(*this);
    return PlanetSurface::Direction(tile.face, f.uc + 2 * p.x / (span * f.count),
                                    f.vc - 2 * p.z / (span * f.count)) *
           (radius + p.y);
}
glm::dvec3 PlanetChart::LocalPoint(glm::dvec3 p) const {
    return glm::transpose(Rotation(Coordinates(*this))) * (ModelPoint(p) - ModelOrigin());
}
glm::dvec3 PlanetChart::ChartPoint(glm::dvec3 p) const {
    const auto f = Coordinates(*this);
    const auto model = ModelOrigin() + Rotation(f) * p;
    const auto uv = PlanetSurface::FaceCoordinates(tile.face, model);
    return {(uv.x - f.uc) * (span * f.count) * .5, glm::length(model) - radius,
            (f.vc - uv.y) * (span * f.count) * .5};
}
glm::dmat3 PlanetChart::Jacobian(glm::dvec3 p) const {
    const auto f = Coordinates(*this);
    const double scale = 2 / (span * f.count), u = f.uc + scale * p.x, v = f.vc - scale * p.z;
    const auto raw = f.axis + u * f.u + v * f.v;
    const double length = glm::length(raw);
    const auto direction = raw / length;
    const auto dx =
        (f.u - direction * glm::dot(direction, f.u)) * ((radius + p.y) * scale / length);
    const auto dz =
        (f.v - direction * glm::dot(direction, f.v)) * (-(radius + p.y) * scale / length);
    return glm::transpose(Rotation(f)) * glm::dmat3(dx, direction, dz);
}
std::string PlanetGeometryFingerprint(const PlanetSettings &settings) {
    PlanetSettings geometry;
    geometry.radius = settings.radius;
    geometry.bands = settings.bands;
    geometry.patches = settings.patches;
    return SceneFingerprintSha256(EncodePlanetSettings(geometry));
}
std::vector<float> PlanetSourceRim(const Source &source) {
    std::vector<float> rim;
    if (source.heights.size() != size_t(source.samplesX) * source.samplesZ)
        return rim;
    for (unsigned x = 0; x < source.samplesX; ++x)
        rim.push_back(source.heights[x]);
    for (unsigned z = 0; z < source.samplesZ; ++z)
        rim.push_back(source.heights[size_t(z) * source.samplesX + source.samplesX - 1]);
    for (unsigned x = 0; x < source.samplesX; ++x)
        rim.push_back(source.heights[size_t(source.samplesZ - 1) * source.samplesX + x]);
    for (unsigned z = 0; z < source.samplesZ; ++z)
        rim.push_back(source.heights[size_t(z) * source.samplesX]);
    return rim;
}
bool ValidatePlanetChart(const PlanetChart &chart, std::string &error) {
    if (!std::isfinite(chart.radius) || chart.radius < 1 || chart.radius > 1e10 ||
        !std::isfinite(chart.span) || chart.span < .5 || chart.span > 8192 ||
        !std::isfinite(chart.originHeight) || std::abs(chart.originHeight) > 100000 ||
        chart.radius + chart.originHeight <= chart.radius * .5 || chart.tile.face >= 6 ||
        chart.tile.level < 3 || chart.tile.level > 26 ||
        chart.tile.x >= (uint64_t(1) << chart.tile.level) ||
        chart.tile.y >= (uint64_t(1) << chart.tile.level)) {
        error = "planet terrain patch requires a bounded cube-face chart (level3..26, span<=8192m)";
        return false;
    }
    error.clear();
    return true;
}
bool CreatePlanetChart(const PlanetSurface &planet, PlanetTileKey key, unsigned cells,
                       Source &output, PlanetChart &coordinates, std::string &error) {
    try {
        if (cells < 16 || cells > 512 || (cells & (cells - 1)))
            throw std::invalid_argument("planet terrain patch needs16..512 power-of-two cells");
        PlanetChart chart{planet.settings.radius, key, 1};
        if (!ValidatePlanetChart(chart, error))
            return false;
        const auto f = Coordinates(chart);
        chart.originHeight = planet.HeightAt(f.direction);
        const auto raw = f.axis + f.uc * f.u + f.vc * f.v;
        const double length = glm::length(raw);
        const double metric = .5 *
                              (glm::length(f.u - f.direction * glm::dot(f.direction, f.u)) +
                               glm::length(f.v - f.direction * glm::dot(f.direction, f.v))) /
                              length;
        const float spacing = float(2 * chart.radius * metric / (f.count * cells));
        auto source = Create(cells + 1, cells + 1, spacing);
        chart.span = double(cells) * spacing;
        if (!ValidatePlanetChart(chart, error))
            return false;
        const double headroom = std::min(4096., chart.radius * .4);
        source.bottom = float(std::max(-chart.radius * .49, -planet.MaximumHeight() - headroom));
        source.top = float(planet.MaximumHeight() + headroom);
        for (unsigned z = 0; z <= cells; ++z)
            for (unsigned x = 0; x <= cells; ++x) {
                const auto direction = PlanetSurface::Direction(
                    key.face, 2 * (key.x + double(x) / cells) / f.count - 1,
                    2 * (key.y + 1 - double(z) / cells) / f.count - 1);
                source.heights[size_t(z) * (cells + 1) + x] = float(planet.HeightAt(direction));
            }
        if (!planet.HasCustomProvider())
            chart.surfaceFingerprint = PlanetGeometryFingerprint(planet.settings);
        chart.rim = PlanetSourceRim(source);
        if (!Validate(source, error))
            return false;
        output = std::move(source);
        coordinates = chart;
        error.clear();
        return true;
    } catch (const std::exception &exception) {
        error = exception.what();
        return false;
    }
}
bool ProjectPlanetChunk(const PlanetChart &chart, Chunk &output, std::string &error) {
    if (!ValidatePlanetChart(chart, error))
        return false;
    auto chunk = output;
    const auto oldAnchor = chunk.anchor;
    auto anchorPoint = glm::dvec3(oldAnchor);
    if (!chunk.mesh.vertices.empty()) {
        double minimum = chunk.mesh.vertices.front().position.y;
        double maximum = minimum;
        for (const auto &vertex : chunk.mesh.vertices) {
            minimum = std::min(minimum, double(vertex.position.y));
            maximum = std::max(maximum, double(vertex.position.y));
        }
        anchorPoint.y += (minimum + maximum) * .5;
    }
    const auto anchor = chart.LocalPoint(anchorPoint);
    // The root and chunk displacement are narrowed only after subtracting the
    // double-precision planetary origin. There are no float32 planet radii.
    chunk.anchor = glm::vec3(anchor);
    for (auto &vertex : chunk.mesh.vertices) {
        const glm::dvec3 point = glm::vec3(oldAnchor + vertex.position);
        if (!std::isfinite(glm::dot(point, point)) || point.y <= -chart.radius * .5 ||
            !std::isfinite(glm::dot(vertex.normal, vertex.normal)) ||
            glm::length(vertex.normal) < 1e-10f ||
            !std::isfinite(glm::dot(vertex.tangent, vertex.tangent)) ||
            glm::length(glm::vec3(vertex.tangent)) < 1e-10f) {
            error = "planet terrain projection requires finite geometry outside the radial core";
            return false;
        }
        const auto jacobian = chart.Jacobian(point);
        const auto normal = glm::transpose(glm::inverse(jacobian)) * glm::dvec3(vertex.normal);
        const auto tangent = jacobian * glm::dvec3(vertex.tangent);
        vertex.position = glm::vec3(chart.LocalPoint(point) - glm::dvec3(chunk.anchor));
        vertex.normal = glm::vec3(glm::normalize(normal));
        vertex.tangent = glm::vec4(glm::vec3(glm::normalize(tangent)), vertex.tangent.w);
        if (!std::isfinite(glm::dot(vertex.position, vertex.position)) ||
            !std::isfinite(glm::dot(vertex.normal, vertex.normal))) {
            error = "planet terrain projection has nonfinite geometry";
            return false;
        }
    }
    output = std::move(chunk);
    error.clear();
    return true;
}
} // namespace TerrainV2
