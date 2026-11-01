# Known-answer fluid geometry: engine versus observer

2026-11-01, Experimental, starting at
`dc861698be320fd7056297b82f47b4bc2f0e54de`.

**Both the legacy observer and the native wet-volume approximation give incorrect
answers on specified curved surfaces. One separate, demonstrated height-admission
discontinuity is corrected. The original production immersion failure is still
active. Advanced Fluids remain locked in Alpha; main is unchanged.**

## What is now established

These tests specify the actual mathematical surface, rather than estimating it
from the runtime's answer. A box has half-extents 0.3 m. Its exact wet fraction
is the integral of vertical box/heightfield ray intersections over its footprint.
For the partial cases, the whole surface stays between the box bottom and top,
so this simplifies to

    fraction = 1/2 + (mean(surface over footprint) - COM_height)/(2*half_height).

For `h=0.5(x²+z²)`, the exact mean over [-0.3,0.3]² is 0.03 m. Placing the COM
at that height makes the box exactly half immersed. The matching negative dome
has mean -0.03 m. Actual binary32 dimensions/COM values are used in the native
reference calculation, accounting for the last decimal places below.

The generator samples those declared columns directly, with no simulation,
settling, fitted plane, mass-derived target or hidden controller. Both a query
volume containing fluid and a displaced solid with interior particle centres
excluded are tested. The original native observer is shared with the production
test, including its original upper median, cross exclusion and 32³ clipping.

Representative **coarse 0.25 m, displaced solid, unrotated** results:

| Specified surface | Exact fraction | Engine | Original observer |
|---|---:|---:|---:|
| Flat | 0.5 | 0.5 | 0.5 |
| Affine slope | 0.5 | 0.46835735 | 0.5625 |
| Bowl | approximately 0.5 | 0.68437505 | 1.0 |
| Dome | approximately 0.5 | 0.11935018 | 0.0 |
| Saddle | 0.5 | 0.39843842 | 0.5 |
| Sine wave | 0.5 | 0.5 | 0.53125 |

Thus the problem cannot honestly be classified as solely a broken ruler. The
engine's spatial approximation has substantial curved-interface bias too. This
still does **not** establish the true nonplanar immersion in the original moving
pool or how much of its particular discrepancy comes from either approximation.

These are frozen **geometry** tests. Curved surfaces are not claimed to be
hydrostatic equilibria under uniform gravity. No pressure, force, torque, energy,
added-mass or free-surface evolution qualification follows from these comparisons.

## Mechanisms and rejected shortcuts

`FluidHydrostatics.cpp` builds a search reach from the supplied radius plus two
particle spacings. `EvaluateFluidHydrostatics` supplies the body's bound radius
for every subvolume. Wet fractions average column occupancy over that reach.
Consequently, different local water heights are mixed. Solid occlusion and
support admission further affect that average; it is not exact local surface
integration.

Two isolated causal ablations were tested against the first 72 configurations:

| Experiment | Comparisons with engine error >0.10 | Worst error |
|---|---:|---:|
| Original | 21 | 0.42048756 |
| Disable wet-support admission | 24 | 0.42048759 |
| Use subvolume radius instead of body radius | 23 | 0.31245325 |

Disabling admission is unsafe and worsens some results. Reducing the search
radius does not reconstruct the missing surface geometry. **Neither experiment
is a candidate correction or part of the final engine.** Their sources/results
are preserved. No radius, impulse, damping or solver-budget tuning was adopted.

## A definite defect found and corrected

A smaller known-answer control isolates the physical query's height admission.
The root water column ends at y=0; neighbors end at y=0.25. A sample slab has
half-height 0.1 m. As its centre rises through 0.1 m, its tiny overlap with the
local water must disappear continuously.

Previously, any positive overlap made the **whole slab** eligible for the much
larger spatial mean of neighboring columns:

| Centre bracket | Before: below / above | Corrected: below / above |
|---|---|---|
| 0.1 +/-0.001 m | 0.90684950 / 0 | 0.00500001 / 0 |
| 0.1 +/-0.00001 m | 0.90638608 / 0 | 0.00004999 / 0 |

The analytical local interval overlap is epsilon/0.2. Both identity and rotated,
translated controls pass. The original engine fails **six** of the new regression
assertions; the corrected engine passes all **335** vertical-support assertions
(the previous 317 plus 18 new assertions).

Connected non-displacing physical queries now integrate admission per height,
as displaced-solid queries already did. The default, unconnected low-level
column-mean contract is preserved. No API signature or authored setting changes.

A straightforward height-sliced prototype established the corrected reference.
The final implementation analytically intersects the root column's wet intervals
with each donor's intervals. With no solid, root-column wetness is required for
connected admission, and that root footprint already contains the query; repeated
hull/support discovery per height band is unnecessary. Solid queries continue to
use their existing geometric support path.

**720 varied queries** (flat/curved/stepped/layered/disconnected/missing-particle
fields; point and finite-height samples; multiple radii; two frames) match the
height-sliced reference **exactly**, including fractions, dry/wet classification,
velocity and acceleration. This is a correction of the demonstrated admission
mechanism, not a replacement spatial surface model.

## Measured query cost

Linux, Ryzen 5 5600, GCC 15.2, Release `-O3 -DNDEBUG`. Five batches per fixture,
200 deterministic queries per batch; median **batch mean**, microseconds/query:

| Field | Original incorrect query | Correct height-sliced reference | Final interval integration |
|---|---:|---:|---:|
| Flat | 20.22 | 25.04 | 11.48 |
| Bowl | 23.41 | 43.96 | 13.38 |
| Dome | 14.48 | 21.01 | 9.07 |

Reference/final output checksums are identical. These are small function-level
measurements, including query allocations but excluding field construction.
The original produces different answers and is a historical cost reference.
This is **not** a whole-fluid-step, frame-rate, p95-step or general speedup claim.
Raw batch timings and the actual workload are preserved.

## Broader known-geometry result and remaining limits

The strict native diagnostic exercises six surfaces, three particle spacings,
five heights (half/30%/70%/full/dry), both volume/solid ownership modes, and two
rotated/translated frames: **360 measurements**. The independent dense ray
integration agrees with the analytical reference within 0.000003052. Its 180
reference evaluations repeat **30 distinct local surface/height geometries**;
they are not 180 independent engine contracts.

After the admission correction:

- **76/360** engine fraction comparisons remain outside the original 0.10 scale,
  versus 78 before. No previously passing comparison becomes failing at that scale.
- **80/360** observer comparisons remain outside that scale.
- **3/180** engine frame comparisons remain outside the separately predeclared
  0.001 covariance scale; worst difference 0.00427097. All three are displaced
  affine cases. Non-displacing saddle's earlier roughly 0.0222 frame jump is gone.
- Observer frame comparisons pass after supplying its required reservoir origin.
- The diagnostic still exits **1**. It is registered as an explicit manual
  `known-failures` diagnostic, not counted as a passing automatic release gate.
  Existing accepted gates were not demoted or removed.

Not every failed row has the same cause. At spacing 0.0625 m, simply omitting
particle centres inside a 0.3 m half-extent solid leaves a **0.012499988 m gap**
between represented particle cubes and the solid. Its contact-admission path
reports zero at half immersion. An additional flat control with half-extent 0.25
m puts cell boundaries on the solid, gives zero gap, and reports 0.5. **Both inputs
and results are retained**; the aligned control does not replace or retune the
failing fixture. This exposes a sampling/contact-coverage limitation, rather than
proving every fine-grid error is curvature bias. It is not an actual flowing-pool
convergence study or permission to weaken the production fixture.

The next spatial-reconstruction work must address local shape, solid-cut sampling
and uncertain support explicitly. A single remote plane, an unqualified fit,
looser tolerance or unconditional wet support does not resolve these findings.

## Verification and preservation

- Vertical support **335**, hydrostatics **124**, lateral support **109**, distributed
  drag **185**, swimming **89 / 2,475 steps**, project-fluid policy **26**, lifecycle
  **21**, Basic Water rendering preview **18**, flat gameplay **24**, planetary
  gameplay **25**: passed.
- Original coarse production run: **2,176 checks, one unchanged failure** at
  immersion 0.6341740926. All **720 non-timing physical rows** match the preceding
  checkpoint exactly.
- The measuring tool was extracted into `tests/FluidImmersionObservation.h` so the
  new diagnostic tests the **actual** original ruler. All **300** archived native
  surface heights and observed fractions replay **exactly** with that helper.
- Python observer controls **13**, build organization **18**, validation tooling
  **14**, registration **9 with two environment skips**: passed.
- Disposable fault injection forcing the sampler's reported value to 0.5 fails
  **48/60** flat-fixture checks. Wrong heights, full wetness and dry states prevent
  an always-half sampler from passing.
- Unknown surface names and invalid output directories return explicit exit 2.
- Final native builds were warning-free. No Windows/Android or human testing is
  claimed. Existing supported liquid architecture and independent research were
  not modified. No new persistent simulation state is introduced.

Setup failures are retained honestly: the first translated observer run failed
to remove the reservoir origin; the observer does not accept that input frame.
The adapter now supplies its authored zero-origin frame while the engine still
sees the translated scene. A disposable fault-injection link initially omitted
the validation entrypoint rename; that harness issue was corrected. Registration
checks also caught the new diagnostic's initially incorrect `water` grouping and
an old hard-coded one-manual-test count. The diagnostic now uses only
`known-failures`, and the test pins the exact two allowed manual entries. No
physical assertion was relaxed to resolve these setup errors.

## Exact changed implementation and test files

- `src/FluidHydrostatics.cpp`: height-local connected admission and equivalent
  interval integration; solid and unconnected paths retained.
- `src/FluidHydrostatics.h`: accurate contract comments; no interface changes.
- `tests/FluidVerticalSupportTests.cpp`: new analytic overlap, shrinking-bracket,
  frame and low-level compatibility controls.
- `tests/FluidImmersionObservation.h`, `tests/ProductionFluidTests.cpp`: shared
  original ruler; replay verifies no observation/acceptance change.
- `tests/FluidKnownGeometryTests.cpp`: strict, known-failing analytic comparison.
- `CMakeLists.txt`, `tools/validation/suites.json`,
  `tests/test_prealpha_validation.py`: explicit diagnostic registration and exact
  manual-test inventory; ordinary gates preserved.

Evidence, source/binary fingerprints, raw failures, rejected ablations, input
settings and commands: [evidence manifest](evidence/fluid-known-geometry-20261101/README.txt).

## Reproduce and inspect

Configure an Experimental Release build, then build these targets:

    cmake --build BUILD --target judas_fluid_known_geometry_tests \
      judas_fluid_vertical_support_tests judas_production_fluid_tests -j2
    BUILD/judas_fluid_known_geometry_tests --output NEW_DIRECTORY/known.csv
    BUILD/judas_fluid_vertical_support_tests
    BUILD/judas_production_fluid_tests --case half_025 --output NEW_DIRECTORY/production

Expected exit codes: **1 / 0 / 1**. A known-failing result is not a pass.
The known diagnostic also supports `--case flat|affine|bowl|dome|saddle|wave`.
Its flat selector still retains the fine-grid contact-coverage failure.

Review boundary: the connected-slab correction is independently checkpointable;
spatial wet-volume reconstruction and the original immersion check remain open.
No main promotion, Alpha fluid unlock, publication or human acceptance is claimed.
