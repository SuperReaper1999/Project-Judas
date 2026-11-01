# Remaining production immersion check: observation audit

Follow-up: [native known-answer geometry comparison](FLUID_KNOWN_GEOMETRY_COMPARISON.md)
tests the engine as well as the ruler, and corrects one demonstrated connected
height-admission defect. The results below remain the historical diagnostic
checkpoint; the original production failure is still preserved.

2026-11-01. Based on Experimental `4feada296189f1e80be8447a297489e379ab4832`.
**The original failure remains active. This is a diagnostic checkpoint, not a
fluid correction, qualification pass, human acceptance or permission to unlock.**
Main and the Alpha Advanced Fluids lock remain unchanged. No runtime implementation,
physical setting, timestep, iteration budget or acceptance threshold changed.

## Result

The coarse `half_025` test still reports **0.6341740926** for its original 3–4 s
independent immersion observation, outside **0.5 +/- 0.10**. All its existing
motion-window checks pass. It is not legitimate to substitute the runtime's
0.5055831606 sampled mean and claim independent validation.

This continuation independently checks the box/plane intersection, records both
observation timestamps, evaluates frozen quadrature refinement and rejects another
nonplanar surface prototype. It does **not** establish the actual nonplanar wet
volume of the moving box. In particular, manufactured failures of the observer do
not prove that the runtime wet sampler is correct on the real pool.

## What was ruled out

| Measurement | Original 3–4 s window | Late 8–12 s window |
|---|---:|---:|
| Original native remote-plane fraction | 0.6341740926 | 0.5662507375 |
| Independent analytic box/that same plane fraction | 0.6341298865 | 0.5662423436 |
| Analytic plane observation at step **start** | 0.6326558999 | 0.5657165572 |
| Applied-force sampled fraction at step start | 0.5055831606 | 0.5017629591 |
| Largest per-frame native plane-quadrature discrepancy | 0.000665992 | 0.001412793 |
| Largest per-frame start/end plane difference | 0.030976939 | 0.038779653 |

The old 32³ clipping quadrature is not responsible for the failing mean. Matching
start/end observation timestamps also does not eliminate it. Applied buoyancy is
computed before rigid integration; the old independent observer reads the body and
particle endpoint afterward, potentially after a 30 Hz fluid update. Diagnostics
now preserve these distinct inputs. This does not change simulation ordering.

The independently integrated geometric fraction uses the CDF of a sum of uniform
intervals. For box half-extents h_i, orthonormal axes e_i and plane n·x <= H, set

    w_i = 2 h_i |n·e_i|, t = H - n·COM + sum(w_i)/2.
    F(t) = sum_S (-1)^|S| max(t - sum_{i in S} w_i, 0)^d
           / (d! product_i w_i).

Zero widths are absent dimensions; d is the remaining count. Symmetry is used at
the midpoint and to evaluate the nearly full side. The diagnostic explicitly
rejects ill-conditioned near-perpendicular evaluations rather than silently
rounding away a nonzero dimension. Independent vertical-ray integration, known
Irwin–Hall values, rotations/translations, complements, wrong heights and
nonfinite inputs validate this geometric calculation. This validates clipping
against a specified plane, **not** the choice of that plane.

## Surface-model counterexamples and rejected experiment

The native observer is unchanged: it chooses the upper median of the highest
particle heights in remote columns, excludes a cross around the **unrotated**
body extents, and treats that single height as a horizontal surface. It never
measures a general local, nonplanar water boundary. The replay deliberately
preserves those legacy choices for reproducibility.

A manufactured smooth surface `h(x,z)=0.5(x²+z²)` over a box of half-extents 0.3 m
and COM height 0.03 m has exactly 50% immersion: the mean surface over its footprint
is 0.03 m, and every surface height lies inside the box's vertical extent. The
remote-plane observer reports **100%** on the specified 0.25 m sample grid. This
negative control remains detectable in the new tests.

A disposable nonplanar prototype excludes whole grid cells intersecting the
actual rotated solid footprint and fills their unobserved heights harmonically.
Whole-cell exclusion avoids mistaking a buried particle below a cut column for
a measured free surface. It reproduces flat, affine and harmonic-saddle half-volume
controls. However, it fails the same smooth curved-bowl control:

| Particle spacing | Known fraction | Harmonic estimate |
|---|---:|---:|
| 0.25 m | 0.5 | 0.8247110202 |
| 0.125 m | 0.5 | 0.6375272512 |
| 0.0625 m | 0.5 | 0.5684350846 |

These are manufactured observations, not native fluid trajectories. The dense
vertical reference itself differs from the analytic 0.5 by about 0.000003052.

On **all** actual coarse observation frames the harmonic prototype gives
0.5435697448 early and 0.6161539698 late. Its favourable early number does not
qualify it; the late value fails the original tolerance and the manufactured
curved surface already disproves adequate accuracy at the coarse scale.
**Rejected. Not adopted in production, the original observer or acceptance.**

There is also a geometric identifiability limit. Above the solid's footprint,
two hypothetical caps `h=+/-0.25(1-(x/0.3)^2)^2(1-(z/0.3)^2)^2` can join the
same exterior `h=0` with zero first derivative on the boundary. They imply
fractions **0.3814814815 / 0.6185185185**, while every exterior observation and
waterline is identical. The difference follows exactly from the mean `8/15` of
`(1-u²)^2` on [-1,1]. **These are hypothetical geometric continuations, not two
solutions of the fluid equations.** The control shows why a displaced-volume
observation needs an explicit continuation definition; more interpolation or
precision alone cannot determine unobserved geometry. It does not invalidate
force/traction measurements or prove the runtime's chosen approximation correct.

Neither this nor yesterday's rejected affine fit is a qualified replacement.
Exterior-column extrema also include sparse/buried sampling effects: their height
span is not automatically a measured physical wave amplitude.

## Runtime sampling controls

A separate frozen, manufactured flat-water sweep covers two spacings (0.25/0.125
m), three body orientations, seven heights, and both displaced and non-displacing
queries: **84 measured configurations**. Analytic plane clipping supplies the
reference, independent of the hydrostatic sampler. The largest absolute fraction
error is 0.0281004001 for a displaced box; non-displacing maximum is 0.0108980066.
This does not reproduce a large flat-interface defect and does not qualify
nonplanar moving geometry. The sweep is evidence, not a newly declared release gate.

Frozen **actual** snapshots evaluated at 2/4/8/16 samples per axis show:

| Endpoint | n=2 | n=4 (current) | n=8 | n=16 |
|---|---:|---:|---:|---:|
| Release | 1 | 1 | 1 | 1 |
| Step 240 | 0.36196813 | 0.42033485 | 0.44571713 | 0.45309624 |
| Step 720 | 0.57635438 | 0.57715291 | 0.58243108 | 0.58304459 |

These are stationary replay calculations at the **endpoint**, not the original
applied-force observation or whole-window means. They reveal finite sampling
error, but do not justify increasing runtime samples or claim to explain the
entire failing window. No such change was made.

## Durable tooling and validation

- `tests/ProductionFluidTests.cpp`: opt-in `--capture-immersion` writes the original
  observations, exact before/after body and particle snapshots, and timestamps.
  Normal acceptance and the normal CSV are unchanged. Capture costs I/O and must
  not be used for performance measurements.
- `tools/validation/audit_fluid_immersion.py`: standard-library-only replay and
  independent analytic plane clipping. Current replay scope is the authored
  uniform -Y fixture. Rotated/nonuniform gravity replay is rejected explicitly;
  the clipping primitive itself has rotation-covariance controls. Missing frames,
  duplicates, unavailable observations and timestamp inconsistencies are errors.
  **Exit 0 means the diagnostic ran, not that immersion passed.** Each window
  retains its original tolerance verdict.
- `tests/test_fluid_immersion_observer.py`: **13 tests passed**, including positive
  and negative geometry controls and replay fixtures. Standalone Python tests;
  these have not been added to or represented as a passing fluid release gate.
- Native coarse run: **2,176 checks, one original acceptance failure**, reproduced
  with capture disabled and enabled.
- **All 720 non-timing physical rows are exactly equal** between the original
  instrumented baseline, final default test and final capture test.
- Existing hydrostatic / vertical-support / distributed-drag / settling-contract /
  project-fluid-policy checks: **124 / 317 / 185 / 46 / 26 passed**.
- Existing build organization: **18 tests passed**. An initial invocation used an
  incorrect discovery filename and ran zero tests; that setup log is preserved
  and is not counted as validation.
- Final native target build completed without warnings. A standalone exploratory
  convergence probe emitted double-to-float initializer warnings; its raw source
  and measurements are preserved separately and are not the final target build.

No new engine correctness repair is claimed. No Windows/Android, cold-save,
whole-suite or human qualification is claimed for this diagnostic continuation.
Those paths were not changed. Independent fluid research and historical evidence
were not modified.

## Reproduction

In a configured Experimental Release build (this run used
`.cache/2d-stage1-build`, GCC 15.2), build `judas_production_fluid_tests`, then:

    .cache/2d-stage1-build/judas_production_fluid_tests \
      --case half_025 --capture-immersion \
      --output .cache/fluid-immersion-20261101/native-capture

Expect exit **1**, preserving the original failed check. Then:

    python3 tools/validation/audit_fluid_immersion.py \
      .cache/fluid-immersion-20261101/native-capture --case half_025 --spacing .25
    python3 -m unittest discover -s tests -p test_fluid_immersion_observer.py -v

The replay reports `original_tolerance_pass: false` for the original window.
The opt-in captures are about 58 MiB of file contents (62 MiB on disk) for this run; use a selected case
rather than collecting every fixture by default. Exact snapshots and hashes are
preserved in the evidence archive, so a re-simulation is not required for replay.

## Next boundary

The unresolved task is an independently qualified **nonplanar local wet-boundary
observation**, with explicit particle-coverage/solid-hole uncertainty. Another
plane fit, harmonic fill, a density-derived target, or the sampler's own fraction
cannot supply that validation. A candidate needs known curved-surface and
body-cut-column controls, incorrect-height negatives and a credible measured-error
bound before it can replace an acceptance observation. If the current particle
representation cannot support that measurement at the coarse scale, document and
review the representation/contract boundary rather than silently widening the
0.10 threshold or calling the current failure a pass.

Evidence: [manifest and reproduction notes](evidence/fluid-immersion-observer-20261101/README.txt).
