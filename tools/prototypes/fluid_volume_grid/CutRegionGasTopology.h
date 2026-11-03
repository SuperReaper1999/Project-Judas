#pragma once

#include "CutRegionGeometry.h"

#include <limits>

namespace judas::volume_grid {

constexpr std::size_t ExteriorCutRegionGas = 0;
constexpr std::size_t NoCutRegionGas = std::numeric_limits<std::size_t>::max();

struct CutRegionGasVent {
    int axis = -1, sign = 0;
    GridVec lo{}, hi{};
};

struct CutRegionGasComponent {
    double volume = 0.;
    std::vector<std::size_t> regions;
};

struct CutRegionGasTopology {
    std::vector<CutRegionGasComponent> components;
    std::vector<std::size_t> regionOwner;
    std::vector<double> regionVolume, sharedDryArea, ventArea;
};

CutRegionGasVent FullCutRegionGasVent(const CutRegionChart& chart, int axis, int sign);

// Builds the gas graph at the beginning of the interval. Gas volume,
// two-sided face apertures and vents use the supplied phase interfaces.
// Component zero is the common exterior and remains present when sealed.
// A solid crossing that splits a dry slab inside one region during the
// frozen-interface interval requires separate pressure owners.
bool BuildCutRegionGasTopology(const CutRegionChart& chart, const CutRegionGeometry& geometry,
                              const std::vector<CutRegionWaterPlane>& water,
                              const std::vector<CutRegionGasVent>& vents,
                              CutRegionGasTopology& output, std::string& error);

} // namespace judas::volume_grid
