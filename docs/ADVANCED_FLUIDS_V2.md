# Advanced Fluids V2 — first integration gate

**Current product direction:** [game-resolution native water](GAME_WATER_PREVIEW.md)
replaces the microscopic runtime approach. A coarse C++ grid now drives a real
MK2/Renderer lab and normal scene/editor/JudasJS integration; the Alpha unlock remains pending.
The original basins are re-sampled at declared game resolution. Their old exact
controls and the first-gate account below remain research evidence, rather than
immutable requirements for the replacement.

Historical follow-up: [incompressible water and finite air](FLUID_WATER_AIR_MODEL.md).
The operator approved replacing dynamic air waves with finite isothermal air
compartments. That implementation is still a prototype; the full authored-basin
advance gate remains required. The first-gate account below is historical.

Follow-up: [bounded connected-material ownership correction](FLUID_CONNECTED_MATERIAL_OWNERS.md).
That report is the newer status; the first checkpoint below is retained as evidence.

Historical first-gate status: **Stage 1 blocked; no runtime backend integrated.** This is an
Experimental implementation/evidence checkpoint, not completion of the approved
four-stage delivery. Main, Basic Water, the production particle solver, Alpha
fluid lock, SDK selection and independent research were not changed.

The source parent is `95b614478957e427f2bb10b386737a5d7f8a5fcf`.
[Reproduction and raw evidence](evidence/advanced-fluids-v2-20261105/README.md)
identify the exact tested source files independently of the eventual commit.

## Implemented and tested foundations

The additions live under `tools/prototypes/fluid_volume_grid/`, outside the
runtime and SDK build. They deliberately cannot be selected in a game.

- `CutDynamicTransport` tests one common dynamic pressure with incompressible
  water, finite gas mass and `rho_air = rho_reference + p / c²`. Pressure and
  phase transfer use the same cut geometry. A water/air boundary can cross a
  shared face; it need not stay inside one cell as the previous hydroacoustic
  storage adapter required.
- Water flux is measured in a finite, face-local departure prism. Moving the
  coordinate origin no longer erases departures thinner than a world-coordinate
  ULP. A partly occluded prism cannot be scaled up into an invented departure.
- Pure single-phase cuts can carry incoming material through during the
  interval. This permission is withheld from mixed cuts and from a pure cut
  receiving any foreign phase. It is not blanket permission to borrow water
  from adjacent dry space.
- Explicit gas vents retain finite conductance, velocity and a boundary mass
  ledger. A microscopic vent remains finite; it is neither deleted nor treated
  as an instantaneous ambient-pressure assignment. Validation is shared with
  the existing gas-topology module, including overlapping/invalid vent rejection.
- `ReconstructCutRegionPhasePlanes` accepts independently owned gas volume.
  Aggregating cuts can round water to the total capacity while retaining a real
  gas pocket smaller than one water ULP. Reconstructing gas as `capacity - water`
  would erase that pocket. Known-answer tests cover both minority materials,
  three axes and both interface orientations at `1e-20 m³`.
- `CutRectangularOwners` is a bounded **static** agglomeration experiment.
  Only exact rectangular unions across fully open faces may merge. It retains
  the original capacity ledger and external apertures; closed walls and tiny
  openings remain closed or finite. It does not qualify moving-owner remapping.
- Publication is transactional. Independent donor, geometric-prism and local
  gas-EOS audits reject a step before replacing the caller's prior result.

The historical water-only reconstruction entry point remains available and
uses its original input contract. No JudasJS or exported SDK API changed.

## Original-basin gate

The early gate uses the preserved `BasinFixture.h` from the previous authored
basin investigation: unchanged float-authored walls, body pose, parcel volumes,
spacing and rotated chart reconstruction. It imports **9.25 m³** and
**9.216000411987... m³** of water. The final driver uses the native float
`1/60` timestep, float gravity and the reference's explicit top gas vent.

The body is frozen for this first admission test. These are the original
preserved basin **fixtures**, not a claim to have run the complete playable
Terrain Planet or rectangular-pool scenes with V2. An operator that cannot
advance these simpler fixtures is not ready to control their native bodies.

| Input | Original regions | Rectangular owners | Smallest original positive capacity |
| --- | ---: | ---: | ---: |
| Level | 20,344 | 17,577 | 2.7941e-12 m³ |
| Rotated | 35,840 | 31,838 | 7.7546e-26 m³ |

Both zero-gravity admission controls pass, with and without agglomeration.
**Neither basin passes a gravity-driven native interval.** Three repeated
attempts for each orientation, ownership variant and timestep produce 36
rejections, all preserving the previously published water and gas state.
The driver returns **exit 1**, not success for an expected failure.

Earlier sealed-chart trials are retained separately. They must not be cited
as matched-boundary comparisons with the final vented gate.

## The actual blocker

The current cut chart treats fine boundary-integration fragments as independent
pressure and material owners. Float-authored boundaries can produce fragments
many orders of magnitude smaller than a normal grid cell. Two separate issues
remain:

1. **Pressure convergence is not local material closure.** A globally small
   residual can exceed the volume of a tiny owner. A manufactured control
   demonstrates a locally emptied gas owner that the absolute residual alone
   would admit. The new relative EOS audit rejects it. Its diagnostic tolerance
   is `1e-10` of local capacity; this is not a claim that every smaller tolerance
   miss would be visible or game-breaking.
2. **Mixed-interface transfer needs actual continuation through cuts.** Even
   without the new local EOS audit, the original fixtures propose transfers
   extending beyond their mixed donors' finite geometric prisms. Pure-phase
   through-flow removes one unnecessary restriction but does not authorize
   moving a water/air front through arbitrary intervening geometry. Exact
   rectangular merging reduces the problem and preserves walls, but leaves
   nonrectangular connected fragments and does not resolve it.

These are implementation/representation blockers, not proof that the pressure
flux is inherently nonphysical or that conservative fluids are impossible.
Deleting a rejection, accepting an unowned phase transfer, globally clamping
water or escalating all arithmetic would not supply the missing material path.
The previously repaired production dry-space launch defect has not been
reopened or changed.

The next required architecture is to separate **material/control-volume
ownership from boundary integration cuts**, retain per-boundary wet footprints,
and supply conservative, geometry-supported phase continuation and remapping
for connected nonrectangular owners. Disconnected pockets must remain distinct;
a narrow vent must retain its finite response. Local pressure residuals must be
scaled/audited against those owners. The rectangular experiment does not prove
that this generalization will work; it identifies where the current adapter
stops being sufficient.

## Qualification and cost

- All **1,030 existing checks across 26 moving-grid suites pass** with the
  independent-phase reconstruction extension. The 22 geometry, 17 gas-topology and 49
  hydroacoustic checks were rerun after extracting shared vent validation.
- **149 new checks pass**, including shared-face transport, three gravity axes,
  closed material ledgers, finite vent response, tiny-phase geometry,
  transactional failures, wall/aperture preservation and invalid inputs.
- The same 149 checks pass under AddressSanitizer and UndefinedBehaviorSanitizer.
  Leak detection was disabled for that runner; this is not leak qualification.
- Removing only the local EOS audit fails its targeted negative control.
  Removing only the occluded-prism audit fails its separate targeted control.
  Neither mutant is used by the engine or the production prototype.
- The rebuilt existing project-fluid-policy test passes **26/26**, for the
  current Experimental configuration, including disabled-region streaming/export.
  The Alpha-channel early-rejection branch was not separately rebuilt; its
  source is unchanged.

`timings.md` in the evidence folder contains mean, median, nearest-rank p95,
worst and process peak RSS. These are **costs of rejected admission attempts**,
not successful fluid-step benchmarks or claimed speedups. There are only three
samples per case, so p95 is the worst sample. Geometry compilation is excluded
from each attempt's timer. The final timing pass launches no compiler jobs;
it remains a shared desktop measurement, not an isolated hardware certification.

There is no meaningful GPU-rendering comparison yet: no valid V2 basin step
exists to render. Renderer V2 and Particles V2 remain unchanged.

## Approved stages and remaining delivery

| Stage | Actual state |
| --- | --- |
| 1. Conservative core / interface transitions | Partial foundations implemented; original-basin gate fails as described above. |
| 2. Native MK2, moving-solid coupling, gravity | Not integrated. No new buoyancy, wall-work, angular momentum or native trajectory acceptance claimed. |
| 3. Performance and presentation | Diagnostic admission costs recorded. No gameplay performance target or new water surface qualified. |
| 4. Authoring, persistence, streaming and export | Not implemented for V2. Existing Alpha lock and runtime boundaries remain in force. |

This checkpoint must not be presented as an unlock candidate. It has no new
playable V2 lab, cold-save format, streaming participant or standalone export.
There was no Windows/Android or spatially varying radial-gravity qualification.

## Human review checklist

- Review the two explicit blockers and the failing authored admission log;
  do not count the passing small controls as gameplay acceptance.
- Confirm the next implementation addresses general material ownership and
  phase continuation, rather than deleting these guards or tuning damping.
- Defer fluid gameplay acceptance until the original basins advance with MK2,
  correct quantity/force accounting, usable cost and genuine save/stream/export
  coverage. There is no new playable build to accept at this checkpoint.
