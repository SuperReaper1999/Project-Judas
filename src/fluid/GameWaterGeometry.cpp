#include "GameWater.h"
#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <cmath>
#include <deque>
#include <limits>

namespace JudasWater {
bool Grid::Inside(glm::vec3 p, const SolidBox &b) const {
    const auto q = glm::abs(glm::conjugate(b.rotation) * (p - b.center));
    return glm::all(glm::lessThanEqual(q, b.half + glm::vec3(settings_.cellSize * 1e-5f)));
}
void Grid::BakeStatic(const std::function<bool(glm::vec3)> &occupied) {
    const float h = settings_.cellSize, volume = h * h * h;
    staticSamples_.assign(cells_.size(), 0);
    sealedAmbiguousCells_ = 0;
    for (int i = 0; i < int(cells_.size()); ++i) {
        unsigned solid = 0, bit = 0;
        for (int z = 0; z < 4; ++z)
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 4; ++x, ++bit) {
                    const auto p =
                        ToWorld(Center(i) + h * ((glm::vec3(x, y, z) + .5f) / 4.f - .5f));
                    if (occupied && occupied(p)) {
                        staticSamples_[i] |= std::uint64_t(1) << bit;
                        ++solid;
                    }
                }
        // One pressure control cannot represent two disconnected pieces of
        // free space in one voxel. Seal that ambiguous static voxel rather
        // than inventing a route through a thin wall. No extra cut cells.
        const std::uint64_t free = ~staticSamples_[i];
        if (free && solid) {
            unsigned first = 0;
            while (!(free & (std::uint64_t(1) << first)))
                ++first;
            std::array<unsigned, 64> queue{};
            unsigned begin = 0, end = 1;
            queue[0] = first;
            std::uint64_t visited = std::uint64_t(1) << first;
            while (begin < end) {
                unsigned at = queue[begin++];
                int x = int(at % 4), y = int((at / 4) % 4), z = int(at / 16);
                for (int axis = 0; axis < 3; ++axis)
                    for (int sign : {-1, 1}) {
                        int coord = axis == 0 ? x : axis == 1 ? y : z;
                        if (coord + sign < 0 || coord + sign > 3)
                            continue;
                        unsigned other = unsigned(int(at) + sign * (axis == 0   ? 1
                                                                    : axis == 1 ? 4
                                                                                : 16));
                        auto bit = std::uint64_t(1) << other;
                        if ((free & bit) && !(visited & bit)) {
                            visited |= bit;
                            queue[end++] = other;
                        }
                    }
            }
            if (visited != free) {
                staticSamples_[i] = ~std::uint64_t(0);
                solid = 64;
                ++sealedAmbiguousCells_;
            }
        }
        auto &c = cells_[i];
        c.staticCapacity = c.capacity = volume * (64 - solid) / 64.f;
        if (c.water > c.capacity) {
            c.displaced += c.water - c.capacity;
            c.water = c.capacity;
        }
        if (!airInitialized_)
            c.airMass = 1.2 * std::max(0., double(c.capacity) - c.water);
    }
    for (std::size_t f = 0; f < faces_.size(); ++f) {
        const auto &face = faces_[f];
        const int a = (face.axis + 1) % 3, b = (face.axis + 2) % 3;
        const auto center = .5f * (Center(face.a) + Center(face.b));
        unsigned open = 0, bit = 0;
        std::uint16_t mask = 0;
        for (int u = 0; u < 4; ++u)
            for (int v = 0; v < 4; ++v, ++bit) {
                auto p = center;
                p[a] += h * ((u + .5f) / 4 - .5f);
                p[b] += h * ((v + .5f) / 4 - .5f);
                if (!occupied || !occupied(ToWorld(p))) {
                    ++open;
                    mask |= std::uint16_t(1) << bit;
                }
            }
        if (cells_[face.a].staticCapacity == 0 || cells_[face.b].staticCapacity == 0) {
            open = 0;
            mask = 0;
        }
        staticFaces_[f] = faces_[f].open = open / 16.f;
        staticFaceSamples_[f] = mask;
    }
    samples_ = staticSamples_;
    faceSamples_ = staticFaceSamples_;
    UpdateHeights();
}
bool Grid::Geometry(std::string &error) {
    const auto start = std::chrono::steady_clock::now();
    const float h = settings_.cellSize, volume = h * h * h;
    if (staticSamples_.empty())
        staticSamples_.assign(cells_.size(), 0);
    auto mask = staticSamples_;
    solidOwner_.assign(cells_.size(), -1);
    std::vector<SolidBox> local;
    for (auto box : solids_) {
        if (!(box.half.x > 0 && box.half.y > 0 && box.half.z > 0) ||
            !std::isfinite(glm::dot(box.half, box.half)) ||
            !std::isfinite(glm::dot(box.center, box.center)) ||
            !(glm::dot(box.rotation, box.rotation) > 1e-10f &&
              std::isfinite(glm::dot(box.rotation, box.rotation)))) {
            error = "Invalid game-water solid box.";
            return false;
        }
        box.center = ToLocal(box.center);
        box.rotation = glm::normalize(glm::conjugate(settings_.rotation) * box.rotation);
        local.push_back(box);
        auto ext = glm::abs(box.rotation * glm::vec3(box.half.x, 0, 0)) +
                   glm::abs(box.rotation * glm::vec3(0, box.half.y, 0)) +
                   glm::abs(box.rotation * glm::vec3(0, 0, box.half.z));
        if (glm::any(glm::lessThan(box.center + ext, glm::vec3(0))) ||
            glm::any(glm::greaterThan(box.center - ext, glm::vec3(settings_.cells) * h)))
            continue;
        const auto lo = glm::ivec3(glm::floor(glm::clamp(
            (box.center - ext) / h, glm::vec3(0), glm::vec3(settings_.cells - glm::ivec3(1)))));
        const auto hi = glm::ivec3(glm::floor(glm::clamp(
            (box.center + ext) / h, glm::vec3(0), glm::vec3(settings_.cells - glm::ivec3(1)))));
        for (int z = lo.z; z <= hi.z; ++z)
            for (int y = lo.y; y <= hi.y; ++y)
                for (int x = lo.x; x <= hi.x; ++x) {
                    int i = Index(x, y, z);
                    unsigned bit = 0;
                    for (int c = 0; c < 4; ++c)
                        for (int b = 0; b < 4; ++b)
                            for (int a = 0; a < 4; ++a, ++bit) {
                                const auto p =
                                    Center(i) + h * ((glm::vec3(a, b, c) + .5f) / 4.f - .5f);
                                if (Inside(p, box)) {
                                    mask[i] |= std::uint64_t(1) << bit;
                                    solidOwner_[i] = int(local.size()) - 1;
                                }
                            }
                }
    }
    samples_ = mask;
    faceSamples_ = staticFaceSamples_;
    for (std::size_t i = 0; i < cells_.size(); ++i) {
        auto &c = cells_[i];
        c.capacity = volume * (64 - std::bitset<64>(mask[i]).count()) / 64.f;
        if (c.water > c.capacity) {
            c.displaced += c.water - c.capacity;
            c.water = c.capacity;
        }
    }
    for (std::size_t f = 0; f < faces_.size(); ++f) {
        auto &face = faces_[f];
        face.open = staticFaces_[f];
        if (face.open == 0) {
            face.velocity = 0;
            continue;
        }
        if (mask[face.a] == staticSamples_[face.a] && mask[face.b] == staticSamples_[face.b])
            continue;
        unsigned bit = 0;
        std::uint16_t free = staticFaceSamples_[f];
        const auto center = .5f * (Center(face.a) + Center(face.b));
        int a = (face.axis + 1) % 3, b = (face.axis + 2) % 3;
        for (int u = 0; u < 4; ++u)
            for (int v = 0; v < 4; ++v, ++bit) {
                auto p = center;
                p[a] += h * ((u + .5f) / 4 - .5f);
                p[b] += h * ((v + .5f) / 4 - .5f);
                for (const auto &box : local)
                    if (Inside(p, box)) {
                        free &= ~(std::uint16_t(1) << bit);
                        break;
                    }
            }
        faceSamples_[f] = free;
        face.open = float(std::bitset<16>(free).count()) / 16.f;
        if (face.open == 0)
            face.velocity = 0;
    }
    stats_.geometryMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return true;
}
void Grid::UpdateHeights() {
    waterHeight_.resize(cells_.size());
    const float h = settings_.cellSize;
    static const auto layers = []() {
        std::array<std::uint64_t, 4> masks{};
        for (int y = 0; y < 4; ++y)
            for (int z = 0; z < 4; ++z)
                for (int x = 0; x < 4; ++x)
                    masks[y] |= std::uint64_t(1) << ((z * 4 + y) * 4 + x);
        return masks;
    }();
    for (int i = 0; i < int(cells_.size()); ++i) {
        const auto free = ~samples_[i];
        const auto count = std::bitset<64>(free).count();
        float level = Coordinates(i).y * h;
        double left = cells_[i].water;
        if (count > 0 && left > 0)
            for (int y = 0; y < 4; ++y) {
                double capacity =
                    double(cells_[i].capacity) * std::bitset<64>(free & layers[y]).count() / count;
                if (capacity <= 0)
                    continue;
                level = h * (Coordinates(i).y + y * .25f) +
                        h * .25f * float(std::min(1., left / capacity));
                left -= capacity;
                if (left <= 1e-14)
                    break;
            }
        waterHeight_[i] = level;
    }
}
float Grid::DryArea(std::size_t index) const {
    const auto &face = faces_[index];
    const float level = std::max(waterHeight_[face.a], waterHeight_[face.b]),
                h = settings_.cellSize;
    const auto center = .5f * (Center(face.a) + Center(face.b));
    // Faces are stored from the lower to the upper grid coordinate. An
    // air-filled upper cell exposes its bottom opening. Compare material
    // occupancy directly, avoiding separately rounded world-plane positions.
    if (face.axis == 1)
        return cells_[face.b].water <= settings_.minimumMobileFraction * cells_[face.b].capacity
                   ? face.open
                   : 0;
    unsigned open = 0, bit = 0;
    const int a = (face.axis + 1) % 3;
    for (int u = 0; u < 4; ++u)
        for (int v = 0; v < 4; ++v, ++bit)
            if ((faceSamples_[index] & (std::uint16_t(1) << bit)) &&
                center.y + h * (((a == 1 ? u : v) + .5f) / 4 - .5f) >= level)
                ++open;
    return open / 16.f;
}
void Grid::ReleaseDisplacement() {
    // Explicit unresolved storage is quantity, not extra collision geometry.
    // Route it through connected free cells; retain it when no capacity exists.
    std::vector<int> seen(cells_.size(), -1), queue;
    for (int root = 0; root < int(cells_.size()); ++root) {
        if (cells_[root].displaced <= 0)
            continue;
        queue.clear();
        queue.push_back(root);
        seen[root] = root;
        for (std::size_t at = 0; at < queue.size() && cells_[root].displaced > 1e-15; ++at) {
            int i = queue[at];
            auto &target = cells_[i];
            const double amount = std::min(cells_[root].displaced,
                                           std::max(0., double(target.capacity) - target.water));
            target.water += amount;
            cells_[root].displaced -= amount;
            for (int f : incident_[i]) {
                auto face = faces_[f];
                int other = face.a == i ? face.b : face.a;
                // Start inside an excluded body, then exit through its old
                // static free-space graph. Never route through a static wall.
                const bool leavesOwner =
                    solidOwner_[root] >= 0 && solidOwner_[i] == solidOwner_[root] &&
                    (solidOwner_[other] == solidOwner_[root] || solidOwner_[other] < 0);
                if (staticFaces_[f] > 0 && cells_[other].staticCapacity > 0 &&
                    seen[other] != root && (faces_[f].open > 0 || leavesOwner)) {
                    seen[other] = root;
                    queue.push_back(other);
                }
            }
        }
    }
}
} // namespace JudasWater
