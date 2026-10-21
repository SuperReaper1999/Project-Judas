#pragma once
// Private causal control only; not an engine API or production implementation.
#include "Broadphase.h"
#include <functional>
#include <vector>
namespace incremental_cache_control {
using Query = std::function<const std::vector<unsigned>&(unsigned, const Aabb&)>;
inline thread_local const Query* query = nullptr;
struct Scope {
    const Query* previous;
    explicit Scope(const Query& value) : previous(query) { query = &value; }
    ~Scope() { query = previous; }
};
} // namespace incremental_cache_control
