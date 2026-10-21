# Persistent support / incremental impact evidence

See [the engineering report](../../PREALPHA_INCREMENTAL_IMPACTS.md). These are
Linux automated results, not human acceptance or a24–30FPS certificate.

## Start here

- `FINAL_MEASUREMENTS.json`: matched270/400/600 windows, queries, memory and live
 10–35s cadence. Baseline6059 implementation and final workload are identical;
 their resulting chaotic trajectories differ intentionally.
- `FINAL_QUALITY.json`: all601 native end-of-step geometry, mass/energy and floor
 observations. Penetration remains mixed, with explicit population scope.
- `FINAL_PARITY.json`:600 unprofiled, observed and profiled final work/trajectory
 rows agree; final comment/diagnostic-label cleanup preserves engine archive bytes.
- `REMAINING_PROFILE.json`: bounded retained tail window, not a whole-run average.
- `causal/PARITY.json`: all347 legacy capture steps preserve original physical and
 work rows. Capture timing is invalid for performance because of serialization.
- `causal/REPLAY-final.json`: ten exact original event states; actual support sets,
 immutable references and a genuine negative control. Event28 changes751→0 bodies.
- `final-checks`, `mechanical`, `negative-final`, `sanitize-final`, `cold`,
 `application`, `hearthworld`: commands, logs, binary hashes and actual outcomes.
- `PRESERVED_INVESTIGATION.json`: verifies the earlier investigation and all its
 manifest entries without modifying any of them.

## Reproduction

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUDAS_ENABLE_VALIDATION=ON
cmake --build build --target judas_impact_solver_tests judas_growing_impact_tests \
  judas_tracked_contact_rows_tests judas_physics_stress_live_benchmark -j2
ctest --test-dir build -R 'prealpha.(growing-impact|tracked-contact-rows)$' --output-on-failure
python3 docs/evidence/prealpha-incremental-impacts-20261021/replay.py \
  --build build --output .cache/replay-original-impact-states
```

The portable replay verifies its input hashes, decompresses the ten original
cases into the requested fresh directory and links the current normal Release
core. It asserts original legacy results, new ownership results, energy policy
and the missing-support negative control. It needs no private prototype build,
legacy binary or unrelated game assets. `causal/Replay.cpp` is the original
measurement source; `ReplayAssertions.cpp` adds explicit acceptance assertions.

For measurements use the existing helper, a fresh output for each run:

```sh
python3 scripts/physics_dense_measure.py freeze --build build --output .cache/impact-current
python3 scripts/physics_dense_measure.py run --snapshot .cache/impact-current \
  --output .cache/impact-600 --steps 600 --seconds 180 --timeout 240 --profile 0
python3 scripts/physics_dense_measure.py run --snapshot .cache/impact-current \
  --output .cache/impact-live --mode realtime --seconds 35 --timeout 90 --profile 0 --visible
```

For an **uncommitted** source experiment, supply each intended new source/header
with `freeze --extra-source PATH`; do not bundle unrelated untracked work. Once
this checkpoint is committed the helper includes the tracked source automatically.
Native GL and a real desktop session are required. Synthetic cadence is not FPS.
To rebuild the baseline, use a separate checkout of6059bafb; never reset the live
worktree. Baseline/final frozen manifests and measured source archives identify
exact inputs. They exclude large executables/archives from Git; those original
binaries remain in the recorded ignored local snapshots with verified hashes.

`SOURCE_FILES.json` identifies final code and current documentation. The measured
source archive precedes the last header-comment/test-label cleanup; an ordinary
rebuild verifies that cleanup leaves the core archive byte-identical. Final code
is also retained in `FINAL_SOURCE_INPUTS.tar.gz`. `FINAL_BINARIES.json` records
actual current/native binary hashes and their relation to tested receipts.
`CHANGED_FILES.txt` is the exact checkpoint inventory, including this evidence.
`MANIFEST.json` hashes every other file; compressed text also records its raw hash.

## Experiments, failures and boundaries

`prototypes/world-v1` throughv11 and `NEXT_IMPLEMENTATION.md` preserve research
sources, commands, hypotheses and measured revisions. Read those as dated work
notes, not current implementation documentation. Historical-reference models and
restarting domain solves were rejected. The exact sparse-row kernel was not
shipped. Some prototype figures are shorter/different windows; they are not the
final matched comparison. Private absolute paths in original recipes are kept as
provenance; use the portable replay/current CMake targets for reproduction.

The first build recorded an include-path error in the new tests, subsequently
corrected. `sanitize-first-ptrace` preserves LSan's environment failure; the final
ASan/UBSan run disables leak detection and explicitly limits instrumentation to
three core translation units plus the new tests. `negative-final` contains real
broken source variants which fail, plus an unmodified control which passes.
A failed variant is not a production regression or a successful positive test.

Do not add frame-summed/inclusive geometry counters together as exclusive time.
The growing solve's discovery time is excluded explicitly; other CCD geometry
and search categories remain inclusive. Within-step geometry is not universally
certified by end-of-step observations. Deferred FLU-001, legacy saves, unsupported
compositions and Windows/Android qualification are not closed here.
