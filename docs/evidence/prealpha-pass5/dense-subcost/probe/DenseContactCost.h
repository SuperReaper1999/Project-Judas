#pragma once
// Cache-only attribution. Timers/counts never supply a collision decision.
#include <array>
#include <chrono>
#include <cstdint>
namespace DenseContactCost {
using Clock = std::chrono::steady_clock;
enum Stage { FullPair, Context, Sat, Witness, StageCount };
struct Bucket {
    std::array<std::uint64_t, StageCount> nanoseconds{}, calls{};
    std::uint64_t queries=0, aabbRejected=0, hits=0, misses=0, uncertain=0;
    std::uint64_t axesAttempted=0, axesEvaluated=0, matrixRows=0;
    std::uint64_t faceWitness=0, edgeWitness=0, clipCalls=0, fallbackCorners=0;
    std::uint64_t predicates=0, exactFallbacks=0, numericFallbacks=0;
    std::uint64_t canonicalHits=0, reversedHits=0;
    std::uint64_t equalPose=0, equalPoseOtherMode=0;
};
inline thread_local Bucket* active = nullptr;
struct UseBucket {
    Bucket* previous;
    explicit UseBucket(Bucket& b): previous(active) { active=&b; }
    ~UseBucket() { active=previous; }
};
struct Timer {
    Bucket* bucket;
    Stage stage;
    Clock::time_point begin;
    explicit Timer(Stage s): bucket(active), stage(s) {
        if(bucket) { ++bucket->calls[s]; begin=Clock::now(); }
    }
    ~Timer() {
        if(bucket) bucket->nanoseconds[stage] += std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-begin).count();
    }
};
inline void Count(std::uint64_t Bucket::*field,std::uint64_t n=1) {
    if(active) active->*field += n;
}
}
