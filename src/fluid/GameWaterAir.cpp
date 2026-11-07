#include "GameWater.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>

namespace JudasWater {
void Grid::ClassifyAir() {
    const int count = int(cells_.size());
    const double voxel = std::pow(settings_.cellSize, 3);
    const auto previous = air_;
    std::vector<int> parent(count);
    std::iota(parent.begin(), parent.end(), 0);
    auto root = [&](int i) {
        while (parent[i] != i) {
            parent[i] = parent[parent[i]];
            i = parent[i];
        }
        return i;
    };
    auto gas = [&](int i) { return std::max(0., double(cells_[i].capacity) - cells_[i].water); };
    auto resolved = [&](int i) {
        bool retained = cells_[i].air >= 0 && cells_[i].air < int(previous.size()) &&
                        !previous[cells_[i].air].atmosphere && cells_[i].airMass > 0;
        return gas(i) > voxel * 1e-12 &&
               (gas(i) > settings_.minimumAirFraction * cells_[i].capacity || retained);
    };
    for (std::size_t f = 0; f < faces_.size(); ++f) {
        const auto &face = faces_[f];
        // Only resolved, wide dry connections share reservoir pressure.
        // Narrow links remain finite resistors in the projection.
        if (resolved(face.a) && resolved(face.b) && DryArea(f) > .5f) {
            int l = root(face.a), r = root(face.b);
            if (l != r)
                parent[std::max(l, r)] = std::min(l, r);
        }
    }
    // Keep sub-resolution daughters in their existing reservoir. Only a
    // resolved separation earns independent pressure ownership. This bounds
    // topology work by game resolution rather than interface fragmentation.
    std::vector<double> componentVolume(count);
    for (int i = 0; i < count; ++i)
        if (resolved(i))
            componentVolume[root(i)] += gas(i);
    const double minimumVolume = voxel * settings_.minimumReservoirCells;
    std::vector<std::map<int, double>> children(previous.size());
    for (int i = 0; i < count; ++i)
        if (resolved(i) && cells_[i].air >= 0)
            children[cells_[i].air][root(i)] += gas(i);
    for (int k = 0; k < int(previous.size()); ++k)
        if (!previous[k].atmosphere && !children[k].empty()) {
            int anchor = children[k].begin()->first;
            for (auto child : children[k])
                if (componentVolume[root(child.first)] > componentVolume[root(anchor)])
                    anchor = child.first;
            for (auto child : children[k]) {
                int a = root(anchor), b = root(child.first);
                if (a != b && componentVolume[b] < minimumVolume) {
                    parent[b] = a;
                    componentVolume[a] += componentVolume[b];
                }
            }
        }
    // Condense small connected throat storage into an adjacent resolved
    // reservoir. A connection to a different resolved reservoir remains a
    // finite resistor; no large chambers are unioned through a narrow vent.
    std::vector<double> componentMass(count);
    for (int i = 0; i < count; ++i)
        if (resolved(i))
            componentMass[root(i)] += cells_[i].airMass;
    auto large = [&](int i) {
        return componentVolume[i] >= minimumVolume || componentMass[i] >= 1.2 * minimumVolume;
    };
    for (unsigned pass = 0; pass < settings_.minimumReservoirCells; ++pass) {
        bool changed = false;
        for (std::size_t f = 0; f < faces_.size(); ++f) {
            const auto &face = faces_[f];
            if (!resolved(face.a) || !resolved(face.b) || DryArea(f) <= 0)
                continue;
            int a = root(face.a), b = root(face.b);
            if (a == b || large(a) == large(b))
                continue;
            if (large(b))
                std::swap(a, b);
            parent[b] = a;
            componentVolume[a] += componentVolume[b];
            componentMass[a] += componentMass[b];
            changed = true;
        }
        if (!changed)
            break;
    }
    std::vector<bool> atmosphericAncestor(count), sealedAncestor(count);
    for (int i = 0; i < count; ++i)
        if (resolved(i) && cells_[i].air >= 0) {
            if (previous[cells_[i].air].atmosphere)
                atmosphericAncestor[root(i)] = true;
            else
                sealedAncestor[root(i)] = true;
        }
    // Reservoirs own mass across moving interfaces. Recompressing each old
    // cell's mass independently would manufacture stiff microscopic pockets.
    std::vector<int> previousOwner(count);
    std::vector<double> oldMass(previous.size());
    for (int i = 0; i < count; ++i) {
        previousOwner[i] = cells_[i].air;
        if (cells_[i].air >= 0)
            oldMass[cells_[i].air] += cells_[i].airMass;
    }
    air_.clear();
    std::vector<int> index(count, -1);
    for (int i = 0; i < count; ++i) {
        auto &cell = cells_[i];
        const bool active = resolved(i);
        cell.air = -1;
        if (!active)
            continue;
        int r = root(i);
        if (index[r] < 0) {
            index[r] = int(air_.size());
            air_.push_back({});
        }
        cell.air = index[r];
        auto &pocket = air_[cell.air];
        pocket.volume += gas(i);
        pocket.atmosphere |= Coordinates(i).y == settings_.cells.y - 1;
        pocket.atmosphere |=
            componentVolume[r] < minimumVolume && atmosphericAncestor[r] && !sealedAncestor[r];
    }
    if (previous.empty()) {
        for (int i = 0; i < count; ++i)
            if (cells_[i].air >= 0)
                air_[cells_[i].air].mass += cells_[i].airMass;
    } else {
        std::vector<std::map<int, double>> overlap(previous.size());
        for (int i = 0; i < count; ++i)
            if (previousOwner[i] >= 0 && cells_[i].air >= 0)
                overlap[previousOwner[i]][cells_[i].air] += gas(i);
        for (int k = 0; k < int(previous.size()); ++k) {
            double volume = 0;
            for (auto entry : overlap[k])
                volume += entry.second;
            if (volume <= 0)
                continue; // unresolved mass remains explicitly stored
            for (auto entry : overlap[k]) {
                double amount = previous[k].atmosphere ? 1.2 * entry.second
                                                       : oldMass[k] * entry.second / volume;
                air_[entry.first].mass += amount;
            }
            if (previous[k].atmosphere)
                exteriorAirMass_ += oldMass[k] - 1.2 * volume;
        }
        for (int i = 0; i < count; ++i)
            if (previousOwner[i] >= 0 && !overlap[previousOwner[i]].empty())
                cells_[i].airMass = 0;
        for (int i = 0; i < count; ++i)
            if (previousOwner[i] < 0 && cells_[i].air >= 0)
                air_[cells_[i].air].mass += cells_[i].airMass;
    }
    for (auto &pocket : air_) {
        if (pocket.atmosphere) {
            const double mass = 1.2 * pocket.volume;
            exteriorAirMass_ += pocket.mass - mass;
            pocket.mass = mass;
            pocket.pressure = 0;
        } else if (pocket.volume < minimumVolume && pocket.mass < 1.2 * minimumVolume) {
            // Sub-resolution bubbles retain their air mass, but use the
            // free-surface traction approximation instead of adding stiff
            // microscopic pressure constraints. They may later merge into a
            // resolved cavity. No material is discarded.
            pocket.unresolved = true;
            pocket.pressure = 0;
        } else if (pocket.mass > 0) {
            pocket.pressure =
                float(101325. *
                      (std::pow(std::max(pocket.mass, 1e-30) / (1.2 * pocket.volume), 1.4) - 1.));
        } else {
            // A numerical free-surface void is not invented conserved air.
            // Vacuum/cavitation dynamics are outside this game model; use
            // atmospheric traction until real air reaches the region.
            pocket.pressure = 0;
        }
    }
    for (int i = 0; i < count; ++i)
        if (cells_[i].air >= 0) {
            const auto &pocket = air_[cells_[i].air];
            cells_[i].airMass = pocket.mass * gas(i) / pocket.volume;
        }
}
} // namespace JudasWater
