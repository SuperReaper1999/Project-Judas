#pragma once

#include "MovingGridGeometry.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <deque>
#include <limits>
#include <numeric>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace judas::volume_grid::basin_probe {

constexpr float tick = 1.f / 60.f;
constexpr float gravity = 9.81f;
constexpr glm::vec3 initialCenter{0.f, .7f, 0.f};
constexpr glm::vec3 bodyHalf{.3f};

struct Parcel {
    GridVec lo, hi;
    glm::vec3 emittedWorldCenter;
};

struct Wall {
    glm::vec3 center, half;
};

inline GridVec Array(glm::dvec3 v) {
    return {v.x, v.y, v.z};
}

inline glm::dvec3 Vector(const GridVec& v) {
    return {v[0], v[1], v[2]};
}

enum class WallMode { Authored, RoundedControl, NativePoseControl };

struct Fixture {
    GridLayout layout;
    GridVec origin;
    float spacing;
    glm::quat rotation;

    static Fixture Authored(bool rotated) {
        if (rotated)
            return {{{30, 40, 30}, .1}, {-1.5, -.5, -1.5}, .2f,
                    glm::angleAxis(.83f, glm::normalize(glm::vec3(1.f, -2.f, 3.f)))};
        return {{{24, 32, 24}, .125}, {-1.5, -.5, -1.5}, .25f,
                glm::quat(1, 0, 0, 0)};
    }

    static constexpr std::array<Wall, 5> wallDefinitions{{
        {{0, -.1f, 0}, {1.1f, .1f, 1.1f}},
        {{-1.1f, 1.5f, 0}, {.1f, 1.5f, 1.1f}},
        {{1.1f, 1.5f, 0}, {.1f, 1.5f, 1.1f}},
        {{0, 1.5f, -1.1f}, {1.f, 1.499f, .1f}},
        {{0, 1.5f, 1.1f}, {1.f, 1.499f, .1f}},
    }};

    static float BodyMass() {
        const double volume = 8. * bodyHalf.x * bodyHalf.y * bodyHalf.z;
        return static_cast<float>(1000. * .5f * volume);
    }

    glm::vec3 WorldGravity() const { return rotation * glm::vec3(0, -gravity, 0); }
    glm::vec3 NativeInitialCenter() const { return rotation * initialCenter; }
    GridVec ChartPoint(glm::vec3 world) const {
        // Match the native orientation while reconstructing the binary32
        // position in binary64. Normalize the authored rounded quaternion.
        return Array(glm::conjugate(glm::normalize(glm::dquat(rotation))) * glm::dvec3(world) - Vector(origin));
    }

    static constexpr std::uint32_t bodyOwner = 6;
    BoxMotion BoxAtChartCenter(const GridVec& center) const {
        BoxMotion box{bodyOwner};
        for (int axis = 0; axis < 3; ++axis) {
            box.lo[axis] = center[axis] - double(bodyHalf[axis]);
            box.hi[axis] = center[axis] + double(bodyHalf[axis]);
        }
        return box;
    }

    BoxMotion InitialBox() const {
        // Both the static placeholder and release use the same native pose.
        // Reconstruct it in the chart once, before assigning initial phase.
        return BoxAtChartCenter(ChartPoint(NativeInitialCenter()));
    }

    std::vector<BoxMotion> Walls(WallMode mode = WallMode::Authored) const {
        std::vector<BoxMotion> walls;
        for (const auto& authored : wallDefinitions) {
            BoxMotion wall{static_cast<std::uint32_t>(walls.size() + 1)};
            if (mode == WallMode::RoundedControl) {
                wall.lo = Array(glm::dvec3(authored.center - authored.half) - Vector(origin));
                wall.hi = Array(glm::dvec3(authored.center + authored.half) - Vector(origin));
            } else {
                const auto center = mode == WallMode::NativePoseControl
                    ? Vector(ChartPoint(rotation * authored.center))
                    : glm::dvec3(authored.center) - Vector(origin);
                wall.lo = Array(center - glm::dvec3(authored.half));
                wall.hi = Array(center + glm::dvec3(authored.half));
            }
            walls.push_back(wall);
        }
        return walls;
    }

    std::vector<Parcel> Parcels() const {
        const int nx = static_cast<int>(std::round(2 / spacing));
        const int ny = static_cast<int>(std::round(2.4f / spacing));
        const auto present = [&](int x, int y, int z) {
            const glm::vec3 pos((x + .5f) * spacing - 1, (y + .5f) * spacing, (z + .5f) * spacing - 1);
            return x < nx && !glm::all(glm::lessThan(glm::abs(pos - initialCenter),
                                                   bodyHalf + glm::vec3(spacing * .36f)));
        };
        std::vector<Parcel> parcels;
        for (int y = 0; y < ny; ++y) {
            for (int z = 0; z < nx; ++z) {
                int first = -1;
                for (int x = 0; x <= nx; ++x) {
                    if (present(x, y, z)) {
                        if (first == -1)
                            first = x;
                        continue;
                    }
                    if (first == -1)
                        continue;
                    const int run = x - first;
                    const glm::vec3 center = rotation * glm::vec3((first + .5f * run) * spacing - 1,
                        (y + .5f) * spacing, (z + .5f) * spacing - 1);
                    for (int i = first; i < x; ++i) {
                        // The lattice cube owns the represented volume. Use a
                        // common interval grid instead of overlapping rounded
                        // individual cube centers at .20 spacing.
                        Parcel p;
                        const std::array<int, 3> k{i, y, z};
                        for (int axis = 0; axis < 3; ++axis) {
                            const double base = (axis == 1 ? 0. : -1.) - origin[axis];
                            p.lo[axis] = base + k[axis] * double(spacing);
                            p.hi[axis] = base + (k[axis] + 1) * double(spacing);
                        }
                        p.emittedWorldCenter = center + rotation * glm::vec3((-.5f * static_cast<float>(run - 1) + static_cast<float>(i - first)) * spacing, 0, 0);
                        parcels.push_back(p);
                    }
                    first = -1;
                }
            }
        }
        return parcels;
    }
};

struct SeedImport {
    double shiftedVolume = 0.;
    std::size_t affectedParcels = 0, maximumHops = 0;
};

struct ExcludedParcelVolume {
    double amount;
    std::vector<std::size_t> represented;
};

template<class Geometry>
bool PlaceExcludedVolume(const Geometry& geometry, std::vector<double>& seed,
                         const std::vector<ExcludedParcelVolume>& excluded,
                         SeedImport& statistics, std::string& error) {
    const auto count = seed.size();
    const auto noPath = std::numeric_limits<std::size_t>::max();
    std::vector<std::vector<std::size_t>> neighbors(count);
    for (const auto& face : geometry.faces) {
        if (face.startArea > 0.) {
            neighbors[face.left].push_back(face.right);
            neighbors[face.right].push_back(face.left);
        }
    }
    std::vector<std::size_t> distance(count);
    for (const auto& parcel : excluded) {
        std::fill(distance.begin(), distance.end(), noPath);
        std::deque<std::size_t> frontier;
        for (const auto root : parcel.represented) {
            distance[root] = 0;
            frontier.push_back(root);
        }
        double remaining = parcel.amount;
        while (!frontier.empty() && remaining > 0.) {
            const auto region = frontier.front();
            frontier.pop_front();
            const double prior = seed[region];
            const double available = geometry.startCapacity[region] - prior;
            if (available > 0.) {
                seed[region] += std::min(remaining, available);
                const double assigned = seed[region] - prior;
                remaining -= assigned;
                statistics.shiftedVolume += assigned;
                if (assigned > 0.)
                    statistics.maximumHops = std::max(statistics.maximumHops, distance[region]);
            }
            for (const auto adjacent : neighbors[region]) {
                if (distance[adjacent] == noPath) {
                    distance[adjacent] = distance[region] + 1;
                    frontier.push_back(adjacent);
                }
            }
        }
        if (remaining > 1e-16) {
            error = "excluded parcel volume has insufficient reachable start capacity";
            return false;
        }
        ++statistics.affectedParcels;
    }
    return true;
}

template<class Chart, class Geometry>
bool WaterByRegion(const Fixture& fixture, const Chart& chart, const Geometry& geometry,
                   bool conservative, std::vector<double>& water, SeedImport& statistics,
                   std::string& error) {
    const auto& capacity = geometry.startCapacity;
    if (chart.regions.size() != capacity.size() || chart.offsets.size() != fixture.layout.CellCount() + 1) {
        error = "basin seed dimensions do not match the cut-region chart";
        return false;
    }
    std::vector<double> seed(chart.regions.size());
    const auto parcels = fixture.Parcels();
    const double parcelVolume = double(fixture.spacing) * fixture.spacing * fixture.spacing;
    std::vector<ExcludedParcelVolume> excluded;
    double total = 0.;
    for (const auto& parcel : parcels) {
        std::array<int, 3> lo, hi;
        for (int axis = 0; axis < 3; ++axis) {
            lo[axis] = std::clamp(int(std::floor(parcel.lo[axis] / fixture.layout.h)), 0,
                                  fixture.layout.cells[axis] - 1);
            hi[axis] = std::clamp(int(std::floor(parcel.hi[axis] / fixture.layout.h)), 0,
                                  fixture.layout.cells[axis] - 1);
        }
        double representedVolume = 0.;
        std::vector<std::size_t> represented;
        for (int z = lo[2]; z <= hi[2]; ++z) {
            for (int y = lo[1]; y <= hi[1]; ++y) {
                for (int x = lo[0]; x <= hi[0]; ++x) {
                    const auto parent = fixture.layout.Cell(x, y, z);
                    for (std::size_t r = chart.offsets[parent]; r < chart.offsets[parent + 1]; ++r) {
                        const auto& region = chart.regions[r];
                        double overlap = 1.;
                        for (int axis = 0; axis < 3; ++axis)
                            overlap *= std::max(0., std::min(parcel.hi[axis], region.hi[axis]) -
                                                     std::max(parcel.lo[axis], region.lo[axis]));
                        if (conservative && overlap > 0.) {
                            overlap = CutRegionClippedVolume(region, geometry.motion, parcel.lo, parcel.hi);
                            if (overlap > 0.)
                                represented.push_back(r);
                        }
                        seed[r] += overlap;
                        representedVolume += overlap;
                        total += overlap;
                    }
                }
            }
        }
        const double missing = parcelVolume - representedVolume;
        if (conservative && missing > 1e-16)
            excluded.push_back({missing, std::move(represented)});
    }
    const double nominal = double(parcels.size()) * parcelVolume;
    if (!std::isfinite(total) || (!conservative && std::abs(total - nominal) > 1e-11)) {
        std::ostringstream report;
        report << std::setprecision(17)
               << "cut-region overlap lost some authored parcel volume"
               << " (nominal=" << nominal << ", assigned=" << total << ", shortfall=" << nominal-total << " m³)";
        error = report.str();
        return false;
    }
    for (std::size_t r = 0; r < seed.size(); ++r) {
        if (!(capacity[r] >= 0.) || !std::isfinite(capacity[r]) || seed[r] > capacity[r] + 1e-16) {
            error = "authored parcel overlaps a solid in region " + std::to_string(r) +
                " (overlap " + std::to_string(seed[r] - capacity[r]) + " m³)";
            return false;
        }
        // Resolve the final sum of binary64 overlap products to the same
        // exact region endpoint without editing any physical occupancy.
        if (seed[r] > capacity[r])
            seed[r] = capacity[r];
    }
    if (conservative && !PlaceExcludedVolume(geometry, seed, excluded, statistics, error))
        return false;
    if (std::abs(std::accumulate(seed.begin(), seed.end(), 0.L) - nominal) > 1e-11) {
        error = "conservative import did not preserve the authored parcel inventory";
        return false;
    }
    water = std::move(seed);
    error.clear();
    return true;
}

} // namespace judas::volume_grid::basin_probe
