# Vertical fluid geometry — unqualified Experimental candidate

**2026-11-01. Uncommitted. Not ready for checkpoint, publication or fluid unlock.**
Based on `90a97547e7148f2ba179fb5d6a547d1e94a21188`; main and the Alpha fluid lock
are unchanged. This is a partial engineering result, not a completed qualification.

**Subsequent cause review:** [Settling contract investigation](FLUID_SETTLING_CAUSE_REVIEW.md)
shows that even the converged calm-water model exceeds the existing four-second
speed comparison. The original failures remain preserved; no gate was relaxed.

## Confirmed mechanisms

The accepted reconstruction replaces every column's intervals by its minimum and
maximum height. Two separate layers therefore become a fictitious filled column.
The original hydrostatics test even explicitly expected full immersion inside a
metre-wide air gap. The original test source is preserved in the evidence.

A second defect appears after simply retaining separate layers: wet admission is
a Boolean for the whole sample, while occupancy is averaged over surrounding
columns. An arbitrarily small wet overlap can admit a much larger surrounding
water fraction. Rotating a tall body across a connected cavity exposed this:
prototypes returned approximately 0.49 immersion instead of the analytic 1/3.

Literal unions of particle cubes are not a usable replacement: ordinary PBF
particles do not tile space. That experiment removed neutral support from a
recorded settled pool. Widening gap closure did not resolve the integration
problem and was not adopted. Connectivity-only experiments also did not solve
connected cavities and did not preserve the settling gate.

## Current candidate

- Reconstruct distinct vertical layers, retaining unresolved internal disorder
  gaps up to one maximum particle spacing. Actual neighbouring rows within the
  existing two-spacing lateral halo can repair a nearest-bin sampling hole;
  neither external free surface is extended.
- If an exterior resolved gap intersects the sample, subdivide its height at the
  reconstructed layer boundaries. Each slice receives its own wet/dry decision,
  solid cross-section and area-weighted occupancy. Integrate the slice lengths.
  A wet slice cannot admit an air slice, and a dry slice cannot erase a wet slice.
- Internal air at the current slice can supply closer dry evidence. An ordinary
  lower free surface still votes dry exactly once in the occupancy mean.
- Preserve the accepted full-query solid exclusion and motion-donor traversal.
  Continuous columns keep their accepted envelope path. No force clamp, artificial
  damping, warm-up delay, mass setting, timestep, solver budget or controller change.
- A continuity proof avoids allocating/sorting layer vectors when the raw interval
  chain already covers the relevant envelope. Opaque profiles are prepared lazily
  only when exterior geometry requires slicing. Failed proofs use full preparation;
  there is no physics-quality cap or skipped simulation.

State is still binary32; geometry, interval arithmetic and integration use binary64.
No new extended-precision arithmetic, pressure solver or research code is used.
The new helper is private; no JudasJS API or persistence format changes.

## Correctness evidence

| Check | Observed result |
|---|---|
| New vertical geometry target | **261/261**: 9 interval controls and 252 field/body checks, including sweeps, partial occupancy, boxes, spheres, compound bodies, swimming queries, rotated/translated geometry and a 4 km local offset |
| Same new checks against pinned baseline implementation | **162 failures**, retained as a negative control |
| Existing hydrostatics | **124/124**; original 120-check source retained. Three assertions that demanded fictitious water were corrected; three independent partial-layer motion checks and one air-pocket assertion were added |
| Existing lateral dry-hole/solid support | **109/109** |
| Production fluid contract | **FAIL: 3 of 8,021 checks**, all unchanged four-second settling checks; see below. Two separately classified existing diagnostic failures also remain |
| Player integration | **5,661 checks, no mandatory failures**; four existing micro-rest diagnostic failures remain |
| Supported stationary container cases | **4,340 checks, no failures**; not a universal moving-container claim |
| Cavity impact | **7 cases pass** |
| Actual pool / radial demo integration | **24/24 and 25/25**, including swimming and cup interaction |
| Lifecycle / Basic Water / Alpha policy | **21/21, 56/56 and 26/26** |
| Build/validation registration | **18 tests pass**; registration module **9 tests, 2 environment skips** |
| Normal rendered flat/radial scenes | 240 steps each, successful exits; **all 480 recorded physical harness rows exactly match the prior accepted numerical-source run** |

The continuity optimization preserves all 240 non-timing light-body trace rows
exactly against the unoptimized sliced implementation. Static geometry and frozen
query forces/fractions agree. The broader combined integration run used that
unoptimized sliced implementation; final optimized static/policy tests and all
three affected settling cases were rerun separately. No repeated full-suite pass
or fresh Windows/Android qualification is claimed.

### Remaining qualification failures

The production test source, inputs, timestep and threshold are unchanged. Its
three bodies must have `abs(finalV) < 0.5 m/s` after four seconds:

| Case | Accepted baseline final vertical speed | Sliced candidate final vertical speed |
|---|---:|---:|
| `half_020` | 0.113925 m/s | 0.632400 m/s — fails |
| `half_025` | 0.087859 m/s | 0.611067 m/s — fails |
| `light_80kg` | 0.433004 m/s | 0.679569 m/s — fails |

The static geometry repair is demonstrated; compatibility with the existing
settling contract is not. This is a behavioural regression against that gate,
not proof by itself of an energy instability. The changed buoyancy/drag admission
and resulting motion need further investigation. Do not raise the threshold,
add damping or classify these as passing. A separate 12-second connectivity
prototype diagnostic is preserved but is **not** qualification of this final
height-sliced candidate.

## CPU cost

Ryzen 5 5600, GNU 15.2 Release. Frozen identical inputs; three alternating-order
pairs, 20 observations per arm/pair plus three warmups. This is **one 64-sample
body's hydrostatic query**, not a whole physics step, rendered frame or GPU cost.
Desktop background activity was not isolated. Raw rows and commands are retained.

| Workload | Accepted baseline median / p95 / worst | Candidate median / p95 / worst |
|---|---|---|
| Ordinary wet | 3.212 / 3.285 / 4.291 ms | 3.623 / 3.832 / 4.347 ms |
| Dry | 0.129 / 0.142 / 0.146 ms | 0.139 / 0.152 / 0.243 ms |
| Separate layers | 3.055 / 3.082 / 3.087 ms | 8.244 / 8.336 / 8.637 ms |
| Connected cavity | 4.697 / 4.742 / 4.752 ms | 7.668 / 7.690 / 7.741 ms |

The baseline is wrong on the last two geometries: it reports fraction 1 and
588.60 N instead of fraction 1/3 and 196.20 N. Their timings are correctness/cost
comparisons, **not** a matched-quality speedup claim. Ordinary wet output remains
exactly fraction 1 and 2116.07421875 N in both arms; its median cost rises ~13%.

A separate matched comparison of the two candidate implementations measured
ordinary wet median 5.282 -> 3.657 ms with the continuity proof. Layered median
7.329 -> 8.297 ms and cavity 7.531 -> 7.746 ms instead became slower. The proof is
not a universal speedup. Geometry-dependent slicing and its worst-case scaling
remain performance work; no 60 Hz guarantee is made.

## Scope and disposition

Resolved in the tested candidate: separated layers, resolved enclosed pockets,
rotation-sensitive wet admission and partial volumes crossing water and air.
Unresolved: the three settling regressions, quiet-water diagnostics, performance
for complicated layered fields, sub-resolution voids and exact arbitrary topology.
The finite-resolution reconstruction is not a conserved-volume model.

Active-particle cold saves, regional ownership and live origin shifts retain their
existing explicit restrictions. Basic Water, conserved liquids, independent
research, the accepted scenes, force equations and unrelated projects are unchanged.
Water's visual appearance has not been redesigned. No candidate package or release
has been published, no branch promoted, and no human validation is claimed.

## Files and reproduction

Numerical changes: `src/FluidHydrostatics.cpp`, `src/FluidWetFootprint.h`, new private
`src/FluidColumnLayers.h`. Contract comments: `src/FluidHydrostatics.h`.
Tests: new `tests/FluidVerticalSupportTests.cpp`, corrected
`tests/FluidHydrostaticsTests.cpp`; registration in `CMakeLists.txt` and
`tools/validation/suites.json`.

```sh
cmake -S . -B build
cmake --build build --target judas_fluid_vertical_support_tests judas_fluid_hydrostatics_tests judas_fluid_wet_support_tests judas_production_fluid_tests -j2
./build/judas_fluid_vertical_support_tests
./build/judas_fluid_hydrostatics_tests
./build/judas_fluid_wet_support_tests
# Expected to fail the three unchanged settling checks in this candidate:
./build/judas_production_fluid_tests --output /tmp/vertical-fluid-production-fresh
```

Use a fresh output directory. The active local build is `.cache/2d-stage1-build`.
Raw results, failed variants and final source/binary fingerprints are retained in
`docs/evidence/fluid-vertical-geometry-20261101/`; active development data are in
`.cache/fluid-vertical-support-20261101/`. Some early source-substitution experiments
changed private storage layout and are **diagnostic only**; the actual CMake
combined build separately confirmed the failure before those variants were rejected.

Before checkpoint/unlock: resolve or independently establish the cause of the
settling regressions, rerun affected combined tests, bound layered-query cost,
then perform visual/gameplay and fresh platform qualification. Preserve the old
failing gates and every negative control. Main and the Alpha lock must stay intact.
