# Rotated Surface1 capacity investigation

Chronology: the first sections record the storage-only correction and its preserved results. The final combined candidate additionally repairs the clipping mechanism discovered by the new physical oracle. **FINAL_COMBINED_RESULTS.json and the final section describe the current source; earlier measurements retain their original scope.**

This subtask inspected Judas only; it did not access, run or edit the independent research directory. The research-storage agent independently reviewed the mathematical derivation and final implementation.

## Exact failure

The preserved `bake-knots/Assets/2.judasbasin` has a one-ULP decrease in cell 3: at heights `0.999999951869590564` and `0.999999951869590675`, volume decreases from `0.249999996550466924` to `0.249999996550466896` m³. The old tree accumulated independent cubic CDF restrictions in long double, while the final capacity used a separate binary64 tetra-volume reduction. A total-only long-double correction repairs this exact endpoint but fails other interior, nearly coincident-height intervals.

The unchanged decoder correctly refuses decreasing capacity. Neither input geometry nor the M54 ownership ledger caused this decrease.

## Generic correction: storage-only stage

An affine coordinate over uniform tetrahedral volume has density `rho(q)=3V/(h3-h0) N_{0,2}(q)` for sorted vertex heights. The quadratic B-spline has nonnegative Bernstein controls on each nonzero span, including repeated heights. Range-add and restrict those density controls using positive Bernstein subdivision, then integrate every sorted interval in one cumulative order:

`dV = width*(d0+d1+d2)/3`.

The cumulative endpoints and final capacity use this single prefix. Cubic CDF controls are `[C, C+w*d0/3, C+w*(d0+d1)/3, C+dV]`. Retain the physical density controls separately so sub-volume-ULP increments do not erase the physical Jacobian. Evaluate CDF, density and its quartic energy primitive in extended intermediate precision and round once. The inverse uses the same CDF evaluator.

All geometric vertex heights, nonlinear capacity, original tolerances, solver budgets, storage format and physical geometry are retained. No volume clamping, repaired knots, volume sorting or global flattening is used. The existing height sorting is unchanged. This is not a new dynamics solver.

Production files: `src/LiquidGeometry.h`, `src/LiquidGeometry.cpp`, `src/LiquidSurface.cpp`. Derived `capacityPolynomials` metadata now contains four cubic volume controls and three quadratic physical-area controls; it is not serialized. Valid old assets reconstruct this derived data normally. The original invalid artifact remains invalid and is not silently repaired; it must be rebaked.

## Meaningful controls: storage-only stage

- Same 512 seeds, 4x4 cells, 128 almost-Y up directions plus 384 general directions: original strict decoder passes 373; total-only candidate passes 449; final passes all 512.
- Seven samples per span, including adjacent representable heights: final has no observed CDF decrease, bounds violation or negative area. The rejected density-to-power-basis variant still had 2705 sampled decreases, 1045 bounds violations and 2569 negative derivatives.
- Four analytic tetrahedral height packets exercise one/three and two/two repeats plus four distinct heights. 128 rigidly rotated boxes check physical equivalence. Independent clipping, geometric full-volume reduction, inverse and exact cubic Simpson-integration controls total 14988 checks. Errors are recorded in RESULTS.json (CDF/geometry errors at several e-15 m³, inverse error 5.15e-14 m).
- The 512 generic cell partitions have an existing maximum geometric partition discrepancy of 2.27188182044812947e-7 m³, identical before/after and below the original 1e-5 m³ bake tolerance. The new polynomial does not alter clipping/simplification geometry; separate per-stored-geometry oracle errors are e-15 m³.

## Cost and numerical scope

Coefficient RAM increases 24 bytes per interval (32 to 56); the exact preserved 16-cell fixture has 104 intervals, +2496 bytes. Construction tree controls fall from four to three long doubles per node. Algorithmic work remains O(tetrahedra log(knots)). A single diagnostic 512-case bake/read/sample run took 1.57 s before versus 1.60 s after, but final performs 139 additional successful strict decodes. This is not a whole-engine performance acceptance measurement.

Area is the derivative of the pre-rounding geometry integral. Absolute CDF controls are rounded; sub-ULP interval volume can quantize to a flat CDF while physical area remains positive. Therefore no bit-exact derivative/finite-difference identity is claimed for unresolvable volume changes. Linux extended evaluation produced no observed decreases; this does not establish every-platform correctly-rounded monotonicity.

## Reproduction

From the repository root, with C++17 and GLM installed:

```
c++ -std=c++17 -O2 -ffunction-sections -fdata-sections -Isrc .cache/prealpha-liquid-rotation/capacity-investigation/runtime-final.cpp src/LiquidGeometry.cpp -Wl,--gc-sections -o /tmp/judas-storage-final
/tmp/judas-storage-final
c++ -std=c++17 -O2 -ffunction-sections -fdata-sections -Isrc .cache/prealpha-liquid-rotation/capacity-investigation/accuracy-final.cpp src/LiquidGeometry.cpp -Wl,--gc-sections -o /tmp/judas-storage-accuracy
/tmp/judas-storage-accuracy
```

The original source, total-only patch, rejected power-basis and Bernstein prototypes, exact original artifact parsing, raw seed logs and outputs remain in this cache directory. Original source/header copies in `baseline-include` allow rebuilding the original probe without changing production:

```
c++ -std=c++17 -O2 -ffunction-sections -fdata-sections -I.cache/prealpha-liquid-rotation/capacity-investigation/baseline-include -Isrc .cache/prealpha-liquid-rotation/capacity-investigation/runtime-baseline.cpp .cache/prealpha-liquid-rotation/capacity-investigation/baseline-include/LiquidGeometry.cpp -Wl,--gc-sections -o /tmp/judas-storage-original
/tmp/judas-storage-original
```

Root performs authoritative CMake/Linux regressions and public-API lifecycle/frame tests. This subtask did not run a shared build, change tests, edit physics thresholds or claim human/platform acceptance.

## Follow-up: independent clipping oracle exposed a real geometry defect

The root's unchanged 37-orientation test initially failed orientation 4 (a 1e-10 rad rigid rotation around normalized `(1,3,2)`). Cell 2 at head `0.749999999979955256` had CDF storage `0.187499999985801746` m³ but direct clipping returned `0.187500568279940549` m³. The CDF was correct; this was a distinct, pre-existing clipping defect, not a reason to loosen the oracle threshold.

`cut` classified edges against `s<=1e-12` but intersected the actual zero plane using `s/(s-t)`. A physical edge with signed heights `+1.11e-16` and `+2.004e-11` was classified as a crossing despite both endpoints being outside. Its negative interpolation parameter extrapolated beyond the edge and inflated individual tetrahedra by 2.769e-7 m³. A simpler independent tetrahedron showed direct clipped volume exceeding total physical volume by 6.66667e-7 m³. The `LiquidCutGeometry` complete fast path separately retained a strictly outside 1.33333e-7 m³ tetrahedron because all positive distances were below the tolerance.

The generic correction makes true inside/edge classifications and complete containment use the same actual zero plane as intersections. Conservative broadphase rejection tolerances, vertex welding/deduplication, all test tolerances and all solver budgets remain unchanged. A shifted tolerance plane was rejected because it expands physical storage and makes opposing half-spaces overlap. Independent geometry review agreed with the zero-plane correction.

The production correction affects only initial/edge classifications in `cut` and complete containment in `LiquidCutGeometry`. `LiquidIntersection` and `LiquidSubtractSolids` already used zero for their complete/inside truths; those lines were not changed. `tests/LiquidRotationTests.cpp` gains four controls: clipped volume cannot exceed total physical volume, complementary half-spaces preserve volume, an entirely outside thin tetrahedron is rejected, and that rejection is independent of world up.

Final standalone public test: **386 checks / 37 orientations / 0 failures**. Maximum direct storage discrepancy is now 5.06026e-12 m³, inverse discrepancy 2.72229e-12 m³ and aggregate rotated storage discrepancy 3.00671e-11 m³, all within the original unchanged test bounds.

`CUT_FOLLOWUP_RESULTS.json` contains the current source hashes. `RESULTS.before-cut.json` preserves the earlier storage-only candidate, whose measured source/probe fingerprints should not be described as the final combined geometry candidate. Original failures and cache variants are retained in `clip-repro.*`, `LiquidGeometry.before-cut.cpp`, `LiquidGeometry.exact-cut*.cpp`, `minimal-cut-before.txt`, `minimal-cut-after.txt`, `rotation-exact-cut.txt` and `rotation-final-cut.txt`.


## Final combined-source refresh

No earlier logs or expected hashes were replaced. `runtime-final-cut.txt` reruns the same 512 seeded surfaces against the current corrected geometry and storage implementation: **512/512 strict decodes, no observed capacity decreases, bounds violations or negative physical areas**. Maximum whole-partition geometric discrepancy is now **2.44307685193234647e-10 m³**, versus 2.27188182044812947e-7 m³ before the physical-plane correction on the same seeds.

`accuracy-final-cut.txt` repeats all **14,988** analytic, repeated-height, rigid rotation, independent clipping, inverse and energy-primitive checks against the combined source. The errors remain: mass 4.44e-16 m³; independent clip 1.78e-15 m³; analytic 5.33e-15 m³; rotated capacity 7.55e-15 m³; inverse coordinate 5.15e-14 m; integral 8.88e-16 m³.

The earlier 1.57/1.60-second timing diagnostic is strictly the earlier storage-only comparison, not a final combined-engine benchmark. No current timing was collected while the root's authoritative CMake build ran. Memory coefficients and asymptotic work bounds are unchanged from the storage-only stage. Root owns final native build/regression and runtime performance results.
