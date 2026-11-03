#include "CutRegionPhase.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace {

using namespace judas::volume_grid;

struct Checks {
    unsigned total = 0, failures = 0;

    void Expect(bool passed, const char* label) {
        ++total;
        failures += !passed;
        std::cout << (passed ? "PASS " : "FAIL ") << label << '\n';
    }
};

bool Near(double first, double second) {
    return std::abs(first - second) < 1e-15;
}

bool IndependentClosure(const CutRegionPhaseInput& input, const CutRegionPhaseResult& output) {
    if (output.waterFlux.size() != input.faces.size() ||
        output.airFlux.size() != input.faces.size() ||
        output.endWater.size() != input.startWater.size())
        return false;
    auto water = input.startWater;
    auto capacity = input.startCapacity;
    for (std::size_t face = 0; face < input.faces.size(); ++face) {
        const auto& edge = input.faces[face];
        const double wet = output.waterFlux[face], dry = output.airFlux[face];
        if (!Near(wet + dry, edge.signedVolume) ||
            std::abs(wet) < edge.minWater || std::abs(wet) > edge.maxWater)
            return false;
        water[edge.left] -= wet;
        water[edge.right] += wet;
        capacity[edge.left] -= wet + dry;
        capacity[edge.right] += wet + dry;
    }
    for (std::size_t region = 0; region < water.size(); ++region) {
        if (!Near(water[region], output.endWater[region]) ||
            !Near(capacity[region], input.endCapacity[region]))
            return false;
    }
    return Near(output.conservationResidual, 0.);
}

void TestThinThroughflow(Checks& checks) {
    constexpr double thin = .001, mixture = 13. * thin, water = .004;
    const double air = mixture - water;
    CutRegionPhaseInput input{{mixture, thin, 0., 0.}, {0., thin, water, air}, {water, 0., 0., 0.},
        {{0, 1, mixture, 0., mixture, 0.},
         {2, 1, -water, water, water, water},
         {1, 3, air, 0., 0., 0.}},
        {false, true, false, false}, 1e-17};
    CutRegionPhaseResult output;
    std::string error;
    checks.Expect(TransportCutRegionPhases(input, output, error) && IndependentClosure(input, output),
        "an admitted one-litre cut-region relay forwards 13 times its starting capacity");
    checks.Expect(output.waterFlux == std::vector<double>{water, -water, 0.} &&
                  output.airFlux == std::vector<double>{air, 0., air} &&
                  output.endWater == std::vector<double>{0., 0., water, 0.},
        "signed face transfers use actual upstream water and keep the thin region dry at the endpoint");
    const auto retained = output;
    input.relayAllowed[1] = false;
    checks.Expect(!TransportCutRegionPhases(input, output, error) &&
                  output.waterFlux == retained.waterFlux && output.endWater == retained.endWater,
        "an ordinary donor cannot spend a same-interval receipt");
    input.relayAllowed[1] = true;
    input.faces[0].maxWater = 0.;
    checks.Expect(!TransportCutRegionPhases(input, output, error) &&
                  output.waterFlux == retained.waterFlux && output.airFlux == retained.airFlux,
        "a required wet outlet cannot use an air-only upstream route");
}

void TestSeparatedPhase(Checks& checks) {
    CutRegionPhaseInput input{{1., 0., 0.}, {.5, .25, .25}, {.75, 0., 0.},
        {{0, 1, .25, .25, .25, .25}, {0, 2, .25, 0., 0., .125}}, {}};
    CutRegionPhaseResult output;
    std::string error;
    checks.Expect(TransportCutRegionPhases(input, output, error) && IndependentClosure(input, output) &&
                  output.waterFlux == std::vector<double>{.25, 0.} &&
                  output.airFlux == std::vector<double>{0., .25},
        "independent wet and dry openings transport their own phases from one mixed region");

    const auto retained = output;
    input.faces[0].minWater = 0.;
    input.faces[0].maxWater = 0.;
    checks.Expect(!TransportCutRegionPhases(input, output, error) &&
                  output.waterFlux == retained.waterFlux && output.endWater == retained.endWater,
        "an air-only gap cannot spend more air than a mixed donor contains");
    input.faces[0].signedVolume = std::numeric_limits<double>::infinity();
    checks.Expect(!TransportCutRegionPhases(input, output, error) &&
                  output.airFlux == retained.airFlux && output.endWater == retained.endWater,
        "nonfinite pressure flux rejects transactionally");
}

void TestIsolatedPhase(Checks& checks) {
    CutRegionPhaseInput input{{1., 0., 0., 1.}, {1., 0., 0., 1.}, {1., 0., 0., 0.},
        {{1, 2, .25, .25, .25, .25}, {1, 2, -.25, .25, .25, .25}},
        {false, true, true, false}};
    CutRegionPhaseResult output;
    output.endWater = {17.};
    std::string error;
    checks.Expect(!TransportCutRegionPhases(input, output, error) &&
                  output.endWater == std::vector<double>{17.},
        "a volume-balanced wet loop cannot draw on a disconnected initial phase");
    input.faces = {{1, 2, 0., 1e-20, 1e-20, 1e-20}};
    input.balanceTolerance = 1e-15;
    checks.Expect(!TransportCutRegionPhases(input, output, error) &&
                  output.endWater == std::vector<double>{17.},
        "roundoff tolerance cannot carry wet phase through a zero-volume opening");
}

void TestTraceDonorInventory(Checks& checks) {
    constexpr double capacity = 0x1p-40, air = 0x1p-69, volume = 0x1p-41;
    constexpr double preferred = volume - 1.5 * air;
    CutRegionPhaseInput input{{capacity, 0.}, {capacity - volume, volume}, {capacity - air, 0.},
        {{0, 1, volume, 0., volume, preferred}}, {}, 1e-16};
    CutRegionPhaseResult output;
    std::string error;
    checks.Expect(TransportCutRegionPhases(input, output, error) &&
                  output.waterFlux[0] == volume - air && output.airFlux[0] == air &&
                  IndependentClosure(input, output),
        "a feasible cut repair replaces a preferred 50 percent trace-air overspend");

    const auto retained = output;
    input.faces[0].maxWater = preferred;
    checks.Expect(!TransportCutRegionPhases(input, output, error) &&
                  output.waterFlux == retained.waterFlux && output.airFlux == retained.airFlux &&
                  output.endWater == retained.endWater,
        "an aperture that requires excess trace air rejects below the whole-grid tolerance");

    input.startWater[0] = air;
    input.faces[0] = {0, 1, volume, 1.5 * air, volume, 1.5 * air};
    checks.Expect(!TransportCutRegionPhases(input, output, error) &&
                  output.waterFlux == retained.waterFlux && output.airFlux == retained.airFlux &&
                  output.endWater == retained.endWater,
        "an aperture that requires excess trace water also rejects transactionally");
}

} // namespace

int main() {
    Checks checks;
    TestThinThroughflow(checks);
    TestSeparatedPhase(checks);
    TestIsolatedPhase(checks);
    TestTraceDonorInventory(checks);
    std::cout << "SUMMARY cut-region phase " << checks.total << " checks " << checks.failures
              << " failures\n";
    return checks.failures != 0;
}
