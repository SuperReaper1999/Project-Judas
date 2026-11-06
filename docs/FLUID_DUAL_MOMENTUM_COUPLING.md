# Velocity-dual momentum integration — 6 November 2026

**Prototype checkpoint, not a repaired runtime backend. The original authored
basins still reject their first fully coupled interval. Advanced Fluids remain
locked; native engine, SDK, main, release channels and independent research are
unchanged.** Parent: `42e233ee430456bb920c61e62947df83baf9d0e1`.

## What the startup observation does and does not explain

The visible `assets/scenes/terrain.judas` Lake is a 5×5×5 particle seed with an
emitter. Its cube release is a startup/splash test, not a hydrostatic pool.
The preserved numerical `BasinFixture.h`, however, already seeds water throughout
the authored basin, excluding the body. These are different cases. Earlier
interactive wording calling the numerical `original` runs a cube drop was
incorrect. Their new coupling failure cannot be attributed solely to that drop.

Three controls must stay distinct:

- Unchanged original numerical basins, including real tiny wall gaps, independent
  gas quantity, finite body exclusion and float-authored geometry.
- New `filled` hydrostatic **domain** controls, with a level surface at chart
  y=2 m across the connected domain, including outside the pool walls. These
  isolate pressure/gravity balance; they are not a basin-only fill with its
  real drainage gaps silently sealed. Only three 1/60-second steps are tested.
- The native Terrain planet cube/emitter release, still on the locked legacy
  solver. This checkpoint does not change or qualify that gameplay scene.

## Implemented, qualified building blocks

`tools/prototypes/fluid_volume_grid/` now separates:

- `CutVelocityDual`: immutable face-normal inertia owners and conservative mass
  routing derived from the same cell mixture used by quantity transport.
- `CutDualMomentum`: frozen-coefficient momentum response, original local row
  checks and the backward-Euler/upwind kinetic-work ledger.
- `CutDualLinear`: signed velocity solve, compensated refinement over binary64
  ILU/GMRES. Positive water/gas certificates are unchanged.
- `CutCoupledResidual`: a transactional evaluation of quantity, pressure, velocity
  and actual arrived momentum together, with the complete analytic tangent.
- `CutCoupledPressure`: bounded nonlinear driver. It publishes only after the
  original per-cell volume, whole-quantity, dry-port, momentum and work checks.
  Its Newton and Oseen modes are research alternatives, not developer APIs.
- `CutOrthogonalField`: bounded initial-state conservative cooking onto an exact
  conforming rectilinear chart. It rejects moving solids, existing flow remaps
  and budget exhaustion rather than deleting positive slivers.

For each velocity component, a cell's actual mixture mass is partitioned among
its incident aperture-connected columns and fixed normal-wall remainder. A
primal mass transfer F from cell L to R is scattered using the product of those
partitions. Self-routes cancel. Therefore dual mass change and carrier divergence
are the same weighted primal mass balance. This is deliberately first-order
cell-mixture transport; it is not a sharp-phase or angular-momentum claim.

On the original geometry the maximum measured dual mass residual is below
`9e-20 kg`. The level/rotated charts have 70,710/121,712 velocity owners and about
714,000/1,220,000 carrier routes. Zero primal transfer produces no invented
momentum exchange. Explicit positive half-column overlap checks protect geometry.

The momentum equation is

`m_end*u - m_start*u_start + sum(upwind mass carrier * u_donor)`
`= dt*A*pressure_jump + dt*m_end*g`.

There is no speed cap, extra damping parameter, energy rescaling or accepted
unconverged state. The work ledger includes the first-order method's unsteady
and mixing losses, boundary energy, and actual mass-defect work. It is a kinetic
work check, **not** a complete gas-internal/gravitational-energy qualification.

The existing pressure/quantity algorithm was left unchanged. Only thin wrappers
expose its already-owned geometry stamp and dry-port admission. The attempted
optional momentum hook into that algorithm was removed after its failure; its
source is archived. `MassTangent` adds transactional readback of actual carrier
derivatives without changing the existing material solve.

## Confirmed pressure-grid inconsistency

The old locally refined chart has partial shared faces between cells with
unequal tangential centers. Its normal two-point pressure jump then includes a
spurious tangential hydrostatic gradient. A minimal affine-field negative control
reproduces this; a quiet level domain on that chart developed about 0.193 m/s.

The conforming compiler removes this particular inconsistency without rounding
away geometry or changing imported quantities:

| Fixture | Original cells | Conforming cells | Water remap error | Gas-mass remap error |
|---|---:|---:|---:|---:|
| Level | 21,696 | 41,376 | -6.56e-19 m³ | 8.03e-17 kg |
| Rotated | 37,334 | 65,338 | 7.18e-17 m³ | -1.67e-17 kg |

However, extending every cut plane across the chart is expensive and preserves
extremely small cells: minimum capacities approximately `5.00e-19`/`5.04e-26 m³`.
This is a bounded consistency control, **not** the final production geometry
strategy. It increases the coupled owner counts to 132,584/208,211.

## Integrated results and rejected approaches

- Small mixed-material fixtures advance three intervals with actual motion.
  Newton and Oseen reach the same admitted velocity/quantity state within
  `1e-10 m/s` and `1e-13 m³`.
- Full hydrostatic domain controls admit three intervals each. Peak speed in the
  first interval is about `2.78e-9`/`1.76e-9 m/s`, falling to
  `1.02e-10`/`6.43e-11 m/s` by the third. Cold quantity ledgers remain closed.
- **Both original numerical basins still fail their first coupled interval.**
  Final Newton runs fail line search after a poor linear correction; its relative
  Krylov residual remains about 1. No failed iterate is published.
- Relaxing actual extensive coefficients in an outer Picard iteration preserves
  mass balance, but its frozen-coefficient pressure subproblem still stagnates
  or fails the unchanged material certificate. It was rejected.
- Starting Newton at zero flux, using a momentum predictor, and changing numerical
  row scaling did not qualify the basins. Predictor initialization remains a
  numerical iterate only; it cannot bypass physical admission.
- Oseen's frozen-momentum Jacobian is much easier to precondition (initial level
  linear relative residual about .0018 instead of .99998), but the original level
  case exhausts 60 nonlinear iterations with failed physical residuals. The
  rotated case fails line search. Neither is an accepted basin trajectory.
- Three Oseen iterations followed by full Newton timed out at 180 seconds.
- A source-mass gravity-kick/transport variant improves the initial linear solve
  but still times out after 240 seconds in each original basin. **Not adopted**;
  its source/logs are retained and the ending-mass formulation restored.
- Isolated thin-layer columns, down to `1e-9 m`, pass both nonlinear methods.
  Thus a small cell alone does not establish the complete failure mechanism.

What is established: the old pressure-only graph is an ineffective preconditioner
for the full arrived-mass/momentum feedback in these connected basin problems;
freezing that feedback improves the linear algebra but does not solve the
nonlinear physical equations. What is **not** established: that the conservative
physical operator has no solution, or that this is fundamentally impossible to
solve efficiently. No such architectural impossibility is claimed.

Next work must address the coupled mass-feedback/pressure block and reconsider
the all-cut-planes geometry cost together, using these failing basins immediately.
Another velocity-only postprocess, tolerance relaxation or small initial-state
perturbation is not a demonstrated correction. Moving bodies and runtime polish
remain deferred; there is no reason to optimize the failed candidate as if it
were a qualified production solver.

## Validation and cost boundaries

- **3,387 focused controls** pass in normal and ASan/UBSan/leak-checked execution,
  including 24 compensated-reference dual controls. The full original material,
  pressure, continuation, gas-port and geometry coverage remains active.
- **842 exact-rational enclosure controls** pass.
- All six unchanged legacy pressure/quantity state files (three intervals in each
  orientation) match the previous checkpoint **byte for byte**.
- Full-domain quiet controls and both original coupled rejections were rerun
  against the final normal objects. They are reported separately from unit gates.
- The first leak-check attempt could not run under the tracing wrapper. It is
  retained; the complete native rerun with leak checking passes.

Final diagnostic coupled intervals cost roughly 3.7–16.7 **seconds**, with peak
process memory about 0.6–1.1 GiB for the quiet controls. Original first-step
failures take about 20/34 seconds. Some runs overlap compilation or other probes;
these are single diagnostic costs, **not** matched benchmark distributions or a
claimed speedup. Normal/sanitized configurations and raw resources are retained.
This candidate is nowhere near a usable game timestep budget.

The prior SDF/profile findings remain in `FLUID_MOMENTUM_GEOMETRY.md`: the sampled
SDF closes genuine openings and geometry queries are not the dominant measured
cost. No shared geometry service, SDF replacement or new fluid renderer was added.

## Scope and next acceptance

There is no native MK2 body reaction, moving geometry, free-surface visual,
spatially varying gravity, native cold-save, streaming, editor or export
qualification for this new backend. The rotated fixtures use the existing
fixed gravity-aligned chart; they do not prove varying radial-gravity dynamics.
No current game/SDK needs updating to this prototype.

Before unlocking: admit the original basins with compatible momentum, sustain the
previous required windows, independently qualify body impulses/displacement and
quantity/energy, then demonstrate practical runtime and native lifecycle behavior.
The user has not human-accepted this checkpoint.

[Commands, raw evidence and machine-readable results](evidence/fluid-dual-coupling-20261106/README.md).
