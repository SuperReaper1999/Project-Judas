# M91 reproduction

Baseline engine: `4628494c4edcbad1654db21da21b392a9bb1f987`, Experimental.
Linux Release, default `-O3 -DNDEBUG`, no fast-math or quality/cadence changes.
Commands below assume the repository root. Do not run compilation, a VM workload
or another benchmark alongside a timing run. No special privileges/sysctl changes
are needed for the benchmark itself; a native graphics session is needed for
application/editor cases.

## Build and focused checks

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target judas judas_editor judas_export \
  judas_collision_continuity_tests judas_contact_interval_tests \
  judas_physics_stress_live_benchmark judas_physics_scaling_benchmarks \
  judas_physics_scaling_diagnostics judas_character_lab_tests \
  judas_balance_lab_tests judas_ball_pusher_application_tests \
  judas_physics_scaling_application_tests -j4
build/judas_collision_continuity_tests
build/judas_contact_interval_tests --benchmark
```

The exact wider focused target list and commands are preserved in
`raw/build-final-targets.txt`, `raw/run_verified_checks.py` and
`raw/verified-native/RESULTS.json`. They are 34 selected executables, not the
entire production suite. Old controller fixtures and thresholds are unchanged.

## Matched baseline

The recorded experiment froze the baseline engine archive and linked the same
updated observer object/Application objects against it. See
`raw/baseline/SOURCE.json`, `BINARIES.json`, `MATCHED_OBSERVER_LINK.json` and the
final binary manifest. The observer's optional endpoint-geometry report runs
outside fixed-step timing. Its new observation code does not drive physics.

For an independent rebuild, use a separate disposable worktree at the baseline
commit, configure the same Release dependencies, copy ONLY the current
`tests/PhysicsStressLiveBenchmark.cpp` observer there, and build the ordinary
`judas_physics_stress_live_benchmark` target. Keep the baseline engine source
unchanged. A new compiler/environment is not promised to recreate the recorded
binary hash. The actual link recipe and hashes identify the binary measured here.

The dense fixture is the exact M87 `room1024.judas` scene, with baseline project
registration and assets, not a smaller replacement. The stored scene is
`raw/frozen/Scenes/room1024.judas.gz`; decompress into a private project beside
`raw/frozen/game.judasproj` and copy the baseline project's `Assets` directory
there. `raw/baseline/SOURCE.json` and `ARTIFACTS.json` identify those bytes.
The font is already tracked in the baseline project and is not duplicated here.
Pass the **scene** path: the retained partial frozen project deliberately lacks
room128, so opening its startup project directly is not this reproduction.

```sh
python3 scripts/m91_measure.py \
  --before /absolute/path/to/baseline-observer \
  --after build/judas_physics_stress_live_benchmark \
  --scene /absolute/path/to/frozen/Scenes/room1024.judas \
  --output /tmp/m91-dense-new --frames 155 240 --repeat 2 --timeout 75 --profile 0
```

Use a fresh output directory. The runner alternates order, records wall timeout,
per-step CSVs, physical digests, endpoint narrowphase and peak RSS. An unfinished
step has no fabricated duration. The process group it kills belongs only to its
own launched benchmark. Exit zero verifies fingerprint consistency, **not**
successful completion of every requested run: inspect `completed_requested_run`,
`timed_out` and the measured step count. The baseline long cases are censored.
For diagnostic stage data use `--profile 1`; do not mix that timing with profile-off
throughput. The final 270-step diagnostic and longer preserved failure are separate.

## Scaling, imported articulations and real consumer

```sh
python3 scripts/m87_benchmark.py \
  --binary build/judas_physics_scaling_benchmarks --output /tmp/m91-scaling-new \
  --counts 64 256 1024 --machine-counts 16 64 256 --steps 120 --profile 1 --timeout 60
python3 scripts/m87_consumer_benchmark.py \
  --baseline /absolute/path/to/baseline-ball-pusher-tests \
  --candidate build/judas_ball_pusher_application_tests \
  --output /tmp/m91-consumer-new --profile 0
python3 scripts/m87_cold_validation.py --run --output /tmp/m91-cold-new
```

The matrix driver retains its original physical acceptance conditions and negative
controls. Repeated interleaved 1024-object controls, explicit allocation-instrumented
cases and missing-contact/frozen-mechanism controls have exact command receipts in
`raw/scaling-verified-repeated/`, `raw/alloc/` and `raw/negative/`.

The imported-rig comparison uses the unchanged observer
`docs/evidence/m78-followup/probes/concave_final.cpp` (SHA-256
`f0ce020eb47613ae82bb2619cdc03e6b477782b744576be2b9a36a52f5079136`).
It loads the actual 13-body Skate rig and cooks a 64x64/8192-triangle floor;
it is **not** a run of the whole Skate game or a new mesh surrogate for that game.
`raw/concave/LINK.json`, `FIXTURE.json`, the compile helper and raw CSVs retain its
inputs/links. Four cases run 900 steps each at 60 Hz. No forced sleep or pose repair.

## Ordinary demo / editor / export

```sh
build/judas projects/physics_stress_lab/physics_stress_lab.judasproj
build/judas_editor projects/physics_stress_lab/physics_stress_lab.judasproj
build/judas_export projects/physics_stress_lab/physics_stress_lab.judasproj \
  /tmp/judas-m91-package build/judas
```

**F5** watch tour; **M** motors; **J/K** impulse one/all; **1/2/3** populations;
**4** rotated gravity; **P** spawn; **F6/F7** save/load; **R** reload; **Esc** pause.
The 1024 pile can still stall for seconds. Start with the normal 128 room.

The actual final-source editor/package/cold-save commands are in
`raw/verified_delivery.py`, `raw/verified-delivery/RESULTS.json` and
`raw/verified-cold/RECEIPT.json`. The editor uses a private project/config copy;
its duplicate/Undo and Play/Stop leave the authored scene identical. The relocated
package is byte-verified before launch, runs with no project argument from an
unrelated `/tmp` directory and without `JUDAS_ENGINE_ROOT`, and exercises all four
scenes/pause/reload. The test script is `raw/verified-moved.script`.

## Provenance and failures

`ARTIFACTS.json` maps original to stored bytes and both SHA-256 identities.
Large CSVs, profile captures and source snapshots use deterministic lossless gzip;
original byte hashes remain available. No executable, object/static archive, core,
private environment dump or unrelated game asset is committed as evidence.
Some early diagnostic prototypes retain source snapshots and logs rather than a
complete reproducible dependency bundle; they are rejected experiments, not final
acceptance. Source hashes and actual measured receipts take precedence over old
folder names such as `final-dense`, `closed-*` and `native-final`. The authoritative
final-source receipts use **verified** names, listed by the current report.

The kernel's `perf_event_paranoid=4` prevented hardware sampling; the original
permission error is retained. No host security setting was changed. M56 aggregate
phase timers/counters and matched wall measurements provide the diagnosis.

Run `python3 docs/evidence/m91/verify.py` to verify this checkpoint's source and
stored evidence. Add `--git` to check the real Git index blobs and `--binaries`
to check retained local builds. These are byte/provenance checks, not a rerun or
human acceptance. The [packaging follow-up](PACKAGING_NOTE.md) records a corrected
CSV newline-normalization issue; original numerical bytes are preserved.
