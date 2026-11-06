# Material reconstruction and physical apertures — 6 November 2026

**This does not complete the bounded-resolution fluid backend.** Exact local
retiling and two boundary-integration corrections are implemented and qualified
in the private prototype. Sustained fragment flow and both authored basins still
reject. The operator has now approved conservative subgrid storage with approximate
below-resolution geometry; that replacement is not yet qualified.
Main, the production fluid path, Basic Water and the Alpha lock are unchanged.

Source parent: `3d03de09347d1c4fd2b06460f4048206e0d6d5b9`.
[Exact sources, commands and raw evidence](evidence/fluid-material-retiling-20261106/README.md).
[Previous fragment investigation](FLUID_MATERIAL_FRAGMENTS.md).

## Adopted implementation

`WaterMaterialRegrid` reconstructs connected regions with exact local sweep
unions. Interval union removes T-junction subdivision history that pairwise box
merging cannot remove. Three deterministic sweep directions choose the least
fragmented canonical result. Water quantity, geometric first moments, air mass
and actual occupied support are audited. Real sub-coordinate-ULP films and
sealed pockets are retained; different air carriers are not pressure-unioned.

The geometric search and coverage audits have explicit work budgets. Adjacency
work is checked before entering quadratic local face-discovery loops. Failure
leaves the published state untouched. This bounds attempted geometric work,
**not the complexity of the physical interface**: unsupported complexity rejects.
It is not yet the finite-resolution reconstruction requested for a runtime.

Connected-cell water mean velocity is the same average already consumed by the
pressure compiler. Retiling makes that existing averaging explicit, checks
momentum, rejects kinetic-energy gain and records passive mixing loss per unit
water density. Same-carrier air mass is assigned by exact volume, with the largest
piece last; isothermal mixing potential must not increase. There is no damping
coefficient, energy clamp, lost-material policy or geometric tolerance snapping.
`WaterFragmentAdvance` invokes this reconstruction after conservative momentum
transport and before its complete-interval quantity ledger and atomic publication.

`WaterGasAperture` corrects a verified partition dependency. The old compiler
applied a rectangular-pipe resistance to **each integration rectangle** of an
opening. Splitting an unchanged square opening into 16 strips introduced fictitious
pipe walls and substantially reduced its conductance. The new compiler rebuilds
the actual opening before measuring resistance. A rectangle matches the analytic
existing rectangular-throat function regardless of its input subdivisions. Separate
openings remain separate parallel channels. Nonrectangular openings use a declared
approximation of canonical rectangular channels in parallel; no hole is filled.
Normal length uses the connected gas control's span, not an arbitrary integration
box's thickness. Pressure owners remain separate and flow remains finite.

Shared-water rectangles between the same oriented pair of water controls now
share one pressure-head trace. Their areas, first moments and gravity potentials
are integrated, rather than creating a new pressure unknown for every quadrature
piece. The same principle was already applied to free/solid normal controls.
This is a declared lowest-order pressure basis change, not a bit-identical private
trajectory promise. Its hydrostatic, momentum, energy and quantity gates remain.

All changes are under `tools/prototypes/fluid_volume_grid`. They add no native
engine backend, component, editor option, JudasJS API or exported SDK dependency.

## Results

Normal and native address/undefined-behaviour/leak-checked controls each pass
**3,559 checks in 36 suites**. The additional 27 checks cover exact T-junction
union, first moments, passive momentum averaging, real thin gaps, microscopic
sealed pockets, work-budget rollback, physical opening resistance, its independent
old-pipe negative control, shared pressure traces and end-to-end finite air exchange.

The 32-piece air-opening integration fixture compiles to two pressure nodes and
one actual opening. Its coupled pressure solution agrees with the independent
finite gas-pocket solve, retains a pressure difference after one interval, and
completes reconstruction with a cold air-mass ledger. It does not instantaneously
equalize pressures. The body-excluding pool still integrates `(0,9810,0) N`.

The retained 240-interval **old water-volume path** still completes with cold
water/air ledgers and adjacent-step total-energy nonincrease. Separate reference
and independent exact-residual controls pass 50 and 842 checks. These results do
not qualify the new fragment coordinator. Its own 240-interval flow still fails
in interval two, retaining the original three continuation/completion failures.
The failing path also ran with sanitizers without sanitizer findings.

Both original complete-basin gates remain explicitly nonzero. The level case
now reaches an unrepresentable hydroacoustic factor after one unpublished
8.138-microsecond prefix. The rotated case still fails the unchanged local work
identity. Neither complete 1/60-second result publishes. No moving MK2 body,
radial field, runtime save, streaming or rendered backend acceptance is claimed.

## What the reconstruction experiment established

Redundant subdivision really can be removed: the non-slicing ten-piece two-phase
fixture becomes two unchanged regions, and a 128-piece partition becomes two.
But the actual flowing case is not mostly redundant subdivision. One developing
state changes only from **50 pieces to 46**. Earlier accepted internal states
change 11 to 10 and 23 to 22. The independent axis-aligned surface sweeps create
new thin sheets/corner parcels; exact set-preserving retiling cannot remove those
real features of the current discrete geometry.

Thus this exact approach does not satisfy the intended bound on geometric
complexity. Continuing to narrow time intervals or increase pressure precision
cannot supply a geometric resolution policy.

## Rejected trials

All trial sources/logs are preserved separately from adopted code:

- Wide-seam, locally bounded pressure-owner agglomeration reused the existing
  `CutConnectedOwners` policy on phase fragments. It failed three unchanged
  air-ownership/geometry controls and did not complete sustained flow. It is not
  in the adopted path; the failures were not changed to new expectations.
- Pressure accuracy tied to the shrinking event-volume uncertainty was tested.
  It did not complete the crossing. Alone it requested residuals around `1e-29 m³`;
  combined with the rejected owner-coarsening trial it reached `1e-59 m³`, beyond
  the observed arithmetic floor. Neither variant is adopted.
- Searching until adjacent binary64 times rather than stopping at 64 epsilon
  widths did not complete the failing event. It is not adopted.
- Reusing authoritative geometric endpoints instead of coordinate round trips
  was tested. A 512-case old/new probe found no remnant failure in either path,
  so no bug-fix claim is made and the speculative change is not adopted.

## Measured cost

Five isolated O3/no-fast-math samples per regular partition, Ryzen 5 5600:

| Integration pieces | Exact output pieces | Median retiling | Maximum sample |
| --- | ---: | ---: | ---: |
| 2 | 2 | 0.00325 ms | 0.02657 ms |
| 32 | 2 | 0.0783 ms | 0.1217 ms |
| 128 | 2 | 0.774 ms | 0.852 ms |
| 512 | 2 | 10.012 ms | 11.401 ms |

These are geometric controls, not successful fluid timesteps. They include face
construction and all reconstruction audits, with no geometry cache. Five samples
do not establish tail-latency guarantees. Quadratic discovery is still expensive
for a heavily fragmented cell. Large-basin rejected-attempt timing is retained
as shared-host qualification diagnostics, not a matched speedup claim; compiler
jobs overlapped some runs. Full-step performance remains unusable/unqualified.

## Approved model decision and next work

The next step must distinguish conserved material quantity from **resolved
geometric detail**. A practical fixed-resolution bulk representation needs an
explicit policy for unresolved sheets, droplets and air pockets. Their water
quantity, air mass, momentum and finite connection laws must remain recorded;
they must not disappear, silently join pressure owners, or receive an energy clamp.
The geometry of those features below resolution would be approximate.

The operator explicitly approved that change to the support/accuracy contract on
6 November 2026, subsequently clarifying that believable game physics is the goal.
This checkpoint does **not** implement the replacement. Next, prove the interval-two column case with
the full energy/quantity accounting first, then the original basins, then native
moving-body coupling and practical pressure/transport costs. The Alpha lock stays.
