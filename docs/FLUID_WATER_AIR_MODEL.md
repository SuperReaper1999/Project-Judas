# Incompressible water and finite air — 6 November 2026

**Follow-up:** [local material-fragment checkpoint](FLUID_MATERIAL_FRAGMENTS.md) implements spatial pieces and crossing events, but complete basin gates remain red. The results below describe this earlier checkpoint.

**Numerical prototype, not a runtime backend or an Advanced Fluids unlock.**
The original authored basins remain the acceptance gate. Pressure proposals,
short controls and sustained two-column flow are separate from that gate.
No main, Basic Water, production particle-fluid, SDK, renderer, release-channel
or independent-research implementation is changed.

Source parent: `ba5fefeddfd95a2115caf63a40117aba993eb902`.
[Raw receipts, source identities and commands](evidence/fluid-water-air-20261106/README.md).

## Approved model and ownership

The operator approved incompressible, conservatively transported water with
atmospheric pressure only at genuine open boundaries, plus finite compressible
trapped air. Dynamic gas waves are intentionally outside this model.

- `WaterGasPockets` owns independent air mass and positive volume. The isothermal
  law is `P_absolute = P_reference * M / (rho_reference * V)`. A sealed pocket
  retains its mass during compression. Explicit exterior vents and real shared
  dry apertures exchange finite mass through viscous/acoustic resistance.
  A microscopic opening does not union pressure owners or pin them to ambient.
- `WaterGasGeometry` measures actual jointly dry apertures and exterior vents.
  Frozen conforming rectangular cells are the initial support boundary. Unknown
  partially obstructed openings, moving capacities, interior atmospheric ports
  and unsupported prescribed boundary flows reject explicitly.
- `WaterMimeticCell` owns a constant-liquid-density, compatible local inertia
  metric. Separate boundary-integration patches do not become independent
  amounts of water. Divergence is eliminated before inversion; area-weighted
  stabilization retains real small patches. Binary64 factors receive
  compensated original-matrix refinement. Pressure acts on gravity-balanced
  head differences, avoiding subtraction of large rounded hydrostatic forces.
- `WaterMimeticGeometry` builds shared-water, air-facing and fixed-solid boundary
  patches. It currently reconstructs a single lower water prism per cell.
- `WaterGasProjection` proposes liquid fluxes and finite-air pressure/mass.
  Restarted Krylov checks the original linear operator. A sparse ILU accelerator
  can fall back to the existing condensed graph accelerator. Failed linear
  applications and fallback work are counted; an unconverged correction is not
  reported as a solved linear system.
- `WaterCellAudit` independently checks full local momentum and implicit work,
  including static-wall pressure reactions. It retains relative head through
  the force audit instead of converting fine differences through large absolute
  pressures. Numerical upwind/backward-Euler loss is measured, not replaced by
  an arbitrary damping term.
- `WaterGasRemap` moves air mass with actual receding-boundary parcels. An
  existing connected local dry slab may receive that material. Two newly
  separated air fronts cannot merge across remaining water. The largest parcel
  is assigned last so ledger subtraction does not erase a tiny native pocket.
- `WaterVolumeAdvance` pairs shared water transfers, transports liquid momentum
  with those same transfers, audits phase quantities and spatial partition, and
  publishes the complete result transactionally. Continued quantities retain
  their compensated parts; a geometry fingerprint rejects stale prepared state.

These modules live under `tools/prototypes/fluid_volume_grid`. They are not
new JudasJS APIs and cannot be selected through Project Settings.

## Geometry, quantity and precision contracts

Water quantity and air **mass** are conservative. Air volume is the void left
by the water/solid geometry; it is not an independently conserved material.
A sustained-flow control found that carrying a pressure proposal's small volume
error into later frames eventually exhausted the partition budget. The remap now
reconciles positive air space to the remaining water geometry within the
**unchanged** containing-cell budget. It does not change air mass or erase a
positive minority pocket when `C-W` is below reconstruction resolution.
The reconstruction's work bound uses the maximum endpoint pressure times the
volume correction. Connected local mixing is checked for passive isothermal
free energy. An independent total-energy control remains separate.

The original occupied-volume criterion is
`min(1e-14 m³, 1e-10 * containing_cell_capacity)`; the gas mass budget is its
reference-density equivalent. Whole-domain quantity/partition checks also run.
A separate, stricter **minority-relative** pressure diagnostic is retained.
That diagnostic does not generally pass the authored basins: a tiny air film
can demand pressure/flux cancellation beyond this eliminated representation's
precision. A passing pressure proposal must not be described as a qualified
material timestep. The whole-advance gate still decides acceptance.

Geometry and global factor/search work use ordinary binary64. Small compatible
local operators, physical ledgers, pressure corrections and residual audits use
compensated pairs of binary64 values. These are not software IEEE binary128 or
fraction arithmetic. Linux reduction helpers also use native `long double`.
No Windows or Android qualification of this new backend is claimed.

## Verified controls and remaining gate

The focused normal and sanitized suites each pass **3,506 checks in 32 suites**.
The independent exact oracle passes 842 checks, a separate reference-mode run
passes 50 checks, and six prior pressure-only states remain byte-identical.
The reproducible evidence records the final exact source hashes, test counts,
raw performance rows and all nonzero gate exits. Important controls include:

- Sealed compression, finite microscopic vents, disconnected air, exterior mass
  accounting, rejection rollback and isothermal free-energy/work checks.
- Affine hydrostatic balance, source velocity, free gravitational acceleration,
  original local momentum/work and corrupted-flux negative controls.
- A body-excluding hydrostatic pool with all six solid reactions integrated:
  buoyancy `(0, 9810, 0) N` for one cubic metre displaced at `1000 kg/m³`.
- 120 stationary intervals and a 240-interval, closed two-column flow control
  with independent cold quantity and adjacent-step energy checks.
- Connected air-parcel remapping, positive sub-ULP native air, and an opposed
  disconnected-front rejection control.

Repeated pressure-only medians are **2.063 s level / 5.492 s rotated** on the
Ryzen 5 5600. These costs are not game-ready, and are not complete timestep
measurements. No production speedup is claimed.

The authored basin pressure proposals can satisfy the original cell-volume
criterion. **The complete basin advances still reject.** The level fixture
receives air from different faces in an initially fully wet cell. Its remaining
water separates those fronts, which one lower-water prism cannot represent.
The rotated fixture also reaches a departure beyond its represented phase
front. Retrying the same frozen geometry, joining the air owners, clipping
negative water, or adding a mass floor would not be a correct repair.

The next architectural correction is local multi-fragment interface ownership
and event/re-cut transport, retaining the location of both air and water until
real positive-area openings connect them. The new pressure/air model can be
preserved; the single-interface reconstruction must not be extended by silently
mixing those states. Moving native MK2 bodies, sustained original basins, radial
fields, cold saves, streaming, rendering and runtime export remain unqualified.

## Preserved repairs and failed approaches

The same checkpoint retains three corrections to the older full-mixture
reference: the zero-normal-flux momentum tangent averages complete donor
momentum products; conforming MAC dual transport follows geometric half-faces
rather than product-partition mixing; and restarted Krylov refuses unqualified
search directions. A frozen initial-grid cook removes only transparent
algorithmic planes while retaining physical gaps, capacities and quantity
ledgers. It is not a moving-geometry velocity remap.

Earlier failed direct, warm-start, numerical-continuation, physical-substep and
preconditioner trials are preserved. One late **old full-mixture** rotated run
actually completed a single original 1/60 interval after numerical continuation
from the same source. It took minutes and does not establish sustained or usable
performance; it also means the old model must not be called mathematically
impossible. The new model was an explicitly approved scope decision.

No inertia floor, arbitrary damping, energy clamp, closed tiny opening or fluid
unlock is part of this checkpoint. The original basin gate must remain red until
the complete intervals really pass.
