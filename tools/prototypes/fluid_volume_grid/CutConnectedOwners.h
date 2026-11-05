#pragma once

#include "CutRegionGeometry.h"

namespace judas::volume_grid {

// Geometry fragments remain immutable integration pieces. An owner is a
// connected union of those pieces, not their bounding box.
struct CutConnectedOwners {
    std::vector<std::size_t> owner;
    std::vector<std::vector<std::size_t>> members;
    std::vector<double> capacity;
    std::vector<GridVec> lo, hi;
};

bool BuildCutConnectedOwners(const CutRegionChart& chart, const CutRegionGeometry& geometry,
                             double minimumFraction, CutConnectedOwners& output, std::string& error);

struct CutOwnedDeparture {
    double water = 0., gas = 0., unsupportedVolume = 0.;
    std::size_t visitedFragments = 0;
};

// Continue a face-local swept prism across an owner's actual fragments.
// Partition the footprint before tracing: a solid or a gap terminates that
// sub-column. Never fill the owner's bounding box or resume behind a wall.
// This is an integration measure, not a mutation of material state.
bool MeasureCutOwnedDeparture(const CutRegionChart& chart, const CutRegionGeometry& geometry,
                              const CutConnectedOwners& owners,
                              const std::vector<CutRegionWaterPlane>& planes,
                              std::size_t face, double signedVolume,
                              CutOwnedDeparture& output, std::string& error);

} // namespace judas::volume_grid
