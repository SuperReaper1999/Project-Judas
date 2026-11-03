#include "CutRegionMomentum.h"

#include <cmath>
#include <iostream>
#include <limits>

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

bool Close(double a, double b) {
    return std::abs(a - b) < 1e-10;
}

bool Close(const GridVec& a, const GridVec& b) {
    for (std::size_t axis = 0; axis < 3; ++axis)
        if (!Close(a[axis], b[axis]))
            return false;
    return true;
}

void WaterInAir(Checks& checks) {
    CutRegionPhaseInput input{{1., 1.}, {.75, 1.25}, {1., 0.},
        {{1, 0, -.25, .25, .25, .25}}, {}};
    CutRegionPhaseResult phases;
    CutRegionMomentumResult result;
    std::string error;
    const std::vector<GridVec> initial{{1., -2., 0.}, {0., 3., 1.}};
    const std::vector<GridVec> impulse{{100., 0., 0.}, {}};
    const bool accepted = TransportCutRegionPhases(input, phases, error) &&
        TransportCutRegionMomentum(input, phases, initial, impulse, result, error);
    checks.Expect(accepted, "reverse wet face carries its signed physical water mass");
    checks.Expect(accepted && Close(result.velocity[0], {1.1, -2., 0.}) &&
                  Close(result.velocity[1], {275. / 251., -497. / 251., 1. / 251.}),
        "receiver's independently weighted speed uses 250 kg water and 1 kg air");
    const double mixing = .5 * 250. / 251. * (1.1 * 1.1 + 25. + 1.);
    checks.Expect(accepted && Close(result.mixingLoss, mixing) && Close(result.impulseWork, 105.) &&
                  Close(result.impulseWorkResidual, 0.) && Close(result.kineticResidual, 0.),
        "pressure impulse work and physical mixing loss close independently");
    checks.Expect(accepted && Close(result.momentumResidual, {}) && Close(result.massResidual, 0.),
        "new momentum equals initial momentum plus impulse");
}

void ThinRelay(Checks& checks) {
    constexpr double thin = .001, mixture = 13. * thin, wet = .004;
    const double air = mixture - wet;
    CutRegionPhaseInput input{{mixture, thin, 0., 0.}, {0., thin, wet, air}, {wet, 0., 0., 0.},
        {{0, 1, mixture, 0., mixture, 0.},
         {2, 1, -wet, wet, wet, wet},
         {1, 3, air, 0., 0., 0.}},
        {false, true, false, false}, 1e-17};
    CutRegionPhaseResult phases;
    CutRegionMomentumResult result;
    std::string error;
    const std::vector<GridVec> initial{{2., 0., 0.}, {-1., 0., 0.}, {}, {}};
    std::vector<GridVec> impulse(4);
    const bool accepted = TransportCutRegionPhases(input, phases, error) &&
        TransportCutRegionMomentum(input, phases, initial, impulse, result, error);
    const double incoming = 1000. * wet + air;
    const double speed = (2. * incoming - thin) / (incoming + thin);
    checks.Expect(accepted && result.velocity[0] == GridVec{} &&
                  Close(result.velocity[1], {speed, 0., 0.}) &&
                  Close(result.velocity[2], {speed, 0., 0.}) &&
                  Close(result.velocity[3], {speed, 0., 0.}),
        "a sourced thin-region relay transports both phases with its mass-weighted mixed speed");
    checks.Expect(accepted && Close(result.massResidual, 0.) && Close(result.momentumResidual, {}) &&
                  Close(result.mixingLoss, .5 * (incoming / (incoming + thin)) * thin * 9.) &&
                  Close(result.kineticResidual, 0.),
        "throughflow closes the independent mass, momentum and kinetic ledgers");

    const auto retained = result;
    impulse[2][0] = 1e-20;
    checks.Expect(!TransportCutRegionMomentum(input, phases, initial, impulse, result, error) &&
                  result.velocity == retained.velocity && result.mixingLoss == retained.mixingLoss,
        "even a tiny pressure impulse on an initially empty region rejects transactionally");
    impulse[2][0] = 0.;
    phases.waterFlux[1] = wet;
    checks.Expect(!TransportCutRegionMomentum(input, phases, initial, impulse, result, error) &&
                  result.velocity == retained.velocity,
        "momentum cannot accept a phase flux opposing its shared pressure face");
    phases.waterFlux[1] = -wet;
    impulse[0][0] = std::numeric_limits<double>::infinity();
    checks.Expect(!TransportCutRegionMomentum(input, phases, initial, impulse, result, error) &&
                  result.velocity == retained.velocity,
        "nonfinite pressure impulse rejects transactionally");
}

void TerminalImpulse(Checks& checks) {
    CutRegionPhaseInput input{{.002, 0., 0.}, {.001, .001, 0.}, {.002, 0., 0.},
        {{0, 1, .001, .001, .001, .001}}, {}};
    CutRegionPhaseResult phases;
    CutRegionMomentumResult result;
    std::string error;
    const std::vector<GridVec> velocity{{2., 0., 0.}, {}, {}};
    const std::vector<GridVec> impulse{{1., 0., 0.}, {}, {}};
    std::vector<GridVec> endImpulse{{}, {3., 0., 0.}, {}};
    const bool accepted = TransportCutRegionPhases(input, phases, error) &&
        TransportCutRegionMomentum(input, phases, velocity, impulse, endImpulse, result, error);
    checks.Expect(accepted && Close(result.velocity[0], {2.5, 0., 0.}) &&
                  Close(result.velocity[1], {5.5, 0., 0.}),
        "terminal pressure kick applies to actual arrived water after transport");
    checks.Expect(accepted && Close(result.beforeKineticEnergy, 4.) &&
                  Close(result.afterImpulseKineticEnergy, 6.25) &&
                  Close(result.afterTransportKineticEnergy, 6.25) &&
                  Close(result.afterEndImpulseKineticEnergy, 18.25) &&
                  Close(result.impulseWork, 2.25) && Close(result.endImpulseWork, 12.) &&
                  Close(result.impulseWorkResidual, 0.) && Close(result.endImpulseWorkResidual, 0.) &&
                  Close(result.kineticResidual, 0.) && Close(result.momentumResidual, {}),
        "initial and terminal impulses have independent work and combined momentum ledgers");
    const auto retained = result;
    endImpulse[2][0] = 1e-20;
    checks.Expect(!TransportCutRegionMomentum(input, phases, velocity, impulse, endImpulse, result, error) &&
                  result.velocity == retained.velocity && result.endImpulseWork == retained.endImpulseWork,
        "terminal impulse without ending material rejects transactionally");
}

void ExhaustedOrdinaryDonor(Checks& checks) {
    const double sent = std::nextafter(1., 2.);
    CutRegionPhaseInput input{{1., 0.}, {0., sent}, {1., 0.},
        {{0, 1, sent, sent, sent, sent}}, {}, 1e-13};
    CutRegionPhaseResult phases;
    CutRegionMomentumResult result;
    std::string error;
    const std::vector<GridVec> velocity{{2., 0., 0.}, {}};
    const std::vector<GridVec> impulse(2);
    const bool accepted = TransportCutRegionPhases(input, phases, error) &&
        TransportCutRegionMomentum(input, phases, velocity, impulse, result, error);
    checks.Expect(accepted && phases.waterFlux[0] == sent && result.velocity[0] == GridVec{} &&
                  Close(result.velocity[1], {2., 0., 0.}) && result.mixingLoss == 0. &&
                  Close(result.kineticResidual, 0.) && Close(result.momentumResidual, {}),
        "arithmetic excess on an exhausted donor preserves the shared flux and velocity");

    const auto retained = result;
    phases.waterFlux[0] = sent - 1e-15;
    phases.airFlux[0] = sent - phases.waterFlux[0];
    input.faces[0].minWater = 0.;
    checks.Expect(!TransportCutRegionMomentum(input, phases, velocity, impulse, result, error) &&
                  result.velocity == retained.velocity &&
                  error.find("ordinary donor before receipt") != std::string::npos,
        "an absent air donor cannot send even a roundoff-sized flux");

    constexpr double small = 6.98e-11, excess = 3.9e-14;
    input = {{small, 0.}, {0., small + excess}, {0., 0.},
        {{0, 1, small + excess, 0., 0., 0.}}, {}, 1e-13};
    checks.Expect(TransportCutRegionPhases(input, phases, error) &&
                  !TransportCutRegionMomentum(input, phases, velocity, impulse, result, error) &&
                  result.velocity == retained.velocity &&
                  error.find("ordinary donor before receipt") != std::string::npos,
        "a local physical deficit still fails within the absolute phase-solve tolerance");

    constexpr double capacity = 1.5625000000000014e-5;
    constexpr double startWater = 1.5624999999998831e-5, water = 1.5624999999998845e-5;
    constexpr double mixture = water + 3.3118392667602105e-18;
    input = {{capacity, 0.}, {0., mixture}, {startWater, 0.},
        {{0, 1, mixture, water, water, water}}, {}, 1.11e-16};
    checks.Expect(TransportCutRegionPhases(input, phases, error) &&
                  TransportCutRegionMomentum(input, phases, velocity, impulse, result, error) &&
                  phases.airFlux[0] > capacity - startWater &&
                  result.velocity[0] == GridVec{} && Close(result.velocity[1], {2., 0., 0.}) &&
                  result.mixingLoss == 0. && Close(result.momentumResidual, {}),
        "trace air roundoff in an exhausted mixed region preserves phase flux and carrier speed");
}

} // namespace

int main() {
    Checks checks;
    WaterInAir(checks);
    ThinRelay(checks);
    TerminalImpulse(checks);
    ExhaustedOrdinaryDonor(checks);
    std::cout << "SUMMARY cut-region momentum " << checks.total << " checks " << checks.failures
              << " failures\n";
    return checks.failures != 0;
}
