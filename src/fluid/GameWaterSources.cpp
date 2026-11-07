#include "GameWater.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace JudasWater {
double Grid::Quantity() const {
    double quantity = 0;
    for (const auto &cell : cells_)
        quantity += cell.water + cell.displaced;
    return quantity;
}
double Grid::ChangeWater(glm::vec3 world, float radius, double quantity) {
    if (cells_.empty() || !std::isfinite(glm::dot(world, world)) || !std::isfinite(radius) ||
        radius <= 0 || !std::isfinite(quantity))
        throw std::invalid_argument(
            "water sources require an initialized grid, finite point/quantity and positive radius");
    if (quantity == 0)
        return 0;
    if (!airInitialized_) {
        UpdateHeights();
        ClassifyAir();
        airInitialized_ = true;
    }
    const auto p = glm::dvec3(ToLocal(world));
    const double r = radius, h = settings_.cellSize;
    const auto dimensions = glm::dvec3(settings_.cells);
    if (glm::any(glm::lessThan(p + r, glm::dvec3(0))) ||
        glm::any(glm::greaterThanEqual(p - r, dimensions * h)))
        return 0;
    const auto lo = glm::ivec3(glm::floor(glm::clamp((p - r) / h, glm::dvec3(0), dimensions - 1.)));
    const auto hi = glm::ivec3(glm::floor(glm::clamp((p + r) / h, glm::dvec3(0), dimensions - 1.)));
    double remaining = std::abs(quantity), accepted = 0;
    for (int z = lo.z; z <= hi.z; ++z)
        for (int y = lo.y; y <= hi.y; ++y)
            for (int x = lo.x; x <= hi.x; ++x) {
                const int i = Index(x, y, z);
                if (glm::length(glm::dvec3(Center(i)) - p) > r)
                    continue;
                auto &cell = cells_[i];
                const bool atmosphere = cell.air >= 0 && air_[cell.air].atmosphere;
                const double capacity =
                    quantity > 0
                        ? (atmosphere ? std::max(0., double(cell.capacity) - cell.water) : 0)
                        : cell.water;
                const double amount = std::min(remaining, capacity);
                if (quantity > 0) {
                    cell.water += amount;
                    const double gas = std::min(cell.airMass, 1.2 * amount);
                    cell.airMass -= gas;
                    exteriorAirMass_ += gas;
                } else {
                    cell.water -= amount;
                    if (atmosphere) {
                        cell.airMass += 1.2 * amount;
                        exteriorAirMass_ -= 1.2 * amount;
                    }
                }
                accepted += amount;
                remaining -= amount;
            }
    if (quantity > 0)
        addedQuantity_ += accepted;
    else
        removedQuantity_ += accepted;
    UpdateHeights();
    ClassifyAir();
    return quantity > 0 ? accepted : -accepted;
}
} // namespace JudasWater
