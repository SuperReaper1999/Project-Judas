#include "CutRegionPressure.h"
#include "MovingVolumePressureInternal.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <map>
#include <queue>
#include <tuple>
#include <utility>

namespace judas::volume_grid {
namespace {

// A recovered small pressure difference must survive addition to its much
// larger common gauge. Retain the roundoff of that sum on thin cut links.
struct PressureValue {
    double value = 0., remainder = 0.;
};

PressureValue Add(PressureValue a, double b) {
    const double sum = a.value + b;
    const double share = sum - a.value;
    const double tail = (a.value - (sum - share)) + (b - share) + a.remainder;
    const double total = sum + tail;
    return {total, tail - (total - sum)};
}

double Difference(PressureValue a, PressureValue b) {
    return (a.value - b.value) + (a.remainder - b.remainder);
}

struct EliminatedRow {
    std::size_t node, anchor;
    double diagonal;
    std::vector<std::pair<std::size_t, double>> weights;
};

class PressureReduction {
public:
    bool Build(const std::vector<CutRegionPressureFace>& faces, std::size_t count,
               std::string& error) {
        using NeighborMap = std::map<std::size_t, double>;
        std::vector<NeighborMap> neighbors(count);
        for (const auto& face : faces) {
            neighbors[face.left][face.right] += face.conductance;
            neighbors[face.right][face.left] += face.conductance;
        }
        std::vector<bool> selected(count);
        for (const auto& face : faces) {
            if (!face.thin || selected[face.left] || selected[face.right])
                continue;
            const auto node = neighbors[face.left].size() <= neighbors[face.right].size()
                ? face.left : face.right;
            selected[node] = true;
        }

        using Candidate = std::tuple<std::size_t, double, std::size_t>;
        std::priority_queue<Candidate, std::vector<Candidate>, std::greater<>> queue;
        const auto schedule = [&](std::size_t node) {
            const auto degree = neighbors[node].size();
            // Leave high-fill rows in the same sparse operator.
            if (degree == 1 || (selected[node] && degree > 1 && degree <= 12))
                queue.emplace(degree, degree == 1 ? neighbors[node].begin()->second : 0., node);
        };
        for (std::size_t node = 0; node < count; ++node)
            schedule(node);
        while (!queue.empty()) {
            const auto [degree, conductance, node] = queue.top();
            queue.pop();
            if (neighbors[node].size() != degree ||
                (degree == 1 && conductance != neighbors[node].begin()->second))
                continue;
            double diagonal = 0.;
            auto anchor = neighbors[node].begin();
            for (auto entry = neighbors[node].begin(); entry != neighbors[node].end(); ++entry) {
                diagonal += entry->second;
                if (entry->second > anchor->second)
                    anchor = entry;
            }
            if (!(diagonal > 0.) || !std::isfinite(diagonal)) {
                error = "cut-region pressure reduction lost a finite diagonal";
                return false;
            }
            EliminatedRow row{node, anchor->first, diagonal, {}};
            for (auto entry = neighbors[node].begin(); entry != neighbors[node].end(); ++entry) {
                const auto [i, conductance] = *entry;
                const double weight = conductance / diagonal;
                row.weights.emplace_back(i, weight);
                neighbors[i].erase(node);
                for (auto other = std::next(entry); other != neighbors[node].end(); ++other) {
                    const auto [j, cj] = *other;
                    const double fill = weight * cj;
                    if (fill > 0.) {
                        neighbors[i][j] += fill;
                        neighbors[j][i] += fill;
                    }
                }
            }
            neighbors[node].clear();
            for (const auto& [neighbor, weight] : row.weights) {
                (void)weight;
                schedule(neighbor);
            }
            eliminated_.push_back(std::move(row));
        }
        for (std::size_t i = 0; i < count; ++i)
            for (const auto& [j, conductance] : neighbors[i])
                if (i < j)
                    links_.push_back({i, j, conductance});
        return true;
    }

    bool Solve(std::vector<double> rhs, double tolerance, double cellVolume,
               std::vector<PressureValue>& output, int& iterations, std::string& error) const {
        std::vector<double> eliminatedSource;
        eliminatedSource.reserve(eliminated_.size());
        for (const auto& row : eliminated_) {
            const double source = rhs[row.node];
            eliminatedSource.push_back(source / row.diagonal);
            for (const auto& [neighbor, weight] : row.weights)
                rhs[neighbor] += weight * source;
            rhs[row.node] = 0.;
        }
        int count = 0;
        std::vector<double> p(rhs.size());
        if (!detail::SolveVolumePressure(links_, rhs, tolerance, cellVolume, p, count, error))
            return false;
        iterations += count;
        output.resize(p.size());
        for (std::size_t i = 0; i < p.size(); ++i)
            output[i] = {p[i], 0.};
        for (std::size_t i = eliminated_.size(); i-- > 0;) {
            const auto& row = eliminated_[i];
            double difference = eliminatedSource[i];
            for (const auto& [neighbor, weight] : row.weights)
                difference += weight * Difference(output[neighbor], output[row.anchor]);
            output[row.node] = Add(output[row.anchor], difference);
        }
        return true;
    }

    std::size_t Connections() const { return links_.size(); }

private:
    std::vector<detail::VolumePressureLink> links_;
    std::vector<EliminatedRow> eliminated_;
};

void AddCompensated(double value, double& sum, double& roundoff) {
    const double adjusted = value - roundoff;
    const double next = sum + adjusted;
    roundoff = (next - sum) - adjusted;
    sum = next;
}

double CheckBalance(const std::vector<CutRegionPressureFace>& faces,
                    const std::vector<double>& source, const std::vector<PressureValue>& pressure,
                    std::vector<double>& deficit, CutRegionPressure& result) {
    deficit = source;
    std::vector<double> roundoff(source.size());
    result.volumeFlux.resize(faces.size());
    result.pressureJump.resize(faces.size());
    result.pressureWork = result.pressureCorrectionEnergy = 0.;
    double workRoundoff = 0., energyRoundoff = 0.;
    for (std::size_t i = 0; i < faces.size(); ++i) {
        const auto& face = faces[i];
        const double delta = Difference(pressure[face.left], pressure[face.right]);
        const double correction = face.conductance * delta;
        const double flux = face.predictedVolume + correction;
        if (!std::isfinite(delta) || !std::isfinite(flux))
            return std::numeric_limits<double>::infinity();
        result.volumeFlux[i] = flux;
        result.pressureJump[i] = delta;
        AddCompensated(-flux, deficit[face.left], roundoff[face.left]);
        AddCompensated(flux, deficit[face.right], roundoff[face.right]);
        AddCompensated(delta * flux, result.pressureWork, workRoundoff);
        AddCompensated(.5 * delta * correction, result.pressureCorrectionEnergy, energyRoundoff);
    }
    double maximum = 0.;
    for (double value : deficit) {
        if (!std::isfinite(value))
            return std::numeric_limits<double>::infinity();
        maximum = std::max(maximum, std::abs(value));
    }
    return maximum;
}

double RelativeWorkError(const std::vector<double>& source,
                         const std::vector<PressureValue>& pressure, double faceWork) {
    double capacityWork = 0., roundoff = 0.;
    for (std::size_t i = 0; i < source.size(); ++i)
        AddCompensated(source[i] * pressure[i].value + source[i] * pressure[i].remainder,
                       capacityWork, roundoff);
    return std::abs(capacityWork - faceWork) / (1. + std::abs(capacityWork) + std::abs(faceWork));
}

} // namespace

bool ProjectCutRegionPressure(const std::vector<double>& volumeSource,
                              const std::vector<CutRegionPressureFace>& faces,
                              double cellVolume, double tolerance,
                              CutRegionPressure& output, std::string& error) {
    const auto count = volumeSource.size();
    if (count == 0 || !(cellVolume > 0.) || !std::isfinite(cellVolume) ||
        !(tolerance > 0.) || !std::isfinite(tolerance) ||
        std::any_of(volumeSource.begin(), volumeSource.end(), [](double v) { return !std::isfinite(v); })) {
        error = "invalid cut-region pressure source or tolerance";
        return false;
    }
    auto rhs = volumeSource;
    std::vector<double> rhsRoundoff(count);
    std::vector<detail::VolumePressureLink> links;
    links.reserve(faces.size());
    for (const auto& face : faces) {
        if (face.left >= count || face.right >= count || face.left == face.right ||
            !(face.conductance > 0.) || !std::isfinite(face.conductance) ||
            !std::isfinite(face.predictedVolume)) {
            error = "invalid cut-region pressure connection";
            return false;
        }
        AddCompensated(-face.predictedVolume, rhs[face.left], rhsRoundoff[face.left]);
        AddCompensated(face.predictedVolume, rhs[face.right], rhsRoundoff[face.right]);
        links.push_back({face.left, face.right, face.conductance});
    }
    CutRegionPressure next;
    PressureReduction plan;
    std::vector<PressureValue> pressure;
    if (!detail::BalanceVolumePressureSources(links, rhs, cellVolume, error) ||
        !plan.Build(faces, count, error) ||
        !plan.Solve(rhs, tolerance * .25, cellVolume, pressure, next.iterations, error))
        return false;
    next.reducedConnections = plan.Connections();
    std::vector<double> deficit;
    next.maximumBalanceError = CheckBalance(faces, volumeSource, pressure, deficit, next);
    double workError = RelativeWorkError(volumeSource, pressure, next.pressureWork);
    constexpr double workTolerance = 1e-11;
    for (int pass = 0; pass < 3 && next.maximumBalanceError > 0. &&
         (next.maximumBalanceError > tolerance || workError > workTolerance); ++pass) {
        // Small local divergence errors can accumulate against pressure
        // over a large chart. Refine the recovered fluxes on the original
        // edges when either conservation or the work pairing needs it.
        const double target = std::min(tolerance, next.maximumBalanceError) * .025;
        std::vector<PressureValue> correction;
        if (!detail::BalanceVolumePressureSources(links, deficit, cellVolume, error) ||
            !plan.Solve(deficit, target, cellVolume, correction, next.iterations, error))
            return false;
        for (std::size_t i = 0; i < count; ++i)
            pressure[i] = Add(Add(pressure[i], correction[i].value), correction[i].remainder);
        next.maximumBalanceError = CheckBalance(faces, volumeSource, pressure, deficit, next);
        workError = RelativeWorkError(volumeSource, pressure, next.pressureWork);
    }
    if (!(next.maximumBalanceError <= tolerance) || !std::isfinite(next.pressureWork) ||
        !std::isfinite(next.pressureCorrectionEnergy)) {
        error = "cut-region pressure failed the physical face balance";
        return false;
    }
    if (!(workError <= workTolerance)) {
        error = "cut-region pressure failed the capacity work pairing";
        return false;
    }
    next.pressure.reserve(count);
    for (const auto& value : pressure)
        next.pressure.push_back(value.value + value.remainder);
    output = std::move(next);
    error.clear();
    return true;
}

} // namespace judas::volume_grid
