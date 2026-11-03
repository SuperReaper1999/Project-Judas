#pragma once

#include "MovingVolumeGrid.h"

namespace judas::volume_grid {

struct HalfDualVolumeState {
    // For each axis, primal cell c owns velocity samples 2*c and 2*c+1
    // on its low and high half-duals. Closed faces never share that state.
    std::array<std::vector<double>, 3> velocity;
};

struct HalfDualVolumeTransport {
    HalfDualVolumeState state;
    GridVec beforeMomentum{}, afterMomentum{}, roundoffMomentum{};
    double beforeKineticEnergy = 0, afterKineticEnergy = 0;
    double maximumHalfMassResidual = 0, maximumDonorCfl = 0;
};

// Split the staggered control volume so each primal cell owns half its mass
// on each side. Exchange across an actual shared face only when the paired
// material ledger says fluid crossed. Face birth/death needs no lost velocity
// or fabricated cross-wall momentum. Pressure ownership remains separate.
bool AdvectHalfDualVolumeMomentum(const GridLayout& layout, const MovingGridGeometry& geometry,
                                  const MovingVolumeState& before,
                                  const MovingVolumeStep& projected,
                                  const HalfDualVolumeState& initial,
                                  HalfDualVolumeTransport& output, std::string& error,
                                  bool* donorCapacityExceeded = nullptr);

// Use the mass-weighted normal velocity for each average-open face without
// changing either owned half velocity. An average-open face with no start
// material has no pressure inertia and requires a separate topology map.
bool BuildHalfDualVolumePredictor(const GridLayout& layout, const MovingGridGeometry& geometry,
                                  const std::vector<double>& water,
                                  const HalfDualVolumeState& initial,
                                  MovingVolumeState& output, std::string& error);

struct HalfDualPressureUpdate {
    HalfDualVolumeState state;
    GridVec impulse{};
    GridVec gravityImpulse{};
    double kineticChange = 0, correctionEnergy = 0;
};

// Map a pressure projection with the same half-mass metric back to its
// owners. The same normal increment is added to both sides, preserving their
// velocity difference until end-of-step topology decides whether to merge.
// The projection must use the matched metric. Any declared residual gravity
// increment is applied to both halves before measuring pressure work.
bool ApplyHalfDualVolumePressure(const GridLayout& layout, const MovingGridGeometry& geometry,
                                 const MovingVolumeState& before,
                                 const MovingVolumeStep& projected,
                                 const HalfDualVolumeState& initial, double dt,
                                 HalfDualPressureUpdate& output, std::string& error);

struct HalfDualFaceMerge {
    HalfDualVolumeState state;
    std::array<std::vector<double>, 3> faceVelocity;
    GridVec stationaryWallImpulse{};
    double stationaryWallKineticLoss = 0, mergeKineticLoss = 0;
};

// Construct a shared normal velocity on each open endpoint face by
// conservative mixing. Closed interior faces retain the two half values;
// a stationary exterior wall zeros its own normal half and records the
// corresponding external impulse. This is a state mapping, not a projection.
bool MergeHalfDualVolumeFaces(const GridLayout& layout, const MovingGridGeometry& geometry,
                              const std::vector<double>& water,
                              const HalfDualVolumeState& initial,
                              HalfDualFaceMerge& output, std::string& error);

} // namespace judas::volume_grid
