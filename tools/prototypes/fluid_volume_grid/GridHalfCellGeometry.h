#pragma once

#include "GridCellPhase.h"

namespace judas::volume_grid {

struct GridHalfCellGeometry {
    int axis = 0;
    std::vector<std::array<double, 2>> startCapacity, endCapacity;
    std::vector<double> midpointAperture;
    // A transverse primal face has one shared opening for each half.
    // The component axis has full primal boundary faces instead.
    std::array<std::vector<std::array<double, 2>>, 3> transverseAperture;
};

// Geometric support for the two owned halves of one velocity component.
// Capacities are exact endpoint cuts. Apertures are exact time averages for
// linear axis-aligned box paths. Failure leaves the previous output intact.
bool BuildGridHalfCellGeometry(const GridLayout& layout, const std::vector<BoxMotion>& motions,
                               int axis, GridHalfCellGeometry& output, std::string& error);

struct GridHalfCellWater {
    std::vector<std::array<double, 2>> volume;
};

// Split reconstructed water by the actual wet geometry of each half.
// The phase axis and the momentum component axis can differ. Boxes must
// describe the same stationary snapshot used to reconstruct the water.
bool BuildGridHalfCellWater(const GridLayout& layout, const std::vector<BoxMotion>& stationary,
                            const GridCellPhase& phase, int axis, GridHalfCellWater& output,
                            std::string& error);

struct HalfCellInflow {
    double axialLow = 0;  // Positive flow enters the low half.
    double axialHigh = 0; // Positive flow leaves the high half.
    std::array<double, 2> netTransverse{};
};

// Derive the common midpoint transfer from the lower half's measured volume
// change. Independently check the upper half, and reject transfer through a
// blocked midpoint. Transverse inputs must already come from shared face
// transfers with real geometric support.
bool BalanceGridHalfCell(const GridHalfCellGeometry& geometry, std::size_t cell,
                         const HalfCellInflow& inflow, double& midpointFlux, std::string& error);

struct GridHalfCellStorage {
    // Lower and upper halves of each cell along splitAxis, in m³.
    std::vector<std::array<double, 2>> capacity, water;
};

struct GridHalfCellAperture {
    double open = 0., leftWet = 0., rightWet = 0.; // m²
};

struct GridHalfCellApertures {
    // Indexed like coarse faces, then by the lower/upper half along splitAxis.
    // The entry normal to splitAxis is empty. Coarse axial faces are unsplit.
    std::array<std::vector<std::array<GridHalfCellAperture, 2>>, 3> transverse;
    // Internal face between the lower and upper half of each coarse cell.
    std::vector<GridHalfCellAperture> midpoint;
};

// Recover exact half-cell storage from one set of local phase planes and a
// stationary snapshot. Call separately at the two endpoints with the phase
// planes reconstructed from the water inventories at those endpoints.
bool BuildGridHalfCellStorage(const GridLayout& layout, const std::vector<BoxMotion>& stationary,
                              int phaseAxis, int splitAxis, const std::vector<double>& waterPlane,
                              GridHalfCellStorage& output, std::string& error);

// Average transverse and midpoint openings along linear box paths. Phase
// planes are held fixed in the grid chart for this integral. Wet area uses
// the interior trace of either side; a missing exterior side has zero wet
// area. The geometric open area does not impose an exterior flow condition.
bool AverageGridHalfCellApertures(const GridLayout& layout, const std::vector<BoxMotion>& moving,
                                 int phaseAxis, int splitAxis, const std::vector<double>& waterPlane,
                                 GridHalfCellApertures& output, std::string& error);

} // namespace judas::volume_grid
