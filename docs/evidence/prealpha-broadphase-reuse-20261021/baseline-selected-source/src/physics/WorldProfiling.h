#pragma once

#include <chrono>
#include <cstdint>

namespace JudasPhysicsDetail {
using Clock = std::chrono::steady_clock;
inline double MillisecondsBetween(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

// Per-step aggregate diagnostics keep CCD searches bounded in M56: recording a
// scope for every predicate would overflow a frame's event lane. Nested work in
// the same category is counted once; categories are inclusive, not additive.
struct PhysicsWorkSample {
    std::uint64_t nanoseconds = 0, calls = 0;
    unsigned depth = 0;
};
class PhysicsWorkTimer {
  public:
    PhysicsWorkTimer(PhysicsWorkSample &value, bool enabled) : sample(enabled ? &value : nullptr) {
        if (sample) {
            outer = sample->depth++ == 0;
            ++sample->calls;
            if (outer)
                start = Clock::now();
        }
    }
    ~PhysicsWorkTimer() {
        if (sample && --sample->depth == 0 && outer)
            sample->nanoseconds += std::uint64_t(
                std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count());
    }
    PhysicsWorkTimer(const PhysicsWorkTimer &) = delete;

  private:
    PhysicsWorkSample *sample;
    bool outer = false;
    Clock::time_point start{};
};

}
