#pragma once

#include "Broadphase.h"
#include <algorithm>
#include <vector>

namespace JudasPhysicsDetail {

// A no-impact certificate is spatial, not a remembered Boolean answer. Every
// proxy encloses its body's complete remaining path. Nonoverlapping proxies
// certify separation for the whole interval, even after a trajectory changes
// inside its previous enclosure. Escaping paths replace their proxy BEFORE any
// neighbour queries. No velocity tolerance is involved.
class ImpactSweepIndex {
  public:
    explicit ImpactSweepIndex(std::size_t slots) : proxies(slots, DynamicAabbTree::kNull) {}
    void Update(unsigned slot, const Aabb& enclosure) {
        auto& proxy = proxies[slot];
        if (proxy == DynamicAabbTree::kNull) proxy = tree.CreateProxy(enclosure, slot);
        else if (!tree.FatAabb(proxy).Contains(enclosure)) tree.MoveProxy(proxy, enclosure);
    }
    bool Separated(unsigned a, unsigned b) const {
        return !tree.FatAabb(proxies[a]).Overlaps(tree.FatAabb(proxies[b]));
    }
    const std::vector<unsigned>& Neighbours(unsigned slot) {
        found.clear();
        tree.Query(tree.FatAabb(proxies[slot]), [&](int proxy) { found.push_back(tree.UserData(proxy)); });
        std::sort(found.begin(), found.end());
        return found;
    }
  private:
    DynamicAabbTree tree;
    std::vector<int> proxies;
    std::vector<unsigned> found;
};
} // namespace JudasPhysicsDetail
