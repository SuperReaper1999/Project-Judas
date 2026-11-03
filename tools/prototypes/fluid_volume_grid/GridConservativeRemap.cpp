#include "GridConservativeRemap.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace judas::volume_grid {
namespace {

constexpr double kRoundoff = 128 * std::numeric_limits<double>::epsilon();
constexpr std::size_t kDenseRelayLimit = 64, kSparsePassLimit = 128;

bool FiniteNonnegative(double value) {
    return std::isfinite(value) && value >= 0;
}

bool Fail(std::string& error, const char* message) {
    error = message;
    return false;
}

bool Factor(std::vector<double>& matrix, std::vector<std::size_t>& pivots, std::size_t size) {
    pivots.resize(size);
    for (std::size_t column = 0; column < size; ++column) {
        std::size_t pivot = column;
        for (std::size_t row = column + 1; row < size; ++row) {
            if (std::abs(matrix[row * size + column]) > std::abs(matrix[pivot * size + column]))
                pivot = row;
        }
        if (std::abs(matrix[pivot * size + column]) <= kRoundoff)
            return false;
        pivots[column] = pivot;
        for (std::size_t entry = 0; entry < size; ++entry)
            std::swap(matrix[column * size + entry], matrix[pivot * size + entry]);
        for (std::size_t row = column + 1; row < size; ++row) {
            auto& multiplier = matrix[row * size + column];
            multiplier /= matrix[column * size + column];
            for (std::size_t entry = column + 1; entry < size; ++entry)
                matrix[row * size + entry] -= multiplier * matrix[column * size + entry];
        }
    }
    return true;
}

void Solve(const std::vector<double>& factors, const std::vector<std::size_t>& pivots,
           std::vector<double>& values) {
    const std::size_t size = values.size();
    for (std::size_t row = 0; row < size; ++row)
        std::swap(values[row], values[pivots[row]]);
    for (std::size_t row = 0; row < size; ++row) {
        for (std::size_t column = 0; column < row; ++column)
            values[row] -= factors[row * size + column] * values[column];
    }
    for (std::size_t row = size; row-- > 0;) {
        for (std::size_t column = row + 1; column < size; ++column)
            values[row] -= factors[row * size + column] * values[column];
        values[row] /= factors[row * size + row];
    }
}

} // namespace

bool ConservativeRemap::Block::CertifySparse() {
    // With nonnegative W, W^k * 1 bounds the contribution of any
    // unresolved concentration. Round outward to certify the bound.
    constexpr double infinity = std::numeric_limits<double>::infinity();
    std::vector<double> survival(nodes.size(), 1.), next(nodes.size());
    for (std::size_t pass = 1; pass <= kSparsePassLimit; ++pass) {
        for (std::size_t row = 0; row < nodes.size(); ++row) {
            double sum = 0.;
            for (auto entry = rowOffsets[row]; entry < rowOffsets[row + 1]; ++entry) {
                const auto& weight = weights[entry];
                const double term = std::nextafter(weight.share * survival[weight.donor], infinity);
                sum = std::nextafter(sum + term, infinity);
            }
            next[row] = sum;
        }
        if (*std::max_element(next.begin(), next.end()) <= kRoundoff / 16.) {
            sparsePasses = pass;
            return true;
        }
        survival.swap(next);
    }
    return false;
}

bool ConservativeRemap::Block::SolveSparse(std::vector<double>& values, double scale) const {
    const auto rhs = values;
    std::vector<double> next(nodes.size());
    const auto step = [&](std::size_t row) {
        double sum = rhs[row];
        for (auto entry = rowOffsets[row]; entry < rowOffsets[row + 1]; ++entry) {
            const auto& weight = weights[entry];
            sum += weight.share * values[weight.donor];
        }
        return sum;
    };
    for (std::size_t pass = 0; pass < sparsePasses; ++pass) {
        for (std::size_t row = 0; row < nodes.size(); ++row)
            next[row] = step(row);
        values.swap(next);
    }
    for (std::size_t row = 0; row < nodes.size(); ++row) {
        if (!std::isfinite(values[row]) || !(std::abs(values[row] - step(row)) <= kRoundoff * scale))
            return false;
    }
    return true;
}

bool BuildConservativeRemap(const ConservativeRemapInput& input, ConservativeRemap& output,
                            std::string& error) {
    const auto count = input.start.size();
    if (input.end.size() != count || input.relayAllowed.size() != count ||
        !FiniteNonnegative(input.balanceTolerance))
        return Fail(error, "conservative remap input dimensions or tolerance are invalid");

    ConservativeRemap plan;
    plan.start_ = input.start;
    plan.end_ = input.end;
    plan.tolerance_ = input.balanceTolerance;
    plan.incoming_.resize(count);
    plan.outgoing_.resize(count);
    plan.arrivals_.resize(count);
    plan.departures_.resize(count);
    plan.relay_.resize(count);
    plan.links_.reserve(input.transfers.size());
    for (const auto& transfer : input.transfers) {
        if (transfer.from >= count || transfer.to >= count || transfer.from == transfer.to ||
            !std::isfinite(transfer.amount))
            return Fail(error, "conservative remap transfer is invalid");
        const bool reversed = transfer.amount < 0;
        const auto donor = reversed ? transfer.to : transfer.from;
        const auto receiver = reversed ? transfer.from : transfer.to;
        const double mass = std::abs(transfer.amount);
        const auto index = plan.links_.size();
        plan.links_.push_back({donor, receiver, mass, reversed});
        if (mass == 0)
            continue;
        plan.arrivals_[receiver].push_back(index);
        plan.departures_[donor].push_back(index);
        plan.incoming_[receiver] += mass;
        plan.outgoing_[donor] += mass;
    }
    for (std::size_t node = 0; node < count; ++node) {
        const double start = plan.start_[node], end = plan.end_[node];
        const double received = start + plan.incoming_[node], spent = end + plan.outgoing_[node];
        if (!FiniteNonnegative(start) || !FiniteNonnegative(end) ||
            !std::isfinite(received) || !std::isfinite(spent) ||
            (received == 0) != (spent == 0) ||
            std::abs(received - spent) > plan.tolerance_ + kRoundoff * std::max(received, spent))
            return Fail(error, "conservative remap does not balance local carrier");
        if (plan.outgoing_[node] > start) {
            if (input.relayAllowed[node]) {
                plan.relay_[node] = true;
            } else if (!input.allowDonorRoundoff || start == 0 ||
                       plan.outgoing_[node] - start > kRoundoff * plan.outgoing_[node]) {
                return Fail(error, "conservative remap spends a non-relay donor before receipt");
            }
        }
    }

    // Kosaraju in iterative form keeps a long exposure chain off the C++ stack.
    // Ordinary nodes are known source concentrations, so they cut dependencies.
    std::vector<bool> visited(count);
    std::vector<std::size_t> order;
    std::vector<std::pair<std::size_t, std::size_t>> stack;
    for (std::size_t node = 0; node < count; ++node) {
        if (!plan.relay_[node] || visited[node])
            continue;
        visited[node] = true;
        stack.emplace_back(node, 0);
        while (!stack.empty()) {
            auto& [source, next] = stack.back();
            if (next == plan.departures_[source].size()) {
                order.push_back(source);
                stack.pop_back();
                continue;
            }
            const auto target = plan.links_[plan.departures_[source][next++]].receiver;
            if (plan.relay_[target] && !visited[target]) {
                visited[target] = true;
                stack.emplace_back(target, 0);
            }
        }
    }
    std::vector<std::size_t> component(count, count), local(count);
    for (auto it = order.rbegin(); it != order.rend(); ++it) {
        if (component[*it] != count)
            continue;
        const auto id = plan.blocks_.size();
        auto& block = plan.blocks_.emplace_back();
        block.nodes.push_back(*it);
        component[*it] = id;
        for (std::size_t j = 0; j < block.nodes.size(); ++j) {
            const auto node = block.nodes[j];
            local[node] = j;
            for (const auto edge : plan.arrivals_[node]) {
                const auto donor = plan.links_[edge].donor;
                if (plan.relay_[donor] && component[donor] == count) {
                    component[donor] = id;
                    block.nodes.push_back(donor);
                }
            }
        }
        const auto size = block.nodes.size();
        if (size == 1)
            continue;
        const bool sparse = size > kDenseRelayLimit;
        bool hasSource = false;
        if (sparse)
            block.rowOffsets.resize(size + 1);
        else
            block.factors.resize(size * size);
        for (std::size_t row = 0; row < size; ++row) {
            const auto node = block.nodes[row];
            const double received = plan.start_[node] + plan.incoming_[node];
            if (sparse)
                block.rowOffsets[row] = block.weights.size();
            else
                block.factors[row * size + row] = 1;
            hasSource = hasSource || plan.start_[node] > 0;
            for (const auto edge : plan.arrivals_[node]) {
                const auto& link = plan.links_[edge];
                if (component[link.donor] != id) {
                    hasSource = true;
                } else if (sparse) {
                    block.weights.push_back({local[link.donor], link.mass / received});
                } else {
                    block.factors[row * size + local[link.donor]] -= link.mass / received;
                }
            }
        }
        if (!hasSource)
            return Fail(error, "conservative remap cycle has no initial material source");
        if (sparse) {
            block.rowOffsets.back() = block.weights.size();
            if (!block.CertifySparse())
                return Fail(error, "conservative remap cyclic relay has unresolved sparse mixing weights");
        } else if (!Factor(block.factors, block.pivots, size)) {
            return Fail(error, "conservative remap cyclic relay has unresolved mixing weights");
        }
    }

    output = std::move(plan);
    error.clear();
    return true;
}

bool ConservativeRemap::Apply(const std::vector<double>& start, RemapScalarResult& output,
                              MixingLedger* ledger, std::string& error) const {
    const auto count = start_.size();
    if (start.size() != count)
        return Fail(error, "conservative remap field dimensions are invalid");
    std::vector<double> initial(count);
    double minimum = std::numeric_limits<double>::infinity();
    double maximum = -minimum;
    for (std::size_t node = 0; node < count; ++node) {
        if (!std::isfinite(start[node]) || (start_[node] == 0 && start[node] != 0))
            return Fail(error, "conservative remap field has invalid or unowned material");
        if (start_[node] == 0)
            continue;
        initial[node] = start[node] / start_[node];
        if (!std::isfinite(initial[node]))
            return Fail(error, "conservative remap initial concentration is not representable");
        minimum = std::min(minimum, initial[node]);
        maximum = std::max(maximum, initial[node]);
    }
    if (!std::isfinite(minimum))
        minimum = maximum = 0;
    const double concentrationScale = std::max(std::abs(minimum), std::abs(maximum));
    const double concentrationTolerance = kRoundoff * concentrationScale;
    const auto bounded = [&](double value) {
        return std::isfinite(value) && !(minimum - value > concentrationTolerance ||
                                         value - maximum > concentrationTolerance);
    };

    std::vector<double> mixed = initial;
    std::vector<std::size_t> solved(count, 0);
    std::vector<double> values;
    for (std::size_t id = 0; id < blocks_.size(); ++id) {
        const auto& block = blocks_[id];
        values.assign(block.nodes.size(), 0);
        for (const auto node : block.nodes)
            solved[node] = id + 1;
        for (std::size_t row = 0; row < block.nodes.size(); ++row) {
            const auto node = block.nodes[row];
            const double received = start_[node] + incoming_[node];
            values[row] = start[node] / received;
            for (const auto edge : arrivals_[node]) {
                const auto& link = links_[edge];
                if (solved[link.donor] != id + 1)
                    values[row] += (link.mass / received) * mixed[link.donor];
            }
        }
        if (!block.factors.empty())
            Solve(block.factors, block.pivots, values);
        if (block.sparsePasses && !block.SolveSparse(values, concentrationScale))
            return Fail(error, "conservative remap sparse relay residual did not close");
        for (std::size_t row = 0; row < block.nodes.size(); ++row) {
            if (!bounded(values[row]))
                return Fail(error, "conservative remap relay produced an unbounded concentration");
            mixed[block.nodes[row]] = values[row];
        }
    }

    RemapScalarResult result;
    result.end.resize(count);
    result.transfers.resize(links_.size());
    for (std::size_t edge = 0; edge < links_.size(); ++edge) {
        const auto& link = links_[edge];
        const double amount = link.mass * mixed[link.donor];
        if (!std::isfinite(amount))
            return Fail(error, "conservative remap transfer field is not representable");
        result.transfers[edge] = link.reversed ? -amount : amount;
    }
    MixingLedger energy;
    double extensiveScale = 0;
    for (std::size_t node = 0; node < count; ++node) {
        const double retained = relay_[node] ? 0 : std::max(0., start_[node] - outgoing_[node]);
        double inbound = 0;
        for (const auto edge : arrivals_[node]) {
            const auto& link = links_[edge];
            inbound += link.reversed ? -result.transfers[edge] : result.transfers[edge];
        }
        const double retainedField = outgoing_[node] == 0 ? start[node] : retained * initial[node];
        const double remaining = relay_[node] ? end_[node] * mixed[node] : retainedField + inbound;
        const double final = end_[node] == 0 ? 0 : remaining;
        const double expected = start[node] + inbound - outgoing_[node] * mixed[node];
        const double local = final - expected;
        const double massScale = std::max(start_[node] + incoming_[node], end_[node] + outgoing_[node]);
        const double allowed = (tolerance_ + kRoundoff * massScale) * concentrationScale;
        if (!std::isfinite(final) || !std::isfinite(local) || !std::isfinite(allowed) ||
            std::abs(local) > allowed ||
            minimum * end_[node] - final > allowed || final - maximum * end_[node] > allowed)
            return Fail(error, "conservative remap field violates local conservation or donor bounds");
        result.end[node] = final;
        result.maximumLocalResidual = std::max(result.maximumLocalResidual, std::abs(local));
        result.conservationResidual += final - start[node];
        extensiveScale += allowed;

        if (!ledger)
            continue;
        const double finalSpeed = end_[node] == 0 ? 0 : final / end_[node];
        const double poolSpeed = relay_[node] ? mixed[node] : finalSpeed;
        const double poolInitial = relay_[node] ? start_[node] : retained;
        const double difference = initial[node] - poolSpeed;
        energy.before += .5 * start[node] * initial[node];
        energy.after += .5 * final * finalSpeed;
        energy.loss += .5 * (poolInitial * difference) * difference;
        for (const auto edge : arrivals_[node]) {
            const auto& link = links_[edge];
            const double delta = mixed[link.donor] - poolSpeed;
            energy.loss += .5 * (link.mass * delta) * delta;
        }
    }
    if (!std::isfinite(result.conservationResidual) || !std::isfinite(extensiveScale) ||
        std::abs(result.conservationResidual) > extensiveScale)
        return Fail(error, "conservative remap field does not conserve the global total");
    if (ledger) {
        if (!FiniteNonnegative(energy.before) || !FiniteNonnegative(energy.after) ||
            !FiniteNonnegative(energy.loss))
            return Fail(error, "conservative remap kinetic energy is not representable");
        const double scale = std::max(energy.before, energy.after);
        if (std::abs((energy.before - energy.after) - energy.loss) >
            kRoundoff * scale + tolerance_ * static_cast<double>(count) * concentrationScale * concentrationScale)
            return Fail(error, "conservative remap kinetic work does not match mixing loss");
        *ledger = energy;
    }
    output = std::move(result);
    error.clear();
    return true;
}

bool ConservativeRemap::RemapScalar(const std::vector<double>& start, RemapScalarResult& output,
                                    std::string& error) const {
    return Apply(start, output, nullptr, error);
}

bool ConservativeRemap::RemapAxisMomentum(const std::vector<double>& start,
                                          RemapAxisMomentumResult& output,
                                          std::string& error) const {
    RemapAxisMomentumResult result;
    MixingLedger energy;
    if (!Apply(start, result.field, &energy, error))
        return false;
    result.beforeKineticEnergy = energy.before;
    result.afterKineticEnergy = energy.after;
    result.mixingLoss = energy.loss;
    result.kineticResidual = (energy.before - energy.after) - energy.loss;
    if (!std::isfinite(result.kineticResidual))
        return Fail(error, "conservative remap axis kinetic energy is not representable");
    output = std::move(result);
    error.clear();
    return true;
}

bool ConservativeRemap::RemapMomentum(const std::vector<std::array<double, 3>>& start,
                                      RemapMomentumResult& output, std::string& error) const {
    RemapMomentumResult result;
    result.end.resize(start.size());
    result.transfers.resize(links_.size());
    std::vector<double> component(start.size());
    for (std::size_t axis = 0; axis < 3; ++axis) {
        for (std::size_t node = 0; node < start.size(); ++node)
            component[node] = start[node][axis];
        RemapScalarResult field;
        MixingLedger energy;
        if (!Apply(component, field, &energy, error))
            return false;
        for (std::size_t node = 0; node < field.end.size(); ++node)
            result.end[node][axis] = field.end[node];
        for (std::size_t edge = 0; edge < field.transfers.size(); ++edge)
            result.transfers[edge][axis] = field.transfers[edge];
        result.conservationResidual[axis] = field.conservationResidual;
        result.beforeKineticEnergy += energy.before;
        result.afterKineticEnergy += energy.after;
        result.mixingLoss += energy.loss;
    }
    result.kineticResidual = (result.beforeKineticEnergy - result.afterKineticEnergy) - result.mixingLoss;
    if (!std::isfinite(result.beforeKineticEnergy) || !std::isfinite(result.afterKineticEnergy) ||
        !std::isfinite(result.mixingLoss) || !std::isfinite(result.kineticResidual))
        return Fail(error, "conservative remap vector kinetic energy is not representable");
    output = std::move(result);
    error.clear();
    return true;
}

} // namespace judas::volume_grid
