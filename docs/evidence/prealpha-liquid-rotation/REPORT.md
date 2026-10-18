# Rotated liquid baking and geometry correction

The two original rotated-liquid baking defects are corrected on Experimental. The investigation also demonstrated and corrected a related physical clipping defect. Solbert's EXP-007, EXP-008 and EXP-013 supplied useful geometric contracts and independent test requirements; their experimental solvers were not imported, executed or adopted as production implementations.

Starting checkpoint: `52800cedcdf47cab743ade96049eb5de1ab8aeb0`, human accepted for Pre-Alpha Passes 1 and 2. This subsequent correction requires its own human acceptance. Main remains at that accepted checkpoint. This is unnumbered Pre-Alpha work; M100 and an Alpha release have not started.

## Research applicability

The independent research remains at `/home/conner/Documents/JudasFluidResearch/fluid_lab/`. Exact consulted paths and hashes are in [research-geometry/RESEARCH_SOURCE_HASHES.json](research-geometry/RESEARCH_SOURCE_HASHES.json) and [research-storage/READ_SOURCE_HASHES.json](research-storage/READ_SOURCE_HASHES.json). Interpretation and preserved successes/failures are recorded in [the geometry comparison](research-geometry/NOTE.md) and [the storage comparison](research-storage/REPORT.md).

| Research | Applicable result | Boundary |
|---|---|---|
| EXP-007 geometry | Storage belongs to actual native geometry: `C(q) = volume(Omega intersect {height <= q})`. Capacity must be nonnegative, bounded and nondecreasing; physical aperture and wall traction require separate checks. | Frozen geometry and a separate research dynamics model; no native quaternion, arbitrary-gravity or runtime publication correction is provided. |
| EXP-008 oblique walls | Preserve oblique walls, nonlinear tetrahedral storage, wet-face geometry, normals and repeated heights. Avoid subtracting nearly equal geometric quantities. | Its horizontal-ceiling adapter bug was dry-at-equality; native Judas already treats that face as wet. That was not the production decreasing-knot cause. Its free-surface counterexample remains unresolved in research. |
| EXP-013 stable orbits | Exact moments and conservation do not prove wet-face accuracy. One preserved case has moment error `2.36e-17` but wet/full area errors approximately `0.04997` / `0.01498`. | Later accurate initial-face fits do not certify evolved histories, arbitrary geometry, native production dynamics or Navier–Stokes convergence. |

For a rigid change `x' = R x + t`, transform gravity-up and normals by `R` and radial centers by the same rigid change. Uniform heads shift by `(R up) dot t`; radial coordinates remain invariant. Physical volume, wet area and first moments must transform accordingly. Rotating geometry while holding gravity fixed is a different physical problem. A square chart can repartition when its tangent basis changes, so aggregate physical storage is the invariant, not arbitrary cell indices.

These contracts guided new native tests. They did not establish a ready-made fix. The independent exact-rational density derivation and probe in [research-storage](research-storage/) were created for this Judas correction; they are not copied research solver implementations.

## Demonstrated causes and corrections

### Bake and runtime used inconsistent frames

The original `normalization` fixture rotates the field and basins with `angleAxis(.8f, normalize(vec3(1,2,3)))`. Baking used raw authored float quaternions while initial runtime construction normalized them. The resulting local gravity-up discrepancy was `3.350149682e-7`, exceeding the unchanged physical-equivalence tolerance `1e-7`. The unshifted paired connection failed before any wave integration.

`PrepareLiquidBake` now uses the same normalized field and owner orientations as runtime. Box gravity regions now use the existing oriented volume instead of an axis-aligned volume. The two original fixtures use spherical regions, so the box discrepancy was an additional independently tested defect, not their asserted cause.

The regional path also retained raw slotless actor rotations and rebuilt fields/cache metadata with raw authored orientations. `WorldRegions` now matches the initial construction convention for actor state, field, oriented volume and the stored frame later consumed by origin reconstruction. Gravity precedence, field implementations, magnitude and equivalence tolerances are unchanged. There is no production origin-code change.

### Independently accumulated capacities disagreed by one ULP

The original `knots` fixture uses `quat(0,.6f,.8f,0)`. Its freshly written Surface1 cell 3 contains:

```
q = .9999999518695906   C = .24999999655046692
q = .9999999518695907   C = .2499999965504669
```

The decoder correctly rejects that decrease. Independently assembled interval CDFs and a separately reduced final tetrahedron volume disagreed in floating arithmetic. A long-double total alone repaired that exact endpoint but still failed other nearly coincident interior spans. Converting positive density back into signed power coefficients also retained cancellation failures. Both unsuccessful variants remain preserved.

For sorted tetrahedral heights `h0 <= h1 <= h2 <= h3` and volume `V`, the projected physical density is the nonnegative quadratic B-spline `3V/(h3-h0) N_0,2`. Its Bernstein controls can be restricted and added using nonnegative weights. The implementation integrates those densities once in height order:

```
deltaV = width * (d0 + d1 + d2) / 3
CDF controls = [C, C+width*d0/3, C+width*(d0+d1)/3, C+deltaV]
```

Adjacent endpoints and final capacity use that same cumulative prefix. Forward storage, bounded inverse and the quartic energy primitive use the same Bernstein representation. Separate positive density controls preserve the physical Jacobian when a real increment is smaller than one binary64 volume ULP. Extended intermediate evaluation rounds once on output. No generated volumes are sorted, clamped or repaired; no height knots are merged.

`JudasBasin` and `Surface1` formats are unchanged. Polynomial metadata is derived from physical tetrahedra on load. Valid existing resources reconstruct it normally; malformed original resources still fail strict decoding and must be rebaked. Non-opted M54 linear tables retain their existing path. Pressure equations, iteration/retry budgets, cadence, damping and thresholds are unchanged.

### Near-plane clipping could extrapolate beyond an edge

A wider independent test found that `cut` classified signed distances against `<=1e-12` but intersected the actual zero plane. Two positive distances could appear to cross. For example, approximately `+1.11e-16` and `+2.004e-11` produced a negative edge parameter and geometry outside the original tetrahedron. An independent clipped-volume oracle could then exceed the original tetrahedron's whole volume.

Convex clipping and its complete-containment shortcut now use the same actual zero half-space as intersection. Conservative broadphase and vertex-welding tolerances remain unchanged. Opposite cuts partition the original volume; moving the physical plane to the tolerance was rejected because complementary cuts would overlap.

The same mechanism was independently reproduced through actual `LiquidSurface::SetSolids`: a nonintersecting near-coplanar solid enlarged a wet aperture by `2.525e-6` to `3.056e-6 m2` across three rigid orientations. Correct physical-side classification fixes those exact cases. A separate true near-parallel intersection then exposed cancellation in binary64 dot-minus-offset distances: the rotated aperture differed from its independent physical oracle by `4.1676e-7 m2`. Promoting those signed distances and the crossing fraction before subtraction fixes the unchanged near-parallel controls. Intersections still use the same physical zero plane and final double geometry; no tolerance or test threshold changes. The test observes the aperture actually used by flow through public kinetic energy of one nonzero discharge, and compares it with independent polygon clipping; a ledger-only pass would not detect this error.

## Validation

Linux Release, GNU 15.2, existing engine settings and unchanged original fixture parameters. [ENVIRONMENT.json](ENVIRONMENT.json), build logs and per-test command/binary/source hashes accompany the raw results. No full production-suite run was needed for these bounded changes. Existing projects and baked assets were exercised without modifying or rebaking them to obtain a pass.

- Both original rotations now bake, strictly decode and publish their actual basin, surface and container through the ordinary resource/world/session path.
- The final rotation/aperture gate passes 419 checks over 37 orientations, including actual solid publication, true intersections, nonintersections and removal/cache invalidation.
- Native frame tests pass 40 checks and cover rigid and liquid-only owners, streamed publication, nested transforms, rebuilt gravity followed by a real 512 m origin shift, oriented noncubic zones with an outside negative control, and radial owner frames.
- Geometry tests use analytic oblique cuts, rigid covariance of volume/first moments/wet faces/normals, repeated and adjacent heights, exact wet-at-equality faces, strictly outside tetrahedra, complementary ownership, nonlinear inverse/energy and strict malformed-resource rejection.
- The same 512 seeded surfaces improve from `373/512` strict decodes to `512/512`, with no observed sampled capacity decrease, bound violation or negative area. Maximum partition discrepancy improves from `2.27188182e-7` to `2.44307685e-10 m3` on the same seeds.
- The separate analytic probe passes 14,988 checks. Maximum independent clipped-volume error is `1.78e-15 m3`, rigidly rotated capacity error `7.55e-15 m3`, inverse coordinate error `5.15e-14 m`.
- The independent Fraction probe checks 210 height packets, 5,895 exact primitive/derivative/energy identities and 1,179 positive restriction identities, including repeated and adjacent binary64 heights.
- M54 conservation, M55 calm/waves/disturbances/solids/rollback, repeated flat and radial drain/refill, actual M54/M55 resource/script/physics/render/reset/Stop applications, gravity selection, uniform scene construction, streaming, world persistence, origin assemblies and CharacterMotor regressions are exercised together.

[RESULT_INDEX.json](RESULT_INDEX.json) records 17 positive end-to-end invocations passing and both original extended failures preserved. The six-case provenance-checked liquid gate passes; all six Python gate negative-control tests pass. The focused final Release build reports no compiler warnings. [FINAL_FINGERPRINTS.json](FINAL_FINGERPRINTS.json) records 13 changed source/tool/current-document hashes and 17 built binary/archive hashes; its actual embedded identity is the pre-checkpoint dirty candidate. [verified-candidate/RESULTS.json](verified-candidate/RESULTS.json) confirms numerical source bytes stayed unchanged throughout those runs. The final results and source fingerprints distinguish corrected contracts from preserved failures. Earlier candidate results remain under their original names; they are not relabelled final.

## Preserved failures and numerical boundaries

**Both original full commands remain failed after successful baking.** Their inputs, timestep, controller settings and assertions are preserved. `--bake-only` isolates actual baking, strict decoding and runtime publication and explicitly makes no origin-continuity claim.

1. **Half-turn grid query.** After the 512 m shift, world-to-owner float rounding moves the original sample from cell 9 to cell 10. Local query position differs by about `1.726e-5 m`, but the disturbed finite-volume surface is discontinuous at that cell boundary. Surface-point/velocity differences are about `0.040015 m` / `0.016 m/s`. Authoritative wave bytes, energy, quantity and complete ledger remain unchanged. Fixing continuous sample representation is separate from baking; no snapping or averaging workaround is added.
2. **Arbitrary-rotation exact total assertion.** That case now passes shifting, physical loading, cold restoration and continuing flow. The out-of-chart request is correctly rejected atomically with its liquid-owner diagnostic. Its unchanged final assertion also compares the summed total across prior flow with exact `==`: `2.5199999999999996` versus `2.52 m3`. M54 accounting error is zero. The failed assertion remains recorded pending separate contract review; it is not silently converted to a tolerance check.
3. **Precision and authoring boundaries.** Positive area is the derivative of the pre-rounding geometric integral; quantized absolute capacities need not change at sub-ULP heights. No universal correctly rounded monotonicity or every-platform extended-precision guarantee is claimed. Radial coordinates retain existing bounded affine refinement. Existing geometric welding/sliver thresholds remain. A pre-existing inherited non-unit-scale authoring gap can reach bake preparation before runtime rejects the flattened liquid scale; it was reviewed and left outside this correction.
4. **Separate work.** FLU-001 legacy-PBF settling, curved-mesh continuation, dense-contact tails, legacy a106 save limitations and unrelated audit gaps remain unchanged. Windows/Android were not rerun for this correction; MSVC may not provide wider `long double`, so the nearly parallel distance accuracy is not certified there; Linux is the acceptance platform. Export code and public JudasJS APIs did not change. Existing application rendering uses offscreen llvmpipe for smoke checks, not hardware GPU performance acceptance.

An early new test sampled nextafter/midpoint positions in an order that is not sorted when adjacent representable heights collapse the midpoint. Only query positions were sorted; no storage values or physical knots were changed. Another new test requested a sub-512 m origin that correctly quantized to zero, then queried as though a shift occurred; it now requests an actual supported anchor. These setup failures are preserved. An intermediate refill invocation supplied an unsupported scene filter and executed zero checks; that is not credited. Correct no-argument invocations exercise both actual scenes and five drain/refill cycles each.

## Costs

Derived interval metadata increases from 32 to 56 bytes; the original 104-interval fixture adds 2,496 bytes. Temporary density-tree controls decrease from four to three long doubles per node. Construction retains `O(tetrahedra log(knots))` work.

A storage-only diagnostic took 1.57 s before and 1.60 s after over the same 512 seeds, but the latter performed 139 additional successful decodes. This is not a speedup or matched full-simulation benchmark. Raw ordinary/refill solve iterations, residuals and timings are retained for both existing scenes. The user-visible physics lab may remain active; application rendering is software. These measurements establish a bounded sanity check, not a new performance acceptance target.

Final existing-scene solve observations:

| Scene and phase | Mean solve ms | Maximum solve ms | Mean / maximum iterations | Maximum residual m3 |
|---|---:|---:|---:|---:|
| Flat ordinary | .477 | .570 | 6.40 / 13 | 1.593e-7 |
| Flat refill | .755 | 4.775 | 21.00 / 116 | 1.080e-7 |
| Radial ordinary | .739 | .818 | 6.67 / 8 | 5.758e-8 |
| Radial refill | .671 | 1.111 | 29.31 / 82 | 4.879e-8 |

The flat refill used three existing bounded retries across 110 measured refill steps; all five cycles passed. No retry policy changed. A separate small O1 matched `SetSolids` sanity test measures approximately `.166 ms/operation` with both double and extended face-distance evaluation; this is not production Release throughput or a speedup claim. Raw samples and exact commands are in [face-investigation/FINAL.json](face-investigation/FINAL.json).

## Reproduction

From the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUDAS_ENABLE_VALIDATION=ON -DJUDAS_BUILD_CHANNEL=Experimental
cmake --build build --target judas_liquid_rotation_tests judas_liquid_bake_frame_tests judas_origin_environment_tests judas_liquid_tests judas_liquid_surface_tests -j2
python3 scripts/prealpha_validate.py --build-dir build --group liquid
python3 -m unittest discover -s tests -p test_prealpha_validation.py -v
```

The preserved full failures remain explicitly runnable and return nonzero:

```sh
build/judas_origin_environment_tests --bake-regression normalization /tmp/new-liquid-normalization
build/judas_origin_environment_tests --bake-regression knots /tmp/new-liquid-knots
```

Use fresh output paths. The ordinary default origin test separately covers its unchanged rotated-uniform/radial fixtures and real fresh-process saves. Direct commands and environments for wider regressions are recorded by `run_final_combined.py` and its results. Standalone C++ seed/accuracy probes and the independent stdlib Fraction probe remain in the evidence directories with commands in their reports. They operate on Judas/evidence only, never by importing or running Solbert's directory.

## Research isolation and historical evidence

[RESEARCH_ISOLATION.json](RESEARCH_ISOLATION.json) compares all 18,794 regular files, totaling 2,346,505,943 bytes, against the pre-investigation size/mtime/mode inventory; access times are intentionally excluded. Selected cited source bytes are verified by SHA-256. The original metadata inventory is preserved compressed. No research file was written, imported or executed; EXP-016 was not restarted. This is metadata attestation for the whole tree and byte attestation for cited files, not a full-tree byte hash claim.

Original Pre-Alpha, FTFT/P1/JSGT, M24–M26/M32, M54/M55 and other milestone evidence is not rewritten. All new results live in this new directory. The baseline executable receipt honestly identifies its existing dirty-parent build provenance; it is not relabelled as a clean accepted-commit binary. Raw original bakes and original failure logs are retained.

## Human checklist

1. Open the existing Liquid Surface project and exercise flat and radial scenes: disturb water, scoop, carry, pour, drain and refill.
2. In a disposable copy, rotate a uniform-gravity region and its basin together; bake, save, Play and verify the paired connection works. Include a rotated noncubic gravity box.
3. Place a real solid against an oblique shared wet face; confirm displacement/blocking behaves sensibly without creating an aperture outside that face.
4. Reload and Stop/Play; verify conserved quantities and no stale liquid handles.
5. Review the two preserved full-origin failures as outstanding limits, not acceptance passes.

Human acceptance of this correction and main promotion remain pending.
