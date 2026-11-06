# Velocity-dual coupling evidence

Parent `42e233ee430456bb920c61e62947df83baf9d0e1`, Experimental.
See `provenance.json`, `qualification.json`, `SOURCE_SHA256SUMS.txt` and the
containing commit for exact identity. Main and the native fluid lock unchanged.

**Overall coupled basin qualification FAILS.** Passing building-block controls
are not an unlock and are not relabelled as complete fluid qualification.

## Reproduce

Linux, C++17 compiler, GLM headers, Python 3, Bash and GNU time. No fast-math.

```sh
bash tools/prototypes/fluid_volume_grid/qualify_internal_transport.sh /tmp/dual-controls
ASAN_OPTIONS=detect_leaks=1 JUDAS_FLUID_SANITIZE=1 \
 bash tools/prototypes/fluid_volume_grid/qualify_internal_transport.sh /tmp/dual-sanitized
/tmp/dual-controls/CutDualMomentumTests --reference
ASAN_OPTIONS=detect_leaks=1 /tmp/dual-sanitized/CutDualMomentumTests --reference
```

LeakSanitizer requires an environment without an incompatible tracing wrapper.
Individual final suite logs in `qualified-normal/` and
`qualified-sanitized-native/` are authoritative; aggregate normal logs retain
earlier runs before the last focused controls were added and rerun.
The first traced failure is retained in `qualified-sanitized.log`; the complete
successful rerun is `qualified-sanitized-native/`. Twenty-one suites plus the
reference invocation total 3,387 controls in each configuration.

Full **currently failing** diagnostic gate:

```sh
bash tools/prototypes/fluid_volume_grid/qualify_dual_coupling.sh /tmp/dual-basins
```

This must return nonzero while original basins reject. It preserves each exit
status and raw output, including timeouts. It runs three hydrostatic-domain
steps, then one original step for each orientation; it does not assert that this
is sustained qualification. `JUDAS_FLUID_PROBE_SECONDS` changes wall-time budget
only. The recorded final commands were run directly against the same source,
objects and flags; `final-*.log/.exit` include GNU time resources in each log.

After building controls, compile individual diagnostics with their `objects.list`:

```sh
mapfile -t objects < /tmp/dual-controls/objects.list
c++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror \
 -Itools/prototypes/fluid_volume_grid \
 -Idocs/evidence/fluid-moving-grid-20261103/shared-cut-regions \
 tools/prototypes/fluid_volume_grid/CutCoupledBasinProbe.cpp \
 "${objects[@]}" -o /tmp/dual-basin
/tmp/dual-basin filled rotated 3 orthogonal
/tmp/dual-basin original level 1 orthogonal
# Alternate nonlinear linearization; NOT an accepted basin correction:
/tmp/dual-basin original level 1 orthogonal oseen
```

`filled` means a level hydrostatic surface over the entire connected domain,
including outside pool walls. `original` retains the actual imported basin seed,
which is already in the pool; it is not the visible Terrain planet cube release.
The conforming chart retains every positive cut and is not a production cache.

The same include/objects command builds retained `ThinColumnProbe.cpp`,
`BasinDerivativeProbe.cpp`, `DualBasinProbe.cpp`, or `OrthogonalCount.cpp`.
These are scope-limited diagnostics, not extra accepted game trajectories.
`ThinColumnProbe` passing a tiny 1-D layer does not establish general cut-cell
conditioning; `BasinDerivativeProbe` is a central finite-difference diagnostic,
with truncation error and no predeclared basin accuracy gate.

## Preserved regression identity

`legacy-comparison.json` records byte-identical state arrays against the previous
committed checkpoint: three original pressure/quantity intervals in each
orientation. Build/run the unchanged `SustainedPressureProbe.cpp` with the same
objects and arguments `3 both OUTPUT_PREFIX --each`. The comparison does not
assert that those old velocities are now corrected. Native game saves were not
changed or qualified.

The exact-rational residual oracle is `final-oracle.log` (842 controls). Build
`CutFastResidualOracle.cpp` with the same objects and run
`python3 tools/prototypes/fluid_volume_grid/qualify_fast_residual.py BINARY`.

## Failed attempts

- `rejected-lagged-coefficients-source.tar.gz`: original frozen-coefficient outer
  approach, including its removed pressure hook. `coefficient-*.log` and
  `original-*.log` retain its failures.
- `joint-first-source.tar.gz`: joint Newton with a momentum predictor before row
  scaling changes. `joint-original-*` predates that predictor and retains the
  initial zero-velocity trial; it was not separately source-snapshotted.
- `joint-oseen-source.tar.gz`: frozen-momentum derivative experiment. The level
  run exhausts 60 outer iterations; rotated rejects line search.
- `endpoint-gravity-source.tar.gz`: source before the gravity-kick experiment;
  also records the unsuccessful three-Oseen-then-Newton trial. Its level run
  times out at 180 seconds (`joint-seeded-newton-level.log`).
- `source-gravity-trial-source.tar.gz`: source-mass gravity-kick variant. Both
  orientations time out at 240 seconds. It is rejected and restored to ending-
  mass gravity in final source. These trials do not establish a general verdict
  on operator splitting.
- `CutDualLinearTests-new.log`: initially failed signed linear solver controls.
  An absolute RHS floor erased tiny inputs; it was removed. The reference
  comparison had requested only 1e-12 accuracy while asserting 1e-24 agreement;
  the reference request was tightened to 1e-26. Original failures retained.

Archives snapshot the prototype sources, not engine integration or independent
Solbert/Terra research. Some older probes predate the final optional Oseen enum;
use their matching archived source when reproducing those older exact commands.
Final active sources and their hashes are authoritative for `final-*` runs.
All timing samples are diagnostics, some contended. No median/p95 speedup or
runtime usability claim is made. `ARTIFACT_SHA256SUMS.txt` fingerprints retained
files. Raw build objects remain under `.cache/fluid-dual-coupling-20261106/`.

Raw logs retain their original trailing whitespace; only source and prose are
subject to whitespace cleanup. They have not been edited to make diff checks pass.
