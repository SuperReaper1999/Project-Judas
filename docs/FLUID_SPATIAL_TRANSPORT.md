# Spatial transport continuation — 6 November 2026

**Spatial transport is implemented as a prototype. Neither original basin has
an accepted, coupled physical timestep.** The Alpha fluid lock and main remain
unchanged. This is outside native runtime/SDK/CMake integration.

## What changed

`CutSpatialTransport` retains water quantity and gas mass in spatially located
cells. Connected material owners retain their physical identity and quantity
ledgers; they are not treated as instant mixing tanks. Subdivision follows real
aperture edges and introduces local midplanes. Thin dimensions are not
needlessly subdivided. Shared faces are built only at actual touching bounds;
no connection is invented through missing regions or solid geometry.

A local kinetic reconstruction provides internal flux. Conservative implicit
upwind transport moves water and **independent gas mass** through those faces.
The gas solver does not infer its stock from `1-waterFraction`, which can
create invalid gas transfers in intermediate pressure trials. Both transported
quantities use positive donor-cell systems and paired face ledgers.

The prototype has an explicit one-million-cell allocation limit. The current
spatial resolution policy is fixed, not a production authoring interface.
Numerical diffusion is present: this is first-order upwind transport, including
small implicit downstream tails, not exact finite-speed interface tracking.
It preserves a spatial field and progression, rather than projecting every
arrival into one whole-owner plane. It is not yet qualified for native wet
queries, forces, free-surface rendering or strict dry-front timing.

## The previous representation failure is removed

The same three-region coface geometry now puts more water on the inlet side
than the outlet side. It agrees with an independent two-cell implicit chain.
Normalized time-subdivision defects are approximately:

| Interval | Defect / interval |
|---|---:|
| 1e-8 | 0.00961075 |
| 1e-9 | 0.000996011 |
| 1e-10 | 0.00009996 |

Unlike the previous single-plane model's constant defect, this converges with
time subdivision. Independent gas mass is nonnegative and spatial quantities
match the conservative owner ledgers in these controls. Tests also carry the
spatial water/mass arrays through successive half steps, without rebuilding
one phase plane and losing their placement.

## Original basins: measured, still rejected

Both original frozen-body authored fixtures now build a spatial field:

| Fixture | Cells | Build | Rejected transport median | p95 / worst |
|---|---:|---:|---:|---:|
| Level | 150,360 | 250.43 ms | 41.79 ms | 43.03 ms |
| Rotated | 264,800 | 422.03 ms | 137.77 ms | 139.78 ms |

Seven fixed-field transport samples per fixture; nearest-rank p95 is the worst
sample. Peak whole-process RSS was 340,184 KiB. These measure **rejected fixed
pressure proposals**, not completed fluid steps or a claimed runtime speedup.
The spatial gas/water systems need 15 and 24 linear sweeps respectively.
Full pressure coupling is more expensive. This is not ready for real-time use.

The old coarse pressure proposal produces local over-capacity cells, including
large relative errors in microscopic cut cells. Initially observed first-cell
errors around 0.15% were not the maximum; full scans found much larger errors.
No overshoot was clipped or accepted.

## Pressure coupling experiments and remaining cause

Three actual alternatives were exercised, retained under the evidence folder:

1. **One thermodynamic pressure per connected owner**, using the new transport
   in its nonlinear residual. It does not close the spatial/quantity constraints.
   An early complement-gas variant reported one accepted result with incomplete
   admission; it is explicitly rejected as qualification evidence. The
   independent-gas formulation does not pass both basins.
2. **Pressure unknowns on every fine spatial cell.** Both original basins fail
   the existing original-row balance gate at the first pressure iteration.
   The smallest requested row tolerance was about `5e-29 m³`. These were retained
   failures, not suppressed or converted to a green qualification.
3. **Global owner pressure plus internal volume correction**, including a
   material-weighted correction. It improves some local capacity errors but
   does not close the complete coupled problem within the tested budget. One
   run ended at its explicit 90-second limit. It was not adopted.

Pressure and spatial transport must solve the same local volume constraints.
A single common owner pressure is an insufficient physical solution basis for
this transport; using the fine graph directly reintroduces conditioning from
microscopic cut cells. Geometric fragments cannot simply become independent
physical pressure owners again. This is now a pressure-coupling architecture
problem, rather than another phase-plane or startup-force patch.

The justified next direction is a conservative, flux-based/condensed pressure
operator which retains local volume constraints while eliminating microscopic
pressure modes stably. Connected owners can remain aggregation/preconditioning
units, but cannot substitute for the local physical constraints. No claim is
made that this operator has been implemented or proven practical yet.

## Selected code and rejected code

The selected code contains the spatial field and transport primitive only.
Experimental pressure callbacks and coordinator changes were removed from the
selected implementation and retained in the attempt archive. The existing
pressure-admission path remains intact.

`CutOwnedFlux` accepts an explicit fragment divergence field, audited against
actual owner port balance. It rejects nonfinite or incompatible source data.
The pressure solve uses a tighter internal residual budget before spatial
reconstruction, rather than loosening the final divergence audit. No scene
counts, apertures, gas inventories or existing failure expectations were reduced.

Validation: 22 new controls; 232 retained internal-transport/connected-owner/
dynamic controls; address, undefined-behaviour and leak checks for the new
controls. Native MK2, rendering, saves, streaming, body motion and platform
integration are not qualified by these prototype tests. No new playable fluid
build is available. Existing independent research and old evidence are unchanged.

[Raw evidence and commands](evidence/fluid-spatial-transport-20261106/README.md).
