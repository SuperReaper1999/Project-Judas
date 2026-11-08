#include "TerrainLodTransition.h"
#include <algorithm>
#include <cmath>

namespace TerrainV2 {
namespace {
bool SameSelection(const std::vector<HeightPatch> &a, const std::vector<HeightPatch> &b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](auto &x, auto &y) {
               return x.key.x == y.key.x && x.key.z == y.key.z && x.key.level == y.key.level &&
                      x.edgeStrides == y.edgeStrides;
           });
}
} // namespace

void LodTransition::Reset() {
    current.clear();
    incoming.clear();
    unchanged.clear();
    fraction = 0;
}

void LodTransition::Update(std::vector<HeightPatch> selected, float seconds) {
    if (selected.empty()) {
        Reset();
        return;
    }
    // Zero-time observers select their own complete camera union immediately.
    // Source publication resets history before any changed geometry is drawn.
    if (current.empty() || !std::isfinite(seconds) || seconds <= 0) {
        current = std::move(selected);
        incoming.clear();
        unchanged.clear();
        fraction = 0;
        return;
    }
    if (Active()) {
        fraction = std::min(1.f, fraction + seconds / Duration);
        if (fraction < 1)
            return;
        current = std::move(incoming);
        incoming.clear();
        unchanged.clear();
        fraction = 0;
    }
    // Retarget only at a completed boundary: rapid camera movement cannot grow
    // a history chain or repeatedly restart a fade. The latest requested union
    // begins next; no camera can affect physical support or native residency.
    if (!SameSelection(current, selected)) {
        std::map<PatchKey, glm::vec4> nextEdges;
        for (const auto &p : selected)
            nextEdges.emplace(p.key, p.edgeStrides);
        for (const auto &p : current) {
            const auto it = nextEdges.find(p.key);
            if (it != nextEdges.end() && it->second == p.edgeStrides)
                unchanged.insert(p.key);
        }
        incoming = std::move(selected);
    }
}
} // namespace TerrainV2
