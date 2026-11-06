# Faster conservative transport — 6 November 2026

**This is a faster numerical prototype, not a game-ready fluid backend.** Both
original basins still pass the first frozen-body interval. Sustained evolution
of the rotated basin still fails. No native runtime, SDK, Alpha lock or main
change is part of this checkpoint.

## Preserved reference and method

The correctness reference is pinned at
`574fa22f5a6d93205c69db24da593628d41797e0`. Its complete prototype sources,
original basin fixture and source checksums are retained in the small reference
archive alongside this evidence. The comparison builds that archive separately;
it does not accidentally compile current headers into the reference. Historical
reports and tests remain unchanged.

The faster implementation changes numerical work, not physical inputs:

- `CutFastDirectedTransport` reuses the reference's positive condensed operator.
  Contiguous reduced rows and binary64 Gauss–Seidel corrections replace map
  traversal and compensated arithmetic inside every correction sweep.
  Compensated refinement, original-row certification and an explicit reference
  fallback remain. It never publishes an uncertified approximate result.
- `CutFastResidual` retains the exact expansion order with in-place small
  storage and unbounded dynamic spill. A conservative floating filter handles
  ordinary rows; ambiguous rows use the exact expansion. Positive-supersolution
  construction and error certification are distinct proof objectives.
- A cheap residual schedules certification; it cannot authorize publication.
  Final certification is always attempted before numerical exhaustion.
- Accepted line-search transport results are reused on the next Newton
  iteration rather than solved again with identical inputs.
- Tiny compensated value operations are available inline without changing
  their arithmetic sequence. There is no fast-math, native-CPU-only build flag,
  lower-resolution mesh, erased microscopic aperture or relaxed final tolerance.

The first reference profile attributed 62.71 s across both basins to transport
solves, including 39.09 s in original residual/barrier enclosures and 18.21 s in
reduced sweeps. Matrix preparation was 1.80 s. This justified targeting audits
and correction arithmetic rather than geometry alone. Instrumentation phases
overlap; do not add whole-function and nested timings together.

## Results and qualification

The machine-readable matched comparison and raw logs are in the linked evidence.

| Fixture | Reference median | Candidate median | Reduction factor | Candidate p95/worst |
|---|---:|---:|---:|---:|
| Level | 34.572 s | 5.985 s | 5.78× | 6.012 s |
| Rotated | 35.774 s | 6.048 s | 5.91× | 6.057 s |

Peak process RSS: reference 155,120 KiB; candidate 157,032 KiB. Maximum field
differences were `3.58e-16 Pa` pressure and `1.49e-24 m³` water. All three
output dumps for each backend/orientation were byte-identical.

Three samples per orientation/backend alternate execution order. With only
three samples, nearest-rank p95 equals the maximum; this is not a reliable
long-tail distribution. These are CPU numerical costs, excluding rendering,
editor work and moving geometry. They are not runtime frame-rate measurements.

Both fixtures retain their exact authored geometry, source quantities, finite
gas ports, gravity and native float timestep `0.01666666753590107 s`. Every
fixture must pass local closure, phase positivity, gas-only boundary and global
quantity checks. Field comparisons report pressure, water, gas and interval
flux differences; each backend's repeated output dumps must be byte-identical.
The candidate is not claimed to be bit-identical to the reference.

The active focused suites pass **3,135 checks**, including **2,511** continuation
controls and **325** fast-transport/enclosure controls, with the existing **299**
checks retained. Address/undefined-behaviour/leak sanitizer runs pass the same
checks outside the tracing wrapper. An independent Python exact-dyadic oracle
checks **842** filtered residual enclosures. Exact rational arithmetic is used
only by this oracle, never by the candidate solver.

The filter uses standard round-to-nearest arithmetic, gradual underflow and no
unsafe reassociation. Linux GCC/x86-64 is qualified here. Changed floating-point
modes, other platforms and arbitrary extreme input ranges are not qualified.
No new native platform dependency is introduced.

## Continuation: progress and remaining failure

The optional continuation path keeps compensated water and gas quantities,
pressure, interior face velocity and gas-port velocity on the same frozen
chart. It validates a geometry mutation stamp and material-law identity;
changed geometry/law, aliased output and solid translation reject explicitly.
The stamp is a diagnostic, not a cryptographic identity or a moving-world cache.

Two defects were exposed and corrected during continuation:

1. Pressure admission allows a finite numerical volume defect; interface
   geometry required a tighter roundoff-level identity. Reconstructing both
   physical phase volumes directly therefore rejected the next interval.
   Geometry now retains the smaller independent phase and uses its geometric
   complement for the larger phase, only within the already accepted volume
   budget. Physical water quantity and gas mass are never changed or clipped.
2. A concentration error certificate alone did not ensure a sufficiently tight
   global ledger over repeated intervals. Physical transport now also requires
   a quantity-defect budget before publication. No corrective mass is inserted.
   Simply forcing a uniform, much tighter fraction tolerance failed the basin;
   that rejected experiment is preserved.

Closed water/gas channels run **500 intervals each** (5 simulated seconds).
The water channel remains hydrostatic. A separate known uniform throughflow
control proves that velocity is retained rather than reset to rest.

The level authored basin advances four intervals with quantity admission.
The rotated basin advances three, then exhausts the pressure line search in
its fourth. An earlier variant aborted at the third interval after an
uncertifiable Newton trial. Rejecting that trial and backtracking the numerical
pressure correction improves this, but does **not** solve sustained evolution.
The actual physical timestep is unchanged. No failed interval is published.
Raw failures, the rejected transport system and their diagnostic sources remain.
A slow-path replay from the identical retained third-step state also stagnates
at normalized volume residual about `4.956e10`, with pressure correction factors
falling to about `1e-14`. That combined reproduction hit its 180-second process
budget (exit 124); the slow retry did not complete. This is evidence of shared
nonlinear difficulty, not proof that no other formulation or globalization can
succeed. Diagnostic replay timings are excluded from the matched statistics.

A captured rejected trial has transported concentrations of order `1e13` in
microscopic cells. The resulting global forward-error bound is too loose for
the requested absolute concentration budget even though the equation residual
is tiny. That explains this rejection; it does not prove that relaxing the
budget would produce valid water. Backtracking eventually also stalls, so no
claim of a general numerical or architectural solution is made.

## Remaining boundary and next work

Despite the improvement, first intervals still cost seconds and the difficult
continuation can be slower. **The practical performance goal has not been met.**
The reference and candidate both remain outside the native engine/SDK.

Continuation is an acoustic/transport experiment. It retains normal velocity
but does not yet implement conservative momentum advection, moving-solid
remapping, pressure-work-adjoint MK2 reactions, wet queries, cold-save/streaming
registration or final water rendering. It is not a complete fluid integrator.
Body motion remains explicitly unsupported. No human gameplay validation is
claimed and there is no new playable backend to unlock.

Next work must address the rotated continuation's nonlinear pressure/transport
admissible domain and momentum evolution before body coupling. It should use
this retained failure, rather than optimize only first intervals or declare
that the faster linear algebra alone fixes the representation. A different
runtime discretization remains an option; these measurements do not prove one
is necessary or sufficient.

[Evidence, matched measurements and reproduction](evidence/fluid-fast-transport-20261106/README.md).
