# Read-only research comparison: nonlinear storage and wet-face accuracy

Research inspected on 2026-10-18. This note is an interpretation of preserved sources, not a new numerical run. No research Python/C++ was executed or imported. No research file was changed. EXP-016 was not opened or restarted. Exact consulted file hashes are in `READ_SOURCE_HASHES.json`; selected existing evidence is copied as JSON values into `EXTRACTED_EVIDENCE.json`.

## What is applicable

**EXP-007 defines geometry-owned nonlinear storage, not linear depth:**

`C_i(q) = |Omega_i intersect {height <= q}|`, `v_i = C_i(q_i)`, `0 <= v_i <= c_i = |Omega_i|`.

Its gravitational energy is `U_i(v)=g[q C_i(q)-integral(qmin,q) C_i(s) ds]`, with `q=C_i^-1(v)` and `U_i'=g q`. Monotonicity is therefore both a geometric requirement and a hypothesis of convex-energy arguments. Stable finite changes integrate cubic derivatives instead of subtracting nearly equal values. Different cell volumes are not a substitute for different heads. Sources: EXP-007 `DERIVATION.md` sections 1–2 and `geometry.py` lines 29–40, 51–72.

**EXP-008 retains the actual clipped tetrahedron storage and oblique wall geometry.** Its frozen tetrahedron has vertices `(0,0,0),(1,1,0),(0,1,1),(0,1,0)`, with physical oblique wall `y=x+z`. It constructs a normally conforming three-mode velocity space on four tetrahedra; zero exterior normal trace and continuous internal trace are distinct from conservation. This is a static plane-gravity, inviscid slip-wall research discretization, not the production `LiquidSurface::Step`, radial gravity, moving walls, or a general Navier–Stokes solution.

The original research storage adapter failed because a native `BakeLiquidBasin` export contained sampled capacities without private `capacityPolynomials`. Its retained replacement evaluates the actual tetrahedra with the positive simplex recurrence and validates against independent native capacity/clipping queries. That does not justify interpolating sampled data or treating sampled capacities as authoritative geometry. Production `src/LiquidSurface.cpp:53–108` already uses a positive local cubic simplex recurrence; importing the recurrence is not itself a fix.

**EXP-008 preserves two relevant numerical failures.**

1. The first face adapter had aperture error `0.5` on an exactly full horizontal ceiling. Repeated/collapsed height knots were treated as dry at equality. Version 3 uses `q >= height` for a point-height distribution. This matters for a 2D face of finite area; a 3D nondegenerate tetrahedron capacity remains continuous at its maximum. This is not proof of the current capacity-knot defect.
2. A full-space momentum representation subtracted nearly equal moments or used the saturated upper-endpoint CDF derivative, yielding a non-normalized mean. Version 4 uses normalized swept-layer means via piecewise three-point Gauss derivative integration. Original physical momentum failures up to `1.0025e-4` remain preserved. This is a research representation correction; it does not establish a defect in current Judas baking.

See `STORAGE_ADAPTER_REVISION.md`, `HORIZONTAL_FACE_REVISION.md`, `LAYER_MEAN_REVISION.md` and `BASIS_CHECKS_V3.json`. The latter records native aperture error `1.1102230246251565e-16`, wall-normal trace error `4.440892098500626e-16` and internal normal jump error `7.044101371529554e-16` for its bounded case.

## EXP-013: what capacity accuracy does not prove

For a native, normally conforming velocity, its orbit coarea observables are

`Q_f = sum_crossings D_if omega_j sigma/r chi_minus`,

`A_wet = |Q_f| / |u_i dot n_f|`,

`A_full = sum_crossings omega_j sigma/(r |u_i dot n_f|)`.

Zero normal speed cannot use this quotient; no minimum-speed floor is introduced. Unequal heads use the donor-side wet trace. Evolved fronts require inverse-history trace evaluation, not resetting to original heads.

The preserved `axis_-0.0001_entropy` run matches all required moments to `2.3635607360183997e-17`, physical full mass to `2.220446049250313e-15` and height to `1.3877787807814457e-17`, but its maximum wet/full face area errors are `0.04996953943370428` / `0.01497738601915033`. A volume, conservation or energy pass cannot certify aperture correctness.

EXP-013 D040 notes that five moments per cell `(v, p_x, p_y, p_z, trace(S))` suffice for the full affine physical mass matrix:

`M_if,ih = D_if D_ih/(9 C_i^2) [trace(S) - (o_if+o_ih) dot p + (o_if dot o_ih) v]`.

It separately fits 20 such physical moments, four capacities and twelve full/wet initial-face rows. The retained first-step face audit reaches wet/full area errors around `1.6e-14/3.3e-14` (eta12) and `1.6e-13/3.3e-13` (eta24). These are finite initial-cut cubature results. The prototype explicitly rejects history input; evolved mass-plus-face initialization and temporal convergence remain unexecuted. `SUMMARY.json` records no accepted original `dt=.03` third physical step. Bounded interruptions are computational limits, not infeasibility proofs. `SOLVER_IDENTITY_CORRECTION.json` documents force trials mistakenly labelled entropy which actually ran sparse fits; these must not be relabelled successful entropy evidence.

## Exact relation to the two preserved Judas failures

**Orientation failure:** the original capacity/surface bakes succeed, but an unchanged, unshifted session reports `spill basins must share one supported equilibrium field`. This is a frame/representation-consistency failure before any dynamic-surface integration. Research says to retain one physical geometry/field; it contains no demonstrated quaternion normalization fix. Trace the bake-time versus runtime local gravity/pose conversion directly, rather than retuning `Equivalent` tolerance.

**Nonmonotonic capacity knots:** an authored world rejects its freshly baked `Surface1` resource with `nonmonotonic capacity knots`. A sum of physical clipped-volume CDFs must be nondecreasing; this is a geometric/storage numerical contradiction, not a conservation-ledger or nonlinear pressure-iteration failure. Investigate native geometry and interval coefficient assembly with independent capacity and face checks. The already existing production recurrence and interval tree need their own numerical proof/reproduction.

Reading-only hypotheses, not established root causes: production `tetraPolynomial` promotes polynomial arithmetic to `long double` after some subtractions are already evaluated in `double`; research `geometry.py` promotes the interval origin before those subtractions. Production geometric clipping also classifies using a tolerance but forms intersections from raw signed distances. Either could matter near oblique vertex cuts, but only actual Judas reproductions can establish causality.

## Rotation-equivalence test obligations (derived here)

For a proper rotation `R` and translation `t`, transform geometry by `x'=R x+t`, normals by `n'=R n`, uniform gravity by `up'=R up`, and radial centre by `c'=R c+t`.

- Radial coordinate is invariant: `|x'-c'|=|x-c|`.
- Uniform coordinate shifts by `k=up' dot t`: `up' dot x'=up dot x+k`.
- Therefore `C'(q+k)=C(q)`, wet/full face areas are unchanged, and the first moment transforms as `m'=R m+t C`.
- For a local owner chart, uniform world/local levels differ by the owner translation's gravity projection. Compare levels in the same chart.
- Rotating the cavity while keeping world gravity fixed legitimately changes capacities; do not demand the previous curve in that physically different setup.
- A square chart's preferred tangent-axis selection can change the partition under rotation. Compare the same physical clipped geometry or invariant aggregate quantities before assuming per-grid-index equality.

For corrected Judas baking, verify capacity endpoints/monotonicity, inverse-level consistency, actual wet-face area, transformed surface normal, first moments and saved resource decoding separately. Include nearly repeated and exactly repeated heights, oblique faces, perturbations on both sides of a vertex height, and the original preserved failures. Ledger agreement alone is insufficient.

## Limits and recommendations

No experimental solver implementation is needed to correct a demonstrated bake/frame bug. The useful transfer is precise geometric contracts and independent adversarial oracles. Keep production M54 volume ownership and M55 numerical update unchanged unless a separate demonstrated mechanism requires alteration. No result above establishes arbitrary radial or moving-geometry fluid solver convergence, wet-face convergence for evolved orbit fronts, or a general physically complete solver.
