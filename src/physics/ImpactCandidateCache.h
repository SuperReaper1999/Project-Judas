#pragma once

#include "Broadphase.h"
#include <cstddef>
#include <unordered_map>
#include <utility>
#include <vector>

namespace JudasPhysicsDetail {

// Derived broadphase candidates for one guarded impact-event pass. The caller
// changes revision whenever ANY fat proxy changes, including the other endpoint.
// No contact, filtering, sleeping, trajectory or response result is retained.
class ImpactCandidateCache {
  public:
    static constexpr std::size_t MaximumSlots = 4096;
    // Only retained candidate-ID capacity is counted here. Filter scratch and
    // map entries are additional bounded memory, not part of this ID budget.
    static constexpr std::size_t MaximumRetainedIds = 262144;

    explicit ImpactCandidateCache(std::size_t maximumSlots = MaximumSlots,
                                  std::size_t maximumIds = MaximumRetainedIds)
        : slotLimit(maximumSlots), idLimit(maximumIds) {}

    template <class Query, class Bounds>
    const std::vector<unsigned> &Get(unsigned slot, const Aabb &fat, const Aabb &query,
                                     std::size_t revision, Query &&original, Bounds &&bounds) {
        if (revision != observedRevision) {
            entries.clear();
            retainedCapacity = 0;
            budgetFull = false;
            observedRevision = revision;
        }
        // A swept or unusual query outside this proxy's enclosure must retain
        // the ordinary tree query. Subset filtering alone would omit candidates.
        if (!fat.Contains(query))
            return original(query);

        auto found = entries.find(slot);
        if (found != entries.end() && !Equal(found->second.fat, fat)) {
            retainedCapacity -= found->second.candidates.capacity();
            entries.erase(found);
            found = entries.end();
        }
        if (found == entries.end()) {
            if (budgetFull || entries.size() >= slotLimit || retainedCapacity >= idLimit)
                return original(query);
            const auto &candidates = original(fat);
            if (candidates.size() > idLimit - retainedCapacity) {
                budgetFull = true;
                return Equal(fat, query) ? candidates : original(query);
            }
            Entry entry{fat, std::vector<unsigned>(candidates.begin(), candidates.end())};
            if (entry.candidates.capacity() > idLimit - retainedCapacity) {
                budgetFull = true;
                return Equal(fat, query) ? candidates : original(query);
            }
            retainedCapacity += entry.candidates.capacity();
            found = entries.emplace(slot, std::move(entry)).first;
        }
        if (Equal(fat, query))
            return found->second.candidates;

        filtered.clear();
        for (unsigned other : found->second.candidates)
            if (bounds(other).Overlaps(query))
                filtered.push_back(other);
        return filtered;
    }

    std::size_t RetainedIdCapacity() const {
        return retainedCapacity;
    }
    std::size_t CachedSlots() const {
        return entries.size();
    }

  private:
    static bool Equal(const Aabb &a, const Aabb &b) {
        return a.min == b.min && a.max == b.max;
    }
    struct Entry {
        Aabb fat;
        std::vector<unsigned> candidates;
    };
    std::size_t slotLimit, idLimit, observedRevision = 0, retainedCapacity = 0;
    bool budgetFull = false;
    std::unordered_map<unsigned, Entry> entries;
    std::vector<unsigned> filtered;
};

} // namespace JudasPhysicsDetail
