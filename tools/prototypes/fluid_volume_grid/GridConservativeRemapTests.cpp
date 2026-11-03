#include "GridConservativeRemap.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>

using namespace judas::volume_grid;

namespace {

struct Checks {
    int count = 0, failures = 0;

    void Expect(bool passed, const std::string& label) {
        ++count;
        if (!passed) {
            ++failures;
            std::cerr << "FAIL " << label << '\n';
        }
    }
};

bool Close(double actual, double expected) {
    return std::abs(actual - expected) < 1e-12;
}

bool Close(const std::vector<double>& actual, const std::vector<double>& expected) {
    return actual.size() == expected.size() &&
        std::equal(actual.begin(), actual.end(), expected.begin(),
                   [](double a, double b) { return Close(a, b); });
}

void OrdinaryDonor(Checks& checks) {
    ConservativeRemap plan;
    std::string error;
    const ConservativeRemapInput input{{1, 1, 0}, {.5, 1., .5},
        {{1, 0, -.5}, {1, 2, .5}, {2, 0, 0}}, {false, false, false}};
    checks.Expect(BuildConservativeRemap(input, plan, error), "ordinary explicit plan " + error);
    RemapScalarResult phase;
    checks.Expect(plan.RemapScalar({1, 0, 0}, phase, error), "ordinary wet tracer " + error);
    checks.Expect(Close(phase.end, {.5, .5, 0}) && Close(phase.transfers, {-.5, 0, 0}),
        "normal donor retains local wet material and sends no fresh wet inflow across another face");
    checks.Expect(phase.maximumLocalResidual == 0 && phase.conservationResidual == 0,
        "ordinary local and global material ledgers");
    RemapScalarResult air;
    checks.Expect(plan.RemapScalar({0, 1, 0}, air, error), "ordinary air tracer " + error);
    checks.Expect(Close(air.end, {0, .5, .5}) && Close(air.transfers, {0, .5, 0}),
        "signed dry transfers complement wet transfers");
}

void ExposureChain(Checks& checks) {
    ConservativeRemap plan;
    std::string error;
    ConservativeRemapInput input{{1, 0, 0, 1}, {0, 0, 1, 1},
        {{0, 1, 1}, {1, 2, 1}}, {false, true, false, false}};
    checks.Expect(BuildConservativeRemap(input, plan, error), "supported zero-storage relay " + error);
    RemapScalarResult field;
    checks.Expect(plan.RemapScalar({.25, 0, 0, 1}, field, error), "exposure field " + error);
    checks.Expect(Close(field.end, {0, 0, .25, 1}) && Close(field.transfers, {.25, .25}),
        "relay forwards only its real inflow and stays empty at the endpoint");
    RemapMomentumResult momentum;
    checks.Expect(plan.RemapMomentum({{2, -3, .25}, {}, {}, {0, 1, 2}}, momentum, error),
        "exposure momentum " + error);
    checks.Expect(momentum.end.size() == 4 && momentum.end[2] == std::array<double, 3>{2, -3, .25} &&
        momentum.end[1] == std::array<double, 3>{},
        "single-source relay inherits signed vector momentum without storing or inventing any");
    checks.Expect(momentum.beforeKineticEnergy == momentum.afterKineticEnergy &&
        momentum.mixingLoss == 0 && momentum.kineticResidual == 0 &&
        momentum.conservationResidual == std::array<double, 3>{},
        "unmixed zero-storage relay preserves kinetic energy");
    const auto retained = field;
    checks.Expect(!plan.RemapScalar({.25, 1, 0, 1}, field, error) &&
        error.find("unowned") != std::string::npos && retained.end == field.end,
        "empty source cannot supply stale material and failure preserves result");

    input.relayAllowed[1] = false;
    checks.Expect(!BuildConservativeRemap(input, plan, error) &&
        error.find("before receipt") != std::string::npos,
        "raw balance cannot authorize an unmarked donor to spend a later receipt");
    checks.Expect(plan.RemapScalar({.25, 0, 0, 1}, field, error) && retained.end == field.end,
        "failed remap construction preserves the usable plan");
}

void CyclicMixing(Checks& checks) {
    // A feeds C, B feeds D. Two units circle through C→D→C, and two
    // leave D. With tracer only at A, 3x_C=1+2x_D and 4x_D=3x_C.
    const ConservativeRemapInput input{{1, 1, 0, 0, 0, 1}, {0, 0, 0, 0, 2, 1},
        {{0, 2, 1}, {1, 3, 1}, {2, 3, 3}, {3, 2, 2}, {4, 3, -2}},
        {false, false, true, true, false, false}};
    ConservativeRemap plan;
    std::string error;
    checks.Expect(BuildConservativeRemap(input, plan, error), "material-rooted cycle " + error);
    RemapScalarResult field;
    checks.Expect(plan.RemapScalar({1, 0, 0, 0, 0, 1}, field, error), "seeded wet tracer " + error);
    checks.Expect(Close(field.end, {0, 0, 0, 0, 1, 1}) &&
        Close(field.transfers, {1, 0, 2, 1, -1}) &&
        Close(field.conservationResidual, 0) && Close(field.maximumLocalResidual, 0),
        "cycle uses analytic mixing fractions and preserves disjoint ownership");
    RemapScalarResult dry;
    checks.Expect(plan.RemapScalar({0, 0, 0, 0, 0, 1}, dry, error) &&
        dry.end == std::vector<double>{0, 0, 0, 0, 0, 1} &&
        dry.transfers == std::vector<double>{0, 0, 0, 0, 0},
        "remote wet inventory cannot seed a locally dry cycle");
    RemapMomentumResult momentum;
    checks.Expect(plan.RemapMomentum({{2, -1, 3}, {-1, 2, 0}, {}, {}, {}, {}}, momentum, error),
        "seeded mixed momentum " + error);
    checks.Expect(momentum.end.size() == 6 && Close(momentum.end[4][0], 1) &&
        Close(momentum.end[4][1], 1) && Close(momentum.end[4][2], 3) &&
        std::all_of(momentum.conservationResidual.begin(), momentum.conservationResidual.end(),
                    [](double value) { return Close(value, 0); }),
        "cycle transports all three components of momentum");
    checks.Expect(Close(momentum.beforeKineticEnergy, 9.5) &&
        Close(momentum.afterKineticEnergy, 2.75) && Close(momentum.mixingLoss, 6.75) &&
        Close(momentum.kineticResidual, 0), "independent kinetic difference equals weighted mixing loss");

    ConservativeRemap reversed;
    auto permuted = input;
    std::reverse(permuted.transfers.begin(), permuted.transfers.end());
    RemapScalarResult comparison;
    checks.Expect(BuildConservativeRemap(permuted, reversed, error) &&
        reversed.RemapScalar({1, 0, 0, 0, 0, 1}, comparison, error) &&
        Close(comparison.end, field.end) &&
        Close(comparison.transfers, {field.transfers.rbegin(), field.transfers.rend()}),
        "reordering signed transfers preserves local and end mixing");
}

void RejectInvalid(Checks& checks) {
    ConservativeRemap plan;
    std::string error;
    ConservativeRemapInput input{{1, 0, 0, 0}, {0, 1, 0, 0},
        {{0, 1, 1}, {2, 3, 1}, {3, 2, 1}}, {false, false, true, true}};
    checks.Expect(!BuildConservativeRemap(input, plan, error) &&
        error.find("no initial material source") != std::string::npos,
        "disconnected circulation cannot manufacture carrier despite zero divergence");
    input.transfers = {{0, 1, .9}};
    checks.Expect(!BuildConservativeRemap(input, plan, error) && error.find("local carrier") != std::string::npos,
        "local carrier defect is rejected even though total storage is unchanged");
    input = {{0, 1}, {.25, .75}, {}, {true, false}, .5};
    checks.Expect(!BuildConservativeRemap(input, plan, error) && error.find("local carrier") != std::string::npos,
        "a balance tolerance cannot give an isolated empty node new material");
    input.transfers = {{0, 1, std::numeric_limits<double>::infinity()}};
    checks.Expect(!BuildConservativeRemap(input, plan, error) && error.find("transfer") != std::string::npos,
        "nonfinite flux is rejected");
    input = {{1, 0, 0, 0}, {0, 0, 0, 1}, {{0, 1, 1}, {1, 2, 1e16}, {2, 1, 1e16}, {2, 3, 1}},
        {false, true, true, false}};
    checks.Expect(!BuildConservativeRemap(input, plan, error) &&
        error.find("unresolved mixing weights") != std::string::npos,
        "cycle rejects a material root lost in its unrepresentable circulation");
}

void LongExposure(Checks& checks) {
    constexpr std::size_t count = 4096;
    ConservativeRemapInput input;
    input.start.resize(count);
    input.end.resize(count);
    input.start.front() = input.end.back() = 1;
    input.relayAllowed.resize(count, true);
    for (std::size_t i = 1; i < count; ++i)
        input.transfers.push_back({i - 1, i, 1});
    ConservativeRemap plan;
    RemapScalarResult field;
    std::string error;
    checks.Expect(BuildConservativeRemap(input, plan, error) &&
        plan.RemapScalar(input.start, field, error) &&
        field.end == input.end && field.conservationResidual == 0,
        "long acyclic exposure uses iterative graph traversal with exact transport");
}

void SeededGraphs(Checks& checks) {
    constexpr std::size_t count = 24;
    std::mt19937 random(275112);
    bool allPassed = true;
    std::string error;
    for (int sample = 0; sample < 128 && allPassed; ++sample) {
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
        std::vector<std::array<double, 3>> momentum(count);
        for (std::size_t node = 0; node < count; ++node) {
            input.start[node] = 1 + std::max(0, -balance[node]);
            input.end[node] = input.start[node] + balance[node];
            for (auto& value : momentum[node])
                value = input.start[node] * (static_cast<int>(random() % 11) - 5);
        }
        ConservativeRemap plan;
        RemapScalarResult carrier;
        RemapMomentumResult transported;
        allPassed = BuildConservativeRemap(input, plan, error) &&
            plan.RemapScalar(input.start, carrier, error) && Close(carrier.end, input.end) &&
            plan.RemapMomentum(momentum, transported, error) &&
            transported.mixingLoss >= 0 && std::abs(transported.kineticResidual) < 1e-10 &&
            std::all_of(transported.conservationResidual.begin(), transported.conservationResidual.end(),
                        [](double value) { return std::abs(value) < 1e-10; });
    }
    checks.Expect(allPassed, "128 seeded signed graphs conserve carrier, momentum and kinetic mixing " + error);
}

} // namespace

int main() {
    Checks checks;
    OrdinaryDonor(checks);
    ExposureChain(checks);
    CyclicMixing(checks);
    RejectInvalid(checks);
    LongExposure(checks);
    SeededGraphs(checks);
    std::cout << checks.count - checks.failures << '/' << checks.count << " checks passed\n";
    return checks.failures ? 1 : 0;
}
