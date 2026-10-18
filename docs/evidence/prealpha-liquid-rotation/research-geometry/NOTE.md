# Read-only comparison: EXP-007 geometry and EXP-008 oblique walls

Research root: `/home/conner/Documents/JudasFluidResearch/fluid_lab/`.
Read-only scope: existing source, Markdown and JSON were read. No research script,
module, executable, build or test was run. No research-directory files were written.
Only this Judas cache directory was used for notes. Selected-source SHA-256 hashes
are in `RESEARCH_SOURCE_HASHES.json` (38 files). This is selected-artifact identity,
not a claim to have independently reproduced or certified all research results.

## What the research actually did

- `experiments/EXP-007-geometry` is a new Python discrete algorithm using frozen
  native geometry exports; it is explicitly NOT native `LiquidSurface::Step`.
  `DERIVATION.md:8-41` defines nonlinear storage from actual wet tetrahedral
  geometry, not a rectangular depth approximation. `geometry.py:45-68` constructs
  cubic pieces with a positive simplex recurrence and long-double accumulation.
- `experiments/EXP-008-oblique` is a different vector-basis research model on the
  tetrahedron `(0,0,0),(1,1,0),(0,1,1),(0,1,0)`, split at its centroid. It gives
  three divergence-free, normally conforming circulation modes with zero normal
  trace on physical walls, including `y=x+z`. Full wet physical inertia is
  retained, including off-diagonal terms; no engine solver patch was made.
- Frozen native witnesses call `BakeLiquidBasin`, `BakeLiquidSurface`,
  `LiquidCutGeometry`, `LiquidCapacity`, and `LiquidFaceArea` to export geometry.
  Python trajectory results do not establish behavior of the current live solver.

## Directly applicable mathematical contracts

For actual cell domain Omega, unit gravity-up n, head q, and an actual face Gamma:

    C(q) = integral_Omega 1[n dot x <= q] dx
    A(q) = integral_Gamma 1[n dot x <= q] dS
    0 <= C(q) <= |Omega|, C(q) is nondecreasing
    0 <= A(q) <= |Gamma|, A(q) is nondecreasing

The research keeps actual nonlinear cubic tetrahedral storage. Face wet areas
come from clipped native polygons. Their storage/area checks test original native
queries against a separately evaluated representation rather than checking a
curve against its own interpolation. Current production already uses a related
positive recurrence in `src/LiquidSurface.cpp` for exact per-cell cubics; importing
an experimental solver would not supply the missing bake/frame correction.

For a rigid proper rotation R and translation t, define

    Omega' = R Omega + t, n' = R n, q' = q + (R n) dot t.

Then `C'(q') = C(q)` and `A'(q') = A(q)` by change of variables. Outward normals
must transform as `R normal`; orientations cannot be selectively changed while
retaining old head coordinates. For radial equilibrium, rotating/translating both
geometry and gravity center preserves radius coordinates directly. These rigid
frame identities are deductions from the research's measure definitions, not a
rotated-world regression claimed by EXP-007/008. Their concrete experiments use
static plane gravity and fixed world-y head, so no native quaternion normalization
or runtime/bake frame round-trip was validated there.

`EXP-007-geometry/DERIVATION.md:82-103` also distinguishes actual integrated face
hydrostatics `H_f = g integral_(q_a)^(q_b) A_f(s) ds` from donor-aperture
approximations. One cannot claim wall correctness from global conservation or
an energy certificate alone. That warning is relevant to test design; it does
not identify the cause of either present bake failure.

## Actual preserved Judas failures compared with these contracts

1. Quaternion/frame disagreement:
   `docs/evidence/prealpha-pass2/integration/diagnostic/run-origin-4.log`
   measures selected-vs-baked local-up error `3.3501496820591159e-7`, exceeding the
   unchanged `1e-7` equality contract. `PrepareLiquidBake` in `src/LiquidTypes.cpp`
   constructs gravity and local coordinates from raw authored float quaternions;
   `RuntimeWorld.cpp` normalizes scene quaternions before gravity/pose construction.
   The research does not fix this path. Its contract requires one physically
   consistent geometry/gravity frame; a shared canonical construction is justified
   only after an exact runtime/bake reproduction establishes it.

2. Nonmonotonic serialized Surface1 capacity:
   Run 5/6 preserved basin assets contain exactly one decreasing knot each in
   surface cell 3 (column 3,row 0):

       q=.9999999518695906, v=.24999999655046692
       q=.9999999518695907, v=.2499999965504669
       dq=1.1102230246251565e-16, dv=-2.7755575615628914e-17

   Run 1/3/4 curves do not contain a decreasing serialized knot. Full parse results
   and exact asset hashes: `PRESERVED_BAKE_INSPECTION.json`. This is a one-ULP head
   separation and tiny negative floating capacity difference, not measured loss
   of macroscopic water. It still violates the actual monotone measure contract
   and current decoder correctly rejects it. Research supports deriving/evaluating
   stable monotone storage from the physical tetrahedra, not weakening the decoder
   or post hoc sorting/clamping capacity values. This read-only inspection alone
   does not establish which arithmetic operation first creates the inconsistency.

Independent current source observation: the bake builds authored box gravity zones
as `BoxVolume` while runtime uses `OrientedGravityVolume`. The two preserved failing
fixtures use spherical zones, so this is NOT their demonstrated cause. It is a
separate frame-consistency risk worth covering if the shared resolver construction
is corrected.

## Useful preserved successes and failures, with limits

EXP-007:
- `GEOMETRY_WITH_MEMBRANE_CHECKS.json`: seven native geometries; storage, ownership,
  actual aperture, component partition, finite secant closure errors are small
  (e.g. original tetra storage 1.39e-17 and aperture 0; box 2.22e-16).
- `runs/G2_initial/summary.json`: 30 cases/35 accepted steps, 3723 rejected attempts;
  maximum mass error 1.7657356262898438e-11 and energy-identity error
  1.2991380190708096e-16. Finite numerical observations, NOT exact conservation.
- `runs/G1_counterexample/result.json`: zero donor-aperture force falsely accepts
  rest despite true face tractions +/-343/48000. G2 distinguishes hydrostatics.
- `runs/membrane/G2_trace_counterexample.json`: zero open aperture but positive
  whole-half-cell inertia allows wall-normal velocity through a wet membrane.
  G3 partitions below-sill ownership to stationary slots, only for a horizontal
  sill in a vertical wall, not arbitrary oblique walls.
- The earlier asymmetric `sill_tetra` crop was an ineffective control because its
  face was entirely above the sill. The later original-tetra membrane is the
  actual negative control. Research retains that distinction.

EXP-008:
- `STORAGE_ADAPTER_REVISION.md`: first adapter lacked private capacity polynomials;
  replacement reconstructs cubics from actual native tetrahedra, validates native
  volume/clipping, and does not interpolate sampled bake curves.
- `HORIZONTAL_FACE_REVISION.md`: V2 falsely reported a horizontal ceiling dry at
  exactly full height (area error .5); V3 treats collapsed point-height
  distribution as wet for q>=height, matching native `LiquidFaceArea`.
  Current production `LiquidFaceArea` already uses <=0 as wet at equality.
  This research adapter repair is NOT the current decreasing-knot mechanism.
- `BASIS_CHECKS_V3.json`: wall trace 4.44e-16, internal normal jump 7.04e-16;
  native aperture error 1.11e-16. Full mass off-diagonal .5948829547 at head .9.
- `runs/H1_geometry_v4/summary.json`: 15 cases/24 accepted steps, 525 rejected
  attempts; mass error 7.475488760996796e-12, energy identity 1.9543763207976944e-17.
- `LAYER_MEAN_REVISION.md`: initial reaction certificate failed by up to1.0025e-4
  due nearly equal moment subtraction/endpoint derivative normalization. Stable
  integrated swept means fixed the certificate while all q,z,pi states stayed
  identical. This supports avoiding subtractive cancellation and distinguishing
  physical state from diagnostics; it is not a production bake algorithm repair.
- `FREE_SURFACE_COUNTEREXAMPLE.json`: donor conservative volume rate differs from
  current clipped geometric free-surface rate by .014997503647221647 despite
  energy and wall trace checks passing. This remains an unresolved physical
  limitation; do not import the research model as a proven fluid replacement.

Neither experiment proves arbitrary moving/radial geometry coupling, continuum
convergence, no-slip Navier-Stokes, universal coupled root existence or dry-state
Newton convergence. They do not justify changing current time steps, tolerances,
iteration policies, capacity ownership, or accepted research.

## Suggested focused production proof

Use the exact preserved two authored quaternions, plus identity and non-axis
rotations with a non-symmetric native tetrahedron. Separately test:
- raw-vs-runtime normalized construction with unchanged equality tolerance;
- monotonic C, endpoint values and direct physical clipped-volume agreement;
- A at all original triangle height knots, just below/at/above repeated heights;
- rotating geometry+gravity together versus rotating only geometry (negative);
- basin Save/Decode, runtime connection, waves and conservative transfers;
- radial center/frame consistency without claiming radial continuum accuracy.

Do not merge Python trajectory code, rerun experiments in place, restart EXP-016,
or label original research equation gates as production engine validation.
