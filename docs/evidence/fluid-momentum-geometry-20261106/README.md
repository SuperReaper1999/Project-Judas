# Momentum/energy and geometry investigation evidence

Parent Experimental commit: `8ed450f057c68653d5595827110a055d77d47329`.
The containing commit and `SOURCE_SHA256SUMS.txt` identify the final source.
No native runtime, game, SDK, main, release-channel or fluid-lock changes.

## Reproduce the accepted controls

From a source checkout/archive with C++17/GLM, Bash, Python 3 and GNU time:

```sh
bash tools/prototypes/fluid_volume_grid/qualify_momentum_geometry.sh /tmp/judas-momentum-geometry
```

This builds the focused controls, runs fast and reference momentum controls,
checks the exact residual oracle, generates three admitted pressure states for
each original fixture, diagnoses their mass/kinetic metrics, and tests uniform
and nonuniform momentum transport on their actual material carriers. Twelve
complete velocity fields are compared against the compensated reference to
`1e-10 m/s`, using all compensated components. The distance experiment must
reproduce its affine positive control and thin-gap negative control. Its 366
false-solid point classifications are an observed **failure of the candidate
sampled-distance representation**, not a claim that it is safe for fluids.

For ASan/UBSan/leak checking, run outside an incompatible tracing wrapper:

```sh
ASAN_OPTIONS=detect_leaks=1 JUDAS_FLUID_SANITIZE=1 \
 bash tools/prototypes/fluid_volume_grid/qualify_momentum_geometry.sh /tmp/judas-momentum-geometry-sanitized
```

Default wall-time budgets are 180 seconds per normal probe and 600 per
sanitized probe; override `JUDAS_FLUID_PROBE_SECONDS` for slower hosts. They do
not change physical dt, accuracy, object/face counts or solve iteration limits.
The first sanitizer reference run hit the old 180-second budget after 11 cases.
Its receipt is retained as `sanitized-reference-timeout.*`. A complete rerun
with 600 seconds finished all 12 cases; no failure is converted into a pass.

`pressure.exit`, `momentum.exit`, `momentum-reference.exit` and `distance.exit`
must all be zero. See `qualification.json` for totals and remaining scope.
The final four arrival assertions were added after the full controls ran;
the same final test was then recompiled and run for both backends, in normal
and sanitized builds. Only diagnostic argument validation changed afterward;
its five negative controls and both first-basin cases were checked in each
build. The library algorithms and the already compared reference fields did
not change during that closeout.

## What is and is not qualified

`CutMassMomentum` is a conservative **cell** carrier, not an integrated pressure
velocity-dual update. Uniform/nonuniform velocities in the carrier tests are
prescribed test fields. Pressure states come from the unchanged pressure
integrator and still exhibit extreme speeds. The small arrival test has a
prescribed carrier and known impulse; it is not a repaired basin simulation.
The previous 120+120 volume/quantity run is preserved in the prior evidence,
not relabelled here as sustained momentum qualification.

The state binaries are the existing little-endian Linux **diagnostic numerical
format**, not Judas game saves. Momentum dumps are cell-ordered triples of
compensated velocity pairs (binary64 high/low), without a portable format
header. Do not feed them to the engine or infer native save/streaming support.
Selected complete fields are compressed here; `RAW_STATE_SHA256SUMS.txt` records
all generated raw fields. Remaining raw fields are retained locally under
`.cache/fluid-momentum-20261106/`. The reproduction creates them afresh.

## Profile and rejected factor-cache experiment

`gprof-gprof.txt` and `gprof-gmon.out` retain the sampled profile and raw data.
`GeometryTimedPressure.cpp` and `geometry-timed.log` time geometry explicitly;
all six instrumented pressure states match the originals byte for byte.
Instrumentation is not used for the matched speed comparison.

`baseline-source.tar.gz` is the committed parent source/fixture archive.
`StateProbe.cpp` adds state capture to that parent's sustained driver; capture
is not a physical change. `rejected-prepared-graph-source.tar.gz` contains the
four trial graph/preconditioner files. It was tested in disposable build
outputs and **is not part of the final implementation**.

`paired-cache.py`, `paired-cache/rows.json`, all raw logs/resources and state
hashes preserve five alternating process pairs, each running three intervals
in both orientations. Build each source variant with C++17 `-O3`, no fast-math,
and link `StateProbe.cpp` against the objects in that variant's
`qualify_internal_transport.sh` manifest, including the shared `BasinFixture.h`
path. The runner records the actual commands via GNU time. Adapt its two binary
paths to those builds when reproducing elsewhere. None of the reported paired
runs overlapped compiler jobs. One process pair is the independent sample;
the three intervals within it are correlated.

The cache did not establish a worthwhile total-runtime improvement, despite
byte-identical physical fields. The failed prototype is retained rather than
adopted or silently discarded. The earlier slow reference source archive at
`../fluid-fast-transport-20261106/slow-reference-source.tar.gz` remains unchanged.

## Other retained failures and corrections

- `momentum-tests-first-failure.log`: the new caller initially demanded mass
  closure tighter than the phase solver's declared error budget. Analytic exact
  controls keep their strict budget; the actual phase-operator control now
  declares the inherited `1e-14 m³ * 1000 kg/m³` budget explicitly.
- `momentum-basin.log`: an initial diagnostic accidentally mixed parent-region
  geometry with the refined spatial field. It failed; final diagnostics use
  the same refined geometry as the saved state and validate dimensions.
- `momentum-basin-corrected.log`: the first uniform-velocity accuracy target
  required a tighter momentum solve. The error budget was tightened; the
  acceptance check was not loosened.
- `global-midpoint-audit.log`: global-coordinate half splits falsely reported
  coverage defects. The final independent audit uses local coordinates like
  the physical geometry and reports zero missing/overlapping column coverage.

These harness failures are not attributed to the engine. New original-row
momentum audits and negative invocation controls prevent equivalent false
passes. No physical timestep, mass, velocity or energy was adjusted to obtain
these results.

## Human review checklist

1. Review `METRIC`/`FASTEST` rows for both original fixtures and the mass-metric
   energy term; confirm this is not presented as a complete fluid energy audit.
2. Reproduce the analytic and original-geometry carrier controls in both
   backends, including the incoming material/known-impulse case.
3. Review the unsuccessful cache timing pairs and the sampled-distance false
   closures before recommending an engine-wide geometry framework.
4. Keep the integrated velocity-dual coupling, moving bodies, native runtime
   integration and fluid unlocking pending further qualification.

[Engineering report](../../FLUID_MOMENTUM_GEOMETRY.md).
