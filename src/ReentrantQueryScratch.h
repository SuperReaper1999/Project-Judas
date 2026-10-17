#pragma once

#include <memory>
#include <vector>

// Per-thread, per-type derived query storage. Retained frames have stable
// addresses through deeper queries. Exceptional unwind restores the lease
// depth. Unusually deep recursion spills into lease-owned storage rather than
// imposing a new query limit or retaining an unbounded thread-local pool.
template<class Frame>
class ReentrantQueryScratch {
    struct Pool {
        std::vector<std::unique_ptr<Frame>> frames;
        unsigned depth = 0;
    };
    static Pool& Storage() { thread_local Pool pool; return pool; }
    static Frame& Acquire(Pool& pool) {
        if (pool.depth == pool.frames.size())
            pool.frames.emplace_back(std::make_unique<Frame>());
        return *pool.frames[pool.depth++];
    }
    Pool& pool;
    std::unique_ptr<Frame> overflow;
public:
    Frame& frame;
    ReentrantQueryScratch()
        : pool(Storage()), overflow(pool.depth >= 32 ? std::make_unique<Frame>() : nullptr),
          frame(overflow ? *overflow : Acquire(pool)) {}
    ~ReentrantQueryScratch() { if (!overflow) --pool.depth; }
    ReentrantQueryScratch(const ReentrantQueryScratch&) = delete;
    ReentrantQueryScratch& operator=(const ReentrantQueryScratch&) = delete;
};
