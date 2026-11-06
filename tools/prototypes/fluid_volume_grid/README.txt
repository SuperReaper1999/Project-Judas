Bounded volume-grid water prototype

This is an isolated numerical experiment, not a production fluid backend.
It is intentionally absent from the engine/CMake/SDK runtime. Advanced Fluids
remain locked. See the preserved 2 November 2026 fluid-grid investigation and
raw evidence for provenance, failures, numerical boundaries and measured costs.

Build from repository root without engine dependencies:
  c++ -std=c++17 -O3 -Wall -Wextra -pedantic \
    tools/prototypes/fluid_volume_grid/VolumeGridPrototype.cpp \
    -o /tmp/JudasVolumeGridPrototype
  /tmp/JudasVolumeGridPrototype
  /tmp/JudasVolumeGridPrototype bench 1 60
  /tmp/JudasVolumeGridPrototype bench 2 60

The test mode runs 57 controls. The CLI admits only refinement 1 or 2 and
1..1000 samples, rejecting malformed inputs before allocating. bench 1/2 are
DIAGNOSTIC 2304/18432-cell
regular-grid workloads, not same-fidelity comparisons with the production
pool: rotating boxes and narrow stair collision are explicitly unsupported.

Implemented: bounded two-phase (water/air 1000:1) MAC pressure; paired conservative
volume transport; same-flux staggered momentum transport; post-transport
pressure projection; static whole-cell solid masks; explicit input/failure
publication checks; quantity-based queries and a diagnostic isosurface mesh.

Not qualified: moving/cut/subcell solids, finite-mass coupling, cups/pours,
thin-gap ownership, open atmosphere boundaries, geometric PLIC/interface
accuracy, angular-momentum transport, origin/streaming/save/SDK integration,
renderer quality or Windows/Android/browser execution. Hydrostatic body traction
is a static affine patch control, not a general moving-body coupling method.
First-order upwind transport is numerically dissipative. Fine-grid pressure
cost is still unsuitable for replacing current advanced water; no unlock or
production replacement is justified by these results.

Later cut-cell work (separate from the regular-grid executable above):
  docs/FLUID_CONNECTED_MATERIAL_OWNERS.md
  docs/FLUID_INTERNAL_TRANSPORT.md
  bash tools/prototypes/fluid_volume_grid/qualify_internal_transport.sh OUTPUT

Internal-flux/channel controls pass; the original authored basins and a
three-region coface counterexample still reject. The full reproduction script
in docs/evidence/fluid-internal-transport-20261106 intentionally returns failure
while that basin qualification is unresolved. No runtime backend is enabled.

Spatial field continuation (6 November): docs/FLUID_SPATIAL_TRANSPORT.md.
The coface transport control now converges with time subdivision. Original
basin fields build, but pressure/transport coupling still rejects; no native
fluid timestep or new playable backend is qualified.

Locally constrained pressure reference (6 November):
  docs/FLUID_LOCAL_PRESSURE.md
Both original frozen-body basin fixtures now pass one original-timestep
pressure/transport interval. It is a slow correctness reference, not an
integrated runtime backend. Main and the fluid lock remain unchanged.

Fast transport continuation (6 November 2026):
See docs/FLUID_FAST_TRANSPORT.md and qualify_fast_pressure.sh. The slow
reference is archived separately. Original first intervals pass faster, but
the rotated sustained case remains a recorded failure; no runtime or unlock.

Sustained-pressure follow-up (6 November 2026):
The retained fourth-step stall is corrected; both authored fixtures advance
120 intervals with original volume/cumulative quantity checks. See
docs/FLUID_SUSTAINED_PRESSURE.md and qualify_sustained_pressure.sh. Earlier
reports above remain historical. This is still a slow numerical prototype;
high-speed small-face modes, momentum transport and native/body integration
remain unqualified. Main and the Alpha fluid lock are unchanged.
