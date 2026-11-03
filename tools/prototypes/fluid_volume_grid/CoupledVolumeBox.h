#pragma once

#include "HalfDualVolumeMomentum.h"
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

struct OwnedCoupledBoxStep {
    CoupledBoxStep box;
    HalfDualVolumeState halfVelocity;
    GridVec pressureImpulse{}, transportMomentumError{};
    GridVec residualGravityImpulse{}, geometricTransportRoundoffImpulse{};
    double geometricPhaseFluxRoundoff = 0.;
    double geometricPressureMixingLoss = 0.;
    double transportKineticChange = 0;
    double mergeKineticLoss = 0;
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

// Fully wet coupled pressure with persistent ownership at closing and opening
// faces. Shared face samples are derived from the half state on publication.
bool AdvanceOwnedCoupledVolumeBox(const GridLayout& layout, const CoupledBoxState& body,
                                 const MovingVolumeState& fluid, const HalfDualVolumeState& owned,
                                 double dt, const GridVec& gravity, OwnedCoupledBoxStep& output,
                                 std::string& error);

bool AdvanceOwnedEvolvingCoupledVolumeBox(const GridLayout& layout, const CoupledBoxState& body,
                                         const MovingVolumeState& fluid,
                                         const HalfDualVolumeState& owned,
                                         double dt, int axis, double signedGravity,
                                         OwnedCoupledBoxStep& output, std::string& error);

bool AdvanceGeometricEvolvingCoupledVolumeBox(const GridLayout& layout, const CoupledBoxState& body,
                                              const MovingVolumeState& fluid, const HalfDualVolumeState& owned,
                                              double dt, int axis, double signedGravity,
                                              OwnedCoupledBoxStep& output, std::string& error);

struct GeometricCoupledBoxInterval {
    OwnedCoupledBoxStep final;
    GridVec bodyPressureImpulse{};
    int substeps = 0, rejectedAttempts = 0;
    double maxPressureEnergyResidual = 0., geometricPhaseFluxRoundoff = 0.;
};

// The external interval remains unchanged. Retry an inadmissible material
// transfer with smaller coupled steps and publish only the whole result.
bool AdvanceGeometricCoupledBoxInterval(const GridLayout& layout, const CoupledBoxState& body,
                                         const MovingVolumeState& fluid, const HalfDualVolumeState& owned,
                                         double dt, int axis, double signedGravity, int subdivisionLimit,
                                         GeometricCoupledBoxInterval& output, std::string& error);

} // namespace judas::volume_grid
