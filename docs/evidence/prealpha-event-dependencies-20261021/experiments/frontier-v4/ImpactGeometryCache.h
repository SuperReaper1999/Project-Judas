#pragma once

#include "Contacts.h"
#include "ImpactEventSchedule.h"
#include <array>
#include <cstring>
#include <unordered_map>

namespace JudasPhysicsDetail {

// Geometry, not contact lifetime or response. During the owner-only event pass
// shapes cannot change. A velocity update can invalidate TOI without changing
// the represented position/orientation at the next event. Reuse a manifold ONLY
// for bit-identical query geometry, including endpoint order and child indices.
// This does not carry impulses, velocities or a separation proof across time.
class ImpactGeometryCache {
  public:
    using Key = std::array<unsigned, 4>;
    using Pose = std::array<float, 14>;
    static constexpr std::size_t MaximumEntries = 16384;

    template <class Query>
    ContactManifold Get(const Key& key, const Pose& pose, Query&& query) {
        const auto found = entries.find(key);
        if (found != entries.end()) {
            if (std::memcmp(found->second.pose.data(), pose.data(), sizeof(pose)) == 0) {
                ++hits;
                return found->second.geometry;
            }
            found->second = {pose, query()};
            return found->second.geometry;
        }
        const auto geometry = query();
        if (entries.size() < MaximumEntries) entries.emplace(key, Entry{pose, geometry});
        return geometry;
    }
    std::size_t Hits() const { return hits; }
    std::size_t Size() const { return entries.size(); }

  private:
    struct Hash {
        std::size_t operator()(const Key& key) const noexcept {
            std::size_t hash = key[0];
            for (std::size_t i=1; i<key.size(); ++i)
                hash ^= std::size_t(key[i]) + 0x9e3779b9U + (hash<<6) + (hash>>2);
            return hash;
        }
    };
    struct Entry { Pose pose; ContactManifold geometry; };
    std::unordered_map<Key, Entry, Hash> entries;
    std::size_t hits = 0;
};
} // namespace JudasPhysicsDetail
