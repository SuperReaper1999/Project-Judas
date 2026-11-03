#pragma once

#include "MovingVolumeGrid.h"

namespace judas::volume_grid::detail {

struct VolumePressureLink {
    std::size_t left, right;
    double conductance;
};

bool BalanceVolumePressureSources(const std::vector<VolumePressureLink>& links, std::vector<double>& rhs,
                                  double cellVolume, std::string& error);

// Solve the same sealed pressure system on a nonuniform control-volume graph.
// Callers validate geometry and independently check the published edge fluxes.
bool SolveVolumePressure(const std::vector<VolumePressureLink>& links, std::vector<double>& rhs,
                         double tolerance, double cellVolume, std::vector<double>& pressure,
                         int& iterations, std::string& error);

// Uses the same mass as the dual momentum transport. The caller owns
// hydrostatic gravity splitting because the geometry changes this metric.
bool AdvanceVolumeWithDualMass(const GridLayout& layout, const MovingGridGeometry& geometry,
                               const MovingVolumeState& state, double dt,
                               MovingVolumeStep& output, std::string& error);

// Same mass metric, with the opening-face predictor supplied by persistent
// side-owned momentum. Do not zero a start-closed face's supplied velocity.
bool AdvanceVolumeWithOwnedDualMass(const GridLayout& layout, const MovingGridGeometry& geometry,
                                    const MovingVolumeState& state, double dt,
                                    MovingVolumeStep& output, std::string& error);

// Called only after the oriented swept-geometry builder independently verifies
// its integration error. Exact endpoint capacities still drive pressure.
bool AdvanceVolumeForVerifiedSweep(const GridLayout& layout, const MovingGridGeometry& geometry,
                                   const MovingVolumeState& state, double dt,
                                   double verifiedSweepError, MovingVolumeStep& output,
                                   std::string& error);

} // namespace judas::volume_grid::detail
