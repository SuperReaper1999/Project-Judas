#include "GridConservativeRemap.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

using namespace judas::volume_grid;

namespace {

bool Identical(double a, double b) {
    return std::memcmp(&a, &b, sizeof(double)) == 0;
}

bool SameField(const RemapAxisMomentumResult& a, const RemapAxisMomentumResult& b) {
    return a.field.end == b.field.end && a.field.transfers == b.field.transfers &&
        Identical(a.field.maximumLocalResidual, b.field.maximumLocalResidual) &&
        Identical(a.field.conservationResidual, b.field.conservationResidual) &&
        Identical(a.beforeKineticEnergy, b.beforeKineticEnergy) &&
        Identical(a.afterKineticEnergy, b.afterKineticEnergy) &&
        Identical(a.mixingLoss, b.mixingLoss) && Identical(a.kineticResidual, b.kineticResidual);
}

struct Checks {
    unsigned total = 0, failed = 0;

    void Expect(bool pass, const char* label) {
        ++total;
        if (!pass) {
            ++failed;
            std::cerr << "FAIL " << label << '\n';
        }
    }
};

bool Compare(const ConservativeRemap& plan, const std::vector<double>& active,
             std::size_t axis) {
    std::vector<std::array<double, 3>> input(active.size());
    for (std::size_t node = 0; node < active.size(); ++node)
        input[node][axis] = active[node];
    RemapAxisMomentumResult actual;
    RemapMomentumResult expected;
    std::string scalarError, vectorError;
    if (!plan.RemapMomentum(input, expected, vectorError) ||
        !plan.RemapAxisMomentum(active, actual, scalarError) ||
        !scalarError.empty() || !vectorError.empty() ||
        actual.field.end.size() != expected.end.size() ||
        actual.field.transfers.size() != expected.transfers.size())
        return false;
    for (std::size_t node = 0; node < expected.end.size(); ++node)
        if (!Identical(actual.field.end[node], expected.end[node][axis]))
            return false;
    for (std::size_t face = 0; face < expected.transfers.size(); ++face)
        if (!Identical(actual.field.transfers[face], expected.transfers[face][axis]))
            return false;
    return Identical(actual.field.conservationResidual, expected.conservationResidual[axis]) &&
        Identical(actual.beforeKineticEnergy, expected.beforeKineticEnergy) &&
        Identical(actual.afterKineticEnergy, expected.afterKineticEnergy) &&
        Identical(actual.mixingLoss, expected.mixingLoss) &&
        Identical(actual.kineticResidual, expected.kineticResidual);
}

void RootedCycle(Checks& checks) {
    const ConservativeRemapInput input{{1, 1, 0, 0, 0, 1}, {0, 0, 0, 0, 2, 1},
        {{0, 2, 1}, {1, 3, 1}, {2, 3, 3}, {3, 2, 2}, {4, 3, -2}},
        {false, false, true, true, false, false}};
    ConservativeRemap plan;
    std::string error;
    const bool built = BuildConservativeRemap(input, plan, error);
    checks.Expect(built, "analytic relay cycle builds");
    if (!built) return;
    const std::vector<double> initial{2, -1, 0, 0, 0, 0};
    RemapAxisMomentumResult actual;
    checks.Expect(plan.RemapAxisMomentum(initial, actual, error) &&
        std::abs(actual.field.end[4] - 1.) < 1e-12 &&
        std::abs(actual.beforeKineticEnergy - 2.5) < 1e-12 &&
        std::abs(actual.afterKineticEnergy - .25) < 1e-12 &&
        std::abs(actual.mixingLoss - 2.25) < 1e-12,
        "independent signed momentum and mixing work in a rooted cycle");
    for (std::size_t axis = 0; axis < 3; ++axis)
        checks.Expect(Compare(plan, initial, axis), "axis specialization preserves cycle bits");

    const auto retained = actual;
    for (const auto& invalid : std::vector<std::vector<double>>{
            {1}, {2, -1, 1, 0, 0, 0},
            {std::numeric_limits<double>::quiet_NaN(), -1, 0, 0, 0, 0},
            {std::numeric_limits<double>::infinity(), -1, 0, 0, 0, 0},
            {1e308, -1e308, 0, 0, 0, 0}}) {
        for (std::size_t axis = 0; axis < 3; ++axis) {
            std::vector<std::array<double, 3>> vectorInput(invalid.size());
            for (std::size_t i = 0; i < invalid.size(); ++i)
                vectorInput[i][axis] = invalid[i];
            RemapMomentumResult vectorOutput;
            std::string scalarError, vectorError;
            checks.Expect(!plan.RemapMomentum(vectorInput, vectorOutput, vectorError) &&
                !plan.RemapAxisMomentum(invalid, actual, scalarError) &&
                !scalarError.empty() && scalarError == vectorError && SameField(actual, retained),
                "size, unowned, nonfinite or overflow failure preserves scalar transaction");
        }
    }
}

void Seeded(Checks& checks) {
    constexpr std::size_t count = 24;
    std::mt19937 random(275112);
    for (int sample = 0; sample < 128; ++sample) {
        ConservativeRemapInput input;
        input.start.resize(count);
        input.end.resize(count);
        input.relayAllowed.resize(count, true);
        std::array<int, count> balance{};
        for (int edge = 0; edge < 72; ++edge) {
            const auto from = random() % count, to = random() % count;
            if (from == to)
                continue;
            const int amount = static_cast<int>(random() % 7) - 3;
            input.transfers.push_back({from, to, static_cast<double>(amount)});
            balance[from] -= amount;
            balance[to] += amount;
        }
        std::vector<double> active(count);
        for (std::size_t node = 0; node < count; ++node) {
            input.start[node] = 1 + std::max(0, -balance[node]);
            input.end[node] = input.start[node] + balance[node];
            active[node] = sample % 5 == 0 ? 0. :
                input.start[node] * (static_cast<int>(random() % 11) - 5);
        }
        ConservativeRemap plan;
        std::string error;
        const bool built = BuildConservativeRemap(input, plan, error);
        checks.Expect(built, "deterministic signed plan builds");
        for (std::size_t axis = 0; axis < 3; ++axis)
            checks.Expect(built && Compare(plan, active, axis), "seeded field, transfers and work are bit-exact");
    }
}

} // namespace

int main() {
    Checks checks;
    RootedCycle(checks);
    Seeded(checks);
    std::cout << checks.total - checks.failed << '/' << checks.total << " axis momentum controls passed\n";
    return checks.failed ? 1 : 0;
}
