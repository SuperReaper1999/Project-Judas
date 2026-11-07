#include "WaterBulkReconcile.h"
#include <algorithm>
#include <cmath>
#include <numeric>
namespace judas::volume_grid {
namespace {
using Real = CutCompensated;
struct Components {
    std::vector<std::size_t> parent;
    explicit Components(std::size_t n) : parent(n) {
        std::iota(parent.begin(), parent.end(), 0);
    }
    std::size_t Root(std::size_t i) {
        while (parent[i] != i) {
            parent[i] = parent[parent[i]];
            i = parent[i];
        }
        return i;
    }
    bool Link(const std::vector<WaterGasLink> &links) {
        for (auto e : links) {
            if (e.left >= parent.size() || e.right >= parent.size() ||
                !(e.massConductance > 0. && std::isfinite(e.massConductance)))
                return false;
            auto a = Root(e.left), b = Root(e.right);
            if (a != b)
                parent[std::max(a, b)] = std::min(a, b);
        }
        return true;
    }
};
bool Fail(std::string &error, const char *text) {
    error = text;
    return false;
}
} // namespace
bool ReconcileWaterBulk(const WaterBulkGeometry &geometry, const WaterBulkState &source,
                        const std::vector<WaterGasLink> &waterLinks,
                        const std::vector<WaterGasLink> &gasLinks,
                        const std::vector<WaterBulkMassReceipt> &receipts,
                        const WaterGasSettings &settings, WaterBulkState &state,
                        WaterBulkReconcileStats &stats, std::string &error) {
    stats = WaterBulkReconcileStats{};
    const auto n = geometry.capacity.size();
    WaterBulkAudit audit;
    if (!AuditWaterBulkStorage(geometry, source, settings, audit, error) ||
        !AuditWaterBulkStorage(geometry, state, settings, audit, error))
        return false;
    Components water(n), air(n);
    if (!water.Link(waterLinks) || !air.Link(gasLinks))
        return Fail(error, "invalid reconciliation connectivity");
    auto next = state;
    WaterBulkReconcileStats work;
    for (bool gas : {false, true}) {
        auto &components = gas ? air : water;
        std::vector<Real> target(n), actual(n);
        std::vector<std::vector<std::size_t>> members(n);
        for (std::size_t i = 0; i < n; ++i) {
            auto root = components.Root(i);
            members[root].push_back(i);
            target[root] = target[root] + (gas ? source.gasMass[i] : source.water[i]);
            actual[root] = actual[root] + (gas ? next.gasMass[i] : next.water[i]);
        }
        if (gas)
            for (auto receipt : receipts) {
                if (receipt.cell >= n || !std::isfinite(receipt.outward.Value()))
                    return Fail(error, "invalid reconciliation receipt");
                auto root = components.Root(receipt.cell);
                target[root] = target[root] - receipt.outward;
            }
        for (std::size_t root = 0; root < n; ++root)
            if (!members[root].empty()) {
                Real delta = target[root] - actual[root];
                if (delta.Value() == 0.)
                    continue;
                double budget = std::min(
                    settings.massTolerance / (gas ? 1. : settings.referenceDensity),
                    settings.relativeTolerance * std::max(target[root].Value(), actual[root].Value()));
                if (std::abs(delta.Value()) > budget) {
                    stats.needsRefinement = true;
                    return Fail(error, "unconverged material ledger cannot be reconciled");
                }
                std::size_t chosen = SIZE_MAX;
                double best = -1.;
                for (auto i : members[root]) {
                    Real quantity = (gas ? next.gasMass[i] : next.water[i]) + delta;
                    if (quantity.Value() < 0.)
                        continue;
                    if (gas) {
                        if (next.gas[i].Value() == 0. && quantity.Value() > 0.)
                            continue;
                    } else {
                        Real dry = next.gas[i] - delta;
                        if (dry.Value() < 0. || (dry.Value() == 0. && next.gasMass[i].Value() > 0.))
                            continue;
                    }
                    double weight = gas ? next.gasMass[i].Value() : geometry.capacity[i];
                    if (weight > best) {
                        chosen = i;
                        best = weight;
                    }
                }
                if (chosen == SIZE_MAX)
                    return Fail(error, "material reconciliation has no positive recipient");
                if (gas) {
                    next.gasMass[chosen] = next.gasMass[chosen] + delta;
                    work.maximumMassCorrection =
                        std::max(work.maximumMassCorrection, std::abs(delta.Value()));
                } else {
                    next.water[chosen] = next.water[chosen] + delta;
                    next.gas[chosen] = next.gas[chosen] - delta;
                    work.maximumWaterCorrection =
                        std::max(work.maximumWaterCorrection, std::abs(delta.Value()));
                }
            }
    }
    if (!AuditWaterBulkStorage(geometry, next, settings, audit, error))
        return false;
    state = std::move(next);
    stats = work;
    error.clear();
    return true;
}
} // namespace judas::volume_grid
