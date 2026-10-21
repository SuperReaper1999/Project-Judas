# Event dependency evidence — 21 October 2026

Read the [engineering report](../../PREALPHA_EVENT_DEPENDENCIES.md) first.
Input checkpoint: `b7db703ca3fc3c0d39ecc31009a1148b5b778762`, Experimental.
The frame-rate limitation remains open. This is not human acceptance or Alpha.

## Contents and interpretation

- `final/MEASUREMENTS.json`: unprofiled short/long/live statistics and query/event
  totals. `runs/` preserves raw CSVs, driver logs, context, RSS and endpoint data.
  Large text files have deterministic gzip compression; `gzip -cd` reads them.
- `final/EXACT_COMPARISONS.json` and individual comparisons: all physical/work
  fields except explicitly allowed query reductions remain exact, including
  complete endpoint geometry. `final/LIVE_PREFIX_COMPARISON.json` identifies
  the different live step counts and their explicitly checked prefixes.
- `final/PROFILE_SUMMARY.json`: the same 118 retained late frames with fixed steps
  on each side; raw profile files remain in `runs/`. Inclusive counters overlap.
- `final/FROZEN.json`, `BUILD_INFO.json` and `source-inputs.tar.gz`: actual source,
  compiler, engine/benchmark hashes and dirty Experimental build provenance.
  Binaries and object files are intentionally not committed. They remain in
  `.cache/event-architecture-20261021/final-snapshot/` on the development machine.
- `final/focused/RESULTS.json`: every attempted native invocation, including one
  usage error. `final/registered-checks/` proves the newly registered positive
  and negative cases execute through the ordinary validation entry point.
- `cold/`: four fresh processes using the unchanged protected M70/M77 observer.
  Its receipt verifies actual physical endpoints and unchanged source/fixtures.
- `application/`: save/stream write/read/read-b, editor Play/Stop on a project
  copy, export and relocated read-only standalone launch/render/normal exit.
  `PACKAGE.json` records the local playable package location. Screenshots are
  automated observations, not human review.
- `experiments/`: actual sources, build commands/hashes, logs and raw rows for
  rejected and intermediate candidates. Some prototype scripts retain their
  original diagnostic wording/absolute paths. Their parent source archives were
  not full candidate archives; adjacent overlay sources and `BUILD.json` identify
  the exact experiment. They are not final production build receipts.
- `claude/`: unchanged independent report, host description, read-only patch and
  raw event trace. The original complete archive remains read-only at
  `/home/conner/Documents/judas-1024-investigation.tar.gz`; its SHA256 is in
  `final/HOST.json`. `CLAUDE_SOURCE_PARITY.json` verifies 22 relevant archived
  physics inputs against the development parent.
- `earlier-causal-proof/`: unchanged earlier sphere/box capture, native replay and
  exact-rational proof inputs/results. These classify one representative pair,
  not the entire pile. To rerun the Python mathematics, copy this directory to a
  disposable directory, decompress its JSON/JSONL files, then run
  `sphere_box_exact.py` followed by `episode_proof.py`. The scripts write their
  results beside themselves; do not run them over preserved evidence.
- `final/POST_MEASUREMENT_DOC_DELTA.json`: the strict snapshot check caught a
  later update to `src/physics/WorldModules.md`. It is documented separately;
  frozen expectations were not regenerated. All executable inputs still match.
- `FINGERPRINTS.json`: current source and validation-input hashes, frozen binary
  hashes and preservation checks. `MANIFEST.json` hashes this evidence inventory.
  `CHANGED_FILES.txt` lists the exact checkpoint paths, including evidence.

## Reproduction

Use the parent checkpoint in a separate checkout to build the baseline, then the
new checkpoint for the candidate. Use the same Release compiler/options on the
same host, sequentially. No VM or Android workload ran during these timings.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUDAS_BUILD_CHANNEL=Experimental
cmake --build build --target judas_physics_stress_live_benchmark \
  judas_physics_scaling_diagnostics judas_impact_event_schedule_tests -j2
python3 scripts/physics_dense_measure.py freeze --build build --output /tmp/judas-snapshot
python3 scripts/physics_dense_measure.py run --snapshot /tmp/judas-snapshot \
  --output /tmp/judas-short --mode exact --steps 270 --seconds 300 --timeout 360 --profile 0
python3 scripts/physics_dense_measure.py run --snapshot /tmp/judas-snapshot \
  --output /tmp/judas-long --mode exact --steps 600 --seconds 300 --timeout 360 --profile 0
python3 scripts/physics_dense_measure.py run --snapshot /tmp/judas-snapshot \
  --output /tmp/judas-live --mode realtime --seconds 35 --timeout 240 --profile 0 --visible
python3 scripts/physics_dense_measure.py compare --baseline /tmp/baseline-long \
  --candidate /tmp/judas-long --output /tmp/comparison.json --allow-query-reduction
python3 scripts/prealpha_validate.py --build-dir build \
  --test impact-event-schedule --test impact-event-schedule-negative --output /tmp/event-checks
```

The new test target exists only in the candidate. Omit it when building the
parent. Freeze destinations/run directories must be fresh. The recorded exact
invocations and execution order are in `MATCHED_COMMANDS.json` and
`LIVE_PROFILE_COMMANDS.json` under `final/`.

The comparison remains strict by default. `--allow-query-reduction` permits only
fewer queries per step; more queries, changed digests, event counts, search work,
missing rows or changed endpoint geometry still fail. The tooling unit tests
exercise those negative controls.

## Preserved unsuccessful invocations

The first multi-target build dispatch failed after generating the new Make
target; a second normal build passed. Initial sanitizer runs failed because
LeakSanitizer cannot run under sandbox ptrace; the native reruns passed. One
save/stream invocation lacked required arguments (exit 2); the correct native
write/read/read-b runs passed. Those are setup failures, not engine regressions
or successful qualification. The negative queue control deliberately prints two
FAIL lines and succeeds only if exactly those two missing mechanisms are detected.

Windows/Android were not rerun. Independent research, protected history,
production fluid implementations and existing projects were not edited.
