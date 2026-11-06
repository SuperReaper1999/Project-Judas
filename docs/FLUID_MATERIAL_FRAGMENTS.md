# Local material fragments — 6 November 2026

**Follow-up:** [exact retiling and physical-aperture corrections](FLUID_MATERIAL_RETILING.md) retain the failed original gates and identify the pending finite-resolution policy. The measurements below describe this earlier checkpoint.

**Isolated numerical prototype. Not a qualified runtime backend or a fluid unlock.**
The original authored basins still fail the complete-interval gate. This checkpoint
preserves useful geometry/event work and a reproducible rejection of the current
box-fragment continuation strategy. No main, Basic Water, production fluid,
renderer, SDK, release-channel or independent-research implementation changes.

Source parent: `c9f39d46959ce800d06b9f628623843cc808da8d`.
[Commands, exact sources and raw evidence](evidence/fluid-material-fragments-20261106/README.md).
[Previous water/finite-air checkpoint](FLUID_WATER_AIR_MODEL.md).

## Implemented responsibilities

All modules are private prototypes under `tools/prototypes/fluid_volume_grid`.
They are not new scene components or JudasJS APIs.

- `WaterMaterialFragments` imports water volume and independent air mass into
  cell-local rectangular pieces. Positive native minority phases survive below
  an ordinary coordinate ULP. The small import reconciliation is explicitly
  recorded, not silently treated as exact source equality. Adjacency requires
  actual positive-area contact through existing physical apertures.
- `WaterFragmentGeometry` separates connected water ownership, air compartments,
  boundary integration rectangles and normal-velocity controls. Water connects
  locally through wet faces. Pieces of the **same existing air carrier** may
  share a pressure node only when locally connected. Distinct carriers retain
  finite hydraulic links; neither a matching label across water nor a microscopic
  opening is permission to union pressures.
- Compatible oriented surface pieces with the same pressure owner share a
  normal-velocity control. Their summed area, first moment and integrated gravity
  potential are retained. The declared 64-piece quiet surface has six velocity
  controls and one gas node, while retaining all 64 contact rectangles.
- `WaterInterfaceEvents` performs conservative local recession/growth, follows
  a translating film through newly occupied space and actual cell openings,
  splits local boxes, and assigns the largest gas parcel last. Water quantity
  and air mass are audited independently; an unrepresentable crossing does not
  replace the published state.
- `WaterFragmentFlux` pairs liquid transfers and recovers conservative fluxes.
  Multiple surface rectangles receive their shared normal motion by exact-area
  weighting. An event endpoint can consume its measured geometric capacity only
  if the **unchanged full momentum/work audit** admits that correction. A failed
  audit is not an acceptable clipping policy.
- `WaterFragmentAdvance` locates finite-feature endpoints rather than repeatedly
  publishing smaller prefixes approaching an event forever. Complete requested
  intervals publish atomically. Search work has an explicit budget. Internal
  accepted prefixes in failure logs are **unpublished diagnostics**, not completed
  material timesteps. After same-interval numerical refinement is exhausted,
  a raw physical-audit failure rejects instead of pretending that arbitrarily
  smaller time intervals will fix it.

The transport still uses an explicit recession-then-growth split. It does not
claim a topology-complete unsplit interface method, a moving-solid geometric
conservation law, or finite-resolution regridding.

## Local numerical corrections and deliberate changes

Thin fragments exposed two avoidable round trips through binary64: boundary
area/centroid data and source-predictor flux. They now retain compensated data
through local integration. Ordinary old geometric patches retain their existing
binary64 authoring representation. The global sparse pressure search/factorization
continues to use binary64; no all-domain fraction or arbitrary-precision solver
has been introduced.

The compatible local mass uses compensated coefficients. The constrained operator
is formed from its generating Gram vectors rather than cancellation of four large
mass entries. Binary64 Cholesky/refinement remains the first attempt; a local
compensated QR fallback can avoid squaring the generating vectors' conditioning.
It must pass the **same 1e-24 original-row gate**. That fallback still fails some
new nonconvex thin owners; it is not a claim to support arbitrary aspect ratios.
The arbitrary 32-patch implementation rejection was removed; this is not a claim
that unbounded dense local factors are affordable.

A numerical pressure seed overload reuses head/air-pressure guesses only within
matching geometry. It does not reuse old physical quantities or admit nonfinite,
nonpositive or dimensionally incompatible guesses. Source geometry, momentum and
all material quantities remain owned by the current input.

These are private numerical type/operator changes. The tiny numerical trajectory
of the old private water path is not promised bit-identical. Existing acceptance
tolerances, physical expectations and negative controls are preserved; native MK2
and exported-game APIs are unchanged.

## Qualified controls and failed acceptance gates

Normal and address/undefined-behaviour/leak-checked controls each pass **3,532
checks in 34 suites**: the previous 3,506 checks plus 26 new checks. The additions
cover translating film, real/closed apertures, thin geometry and its rounded
negative control, independent air ownership, malformed input, surface control
reduction and body-excluding hydrostatics. The latter integrates `(0, 9810, 0) N`
for one cubic metre displaced at water density 1000 kg/m³.

The separate reference mode passes 50 checks; the independent dyadic residual
oracle passes 842. The **existing** 240-interval flow completes with cold water/air
ledgers and adjacent-step total energy nonincrease. Final water columns are
approximately `(0.503672184160266, 0.496327815839734) m³`, with energy
`2452.637754125 J`, unchanged to the displayed baseline precision.

**The new fragment path does not inherit that sustained-flow pass.** Its own
240-interval test rejects during interval two, preserving three failed completion,
quantity-continuation and energy-continuation checks. They are uncompleted gates,
not independent proof of energy creation. A rejected whole interval leaves output
unchanged. The authored level and rotated basin tests both exit nonzero.

The level gate produces a newly wetted L-shaped owner of about `7.055e-12 m³`
that fails the original local inverse-row criterion. The rotated gate rejects
its original work identity: the relative residual is about `1.098e-10` against
the unchanged `1e-10` threshold. This is a close numerical failure, not evidence
of a large measured energy explosion. Their full requested 1/60 intervals do not publish.
The earlier single-prism representation failure is therefore bypassed for some
crossings, but the basins are **not fixed**.

The final serial, trace-disabled failed-interval measurements on Ryzen 5 5600,
GNU C++ 15.2, O3/no fast-math are:

| Original fixture | Geometry total | Pressure total | Transport total | Wall time | Peak RSS |
| --- | ---: | ---: | ---: | ---: | ---: |
| Level | 312 ms | 23.404 s | 3.469 s | 27.212 s | 199 MiB |
| Rotated | 267 ms | 67.490 s | 1.171 s | 68.982 s | 313 MiB |

One sample per orientation, not a latency distribution. Geometry totals include
failed preparation. Timing excludes initial fixture cooking; RSS includes that
fixture. The level coordinator records one internal 8.138 microsecond prefix
before rejecting the complete interval; the rotated coordinator records none.
Neither output is published. The serialized measured receipts retain the exact
values. These are unacceptable failed-step costs, not achieved simulation rates.

## Demonstrated representation blocker

A frozen two-column trace makes the remaining mechanism small and inspectable.
Within the second requested interval, successive internally accepted endpoints
produce 11, 23, 50, 118 and 267 pieces while time approaches 9.09 ms. The first
unaggregated experiment also produced 273 boundary velocity controls for a single
water owner after about 5 ms. Surface-control aggregation repairs that bookkeeping
error, but **does not bound material box-history growth**.

Independent axis-aligned recession/growth continually cuts nonconvex thin sheets
and new corner parcels. Those cuts create more interfaces and air compartments,
which create further cut events. New local inertia and pressure/transport
corrections must resolve increasingly disparate geometric scales. Both geometric
completion and the original physical audit matter: forcing completion can violate
momentum/work, while stopping just before each event creates a shrinking-time
cascade. More precision and naive time halving do not resolve this representation
problem.

Preserved rejected variants include binary64-only boundary geometry, direct local
Cholesky, QR without original-row refinement, immediate-prefix halving, strict
minority pressure rows, positive-air-mass line-search restrictions, and conservative
small-water-cell agglomeration. The latter retains volume but changes the pressure
resolution; it was **not adopted**, because neither the simple flow nor complete
original basins qualified. Combined trials remained red or reached their wall-time
limits. The attempted 100-digit frozen pressure reference stalled under its chosen
domain-preserving Newton search; that is **inconclusive**, not proof that the
physical equations have no root or that arbitrary precision is a runtime remedy.

## Cost and next engineering decision

The raw evidence includes initial preparation/pressure costs, complete failed
attempt wall time, pressure/geometry/transport phase totals, work counts and RSS.
The final serial receipt uses the exact same original fixtures, settings and
unmodified physical acceptance checks. These are **failed-step diagnostic costs**,
not a usable throughput benchmark. Earlier exploratory timings overlap compiler
jobs and are labelled accordingly. No production speedup or sustained timestep
rate is claimed.

Do not optimize the current fragment-history scheme into a runtime backend.
The next substantial correction should replace accumulated box-subtraction history
with a bounded cell-local interface representation/regridding step. Preserve each
connected material's quantity and air mass, retain real contact/opening topology,
and explicitly define the resolution/continuation policy for unresolved sheets and
bubbles. Internal quadrature pieces must remain separate from physical owners and
velocity/pressure unknowns. Simply deleting small pieces, unioning trapped-air
pressures, reducing body coupling, clamping energy or adding damping is not an
acceptable substitute.

That is a representation/accuracy-policy decision, not another pressure-tolerance
adjustment. Prove the two-column interval-two case first, then the original basins,
before moving bodies or runtime optimization. Moving native MK2 coupling, sustained
original basins, radial fields, runtime saves, streaming, rendering and export of
this backend remain unqualified. The production Alpha fluid lock stays in place.

## Review checklist

- Verify the green control receipts and the explicitly red flow/basin exits;
  distinguish internal diagnostic prefixes from published complete intervals.
- Review the bounded-interface/regridding recommendation and choose its spatial
  resolution/continuation policy before another implementation attempt.
- Keep the Alpha lock in place. This checkpoint has no new native editor demo
  or gameplay backend for human acceptance; the production paths are unchanged.
