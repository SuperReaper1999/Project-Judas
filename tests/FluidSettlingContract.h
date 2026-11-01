#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

// Test-fixture contract only, not engine damping or a gameplay controller.
namespace FluidTest {
struct SettlingWindow {
    std::size_t first, end; // observations at (step+1)*dt; interval is (start,end]
    std::size_t Size() const { return end - first; }
    bool Contains(std::size_t step) const { return step >= first && step < end; }
};
struct SettlingContract {
    double timeConstant, speedLimit = .5, decayRatio = .5, equilibrium, immersionTolerance = .10;
    SettlingWindow early, late;
    static SettlingContract Make(double dragRate, double equilibrium, double dt) {
        if (!(dragRate > 0) || !std::isfinite(dragRate) || !(equilibrium > 0 && equilibrium < 1) ||
            !std::isfinite(dt) || !(dt > 0)) throw std::invalid_argument("invalid settling-contract parameters");
        const double tau = 2 / (dragRate * equilibrium);
        const double total = 6 * tau / dt;
        if (!std::isfinite(total) || total < 6 || total > std::numeric_limits<int>::max())
            throw std::invalid_argument("unrepresentable settling-contract horizon");
        const auto frame = [&](double time) { return std::size_t(std::llround(time / dt)); };
        SettlingContract contract{tau, .5, .5, equilibrium, .10,
                                  {frame(tau), frame(2 * tau)}, {frame(4 * tau), frame(6 * tau)}};
        if (!contract.early.Size() || !contract.late.Size()) throw std::invalid_argument("empty settling window");
        return contract;
    }
};
struct WindowMotion {
    std::size_t count = 0, independentCount = 0;
    double squaredSpeed = 0, peakSpeed = 0, minY = std::numeric_limits<double>::infinity();
    double maxY = -std::numeric_limits<double>::infinity(), immersion = 0, independentImmersion = 0;
    bool valid = true;
    void Add(double y, double v, double fraction, bool hasIndependent = false, double observedFraction = 0) {
        if (!std::isfinite(y) || !std::isfinite(v) || !std::isfinite(fraction) || fraction < 0 || fraction > 1 ||
            (hasIndependent && (!std::isfinite(observedFraction) || observedFraction < 0 || observedFraction > 1))) {
            valid = false;
            return;
        }
        ++count;
        squaredSpeed += v * v;
        peakSpeed = std::max(peakSpeed, std::abs(v));
        minY = std::min(minY, y); maxY = std::max(maxY, y); immersion += fraction;
        if (hasIndependent) { ++independentCount; independentImmersion += observedFraction; }
        valid &= std::isfinite(squaredSpeed) && std::isfinite(maxY - minY) &&
                 std::isfinite(immersion) && std::isfinite(independentImmersion);
    }
    double Rms() const { return count ? std::sqrt(squaredSpeed / count) : 0; }
    double Range() const { return count ? maxY - minY : 0; }
    double MeanImmersion() const { return count ? immersion / count : 0; }
    double MeanIndependent() const { return independentCount ? independentImmersion / independentCount : 0; }
};
struct SettlingAssessment {
    bool coverage, peak, rmsDecay, rangeDecay, immersion, independentImmersion;
    bool Passed() const { return coverage && peak && rmsDecay && rangeDecay && immersion && independentImmersion; }
};
inline SettlingAssessment Assess(const SettlingContract& contract, const WindowMotion& early,
                                 const WindowMotion& late, double gravitySpeedScale) {
    const bool configured = std::isfinite(gravitySpeedScale) && std::isfinite(contract.timeConstant) &&
                            contract.timeConstant > 0 && std::isfinite(contract.speedLimit) && contract.speedLimit > 0 &&
                            std::isfinite(contract.decayRatio) && contract.decayRatio > 0 && contract.decayRatio < 1 &&
                            std::isfinite(contract.equilibrium) && contract.equilibrium > 0 && contract.equilibrium < 1 &&
                            std::isfinite(contract.immersionTolerance) && contract.immersionTolerance >= 0 &&
                            contract.immersionTolerance < 1 && contract.early.end > contract.early.first &&
                            contract.late.end > contract.late.first;
    const bool coverage = configured && early.valid && late.valid && early.count == contract.early.Size() &&
                          late.count == contract.late.Size() && late.independentCount == late.count;
    // These floors account only for binary32 state representation, not a
    // physical noise allowance. They cannot admit persistent visible bobbing.
    const double epsilon = 64 * std::numeric_limits<float>::epsilon();
    const double speedFloor = epsilon * (1 + std::abs(gravitySpeedScale));
    const double rangeFloor = epsilon * (1 + std::max(std::abs(early.minY), std::abs(early.maxY)));
    return {coverage, coverage && late.peakSpeed < contract.speedLimit,
            coverage && late.Rms() <= std::max(speedFloor, contract.decayRatio * early.Rms()),
            coverage && late.Range() <= std::max(rangeFloor, contract.decayRatio * early.Range()),
            coverage && std::abs(late.MeanImmersion() - contract.equilibrium) <= contract.immersionTolerance,
            coverage && std::abs(late.MeanIndependent() - contract.equilibrium) <= contract.immersionTolerance};
}
} // namespace FluidTest
