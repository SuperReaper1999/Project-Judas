#include "FluidSettlingContract.h"
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool value, const char* label) {
    ++checks;
    if (!value) { ++failures; std::cout << "FAIL " << label << '\n'; }
}
constexpr double dt = 1. / 60., pi = 3.14159265358979323846;
const auto contract = FluidTest::SettlingContract::Make(2, .5, dt);
struct Observations { FluidTest::WindowMotion early, late; };
template<class Motion> Observations Sample(Motion motion) {
    Observations observations;
    for (std::size_t step = 0; step < contract.late.end; ++step) {
        const auto state = motion((step + 1) * dt);
        const double fraction = .5 - (state.first - 1.8) / .6;
        if (contract.early.Contains(step)) observations.early.Add(state.first, state.second, fraction);
        if (contract.late.Contains(step)) observations.late.Add(state.first, state.second, fraction, true, fraction);
    }
    return observations;
}
FluidTest::SettlingAssessment Verdict(const Observations& observations) {
    return FluidTest::Assess(contract, observations.early, observations.late, std::sqrt(9.81 * .3));
}
Observations Signals(double earlyRange, double lateRange, double earlySpeed, double lateSpeed) {
    Observations observations;
    for (std::size_t i = 0; i < contract.early.Size(); ++i)
        observations.early.Add(1.8 + (i % 2 ? earlyRange : -earlyRange), i % 2 ? earlySpeed : -earlySpeed, .5);
    for (std::size_t i = 0; i < contract.late.Size(); ++i)
        observations.late.Add(1.8 + (i % 2 ? lateRange : -lateRange), i % 2 ? lateSpeed : -lateSpeed, .5, true, .5);
    return observations;
}
} // namespace
int main() {
    Check(contract.timeConstant == 2, "configured amplitude time constant");
    Check(contract.early.first == 120 && contract.early.end == 240, "early (2,4] second window");
    Check(contract.late.first == 480 && contract.late.end == 720, "late (8,12] second window");
    Check(!contract.early.Contains(119) && contract.early.Contains(120) && contract.early.Contains(239) &&
          !contract.early.Contains(240), "early boundaries use actual post-step observations");
    Check(!contract.late.Contains(479) && contract.late.Contains(480) && contract.late.Contains(719) &&
          !contract.late.Contains(720), "late boundaries exclude observations outside horizon");
    const auto fine = FluidTest::SettlingContract::Make(2, .5, 1. / 120.);
    Check(fine.early.first == 240 && fine.late.end == 1440, "observation count follows dt without changing physical horizon");
    const auto slower = FluidTest::SettlingContract::Make(1, .5, dt);
    Check(slower.timeConstant == 4 && slower.late.end == 1440, "observation horizon follows configured drag");
    Check(Verdict(Sample([](double) { return std::pair<double,double>{1.8, 0}; })).Passed(), "already stationary equilibrium passes");
    auto decaying = Sample([](double t) {
        const double amplitude = .08 * std::exp(-.25 * t);
        return std::pair<double,double>{1.8 + amplitude * std::cos(2*pi*t),
                                       amplitude * (-.25*std::cos(2*pi*t) - 2*pi*std::sin(2*pi*t))};
    });
    Check(Verdict(decaying).Passed(), "bounded independently specified decaying oscillation passes");
    const auto persistent = Sample([](double t) {
        return std::pair<double,double>{1.8 + .02*std::cos(2*pi*t), -.02*2*pi*std::sin(2*pi*t)};
    });
    const auto constant = Verdict(persistent);
    Check(constant.peak, "persistent small bob remains below old speed bound");
    Check(!constant.rmsDecay && !constant.rangeDecay && !constant.Passed(), "nondecaying low-speed bob is rejected");
    const double oldEndpoint = -.02*2*pi*std::sin(2*pi*4);
    Check(std::abs(oldEndpoint) < .5, "old endpoint would accept the nondecaying counterexample");
    auto spike = Signals(.1, .01, .2, .02);
    spike.late.peakSpeed = .6;
    Check(!Verdict(spike).peak && !Verdict(spike).Passed(), "one late speed spike cannot be hidden by low RMS or quiet endpoint");
    spike.late.peakSpeed = .5;
    Check(!Verdict(spike).peak, "original strict 0.5 speed boundary is retained");
    const auto rmsOnly = Verdict(Signals(.1, .01, .2, .15));
    Check(!rmsOnly.rmsDecay && rmsOnly.rangeDecay && rmsOnly.peak, "RMS decay is independently required");
    const auto rangeOnly = Verdict(Signals(.1, .08, .2, .02));
    Check(rangeOnly.rmsDecay && !rangeOnly.rangeDecay && rangeOnly.peak, "travel-range decay is independently required");
    auto missing = decaying; --missing.late.count;
    Check(!Verdict(missing).coverage, "missing late sample rejects qualification");
    missing = decaying; --missing.early.count;
    Check(!Verdict(missing).coverage, "missing early sample rejects qualification");
    missing = decaying; --missing.late.independentCount;
    Check(!Verdict(missing).coverage, "missing independent surface cannot silently reduce denominator");
    missing = decaying; ++missing.late.count;
    Check(!Verdict(missing).coverage, "extra observations cannot shift the declared window");
    auto immersion = decaying; immersion.late.immersion = .75 * immersion.late.count;
    Check(!Verdict(immersion).immersion && Verdict(immersion).independentImmersion, "sampled equilibrium is independently required");
    immersion = decaying; immersion.late.independentImmersion = .75 * immersion.late.independentCount;
    Check(Verdict(immersion).immersion && !Verdict(immersion).independentImmersion, "independent equilibrium is independently required");
    for (double invalid : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        auto bad = decaying; bad.late.Add(1.8, invalid, .5, true, .5);
        Check(!Verdict(bad).Passed(), "nonfinite speed cannot qualify");
        bad = decaying; bad.late.Add(invalid, 0, .5, true, .5);
        Check(!Verdict(bad).Passed(), "nonfinite position cannot qualify");
        bad = decaying; bad.late.Add(1.8, 0, invalid, true, .5);
        Check(!Verdict(bad).Passed(), "nonfinite immersion cannot qualify");
        bad = decaying; bad.late.Add(1.8, 0, .5, true, invalid);
        Check(!Verdict(bad).Passed(), "nonfinite independent immersion cannot qualify");
    }
    FluidTest::WindowMotion overflow;
    overflow.Add(-1e308, 0, .5); overflow.Add(1e308, 0, .5);
    Check(!overflow.valid, "overflowed finite-position range is rejected");
    overflow = {}; overflow.Add(1.8, 1e308, .5);
    Check(!overflow.valid, "overflowed speed square is rejected");
    for (double fraction : {-.1, 1.1}) {
        auto bad = decaying; bad.late.Add(1.8, 0, fraction, true, .5);
        Check(!Verdict(bad).Passed(), "out-of-range immersion cannot qualify");
    }
    Check(!FluidTest::Assess(contract, decaying.early, decaying.late, std::numeric_limits<double>::quiet_NaN()).Passed(),
          "invalid numerical speed scale is rejected");
    struct Input { double rate, fraction, dt; };
    for (const auto& input : std::vector<Input>{{0,.5,dt},{-1,.5,dt},{2,0,dt},{2,1,dt},{2,.5,0},
             {2,.5,-dt},{2,.5,1e-100},{2,.5,10},{std::numeric_limits<double>::infinity(),.5,dt},
             {2,std::numeric_limits<double>::quiet_NaN(),dt},{2,.5,std::numeric_limits<double>::quiet_NaN()}}) {
        bool rejected = false;
        try { (void)FluidTest::SettlingContract::Make(input.rate,input.fraction,input.dt); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected, "invalid or unrepresentable contract configuration is rejected");
    }
    std::cout << "Fluid settling contract: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
