#pragma once

#include <algorithm>
#include <array>
#include <vector>

namespace JudasPhysicsDetail {

using ImpactSupportKey = std::array<unsigned, 4>;

// Both lists are sorted, unique primitive-pair sets. Preserve per-pair
// publication order, but do not insert a duplicate and scan the entire list
// to remove it. Most classifications in a persistent island are unchanged.
inline void SetImpactSupport(std::vector<ImpactSupportKey> &support,
                             std::vector<ImpactSupportKey> &spinning,
                             const ImpactSupportKey &key, bool spin) {
    auto &from = spin ? support : spinning;
    auto &into = spin ? spinning : support;
    const auto removed = std::lower_bound(from.begin(), from.end(), key);
    if (removed != from.end() && *removed == key)
        from.erase(removed);
    const auto added = std::lower_bound(into.begin(), into.end(), key);
    if (added == into.end() || *added != key)
        into.insert(added, key);
}

} // namespace JudasPhysicsDetail
