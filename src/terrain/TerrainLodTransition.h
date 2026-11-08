#pragma once

#include "TerrainLod.h"

namespace TerrainV2 {
// Presentation-only history. Each selection is complete and independently
// stitched; complementary screen coverage blends them without transparent
// sorting, duplicate depth or interpolating the physical surface.
class LodTransition {
  public:
    static constexpr float Duration = .18f;
    void Update(std::vector<HeightPatch> selected, float seconds);
    void Reset();
    const std::vector<HeightPatch> &Current() const { return observing ? observed : current; }
    const std::vector<HeightPatch> &Incoming() const { return incoming; }
    float Fraction() const { return observing ? 0 : fraction; }
    bool Active() const { return !observing && !incoming.empty(); }
    bool Shared(PatchKey key) const { return !observing && unchanged.find(key) != unchanged.end(); }

  private:
    std::vector<HeightPatch> current, incoming, observed;
    std::set<PatchKey> unchanged;
    float fraction = 0;
    bool observing = false;
};
} // namespace TerrainV2
