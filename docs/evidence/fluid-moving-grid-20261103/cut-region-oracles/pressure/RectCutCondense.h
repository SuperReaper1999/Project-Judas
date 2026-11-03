#pragma once

#include <map>

namespace {

struct PressureValue {
    double base = 0., correction = 0.;
};

double Difference(const PressureValue& left, const PressureValue& right) {
    return (left.base - right.base) + (left.correction - right.correction);
}

struct EliminatedPressure {
    std::size_t node;
    double source;
    std::vector<std::pair<std::size_t, double>> weights;
};

// Eliminate the selected cut nodes by symmetric star–mesh transformations.
// Each recovery is stored relative to its strongest retained neighbor so
// the pressure difference on a narrow, highly conductive link is retained.
bool SolveCondensed(const std::vector<judas::volume_grid::FaceFlux>& graph,
                    std::vector<double> rhs, std::vector<bool> eliminate,
                    double tolerance, double cellVolume, std::vector<PressureValue>& output,
                    int& iterations, std::size_t& retainedEdges, std::string& error) {
    using namespace judas::volume_grid;
    std::vector<std::map<std::size_t, double>> neighbors(rhs.size());
    for (const auto& edge : graph) {
        neighbors[edge.left][edge.right] += edge.conductance;
        neighbors[edge.right][edge.left] += edge.conductance;
    }
    std::vector<EliminatedPressure> recovery;
    for (;;) {
        std::size_t chosen = rhs.size(), degree = rhs.size();
        for (std::size_t i = 0; i < rhs.size(); ++i) {
            if (!eliminate[i] || neighbors[i].empty())
                continue;
            if (neighbors[i].size() < degree) {
                chosen = i;
                degree = neighbors[i].size();
            }
        }
        if (chosen == rhs.size())
            break;
        double sum = 0.;
        for (const auto& entry : neighbors[chosen])
            sum += entry.second;
        if (!(sum > 0.) || !std::isfinite(sum)) {
            error = "invalid condensed pressure diagonal";
            return false;
        }
        EliminatedPressure row{chosen, rhs[chosen] / sum, {}};
        for (const auto& [i, ci] : neighbors[chosen]) {
            const double weight = ci / sum;
            row.weights.emplace_back(i, weight);
            rhs[i] += weight * rhs[chosen];
            neighbors[i].erase(chosen);
            for (auto other = neighbors[chosen].upper_bound(i); other != neighbors[chosen].end(); ++other) {
                const auto [j, cj] = *other;
                const double fill = weight * cj;
                neighbors[i][j] += fill;
                neighbors[j][i] += fill;
            }
        }
        rhs[chosen] = 0.;
        neighbors[chosen].clear();
        eliminate[chosen] = false;
        recovery.push_back(std::move(row));
    }

    std::vector<FaceFlux> condensed;
    for (std::size_t i = 0; i < neighbors.size(); ++i) {
        for (const auto& [j, value] : neighbors[i]) {
            if (i < j && value > 0.)
                condensed.push_back({i, j, 0, 0, 0., 0., value, 0., 0.});
        }
    }
    retainedEdges = condensed.size();
    std::vector<double> pressure(rhs.size());
    if (!SolvePressure(condensed, rhs, tolerance, cellVolume, pressure, iterations, error))
        return false;
    std::vector<PressureValue> values;
    values.reserve(rhs.size());
    for (const double p : pressure)
        values.push_back({p, 0.});
    for (auto item = recovery.rbegin(); item != recovery.rend(); ++item) {
        const auto anchor = std::max_element(item->weights.begin(), item->weights.end(),
            [](const auto& lhs, const auto& rhs) { return lhs.second < rhs.second; })->first;
        double offset = item->source;
        for (const auto& [j, weight] : item->weights)
            offset += weight * Difference(values[j], values[anchor]);
        values[item->node] = {values[anchor].base, values[anchor].correction + offset};
    }
    output = std::move(values);
    return true;
}

} // namespace
