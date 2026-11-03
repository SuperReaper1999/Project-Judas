#pragma once

#include "MovingGridGeometry.h"

namespace judas::volume_grid {

struct GridParcelSeed {
    std::vector<double> water; // m³ per cell, limited by its stationary cut capacity.
    double movedVolume = 0.;   // m³ moved out of overfilled source cells.
    std::size_t maximumGraphRadius = 0;
};

// Retain the authored portion that fits each cell. Route only the excess,
// visiting reachable cells by open-face distance and then by cell index.
// An empty or partially seeded receiver may accept water up to its capacity.
// Connectivity is the coarse cut-cell graph, with one material volume per cell.
// A radius of -1 searches the reachable component. Radius 0 disallows moves.
// A bounded greedy search may exhaust available receivers in source-index
// order. Insufficient or unrepresentable free capacity rejects the whole
// import, as does any positive seed in an entirely solid cell.
bool ImportGridParcelSeed(const GridLayout& layout, const std::vector<BoxMotion>& stationary,
                          const std::vector<double>& authoredWater, int maxGraphRadius,
                          GridParcelSeed& output, std::string& error);

} // namespace judas::volume_grid
