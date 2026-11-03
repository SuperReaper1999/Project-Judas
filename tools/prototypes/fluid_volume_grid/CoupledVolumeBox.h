#pragma once

#include "MovingVolumeMomentum.h"

namespace judas::volume_grid {

struct CoupledBoxState {
    std::uint32_t owner = 0;
    GridVec lo{}, hi{}, velocity{};
    double mass = 0;
};

struct CoupledBoxStep {
    CoupledBoxState body;
    MovingVolumeStep fluid; // Dynamic pressure after removing hydrostatic equilibrium.
    GridVec pressureForce{};
    double rigidResidual = 0; // Maximum displacement residual in metres.
    double pressureEnergyResidual = 0;
    int geometryEvaluations = 0;
    MovingVolumeMomentumStep transport;
    GridVec stationaryWallImpulse{};
    double stationaryWallKineticLoss = 0;
};

// A translation-only box in a sealed, fully filled, uniform-density tank.
// The pressure reaction and the displacement are solved together. The body
// rotation is constrained, and the tank's external wall impulses are external.
bool AdvanceCoupledVolumeBox(const GridLayout& layout, const CoupledBoxState& body,
                             const MovingVolumeState& fluid, double dt, const GridVec& gravity,
                             CoupledBoxStep& output, std::string& error);

// The same coupled solve with an additional conservative momentum transport
// stage. Open interior-face topology must persist for the whole step.
// fluid.state.velocity is the advected, wall-admissible next predictor.
// Pressure work still describes the projection stage before transport.
bool AdvanceTransportedVolumeBox(const GridLayout& layout, const CoupledBoxState& body,
                                 const MovingVolumeState& fluid, double dt,
                                 const GridVec& gravity, CoupledBoxStep& output,
                                 std::string& error);

// Coupled pressure and water transport with a reconstructing free surface.
// Gravity is signed along the chart axis. Momentum advection is separate.
bool AdvanceEvolvingCoupledVolumeBox(const GridLayout& layout, const CoupledBoxState& body,
                                     const MovingVolumeState& fluid, double dt, int axis,
                                     double signedGravity, CoupledBoxStep& output,
                                     std::string& error);

} // namespace judas::volume_grid
