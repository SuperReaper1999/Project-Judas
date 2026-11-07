#include "GameWater.h"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace JudasWater {
void Grid::UpdateVelocity() {
    std::fill(velocity_.begin(), velocity_.end(), glm::vec3(0));
    std::vector<glm::vec3> weights(cells_.size(), glm::vec3(0));
    for (auto f : faces_)
        if (f.open > 0) {
            velocity_[f.a][f.axis] += f.open * f.velocity;
            velocity_[f.b][f.axis] += f.open * f.velocity;
            weights[f.a][f.axis] += f.open;
            weights[f.b][f.axis] += f.open;
        }
    for (std::size_t i = 0; i < cells_.size(); ++i)
        for (int a = 0; a < 3; ++a)
            velocity_[i][a] =
                cells_[i].water > 0 && weights[i][a] > 0 ? velocity_[i][a] / weights[i][a] : 0;
}
void Grid::AdvectVelocity(float dt) {
    const float h = settings_.cellSize;
    const auto gravity = glm::conjugate(settings_.rotation) * settings_.gravity;
    // Semi-Lagrangian Eulerian velocity transport. It is deliberately low-order
    // and numerically diffusive; authoritative water quantity is separate.
    auto sample = [&](glm::vec3 p) {
        p = p / h - .5f;
        const auto at = glm::ivec3(glm::floor(p));
        const auto t = glm::fract(p);
        glm::vec3 value(0);
        for (int z = 0; z < 2; ++z)
            for (int y = 0; y < 2; ++y)
                for (int x = 0; x < 2; ++x) {
                    auto c = glm::clamp(at + glm::ivec3(x, y, z), glm::ivec3(0),
                                        settings_.cells - glm::ivec3(1));
                    value += velocity_[Index(c.x, c.y, c.z)] *
                             ((x ? t.x : 1 - t.x) * (y ? t.y : 1 - t.y) * (z ? t.z : 1 - t.z));
                }
        return value;
    };
    for (auto &f : faces_) {
        if (f.open == 0 ||
            (cells_[f.a].water <= settings_.minimumMobileFraction * cells_[f.a].capacity &&
             cells_[f.b].water <= settings_.minimumMobileFraction * cells_[f.b].capacity)) {
            f.velocity = 0;
            continue;
        }
        const auto p = .5f * (Center(f.a) + Center(f.b));
        const auto velocity = sample(p);
        f.velocity = sample(p - dt * velocity)[f.axis] + dt * gravity[f.axis];
    }
}
void Grid::Transport(float dt) {
    const auto start = std::chrono::steady_clock::now();
    const double area = settings_.cellSize * settings_.cellSize;
    std::vector<double> transfer(faces_.size()), incoming(cells_.size()), outgoing(cells_.size());
    for (std::size_t f = 0; f < faces_.size(); ++f) {
        auto face = faces_[f];
        const auto &donor = cells_[face.velocity >= 0 ? face.a : face.b];
        double fraction = donor.capacity > 0 ? std::min(1., donor.water / donor.capacity) : 0;
        transfer[f] = face.velocity * face.open * area * dt * fraction;
    }
    // Monotone paired flux limiting enforces finite inventory/capacity. This
    // changes transfers, not quantity ledgers or a hidden volume correction.
    for (unsigned iteration = 0; iteration < 24; ++iteration) {
        std::fill(incoming.begin(), incoming.end(), 0);
        std::fill(outgoing.begin(), outgoing.end(), 0);
        for (std::size_t f = 0; f < faces_.size(); ++f) {
            auto face = faces_[f];
            double q = transfer[f];
            outgoing[q >= 0 ? face.a : face.b] += std::abs(q);
            incoming[q >= 0 ? face.b : face.a] += std::abs(q);
        }
        bool changed = false;
        for (std::size_t f = 0; f < faces_.size(); ++f) {
            auto face = faces_[f];
            double &q = transfer[f];
            int donor = q >= 0 ? face.a : face.b, receiver = q >= 0 ? face.b : face.a;
            double scale =
                std::min({1., cells_[donor].water / std::max(outgoing[donor], 1e-30),
                          std::max(0., double(cells_[receiver].capacity) -
                                           (gasReserve_.empty() ? 0 : gasReserve_[receiver]) -
                                           cells_[receiver].water + outgoing[receiver]) /
                              std::max(incoming[receiver], 1e-30)});
            changed |= scale < 1. - 1e-10;
            q *= scale;
        }
        if (!changed)
            break;
    }
    for (std::size_t f = 0; f < faces_.size(); ++f) {
        auto face = faces_[f];
        cells_[face.a].water -= transfer[f];
        cells_[face.b].water += transfer[f];
    }
    for (auto &c : cells_) {
        if (c.water > c.capacity) {
            c.displaced += c.water - c.capacity;
            c.water = c.capacity;
        }
        if (c.water < 0 && c.water > -1e-12)
            c.water = 0; // binary64 summation roundoff only
    }
    // A partly filled voxel is a mixed region at this resolution, not an
    // impermeable water film across its entire lower face. Let dense water
    // settle through that mixture while the same volume of gas rises. Without
    // this subgrid phase slip, numerical interface diffusion can strand a
    // compressed gas layer below a second, disconnected free-surface layer.
    // The gravity speed sets the finite exchange rate. This is a dissipative
    // unresolved phase closure, not a pressure clamp or discarded material.
    const auto gravity = glm::conjugate(settings_.rotation) * settings_.gravity;
    const double drift = std::sqrt(std::abs(gravity.y) * settings_.cellSize) * dt * area;
    for (const auto &face : faces_) {
        if (face.axis != 1 || face.open <= 0)
            continue;
        auto &lower = cells_[face.a];
        auto &upper = cells_[face.b];
        const double lowerGas = std::max(0., double(lower.capacity) - lower.water);
        const double upperGas = std::max(0., double(upper.capacity) - upper.water);
        if (lowerGas <= 0 || upperGas <= 0 || lower.capacity <= 0 || upper.capacity <= 0)
            continue;
        const double quantity =
            std::min({lowerGas, upper.water,
                      drift * face.open *
                          std::min(lowerGas / lower.capacity, upper.water / upper.capacity)});
        const double gasMass = lower.airMass * quantity / lowerGas;
        lower.water += quantity;
        upper.water -= quantity;
        lower.airMass -= gasMass;
        upper.airMass += gasMass;
    }
    stats_.transportMs +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}
} // namespace JudasWater
