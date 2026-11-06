# Fluid momentum, face-metric energy and geometry cost — 6 November 2026

The conservative pressure/quantity prototype still has an incomplete motion
integrator. This investigation identifies a concrete, previously unaccounted
kinetic-energy term, adds a checked conservative momentum carrier, and measures
where remaining CPU time goes. **It does not fix the integrated basin velocities,
unlock fluids or introduce a shared engine geometry service.**

Parent checkpoint: `8ed450f057c68653d5595827110a055d77d47329` on Experimental.
The containing commit and evidence source hashes identify this checkpoint.
The slow correctness reference at `574fa22f` and the prior pressure-stagnation
fixtures remain unchanged.

## What actually produces the fast faces

The existing pressure law uses the material in the face-connected columns at
the **start** of an interval to compute an inertial mass `m0`. It solves

`u1 = u0 + dt*g + dt*A*(pL-pR)/m0`.

Material transport then changes the water/gas inventory. The next interval
reconstructs a new column mass `m1`, while retaining `u1` unchanged. There is
no matching conservative momentum transfer for that mass change.

On the first interval of the actual rotated fixture, face 16836 starts with
mass `1.3411045074e-10 kg` and ends with `1.1175865106e-7 kg`, a factor of about
833.33. A pressure jump of `-11876.65 Pa` produces speed `3299.23 m/s` using the
initial gas inertia. Subsequent intervals retain that speed as water arrives.
The source acceleration law itself is reproduced to about `6e-16` relative
error; this is not an unexplained extra impulse or a graphics artefact.

An independent diagnostic separates the quantities:

`K_endpoint - K_start = J * (u0+u1)/2 + (m1-m0)*u1^2/2`.

Summed over the actual rotated face field, first-interval pressure/gravity work
is `41.8509 J`, but endpoint kinetic energy in the reconstructed pressure metric
is `57.2290 J`. The additional mass-metric term is `15.3781 J`. Its counterpart
is absent from the current motion integrator. The level fixture shows the same
mechanism: `75.3350 J` source-metric work and a further `9.46854 J` metric term.

These are **face-metric kinetic diagnostics**, not a complete fluid-plus-gas-
internal-plus-gravitational-energy audit. Missing momentum transport is a
confirmed defect; these measurements do not prove that it explains every fast
mode. In particular, gas can accelerate rapidly under the current large
pressure jump before the water arrives. Simply shrinking the velocity when
mass changes would neither establish the required advective impulse nor keep
pressure flux and material transport consistent.

## Implemented momentum building block

`tools/prototypes/fluid_volume_grid/CutMassMomentum.{h,cpp}` solves the implicit
upwind momentum balance using the **actual accepted extensive material mass
flux**, including the existing gas-only boundary policy. The prepared material
operator exposes those signed mass fluxes through
`CutSelectiveTransport::InteriorMassFlux`; it does not infer mass from wet area.

For each cell, the unknown is ending momentum divided by geometric capacity.
The outgoing coefficient is mass flux divided by ending mass density. This
retains compensated ending mass without rounding it into a binary64 capacity.
All coefficients are rebuilt from the supplied mass state and carriers.

The result independently audits:

- Starting/ending mass and all material carriers.
- Every original local momentum row, its L1 residual, and global impulse balance.
- Incoming/outgoing momentum and kinetic energy at gas boundaries.
- The backward-Euler/upwind work identity, including unsteady and mixing losses.
- The work attributable to the actual mass-balance defect, without changing mass
  or energy to conceal it.
- Transactional rejection: failed inputs/solves do not overwrite prior output.

The kinetic loss is the computed sum of mass-weighted squared velocity
changes/differences from the discrete upwind equations. There is **no damping
parameter, speed clamp or energy rescaling**. This first-order transport does
have numerical mixing, and is not claimed to preserve angular momentum or
sharp material interfaces. Zero-mass velocity owners are explicitly unsupported.

Both the fast solver and the retained compensated reference operator are
exercised. Controls include analytic two-cell mixing, a separately inverted
cyclic carrier, uniform flow with changing mass, both flux signs, imposed
impulses, open boundaries, mass scales, invalid inputs and unowned mass gain.
The original authored geometry is also used with its actual accepted mass
fluxes and independent uniform/nonuniform prescribed velocity fields. Those
are real advection controls, **not a newly coupled pressure trajectory**.
A separate two-owner arrival control uses the observed mass ratio and impulse:
its ending velocity is about 3.96 m/s rather than cloning 3,299 m/s, with impulse
and discrete work balanced. Its incoming carrier is prescribed. It is not a
basin correction and cannot be used while retaining the old pressure flux.

## Why it is not wired into pressure yet

The current pressure velocity belongs to an aperture-connected half-column,
whereas the accepted quantity flux belongs to a spatial cell face. They are
not interchangeable owners. The original fixtures have 1,228 (level) and
1,652 (rotated) half-cell sides with multiple open faces. Collapsing those
independent samples to one cell velocity and interpolating back would introduce
unqualified mixing and change pressure work. A corrected local-frame coverage
audit finds no overlapping or missing connected column coverage in these tests.
An earlier audit using rounded global midpoints falsely reported coverage errors;
that harness failure is retained and is not reported as an engine defect.

Existing `HalfDualVolumeMomentum` and `GridGeometricMomentum` research already
contains useful ideas: owned half states, phase-aware transfer splits, midpoint
continuity and explicit mixing/work ledgers. It is not absent infrastructure.
However, it targets a different regular-grid state, fixed material densities,
and different donor/roundoff contracts. Its explicit donor-capacity rejection
cannot simply be bypassed to accept these large implicit cut-cell transfers.

The next integration must provide conservative, geometrically owned **velocity-
dual mass transfers**, then use their arrived inertia and momentum in the
pressure/transport solve. It must satisfy the same material flux and work
balances, rather than updating momentum after pressure while retaining an
incompatible pressure flux. The new carrier supplies the checked transport
operator, but not that missing geometric/coupling map. Moving bodies remain
postponed until this fixed-geometry integration is qualified.

## Runtime profile and rejected optimization

A three-interval rotated run with GCC `-O3 -pg` attributes sampled self time
approximately as follows. These percentages are diagnostic, not an
uninstrumented latency benchmark:

| Operation | Sampled self time |
|---|---:|
| Pressure graph solve | 24.39% |
| Fast transport corrections | 16.82% |
| Transport solve/check orchestration | 9.26% |
| Pressure preconditioner construction | 9.07% |
| Filtered residual certificates | 7.94% |
| Directed transport preparation | 4.35% |
| Exact residual fallback rows | 3.02% |

Separately instrumented geometry work takes about **3.5–9.2 ms per interval**,
plus **1.3–2.4 ms** for admission-time dry-port geometry, compared with whole
intervals costing roughly **1–2 seconds** in this early window. Timing
instrumentation preserves all six pressure-state dumps byte for byte.
The current chart already caches conservative cell capacities, face apertures
and connectivity; this is not a fresh collision-mesh query on every Newton row.

An owner-local immutable pressure graph/factor cache was prototyped, retaining
all original pressure-row checks and grounded-component validation. Five
alternating process pairs, each advancing the same three intervals in each
orientation, produced byte-identical physical states:

| Fixture | Previous median | Cache median | Previous / cache worst |
|---|---:|---:|---:|
| Level | 973.742 ms | 968.138 ms | 1048.537 / 982.214 ms |
| Rotated | 2005.807 ms | 2021.255 ms | 2175.422 / 2105.670 ms |

There are 15 interval samples per backend/orientation, but only five independent
process pairs. Nearest-rank p95 equals the maximum at this sample size. The
median result is essentially unchanged and reverses sign with orientation.
**The cache is not adopted.** Its preparation moved work between profiler
categories without a convincing overall gain. Source and raw receipts remain.
No runtime speedup is claimed for this checkpoint.

## Signed distances and a possible shared geometry contract

A bounded binary64, trilinearly sampled box-union distance prototype was tested
on the same authored static solids. It passes its affine-distance control.
At the existing grid spacings, however:

| Fixture | Distance-grid bytes | Analytic / sampled query time | Open centres classified solid | Maximum sampled distance error |
|---|---:|---:|---:|---:|
| Level | 165,000 | 1.02 / 0.74 ms | 0 | 0.0619 m |
| Rotated | 315,208 | 1.81 / 1.32 ms | 366 | 0.0500 m |

These are single-run point-query diagnostics, not general SDF performance
benchmarks or integrated conservative solves. A separate exact thin-gap control
shows why resolution matters: a real 1 mm positive gap is reconstructed as
solid when both surrounding samples are negative. The actual fixtures also
retain positive distances down to roughly `3.34e-10 m`. Increasing arithmetic
precision does not recover geometry that was never sampled.

Conclusion: **a uniform pre-baked SDF is not a safe replacement for conserved
volumes/apertures, and will not materially accelerate the measured hot solve**.
It can still be useful as a bounded broadphase/approximate-query accelerator,
with certified error bounds and exact fallback near ambiguous features.
These six-box basin workloads do not establish whether complex terrain/mesh
queries would benefit materially from an SDF accelerator. That needs its own
representative consumer benchmark. A richer cached conservative velocity-dual geometry is relevant to the missing
momentum map, but has not yet demonstrated a runtime benefit.

Before proposing a shared service, actual source was checked:

- `src/CollisionGeometry.h` already provides double-precision point/segment
  geometry queries; triangle meshes deliberately have no invented solid inside.
- `src/particles/CollisionSnapshot.h` already owns immutable captures, geometry
  revisions, asset-local BVHs, local frames and explicit unsupported/exhausted
  query statuses. Reuse these ownership ideas rather than create a rival cache.
- `src/VolumeTerrainGeometry.h` has an editable implicit density field. It is
  not automatically a metrically exact signed distance or a conservative fluid
  volume representation. Terrain edits require geometry revision invalidation.

A future small interface should distinguish optional distance/normal queries
(with error bounds and inside/solid semantics) from **conservative measures**
(cell volumes, oriented face areas, topology, moments and stable surface IDs).
Both would refer to the same immutable, revisioned local-frame source, with
explicit support boundaries and transactional replacement. It should consume
existing collision assets/terrain, not replace the renderer, MK2 or scene system.
Ragdoll/static-query and terrain consumers should be evaluated separately;
this investigation does not claim they are already accelerated or compatible.
No engine-wide interface was implemented.

## Qualification and scope

**3,293 focused checks** pass in normal and ASan/UBSan/leak-checked builds,
including the original 3,179, 14 existing graph controls and 50 new momentum
checks in each backend. **842 exact rational enclosure checks** and five
negative diagnostic invocations also pass. All 12 original-geometry fast/
reference velocity fields agree within `8.21e-24 m/s` (the gate is `1e-10 m/s`).
The sanitizer reference run initially hit its 180-second time budget; that run
is retained. Its complete rerun used 600 seconds and passed. No solver accuracy
or physical interval changed.

The evidence directory records final test totals, sanitizer results, all 12
fast/reference original-geometry velocity comparisons, raw profile/timing logs,
failed harness runs, the rejected cache source, state hashes and reproduction.
The previous 120+120 pressure/quantity qualification is preserved; this turn
adds early-interval momentum diagnostics and does **not** claim 120 intervals
of coupled momentum evolution.

Main, the Alpha lock, native engine, SDK, release channels, games and independent
research are unchanged. The full motion/energy correction, moving bodies,
arbitrary-gravity momentum, native saves/streaming and portable platform
qualification remain open. There is no new playable fluid backend to accept yet.

[Evidence and commands](evidence/fluid-momentum-geometry-20261106/README.md).
