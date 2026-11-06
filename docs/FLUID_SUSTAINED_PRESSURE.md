# Sustained cut-volume pressure correction — 6 November 2026

**The retained fourth-interval pressure stagnation is corrected.** Both authored
fixtures advance 120 intervals at the unchanged native float timestep, with
all local admission and cumulative quantity checks passing. This remains a
CPU numerical prototype outside the native engine/SDK. **It is not a fluid
release or an unlock:** runtime still costs seconds, high-speed small-face
modes remain, and moving bodies/momentum advection are not qualified.

## Mechanism and correction

The previous formulation solved the cell-volume residual

`R = C − W(p) − M(p)/rho(p)`.

In the retained rotated-basin state, a Newton trial nearly drains a tiny cell
while its pressure still implies ordinary gas density. One traced cell has
capacity about `4.47e-12 m³` but represented phase volume about `5e-22 m³`.
The local density derivative then becomes tiny. The proposed pressure correction
is of order `−2e13 Pa`; positivity and merit backtracking reduce the common
correction to roughly `1e-14`. The volume residual stagnates. The slow path
exhibits the same difficulty; this was not cured by optimizing transport alone.

The selected formulation uses conservative face balance:

`B(p) = T(p) diag(C)^−1 R(p)`.

`T` is the positive upwind transport operator with positive cell capacity.
Its columns are strictly diagonally dominant, so it is nonsingular. This
transformation preserves the physical root. Newton applies the complete
new derivative, including `dT diag(C)^−1 R`, and the line search measures this
same balance. Final admission still evaluates **the original volume residual**,
phase bounds and quantity ledgers, not merely the transformed equation.

The old volume-residual derivative was not simply a missing-`dT` bug: it was
correct for its old equation. The new equation changes globalization and
conditioning. Omitting `dT` from the **new** derivative is wrong, and an
independent finite-difference negative control detects that error.

Mass-residual, logarithmic-density and transported-storage experiments did not
resolve the retained case within their budgets. Their sources/results remain.
The conservative balance restores flow/volume balance instead of trying to
almost evacuate a microscopic cell. No physical damping, energy clamp,
quantity insertion, aperture removal, timestep reduction or body-count change
is used.

## Gas-only boundary correction

Once pressure converged, the old transport exposed another defect: it sent
both mixture fractions through a declared gas-only port, then rejected the
water it had removed. The corrected operator is phase-selective:

- Water has no exterior edge at a gas-only port.
- Outgoing gas uses `M/(C−W)`, its gas density, rather than `M/C`, a mixture-
  diluted density. Incoming gas uses the configured ambient density.
- The tangent includes the dependence of outgoing gas density on water volume.
- Actual water boundary flux and whole-field ledgers are still checked.
- The opening must remain geometrically dry. A wet opening or an interior
  solid face misdeclared as exterior rejects explicitly; it is not silently
  converted to atmosphere or permitted to leak.

Known-answer tests cover a partly filled cell, independent quadratic pressure
balance, gas inflow/outflow, non-default ambient density, both transport
sensitivities, blocked/invalid opening ownership and transactional rejection.
There is no rule that discards small water quantities. Geometric interface
support remains distinct from the mere presence of a water trace in a cell.

## Ownership and performance changes

The work is split into cohesive prototype modules:

- `CutConservativeBalance`: residual coordinates and their complete tangent.
- `CutSelectiveTransport`: phase-selective flux laws and prepared material
  matrices, reused for water/gas and sensitivity solves when compatible.
- `CutPressurePreconditioner`: a reusable approximate pressure accelerator for
  an outer Krylov solve. It is rebuilt at each nonlinear state and reused
  across that state's right-hand sides. Its output is never a physical result.
- `CutLocalPressure`: coordinates preparation, Newton evaluation, accepted-
  trial ownership and unchanged physical admission. The obsolete transport
  wrapper was removed after its fast implementation moved into the module.

Caches belong to the evaluation that produced their coefficients. A rejected
trial cannot replace the current prepared state; accepting a trial moves its
matching matrices with its values. There is no global cache or cross-world
reuse. Purely numerical gauges exist only in the preconditioner.

Material solves reserve additional accumulation headroom within the existing
public tolerance: the normal physical quantity budget is multiplied by `1e-4`
before transport's existing `/8` allocation. This **tightens** numerical solving;
it does not adjust stored mass or relax any physical gate.

## Measurements

Matched first intervals against Experimental parent
`5d412ff196ca8790999625faf3960514977ca340`, selecting its Fast backend:

| Fixture | Previous median | Current median | Factor | Current p95/worst |
|---|---:|---:|---:|---:|
| Level | 6.009 s | 1.066 s | 5.64× | 1.104 s |
| Rotated | 6.818 s | 2.227 s | 3.06× | 2.314 s |

Three samples per backend/orientation alternate order on the Ryzen 5 5600.
Nearest-rank p95 equals the maximum at this sample size; no broad tail claim.
All repeated field dumps are byte-identical within each backend. Maximum
cross-backend differences are about `2.16e-11 Pa` pressure and `2.52e-17 m³`
water; the candidate is not claimed to be bit-identical to the prior method.

Peak first-interval process RSS: previous 156,764 KiB; current 212,444 KiB.
Prepared-state reuse has a memory cost. Source flags remain C++17 `-O3` with
warnings treated as errors, without fast-math or a native-only CPU requirement.

The older slow correctness reference at `574fa22f` remains archived unchanged.
A separate fresh run compares its physical fields with the current solution.
Its single-run timings are not used for the headline performance claim.

A paired retained-state study reduced pressure linear-solve time from about
1.43 s to 0.53 s, but total step cost improved only about 9% because transport
then dominated. These intermediate pairs preceded the tighter accumulation
budget; final timings are reported separately above. A hardware-specific FMA
experiment and a certified unit-barrier shortcut were not adopted: they did
not establish a worthwhile general improvement for the added work/constraints.

## Qualification and limits

- **3,179 focused checks**, all passing, including the original 3,135.
- Address/undefined-behaviour/leak checks pass the same suite and the retained
  full 37,334-cell cold numerical replay, outside the tracing wrapper.
- **842 independent exact-dyadic enclosure checks**, all passing.
- Both authored basins advance **120 intervals each** (`2.0000001043 s` per
  fixture), with every prefix checked against the original cumulative water
  and gas budgets. Detailed per-step closure, density, pressure, speed and
  timing records are retained. No failed interval is published.
- A final ownership-neutral cleanup reproduces the same retained-state output
  byte for byte. Fresh qualification from another working directory also
  checks the documented build/replay path. The object manifest prevents stale
  unselected object files from entering that path.

This is pressure/quantity qualification, **not complete fluid-motion reliability**.
There are very high velocities on tiny water-bearing faces. They already exist
in the retained third state: a `2.24e-9 m²` face carrying about `1.12e-7 kg` of
water has speed around 3,305 m/s. Fourth-state water-weighted normal RMS is
about 0.124 m/s, but the extreme local modes must not be dismissed. Neither
these diagnostics nor low total mass make them safe for body coupling.

The current continuation carries normal velocity while material moves; it
still lacks conservative momentum advection. That omission is confirmed,
but it has **not** been established as the sole cause of the fast face modes.
Momentum/energy transport and small-aperture modes need separate investigation
before calling sustained fluid motion reliable or adding moving-body coupling.
No artificial velocity cap was added.

Runtime remains **seconds per 1/60-second interval**, far outside a game budget.
The later rotated window also becomes more expensive than its first interval.
Further work should address repeated sparse preparation/material solves and
momentum transport, using these retained workloads rather than only first-step
benchmarks. The current results do not establish a fundamentally impossible
architecture or a real-time solution.

The model is still frozen-geometry, sealed to water except internal transfers,
with supported exterior gas-only ports. Native MK2 reaction forces, moving-
solid remaps, water outflow boundaries, full arbitrary-gravity reconstruction,
wet queries, native saves/streaming/export and final rendering remain outside
this qualification. The diagnostic binary restart is not a Judas game save.
Linux GCC/default floating-point modes are qualified; Windows/Android/browser
and altered floating-point modes are not claimed here. Main, projects,
independent research and the Alpha fluid lock are unchanged.

[Raw evidence, exact reproduction and review checklist](evidence/fluid-sustained-pressure-20261106/README.md).
