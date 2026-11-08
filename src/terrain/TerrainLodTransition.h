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
    const std::vector<HeightPatch> &Current() const { return current; }
    const std::vector<HeightPatch> &Incoming() const { return incoming; }
    float Fraction() const { return fraction; }
    bool Active() const { return !incoming.empty(); }
    bool Shared(PatchKey key) const { return unchanged.find(key) != unchanged.end(); }

  private:
    std::vector<HeightPatch> current, incoming;
    std::set<PatchKey> unchanged;
    float fraction = 0;
};
} // namespace TerrainV2
