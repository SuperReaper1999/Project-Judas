#pragma once

#include "../GridStaticUnion.h"
#include "GridHalfCellGeometry.h"

namespace judas::volume_grid {

// Geometry only. Dynamic boxes must stay strictly disjoint from the static
// union and from each other. The caller owns combined pocket admission.
bool ReconstructStaticMovingPhase(const GridStaticUnion& fixed, const std::vector<BoxMotion>& snapshot,
                                  int phaseAxis, const std::vector<double>& water,
                                  GridCellPhase& output, std::string& error);

bool BuildStaticMovingHalfStorage(const GridStaticUnion& fixed, const std::vector<BoxMotion>& snapshot,
                                  const GridCellPhase& phase, int splitAxis,
                                  GridHalfCellStorage& output, std::string& error);

// Average open rectangles with independently clipped wet traces on either
// side. Phase planes remain fixed over the prescribed box motion interval.
bool AverageStaticMovingWetApertures(const GridStaticUnion& fixed, const std::vector<BoxMotion>& motion,
                                     const GridCellPhase& phase,
                                     std::array<std::vector<GridHalfCellAperture>, 3>& output,
                                     std::string& error);

bool AverageStaticMovingHalfApertures(const GridStaticUnion& fixed, const std::vector<BoxMotion>& motion,
                                      const GridCellPhase& phase, int splitAxis,
                                      GridHalfCellApertures& output, std::string& error);

} // namespace judas::volume_grid
