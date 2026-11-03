#include "GridPhaseFluxRepair.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>

using namespace judas::volume_grid;

namespace {

struct Checks {
    int total = 0, failed = 0;

    void Expect(bool passed, const char* label) {
        ++total;
        if (!passed) {
            ++failed;
            std::cerr << "FAIL " << label << '\n';
        }
    }
};

void MixedDonor(Checks& checks) {
    const GridPhaseFluxRepairInput input{{1., 0., 0.}, {.25, .375, .375}, {.75, 0., 0.},
        {{0, 1, .375, 0., .375, .125}, {0, 2, .375, 0., .375, .125}}, {}};
    GridPhaseFluxRepairResult result;
    std::string error;
    checks.Expect(RepairGridPhaseFlux(input, result, error), "repair preferred outflow that exhausts air");
    checks.Expect(result.waterFlux.size() == 2 && result.waterFlux[0] + result.waterFlux[1] == .5,
                  "mixed donor spends exactly its minimum admissible water");
    checks.Expect(result.endWater.size() == 3 && result.endWater[0] == .25 &&
                  result.endWater[1] == result.waterFlux[0] && result.endWater[2] == result.waterFlux[1] &&
                  result.maximumVolumeResidual == 0. && result.maximumWaterResidual == 0. &&
                  result.endpointWaterCorrection == 0. && result.conservationResidual == 0.,
                  "water, air and all endpoint stores close exactly");

    auto feasible = input;
    feasible.edges[0].preferredWater = .375;
    feasible.edges[1].preferredWater = .25;
    checks.Expect(RepairGridPhaseFlux(feasible, result, error) &&
                  result.waterFlux == std::vector<double>{.375, .25},
                  "admissible preferred partition is preserved exactly");
    feasible.edges[0].preferredWater = .625;
    checks.Expect(RepairGridPhaseFlux(feasible, result, error) &&
                  result.waterFlux == std::vector<double>{.375, .25},
                  "preferred value clamps to its own face bound before repair");
}

void Relay(Checks& checks) {
    GridPhaseFluxRepairInput input{{1., 0., 0., 0.}, {.5, 0., .25, .25}, {.25, 0., 0., 0.},
        {{0, 1, .5, 0., .5, 0.}, {1, 2, .25, .25, .25, .25}, {1, 3, .25, 0., 0., 0.}},
        {false, true, false, false}};
    GridPhaseFluxRepairResult result;
    std::string error;
    checks.Expect(RepairGridPhaseFlux(input, result, error), "relay's two outgoing phase bounds repair its incoming partition");
    checks.Expect(result.waterFlux == std::vector<double>{.25, .25, 0.} &&
                  result.endWater == std::vector<double>{0., 0., .25, 0.} &&
                  result.conservationResidual == 0.,
                  "exposed relay receives the exact water and air it forwards");
    const auto before = result;
    input.relayAllowed[1] = false;
    checks.Expect(!RepairGridPhaseFlux(input, result, error) && error.find("empty ordinary") != std::string::npos &&
                  result.waterFlux == before.waterFlux && result.endWater == before.endWater,
                  "unmarked relay cannot spend same-step material and failure is transactional");
}

void RejectImpossible(Checks& checks) {
    GridPhaseFluxRepairInput dry{{1., 0.}, {.25, .75}, {.75, 0.},
        {{0, 1, .75, 0., 0., 0.}}, {}};
    GridPhaseFluxRepairResult result;
    result.endWater = {17.};
    std::string error;
    checks.Expect(!RepairGridPhaseFlux(dry, result, error) && error.find("geometric phase bounds") != std::string::npos &&
                  result.endWater == std::vector<double>{17.},
                  "must reject when a dry face cannot carry the displaced wet inventory");
    dry.edges[0] = {0, 1, .5, 0., .5, .5};
    checks.Expect(!RepairGridPhaseFlux(dry, result, error) && error.find("mixture flux") != std::string::npos,
                  "cannot change a fixed mixture transfer to repair a pressure residual");
    dry.edges[0].volume = std::numeric_limits<double>::quiet_NaN();
    checks.Expect(!RepairGridPhaseFlux(dry, result, error) && error.find("invalid geometric") != std::string::npos,
                  "nonfinite bounds reject");
    GridPhaseFluxRepairInput zero{{1., 0.}, {.75, .25}, {0., 0.},
        {{0, 1, .25, 0., .25, 1e-12}}, {}, 1e-8};
    checks.Expect(RepairGridPhaseFlux(zero, result, error) && result.waterFlux == std::vector<double>{0.},
                  "numerical tolerance cannot introduce an absent water phase");
    zero.edges[0].minWater = 1e-12;
    checks.Expect(!RepairGridPhaseFlux(zero, result, error) && error.find("absent donor phase") != std::string::npos,
                  "even a tiny mandatory transfer cannot draw water from a dry donor");

    GridPhaseFluxRepairInput cycle{{1., 0., 0.}, {1., 0., 0.}, {1., 0., 0.},
        {{1, 2, .125, 0., .125, .125}, {2, 1, .125, 0., .125, .125}}, {false, true, true}};
    checks.Expect(!RepairGridPhaseFlux(cycle, result, error) && error.find("unowned material") != std::string::npos,
                  "isolated phase circulation cannot use a remote initial supply");
    cycle = {{1., 0., 0., 0.}, {.75, 0., 0., .25}, {0., 0., 0., 0.},
        {{0, 1, .25, 0., .25, 0.}, {1, 2, .125, 0., .125, .125},
         {2, 1, .125, 0., .125, .125}, {1, 3, .25, 0., .25, 0.}}, {false, true, true, false}};
    checks.Expect(RepairGridPhaseFlux(cycle, result, error) && result.waterFlux == std::vector<double>(4, 0.),
                  "air-rooted circulation repairs an unseeded preferred water cycle");
}

bool IndependentLedger(const GridPhaseFluxRepairInput& input, const std::vector<double>& water) {
    std::vector<double> in(input.startWater.size()), out = in, airOut = in;
    for (std::size_t edge = 0; edge < water.size(); ++edge) {
        const auto& f = input.edges[edge];
        if (water[edge] < f.minWater || water[edge] > f.maxWater)
            return false;
        in[f.to] += water[edge];
        out[f.from] += water[edge];
        airOut[f.from] += f.volume - water[edge];
    }
    for (std::size_t node = 0; node < in.size(); ++node) {
        if (!input.relayAllowed[node] &&
            (out[node] > input.startWater[node] || airOut[node] > input.startCapacity[node] - input.startWater[node]))
            return false;
        const double remaining = input.startWater[node] + in[node] - out[node];
        if (remaining < 0. || remaining > input.endCapacity[node])
            return false;
    }
    return true;
}

bool Exhaustive(const GridPhaseFluxRepairInput& input, std::vector<double>& candidate, std::size_t edge) {
    if (edge == candidate.size())
        return IndependentLedger(input, candidate);
    const auto& f = input.edges[edge];
    for (double water = f.minWater; water <= f.maxWater; ++water) {
        candidate[edge] = water;
        if (Exhaustive(input, candidate, edge + 1))
            return true;
    }
    return false;
}

void SmallGraphOracle(Checks& checks) {
    std::mt19937 random(165826);
    bool match = true;
    int feasible = 0, impossible = 0;
    for (int sample = 0; sample < 384 && match; ++sample) {
        GridPhaseFluxRepairInput input;
        input.startCapacity.resize(5);
        input.endCapacity.resize(5);
        input.startWater.resize(5);
        input.relayAllowed = {false, false, true, true, false};
        std::vector<double> in(5), out(5);
        for (const auto pair : {std::pair{0U, 2U}, {0U, 3U}, {1U, 2U},
                                {2U, 3U}, {2U, 4U}, {3U, 4U}}) {
            const auto [from, to] = pair;
            const auto volume = random() % 4;
            const auto lower = random() % (1 + volume);
            const auto upper = lower + random() % (1 + volume - lower);
            const auto preferred = random() % (1 + volume);
            input.edges.push_back({from, to, static_cast<double>(volume), static_cast<double>(lower),
                                   static_cast<double>(upper), static_cast<double>(preferred)});
            in[to] += static_cast<double>(volume);
            out[from] += static_cast<double>(volume);
        }
        for (std::size_t node = 0; node < in.size(); ++node) {
            input.startCapacity[node] = (input.relayAllowed[node] ? std::max(0., out[node] - in[node])
                : out[node]) + static_cast<double>(random() % 3);
            input.endCapacity[node] = input.startCapacity[node] + in[node] - out[node];
            input.startWater[node] = static_cast<double>(random() %
                (1 + static_cast<unsigned long>(input.startCapacity[node])));
        }
        std::vector<double> candidate(input.edges.size());
        const bool expected = Exhaustive(input, candidate, 0);
        expected ? ++feasible : ++impossible;
        GridPhaseFluxRepairResult result;
        std::string error;
        const bool actual = RepairGridPhaseFlux(input, result, error);
        match = actual == expected && (!actual || (IndependentLedger(input, result.waterFlux) &&
            result.maximumVolumeResidual == 0. && result.maximumWaterResidual == 0. &&
            result.conservationResidual == 0.));
        if (!match)
            std::cerr << "Oracle disagreement on sample " << sample << " expected " << expected
                      << " actual " << actual << " error " << error << '\n';
    }
    checks.Expect(match && feasible > 20 && impossible > 20,
                  "384 partition feasibility cases agree with independent exhaustive oracle");
}

void LongRelay(Checks& checks) {
    constexpr std::size_t count = 4096;
    GridPhaseFluxRepairInput input;
    input.startCapacity.resize(count);
    input.startWater.resize(count);
    input.endCapacity.resize(count);
    input.relayAllowed.resize(count, true);
    input.startCapacity.front() = input.startWater.front() = input.endCapacity.back() = 1.;
    input.relayAllowed.front() = input.relayAllowed.back() = false;
    for (std::size_t node = 1; node < count; ++node)
        input.edges.push_back({node - 1, node, 1., 0., 1., 0.});
    GridPhaseFluxRepairResult result;
    std::string error;
    checks.Expect(RepairGridPhaseFlux(input, result, error) && result.endWater == input.endCapacity &&
                  std::all_of(result.waterFlux.begin(), result.waterFlux.end(), [](double v) { return v == 1.; }),
                  "iterative blocking flow carries the whole phase through 4096 nodes");
}

} // namespace

int main() {
    Checks checks;
    MixedDonor(checks);
    Relay(checks);
    RejectImpossible(checks);
    SmallGraphOracle(checks);
    LongRelay(checks);
    std::cout << "SUMMARY phase flux repair " << checks.total << " checks " << checks.failed << " failures\n";
    return checks.failed ? 1 : 0;
}
