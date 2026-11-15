#pragma once
#include <chrono>
#include <cstdint>
// Owner-local diagnostic exclusion. Physical dt and raw profiler/GPU clocks
// never change. Only CPU work timers omit synchronous inspection/wait time.
struct PhysicsDebugClock {
    using base=std::chrono::steady_clock;
    using time_point=base::time_point;
    inline static thread_local std::chrono::nanoseconds excluded{};
    static time_point now(){return base::now()-excluded;}
    static std::uint64_t Excluded(){return std::uint64_t(excluded.count());}
    static void Exclude(base::duration value){excluded+=std::chrono::duration_cast<std::chrono::nanoseconds>(value);}
};
