# Whole-engine performance

Measure the actual application and its displayed content. Passing isolated kernels,
functional tests or relative regression gates does not qualify an empty scene,
all hardware or an absolute gameplay budget. The current
[development boundary](DEVELOPMENT_READINESS.md) distinguishes usable supported
features from stress/performance and device limits.

## Measurement contract

Reference goals are 120 FPS / 8.33 ms, 60 FPS / 16.67 ms minimum in a declared
workload, and a 5 ms combined-engine critical-path stretch goal. These are whole-
engine goals, not separate allowances for every subsystem or arbitrary-scene promises.

Record executable SHA/build identity, source and frozen input hashes; CPU/GPU/driver,
resolution, viewport policy, VSync/cap, selected graphics/particle/light policies;
p50/p95/p99/max, exceptional sequences and sample counts; process RSS and GPU
accounting; fixed steps, incoming clipping, discarded backlog and catch-up caps.
Loading, warmup, steady gameplay, exceptional impacts and editor overhead stay separate.

CPU and GPU overlap. Nested scopes overlap their parent. Neither their sums nor
summed worker CPU times are frame latency. GL-call elapsed time includes driver
waits and is not GPU execution duration. Queue-inclusive completion differs from
submission throughput. Capture/PNG/diagnostic observation costs must remain visible.
Detailed profiler truncation makes attribution incomplete; a lower-depth capture
can answer a bounded question but cannot erase a larger profiler-off hitch.

Do not lower fidelity, discard time, move a camera off-map, background an app or
change content to obtain a pass. Inspect actual images, live actor/particle counts,
contacts/support and terrain revisions. Preserve failed measurements and explain
whether they are invalid observers, real regressions or unassigned hitches.

## Reproducible workload preparation

Run from a clean source checkout using a matching build. Reuse a valid build and
one bounded current campaign; [the storage guard](WORKSPACE_STORAGE.md) never deletes
outputs automatically. Never run benchmarks beside compilation or another game.

```sh
python3 tools/workspace_storage.py run -- cmake --build build \
  --target judas_whole_engine_benchmark judas_present_capture judas_terrain_author judas_scene_author -j2
python3 tools/benchmarks/prepare_workloads.py --revision HEAD \
  --out .cache/current-performance/inputs \
  --large-terrain-tool build/judas_terrain_author --scene-tool build/judas_scene_author
python3 tools/benchmarks/run_matrix.py --baseline /path/to/reference-benchmark \
  --candidate build/judas_whole_engine_benchmark \
  --workloads .cache/current-performance/inputs --out .cache/current-performance/comparison \
  --cases animation animation-ik combined --seconds 20 --repeats 3 \
  --resolution 1280x720 --cap 0 --no-profile
```

Outputs must be new owned paths. Preparation reads the specified Git revision,
not local edits; `WORKLOADS.json` freezes exact content. Supply the terrain and
scene tools to generate the large/combined cases. The matrix freezes executables,
interleaves trials, checks workload identities/live outcomes and computes relative
p99 regression results. IK enforces its longer observation duration. The default
10% relative allowance is not an absolute latency or human-acceptance certificate.

Available cases include lightweight gameplay, Willow Town, populated rendering,
animation/IK, particles, Basic Water, terrain excavation/large traversal, vehicle/
fleet and combined terrain/32 supported actors/5,000 particles. Required-GPU content
must not silently become CPU content. Portable-policy controls use explicit expected
CPU particle outcomes. Private vehicle workloads require independently owned assets;
public source/SDKs do not depend on private downloads or old temporary directories.

For a single capture use `run_whole_engine.py --help`; include its workload manifest,
intended timestep/inputs and declared graphics policy. `report_capture.py` reports
outcomes and completion intervals. `compare_captures.py` compares matched captures.
`observe_editor.py` observes real Edit/Play through the existing presenter rather
than substituting an isolated render loop. Existing SDL harness inputs and
[the test harness](TEST_HARNESS.md) drive real applications.

## Attribution and comparison controls

Use `JUDAS_PROFILE_DEPTH` to keep a complete bounded trace. With profiling active,
`JUDAS_PROFILE_RENDER_DETAIL=1` aggregates colour/shadow material binding and GL
draw CPU elapsed/call counts. The benchmark's observation scope exposes snapshot/
JSON overhead. Counters report actual clock loss even with detailed profiling off.

`JUDAS_SHADOW_COALESCE=0`, `JUDAS_SHADOW_REUSE=0`, `JUDAS_RENDER_BATCHING=0` and
explicit light-list/graphics policies are diagnostic controls. Record them; do not
mix them between supposedly matched runs. Use one implementation per responsibility.

## Current scoped baseline

The retained [qualification receipt](qualification/rendering.json) records all
18 matched trials and exact measured executable/input/device identities. The
baseline/current median per-trial p99 values were:

| Workload | Reference ms | Current ms | Worst current frame ms |
| --- | ---: | ---: | ---: |
| Animation | 11.302 | 9.747 | 13.452 |
| Animation and IK | 14.569 | 12.134 | 33.142 |
| Combined | 6.553 | 6.267 | 8.568 |

These captures preserved geometry, lights, simulation and all requested steps:
zero incoming clipping/discarded backlog/catch-up caps. The bounded change combines
compatible opaque shadow ranges instead of repeating per-part binds/draws. Full
shadow-depth/triangle equivalence covers modern and portable NVIDIA policy.
The animation workload has 129 characters; 16 independent IK actors submit
64 effector targets, not 129 IK solvers or 64 physical contacts.

A complete 5,154-frame shallow IK trace had no lost records/time, max 14.977 ms,
332.5 shadow draws/frame, about 0.518 ms GL draw CPU elapsed and 0.177 ms material
binding. Callback observation max was 0.411 ms. It does not explain or erase the
33.142 ms profiler-off hitch. An earlier incomplete 78 ms frame remains unattributed.
Animation/IK still miss the 120 FPS target. Severe dense vehicle/CCD/material
impacts and private-asset footprints retain content-dependent limits; those older
stress results are not converted into fresh measurements of this snapshot.

## Maintenance

Retain concise qualification receipts needed for current support decisions; raw
captures belong to the current bounded campaign and CI retention, not an ever-growing
tracked archive. Git records superseded experiments. When implementation or workload
changes, select affected cases plus a combined application regression, record new
identities, and revise the supported boundary honestly. A source-only documentation
change does not justify repeating an unchanged expensive performance campaign.
