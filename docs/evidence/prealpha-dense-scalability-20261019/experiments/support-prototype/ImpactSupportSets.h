#pragma once

#include <algorithm>
#include <array>
#include <iterator>
#include <vector>

namespace JudasPhysicsDetail {
using ImpactSupportKey = std::array<unsigned, 4>;

// Both lists are sorted unique sets. Publishing one classification retains
// encounter order and makes no change when the membership already matches.
inline void SetImpactSupport(std::vector<ImpactSupportKey>& support,
                             std::vector<ImpactSupportKey>& spinning,
                             const ImpactSupportKey& key,bool spin) {
    auto& from=spin?support:spinning;
    auto& into=spin?spinning:support;
    const auto removed=std::lower_bound(from.begin(),from.end(),key);
    if(removed!=from.end() && *removed==key) from.erase(removed);
    const auto added=std::lower_bound(into.begin(),into.end(),key);
    if(added==into.end() || *added!=key) into.insert(added,key);
}

// Publish a batch of classifications without repeatedly scanning sorted sets.
// Predicates retain encounter order. On predicate failure publish the completed
// prefix, exactly as sequential per-key publication did before the failure.
template<class Classify>
void ReclassifyImpactSupports(std::vector<ImpactSupportKey>& support,
                             std::vector<ImpactSupportKey>& spinning,
                             const std::vector<ImpactSupportKey>& events,
                             Classify&& classify) {
    struct Update { ImpactSupportKey key; bool spinning; };
    std::vector<Update> updates;
    updates.reserve(events.size());
    const auto publish = [&] {
        std::stable_sort(updates.begin(), updates.end(),
                         [](const Update& a, const Update& b) { return a.key < b.key; });
        std::vector<ImpactSupportKey> intoSupport, intoSpinning;
        for (std::size_t begin=0; begin<updates.size();) {
            std::size_t end=begin+1;
            while (end<updates.size() && updates[end].key==updates[begin].key) ++end;
            const auto& last=updates[end-1];
            (last.spinning ? intoSpinning : intoSupport).push_back(last.key);
            begin=end;
        }
        const auto merge = [](std::vector<ImpactSupportKey>& existing,
                              const std::vector<ImpactSupportKey>& removed,
                              const std::vector<ImpactSupportKey>& added) {
            std::vector<ImpactSupportKey> retained, result;
            retained.reserve(existing.size());
            std::set_difference(existing.begin(),existing.end(),removed.begin(),removed.end(),
                                std::back_inserter(retained));
            result.reserve(retained.size()+added.size());
            std::set_union(retained.begin(),retained.end(),added.begin(),added.end(),
                           std::back_inserter(result));
            existing.swap(result);
        };
        merge(support,intoSpinning,intoSupport);
        merge(spinning,intoSupport,intoSpinning);
    };
    try {
        for (const auto& key : events) updates.push_back({key,classify(key)});
    } catch (...) {
        publish();
        throw;
    }
    publish();
}
} // namespace JudasPhysicsDetail
