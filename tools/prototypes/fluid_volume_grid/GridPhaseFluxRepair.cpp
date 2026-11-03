#include "GridPhaseFluxRepair.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace judas::volume_grid {
namespace {

constexpr double kRoundoff = 256 * std::numeric_limits<double>::epsilon();

bool Fail(std::string& error, const char* reason) {
    error = reason;
    return false;
}

bool Nonnegative(double value) {
    return std::isfinite(value) && value >= 0.;
}

struct ArcRef {
    std::size_t node, index;
};

struct BoundedArc {
    ArcRef ref;
    double lower, upper, initial;
};

class ResidualNetwork {
public:
    explicit ResidualNetwork(std::size_t nodes)
        : edges_(nodes + 2), balance_(nodes), scale_(nodes), source_(nodes), sink_(nodes + 1) {}

    BoundedArc Add(std::size_t from, std::size_t to, double lower, double upper, double initial) {
        balance_[from] -= initial;
        balance_[to] += initial;
        scale_[from] += initial;
        scale_[to] += initial;
        return {AddResidual(from, to, upper - initial, initial - lower), lower, upper, initial};
    }

    void Supply(std::size_t node, double amount) {
        balance_[node] += amount;
        scale_[node] += std::abs(amount);
    }

    double Flow(const BoundedArc& arc) const {
        const auto& forward = Get(arc.ref);
        const double reverse = edges_[forward.to][forward.reverse].capacity;
        if (reverse == arc.initial - arc.lower)
            return arc.initial;
        return std::clamp(arc.lower + reverse, arc.lower, arc.upper);
    }

    struct DepartureLimit {
        bool ordinary = false;
        GridPhaseInventoryRoundoff bound{};
    };

    bool Close(double tolerance, const std::vector<DepartureLimit>& departureLimits) {
        const auto errorAt = [&](std::size_t node) {
            if (node < departureLimits.size() && departureLimits[node].ordinary) {
                const auto& bound = departureLimits[node].bound;
                // A water deficit overspends water. A water surplus leaves
                // less water than the minimum needed to cover outgoing volume.
                return balance_[node] < 0. ? bound.water : bound.air;
            }
            return tolerance + kRoundoff * scale_[node];
        };
        std::vector<ArcRef> demands;
        std::vector<double> errors;
        for (std::size_t node = 0; node < balance_.size(); ++node) {
            if (!std::isfinite(balance_[node]) || !std::isfinite(scale_[node]))
                return false;
            const double error = errorAt(node);
            if (!std::isfinite(error))
                return false;
            if (std::abs(balance_[node]) <= error)
                continue;
            const auto arc = balance_[node] > 0.
                ? AddResidual(source_, node, balance_[node], 0.)
                : AddResidual(node, sink_, -balance_[node], 0.);
            demands.push_back(arc);
            errors.push_back(error);
        }
        // Register larger defects first, but retain small real source/sink
        // residuals whenever a larger defect needs them as a matching route.
        if (!demands.empty()) {
            for (std::size_t node = 0; node < balance_.size(); ++node) {
                const double error = errorAt(node);
                if (balance_[node] == 0. || std::abs(balance_[node]) > error)
                    continue;
                if (balance_[node] > 0.)
                    AddResidual(source_, node, balance_[node], 0.);
                else
                    AddResidual(node, sink_, -balance_[node], 0.);
            }
            MaxFlow();
        }
        for (std::size_t i = 0; i < demands.size(); ++i)
            if (Get(demands[i]).capacity > errors[i])
                return false;
        return true;
    }

private:
    struct Arc {
        std::size_t to, reverse;
        double capacity;
    };

    ArcRef AddResidual(std::size_t from, std::size_t to, double forward, double reverse) {
        const ArcRef ref{from, edges_[from].size()};
        edges_[from].push_back({to, edges_[to].size(), forward});
        edges_[to].push_back({from, ref.index, reverse});
        return ref;
    }

    Arc& Get(const ArcRef& ref) { return edges_[ref.node][ref.index]; }
    const Arc& Get(const ArcRef& ref) const { return edges_[ref.node][ref.index]; }

    void MaxFlow() {
        const auto absent = edges_.size();
        std::vector<std::size_t> level(absent), next(absent), queue;
        std::vector<ArcRef> path;
        queue.reserve(absent);
        for (;;) {
            std::fill(level.begin(), level.end(), absent);
            std::fill(next.begin(), next.end(), 0);
            queue.assign(1, source_);
            level[source_] = 0;
            for (std::size_t i = 0; i < queue.size(); ++i) {
                const auto from = queue[i];
                for (const auto& arc : edges_[from]) {
                    if (arc.capacity > 0. && level[arc.to] == absent) {
                        level[arc.to] = level[from] + 1;
                        queue.push_back(arc.to);
                    }
                }
            }
            if (level[sink_] == absent)
                return;

            // Iterative blocking flow. Long chains of cells do not consume
            // stack space, and each augmentation saturates at least one arc.
            auto node = source_;
            path.clear();
            for (;;) {
                if (node == sink_) {
                    double amount = std::numeric_limits<double>::infinity();
                    std::size_t bottleneck = 0;
                    for (std::size_t i = 0; i < path.size(); ++i) {
                        if (Get(path[i]).capacity < amount) {
                            amount = Get(path[i]).capacity;
                            bottleneck = i;
                        }
                    }
                    for (const auto& ref : path) {
                        auto& arc = Get(ref);
                        arc.capacity -= amount;
                        edges_[arc.to][arc.reverse].capacity += amount;
                    }
                    node = path[bottleneck].node;
                    path.resize(bottleneck);
                }
                auto& index = next[node];
                while (index < edges_[node].size()) {
                    const auto& arc = edges_[node][index];
                    if (arc.capacity > 0. && level[arc.to] != absent && level[arc.to] == level[node] + 1)
                        break;
                    ++index;
                }
                if (index < edges_[node].size()) {
                    path.push_back({node, index});
                    node = edges_[node][index].to;
                } else {
                    level[node] = absent;
                    if (path.empty())
                        break;
                    node = path.back().node;
                    path.pop_back();
                }
            }
        }
    }

    std::vector<std::vector<Arc>> edges_;
    std::vector<double> balance_, scale_;
    std::size_t source_, sink_;
};

struct Traffic {
    double in = 0., out = 0.;
};

template<class Allows>
std::vector<bool> ReachPhase(const GridPhaseFluxRepairInput& input, bool wet, const Allows& allows) {
    const auto count = input.startWater.size();
    std::vector<std::vector<std::size_t>> destinations(count);
    for (std::size_t face = 0; face < input.edges.size(); ++face) {
        const auto& edge = input.edges[face];
        if (allows(face))
            destinations[edge.from].push_back(edge.to);
    }
    std::vector<bool> reached(count);
    std::vector<std::size_t> queue;
    for (std::size_t cell = 0; cell < count; ++cell) {
        if ((wet ? input.startWater[cell] : input.startCapacity[cell] - input.startWater[cell]) > 0.) {
            reached[cell] = true;
            queue.push_back(cell);
        }
    }
    for (std::size_t i = 0; i < queue.size(); ++i) {
        for (const auto target : destinations[queue[i]]) {
            if (!reached[target] && (!input.repairUnrootedRelays || input.relayAllowed[target])) {
                reached[target] = true;
                queue.push_back(target);
            }
        }
    }
    return reached;
}

} // namespace

GridPhaseInventoryRoundoff MeasureGridPhaseInventoryRoundoff(
    double capacity, double volumeResidual, double massInAirVolumeUnits) {
    const double limit = kRoundoff * capacity;
    return {limit, limit + std::min(volumeResidual, kRoundoff * massInAirVolumeUnits)};
}

namespace {

bool SolveGridPhaseFlux(const GridPhaseFluxRepairInput& input,
                        GridPhaseFluxRepairResult& output, std::string& error) {
    const auto count = input.startCapacity.size();
    if (count > (std::numeric_limits<std::size_t>::max() - 3) / 3 ||
        input.endCapacity.size() != count || input.startWater.size() != count ||
        (!input.relayAllowed.empty() && input.relayAllowed.size() != count) ||
        !Nonnegative(input.balanceTolerance) || !Nonnegative(input.localOrdinaryDensityRatio))
        return Fail(error, "invalid phase flux repair dimensions or tolerance");

    GridPhaseFluxRepairResult result;
    result.waterFlux.resize(input.edges.size());
    const auto relay = [&](std::size_t node) {
        return !input.relayAllowed.empty() && input.relayAllowed[node];
    };
    for (std::size_t node = 0; node < count; ++node) {
        const double start = input.startCapacity[node], end = input.endCapacity[node];
        const double water = input.startWater[node];
        if (!Nonnegative(start) || !Nonnegative(end) || !Nonnegative(water) || water > start)
            return Fail(error, "invalid endpoint material in phase flux repair");
    }
    std::vector<Traffic> mixture(count), preferred(count);
    std::vector<std::pair<double, double>> faceBounds;
    faceBounds.reserve(input.edges.size());
    for (std::size_t face = 0; face < input.edges.size(); ++face) {
        const auto& edge = input.edges[face];
        if (edge.from >= count || edge.to >= count || edge.from == edge.to || !Nonnegative(edge.volume) ||
            !Nonnegative(edge.minWater) || !Nonnegative(edge.maxWater) ||
            edge.minWater > edge.maxWater || edge.maxWater > edge.volume || !std::isfinite(edge.preferredWater))
            return Fail(error, "invalid geometric bounds in phase flux repair");
        double lower = edge.minWater, upper = edge.maxWater;
        if (!relay(edge.from) && edge.volume > 0.) {
            const double start = input.startCapacity[edge.from], water = input.startWater[edge.from];
            if (start == 0.)
                return Fail(error, "fixed mixture flux leaves an empty ordinary donor");
            if (water == 0.)
                upper = 0.;
            else if (water == start)
                lower = edge.volume;
            if (lower > upper)
                return Fail(error, "geometric phase bounds require an absent donor phase");
        }
        faceBounds.emplace_back(lower, upper);
        mixture[edge.from].out += edge.volume;
        mixture[edge.to].in += edge.volume;
    }
    if (!input.relayAllowed.empty()) {
        // A permitted face alone is not a material source. Eliminate a
        // phase from regions unreachable even if every admissible port opens.
        const auto wet = ReachPhase(input, true, [&](std::size_t f) { return faceBounds[f].second > 0.; });
        const auto air = ReachPhase(input, false, [&](std::size_t f) {
            return faceBounds[f].first < input.edges[f].volume;
        });
        for (std::size_t face = 0; face < input.edges.size(); ++face) {
            const auto& edge = input.edges[face];
            auto& [lower, upper] = faceBounds[face];
            if (!wet[edge.from])
                upper = 0.;
            if (!air[edge.from])
                lower = edge.volume;
            if (lower > upper)
                return Fail(error, "phase flux bounds require an unowned material route");
        }
    }
    for (std::size_t face = 0; face < input.edges.size(); ++face) {
        const auto& edge = input.edges[face];
        const auto [lower, upper] = faceBounds[face];
        const double water = std::clamp(edge.preferredWater, lower, upper);
        result.waterFlux[face] = water;
        preferred[edge.from].out += water;
        preferred[edge.to].in += water;
    }

    const auto store = [](std::size_t node) { return 3 * node; };
    const auto departure = [&](std::size_t node) { return store(node) + !relay(node); };
    const auto arrival = [&](std::size_t node) { return store(node) + 2 * !relay(node); };
    const auto sink = 3 * count;
    ResidualNetwork network(sink + 1);
    std::vector<ResidualNetwork::DepartureLimit> departureLimits;
    if (input.localOrdinaryDensityRatio > 0.)
        departureLimits.resize(sink + 1);
    for (std::size_t node = 0; node < count; ++node) {
        const double start = input.startCapacity[node], end = input.endCapacity[node];
        const double water = input.startWater[node];
        const double available = start + mixture[node].in, spent = end + mixture[node].out;
        const double mismatch = std::abs(available - spent);
        const double allowed = input.balanceTolerance + kRoundoff * std::max(available, spent);
        if (!std::isfinite(available) || !std::isfinite(spent) ||
            (available == 0.) != (spent == 0.) ||
            !std::isfinite(allowed) || mismatch > allowed)
            return Fail(error, "fixed mixture flux does not balance endpoint capacity");
        result.maximumVolumeResidual = std::max(result.maximumVolumeResidual, mismatch);

        double retained = water;
        double outgoing = preferred[node].out;
        network.Supply(store(node), water);
        network.Supply(sink, -water);
        if (!relay(node)) {
            const double air = start - water;
            const double upper = std::min(water, mixture[node].out);
            const double requested = std::max(0., mixture[node].out - air);
            double donorAllowance = allowed;
            if (!departureLimits.empty()) {
                const double massInAirVolumeUnits = input.localOrdinaryDensityRatio * water + air;
                if (!std::isfinite(massInAirVolumeUnits))
                    return Fail(error, "ordinary phase inventory mass is not representable");
                const auto bound = MeasureGridPhaseInventoryRoundoff(start, mismatch, massInAirVolumeUnits);
                departureLimits[departure(node)] = {true, bound};
                donorAllowance = bound.water + bound.air;
            }
            if (requested - upper > donorAllowance)
                return Fail(error, "fixed mixture flux exhausts an ordinary donor's starting capacity");
            const double lower = std::min(requested, upper);
            outgoing = std::clamp(outgoing, lower, upper);
            retained = water - outgoing;
            network.Add(store(node), departure(node), lower, upper, outgoing);
            network.Add(store(node), arrival(node), 0., water, retained);
        } else {
            retained -= outgoing;
        }
        const double preferredEnd = std::clamp(retained + preferred[node].in, 0., end);
        network.Add(arrival(node), sink, 0., end, preferredEnd);
    }

    std::vector<BoundedArc> faceArcs;
    faceArcs.reserve(input.edges.size());
    for (std::size_t face = 0; face < input.edges.size(); ++face) {
        const auto& edge = input.edges[face];
        const auto [lower, upper] = faceBounds[face];
        faceArcs.push_back(network.Add(departure(edge.from), arrival(edge.to), lower, upper,
                                       result.waterFlux[face]));
    }
    if (!network.Close(input.balanceTolerance, departureLimits))
        return Fail(error, "geometric phase bounds cannot satisfy the water and air inventory ledger");

    std::vector<Traffic> repaired(count), repairedAir(count);
    for (std::size_t face = 0; face < input.edges.size(); ++face) {
        const auto& edge = input.edges[face];
        const double water = network.Flow(faceArcs[face]);
        const double air = edge.volume - water;
        result.waterFlux[face] = water;
        repaired[edge.from].out += water;
        repaired[edge.to].in += water;
        repairedAir[edge.from].out += air;
        repairedAir[edge.to].in += air;
    }
    result.endWater.resize(count);
    double globalBudget = 0.;
    for (std::size_t node = 0; node < count; ++node) {
        const double initial = input.startWater[node], end = input.endCapacity[node];
        const auto [received, sent] = repaired[node];
        const auto [airReceived, airSent] = repairedAir[node];
        const double amount = relay(node) ? (initial + received) - sent : (initial - sent) + received;
        const double airInitial = input.startCapacity[node] - initial;
        if (!departureLimits.empty() && !relay(node)) {
            const auto& bound = departureLimits[departure(node)].bound;
            if ((initial == 0. && sent > 0.) || (airInitial == 0. && airSent > 0.) ||
                sent - initial > bound.water || airSent - airInitial > bound.air)
                return Fail(error, "repaired phase flux spends an ordinary donor before receipt");
        }
        const double airAmount = relay(node) ? (airInitial + airReceived) - airSent
                                             : (airInitial - airSent) + airReceived;
        const double endpoint = std::clamp(amount, 0., end);
        const double endCorrection = std::abs(endpoint - amount);
        double residual = endCorrection;
        if (!relay(node))
            residual = std::max({residual, sent - initial, airSent - airInitial});
        const double allowed = input.balanceTolerance + kRoundoff *
            std::max(input.startCapacity[node] + mixture[node].in, end + mixture[node].out);
        if (!std::isfinite(amount) || !std::isfinite(airAmount) || !std::isfinite(residual) ||
            residual > allowed || airAmount < -allowed || airAmount - end > allowed ||
            std::abs(airAmount - (end - endpoint)) > allowed + endCorrection)
            return Fail(error, "repaired phase transfer fails the independent local inventory check");
        result.endWater[node] = endpoint;
        result.maximumWaterResidual = std::max(result.maximumWaterResidual, residual);
        result.endpointWaterCorrection += endCorrection;
        result.conservationResidual += endpoint - initial;
        globalBudget += allowed;
    }
    if (!std::isfinite(result.conservationResidual) || !std::isfinite(result.endpointWaterCorrection) ||
        !std::isfinite(globalBudget) || std::abs(result.conservationResidual) > globalBudget)
        return Fail(error, "repaired phase transfer fails the global water inventory check");
    output = std::move(result);
    error.clear();
    return true;
}

struct UnownedRoute {
    std::size_t face;
    bool water, air;
};

std::vector<UnownedRoute> FindUnownedRoutes(const GridPhaseFluxRepairInput& input,
                                          const GridPhaseFluxRepairResult& solved) {
    std::vector<UnownedRoute> routes;
    if (input.relayAllowed.empty())
        return routes;
    const auto wet = ReachPhase(input, true, [&](std::size_t f) { return solved.waterFlux[f] > 0.; });
    const auto air = ReachPhase(input, false, [&](std::size_t f) {
        return solved.waterFlux[f] < input.edges[f].volume;
    });
    for (std::size_t face = 0; face < input.edges.size(); ++face) {
        const auto& edge = input.edges[face];
        const double water = solved.waterFlux[face];
        const bool unownedWater = water > 0. && !wet[edge.from];
        const bool unownedAir = water < edge.volume && !air[edge.from];
        if (unownedWater || unownedAir)
            routes.push_back({face, unownedWater, unownedAir});
    }
    return routes;
}

} // namespace

bool RepairGridPhaseFlux(const GridPhaseFluxRepairInput& input,
                         GridPhaseFluxRepairResult& output, std::string& error) {
    GridPhaseFluxRepairResult next;
    if (!SolveGridPhaseFlux(input, next, error))
        return false;
    const auto unowned = FindUnownedRoutes(input, next);
    if (!unowned.empty()) {
        if (!input.repairUnrootedRelays)
            return Fail(error, "repaired phase circulation has an unowned material route");
        auto supported = input;
        for (const auto& route : unowned) {
            auto& edge = supported.edges[route.face];
            // Find another partition on the same physical flow. The
            // complementary phase must occupy the port within its original
            // geometric bounds and pass the full circulation checks again.
            if (route.water)
                edge.maxWater = 0.;
            if (route.air)
                edge.minWater = edge.volume;
            if (edge.minWater > edge.maxWater)
                return Fail(error, "repaired phase circulation has an unowned material route");
        }
        if (!SolveGridPhaseFlux(supported, next, error))
            return false;
        if (!FindUnownedRoutes(supported, next).empty())
            return Fail(error, "repaired phase circulation has an unowned material route");
    }
    output = std::move(next);
    error.clear();
    return true;
}

} // namespace judas::volume_grid
