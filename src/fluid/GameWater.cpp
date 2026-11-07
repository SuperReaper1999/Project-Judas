#include "GameWater.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace JudasWater {
bool Grid::Initialize(const Settings &settings, std::string &error) {
    const auto n = settings.cells;
    if (n.x < 2 || n.y < 2 || n.z < 2 || n.x > 262144 || n.y > 262144 || n.z > 262144) {
        error = "Invalid game-water grid dimensions.";
        return false;
    }
    const std::int64_t count = std::int64_t(n.x) * n.y * n.z;
    if (n.x < 2 || n.y < 2 || n.z < 2 || count > 262144 ||
        !(settings.cellSize > .01f && std::isfinite(settings.cellSize)) ||
        !std::isfinite(settings.cellSize * settings.cellSize * settings.cellSize) ||
        !(settings.density > 0 && std::isfinite(settings.density)) ||
        settings.pressureIterations == 0 || settings.pressureIterations > 1000 ||
        !(settings.pressureTolerance > 0 && settings.pressureTolerance < .1f) ||
        !(settings.minimumAirFraction >= .02f && settings.minimumAirFraction <= .4f) ||
        !(settings.minimumMobileFraction >= 0 && settings.minimumMobileFraction <= .05f) ||
        settings.minimumReservoirCells < 1 || settings.minimumReservoirCells > 64 ||
        !std::isfinite(glm::dot(settings.origin, settings.origin)) ||
        !std::isfinite(glm::dot(settings.gravity, settings.gravity)) ||
        !(glm::dot(settings.rotation, settings.rotation) > 1e-10f &&
          std::isfinite(glm::dot(settings.rotation, settings.rotation)))) {
        error = "Game water requires a finite 3D grid (at most 262144 cells).";
        return false;
    }
    const auto rotation = glm::normalize(settings.rotation);
    const auto localGravity = glm::conjugate(rotation) * settings.gravity;
    const float gravityTolerance = 1e-5f * std::max(1.f, glm::length(settings.gravity));
    if (std::abs(localGravity.x) > gravityTolerance ||
        std::abs(localGravity.z) > gravityTolerance || localGravity.y > gravityTolerance) {
        error = "Game-water gravity must follow the chart's local -Y axis; rotate "
                "the chart "
                "together with gravity. Spatially varying gravity is not yet "
                "supported.";
        return false;
    }
    settings_ = settings;
    settings_.rotation = rotation;
    gravity_.clear();
    cells_.assign(std::size_t(count), {});
    incident_.assign(std::size_t(count), {});
    velocity_.assign(std::size_t(count), {});
    faces_.clear();
    air_.clear();
    solids_.clear();
    exteriorAirMass_ = 0;
    initialQuantity_ = addedQuantity_ = removedQuantity_ = 0;
    airInitialized_ = false;
    const float volume = std::pow(settings.cellSize, 3.f);
    for (auto &c : cells_) {
        c.capacity = c.staticCapacity = volume;
        c.airMass = 1.2 * volume;
    }
    for (int z = 0; z < n.z; ++z)
        for (int y = 0; y < n.y; ++y)
            for (int x = 0; x < n.x; ++x) {
                const int a = Index(x, y, z);
                for (int axis = 0; axis < 3; ++axis) {
                    glm::ivec3 at(x, y, z);
                    ++at[axis];
                    const int b = Index(at.x, at.y, at.z);
                    if (b < 0)
                        continue;
                    const int f = int(faces_.size());
                    faces_.push_back({a, b, axis, 0, 1});
                    incident_[a].push_back(f);
                    incident_[b].push_back(f);
                }
            }
    staticFaces_.assign(faces_.size(), 1.f);
    staticSamples_.assign(cells_.size(), 0);
    gasReserve_.clear();
    samples_ = staticSamples_;
    staticFaceSamples_.assign(faces_.size(), 65535);
    faceSamples_ = staticFaceSamples_;
    waterHeight_.assign(cells_.size(), 0);
    solidOwner_.assign(cells_.size(), -1);
    sealedAmbiguousCells_ = 0;
    stats_ = {};
    error.clear();
    return true;
}
int Grid::Index(int x, int y, int z) const {
    const auto n = settings_.cells;
    return x < 0 || y < 0 || z < 0 || x >= n.x || y >= n.y || z >= n.z ? -1
                                                                       : (z * n.y + y) * n.x + x;
}
glm::ivec3 Grid::Coordinates(int i) const {
    return {i % settings_.cells.x, (i / settings_.cells.x) % settings_.cells.y,
            i / (settings_.cells.x * settings_.cells.y)};
}
glm::vec3 Grid::Center(int i) const {
    return (glm::vec3(Coordinates(i)) + .5f) * settings_.cellSize;
}
glm::vec3 Grid::ToWorld(glm::vec3 p) const { return settings_.origin + settings_.rotation * p; }
glm::vec3 Grid::ToLocal(glm::vec3 p) const {
    return glm::conjugate(settings_.rotation) * (p - settings_.origin);
}
void Grid::SetSolids(const std::vector<SolidBox> &solids) { solids_ = solids; }
bool Grid::SetGravityField(const std::vector<glm::vec3> &field, std::string &error) {
    if (!field.empty() && field.size() != cells_.size()) {
        error = "Grid-water gravity field must have one sample per cell.";
        return false;
    }
    for (auto g : field) {
        g = glm::conjugate(settings_.rotation) * g;
        if (!std::isfinite(glm::dot(g, g)) || g.y > 1e-6f ||
            glm::length(glm::vec2(g.x, g.z)) > .5f * std::max(1e-6f, -g.y)) {
            error = "Grid-water region spans too much gravity curvature; use a "
                    "smaller tangent chart (less than 26 degrees).";
            return false;
        }
    }
    gravity_ = field;
    error.clear();
    return true;
}
glm::vec3 Grid::Gravity(glm::vec3 point) const {
    if (gravity_.empty())
        return settings_.gravity;
    const auto local = ToLocal(point) / settings_.cellSize;
    if (!std::isfinite(glm::dot(local, local)))
        return settings_.gravity;
    const auto at =
        glm::ivec3(glm::clamp(glm::floor(local), glm::vec3(0), glm::vec3(settings_.cells - 1)));
    return gravity_[Index(at.x, at.y, at.z)];
}
double Grid::FillBox(glm::vec3 lo, glm::vec3 hi) {
    return Fill([=](glm::vec3 p) {
        return glm::all(glm::greaterThanEqual(p, lo)) && glm::all(glm::lessThan(p, hi));
    });
}
double Grid::Fill(const std::function<bool(glm::vec3)> &volume) {
    if (airInitialized_)
        throw std::logic_error("Fill seeds initial game water; runtime sources are "
                               "available through ChangeWater instead.");
    double added = 0;
    const float h = settings_.cellSize;
    for (int i = 0; i < int(cells_.size()); ++i) {
        auto &c = cells_[i];
        unsigned wet = 0;
        for (int z = 0; z < 2; ++z)
            for (int y = 0; y < 2; ++y)
                for (int x = 0; x < 2; ++x) {
                    const auto p = ToWorld(Center(i) + h * (glm::vec3(x, y, z) * .5f - .25f));
                    wet += volume(p);
                }
        const double next = std::max(c.water, double(c.capacity) * wet / 8.);
        added += next - c.water;
        c.water = next;
        c.airMass = 1.2 * (c.capacity - next);
    }
    initialQuantity_ += added;
    UpdateHeights();
    return added;
}
bool Grid::Step(float dt, std::string &error) {
    if (!(dt > 0 && dt <= .1f && std::isfinite(dt)) || cells_.empty()) {
        error = "Game water step must be finite, positive and at most 0.1 seconds.";
        return false;
    }
    const auto start = std::chrono::steady_clock::now();
    stats_ = {};
    if (!airInitialized_) {
        UpdateHeights();
        ClassifyAir();
        airInitialized_ = true;
    }
    if (!Geometry(error))
        return false;
    ReleaseDisplacement();
    UpdateHeights();
    UpdateVelocity();
    double speed = 0;
    for (auto f : faces_)
        speed = std::max(speed, double(std::abs(f.velocity)));
    // Resolve the requested interval, never silently drop simulation time.
    float maximumGravity = glm::length(settings_.gravity);
    for (auto g : gravity_)
        maximumGravity = std::max(maximumGravity, glm::length(g));
    const double requested =
        std::ceil((speed + maximumGravity * dt) * dt / (.5f * settings_.cellSize));
    if (!std::isfinite(requested) || requested > 64) {
        error = "Game water CFL budget exceeded; reduce the step or inspect the "
                "applied impulse.";
        return false;
    }
    const unsigned count = std::max(1u, unsigned(requested));
    stats_.substeps = count;
    for (unsigned step = 0; step < count; ++step) {
        ClassifyAir();
        AdvectVelocity(dt / count);
        if (!Project(dt / count, error))
            return false;
        Transport(dt / count);
        UpdateHeights();
        UpdateVelocity();
    }
    ClassifyAir();
    for (std::size_t i = 0; i < cells_.size(); ++i) {
        const auto &c = cells_[i];
        stats_.water += c.water + c.displaced;
        stats_.storedDisplacement += c.displaced;
        stats_.kineticEnergy +=
            .5 * settings_.density * c.water * glm::dot(velocity_[i], velocity_[i]);
        if (c.water < settings_.minimumMobileFraction * c.capacity)
            stats_.unresolvedWater += c.water;
        stats_.totalAirMass += c.airMass;
    }
    stats_.totalAirMass += exteriorAirMass_;
    for (const auto &pocket : air_)
        if (!pocket.atmosphere)
            stats_.sealedAirMass += pocket.mass;
    stats_.rmsSpeed = std::sqrt(2 * stats_.kineticEnergy /
                                std::max(double(settings_.density) * stats_.water, 1e-30));
    for (auto f : faces_)
        stats_.maximumSpeed = std::max(stats_.maximumSpeed, double(std::abs(f.velocity)));
    stats_.airPockets = air_.size();
    for (const auto &pocket : air_)
        if (!pocket.atmosphere && !pocket.unresolved && pocket.mass > 0)
            ++stats_.resolvedAirPockets;
    stats_.exteriorAirMass = exteriorAirMass_;
    stats_.bytes =
        cells_.capacity() * sizeof(Cell) + faces_.capacity() * sizeof(Face) +
        (velocity_.capacity() + gravity_.capacity()) * sizeof(glm::vec3) +
        incident_.capacity() * sizeof(std::vector<int>) + staticFaces_.capacity() * sizeof(float) +
        (staticSamples_.capacity() + samples_.capacity()) * sizeof(std::uint64_t) +
        (staticFaceSamples_.capacity() + faceSamples_.capacity()) * sizeof(std::uint16_t) +
        waterHeight_.capacity() * sizeof(float) + solidOwner_.capacity() * sizeof(int) +
        gasReserve_.capacity() * sizeof(double) + air_.capacity() * sizeof(AirPocket);
    for (const auto &links : incident_)
        stats_.bytes += links.capacity() * sizeof(int);
    stats_.sealedAmbiguousCells = sealedAmbiguousCells_;
    const double expected = initialQuantity_ + addedQuantity_ - removedQuantity_;
    if (std::abs(stats_.water - expected) > 1e-6 * std::max(1., std::abs(expected))) {
        error = "Game-water quantity differs from its authored/source ledger.";
        return false;
    }
    stats_.milliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    error.clear();
    return true;
}
bool Grid::CellSurface(int i, float &height) const {
    if (i < 0 || i >= int(cells_.size()))
        return false;
    const auto &c = cells_[i];
    if (c.water <= std::max(1e-7 * std::pow(settings_.cellSize, 3),
                            double(settings_.minimumMobileFraction) * c.capacity))
        return false;
    height = waterHeight_[i];
    return true;
}
bool Grid::Surface(int x, int z, float &height) const {
    if (cells_.empty() || x < 0 || z < 0 || x >= settings_.cells.x || z >= settings_.cells.z)
        return false;
    for (int y = settings_.cells.y - 1; y >= 0; --y)
        if (CellSurface(Index(x, y, z), height))
            return true;
    return false;
}
glm::vec3 Grid::Velocity(glm::vec3 p) const {
    if (cells_.empty() || !std::isfinite(glm::dot(p, p)))
        return glm::vec3(0);
    const auto local = ToLocal(p) / settings_.cellSize;
    if (glm::any(glm::lessThan(local, glm::vec3(-1))) ||
        glm::any(glm::greaterThan(local, glm::vec3(settings_.cells))))
        return glm::vec3(0);
    const auto at = glm::ivec3(glm::floor(local));
    glm::dvec3 sum(0);
    double weight = 0;
    for (int z = -1; z <= 1; ++z)
        for (int y = -1; y <= 1; ++y)
            for (int x = -1; x <= 1; ++x) {
                int i = Index(at.x + x, at.y + y, at.z + z);
                if (i < 0)
                    continue;
                sum += cells_[i].water * glm::dvec3(velocity_[i]);
                weight += cells_[i].water;
            }
    return weight > 0 ? settings_.rotation * glm::vec3(sum / weight) : glm::vec3(0);
}
float Grid::NearbyMass(glm::vec3 world, float radius) const {
    if (cells_.empty() || radius <= 0 || !std::isfinite(radius) ||
        !std::isfinite(glm::dot(world, world)))
        return 0;
    const auto p = ToLocal(world);
    const float h = settings_.cellSize;
    const auto limits = glm::vec3(settings_.cells);
    if (glm::any(glm::lessThan(p + radius, glm::vec3(0))) ||
        glm::any(glm::greaterThanEqual(p - radius, limits * h)))
        return 0;
    const auto lo =
        glm::ivec3(glm::floor(glm::clamp((p - radius) / h, glm::vec3(0), limits - 1.f)));
    const auto hi =
        glm::ivec3(glm::floor(glm::clamp((p + radius) / h, glm::vec3(0), limits - 1.f)));
    double volume = 0;
    for (int z = lo.z; z <= hi.z; ++z)
        for (int y = lo.y; y <= hi.y; ++y)
            for (int x = lo.x; x <= hi.x; ++x) {
                int i = Index(x, y, z);
                if (glm::length(Center(i) - p) <= radius)
                    volume += cells_[i].water;
            }
    return float(settings_.density * volume);
}
void Grid::AddImpulse(glm::vec3 world, glm::vec3 impulse, float radius) {
    if (!std::isfinite(glm::dot(impulse, impulse)))
        return;
    if (radius <= 0)
        radius = 2 * settings_.cellSize;
    const float mass = NearbyMass(world, radius);
    if (mass <= 0)
        return;
    const auto p = ToLocal(world), delta = glm::conjugate(settings_.rotation) * impulse / mass;
    const float h = settings_.cellSize;
    const auto limits = glm::vec3(settings_.cells);
    const auto lo =
        glm::ivec3(glm::floor(glm::clamp((p - radius) / h, glm::vec3(0), limits - 1.f)));
    const auto hi =
        glm::ivec3(glm::floor(glm::clamp((p + radius) / h, glm::vec3(0), limits - 1.f)));
    for (int z = lo.z; z <= hi.z; ++z)
        for (int y = lo.y; y <= hi.y; ++y)
            for (int x = lo.x; x <= hi.x; ++x) {
                int i = Index(x, y, z);
                if (cells_[i].water <= 0 || glm::length(Center(i) - p) > radius)
                    continue;
                for (int f : incident_[i])
                    if (faces_[f].open > 0)
                        faces_[f].velocity += .5f * delta[faces_[f].axis];
            }
}
} // namespace JudasWater
