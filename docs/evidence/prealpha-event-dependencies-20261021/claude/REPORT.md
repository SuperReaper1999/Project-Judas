# Judas 1,024-piece physics slowdown — independent hardware-scaling and root-cause investigation

Date: 2026-10-20. Independent investigation; no Judas source, branch, test or evidence was modified,
committed or pushed. Repository working tree remained clean at `3ecf26cd`.

## Bottom line

Faster hardware does **not** resolve the stalls; it executes the same, growing, mostly redundant work
faster. Workload is bit-identical across machines/compilers. Step cost tracks the number of CCD
re-queries (r = 0.988), which grows with the size of the connected pile, not with the number of
collisions (r = 0.14). Each TOI event re-solves and re-schedules nearly the whole pile; ~95% of that
re-scheduling recomputes an unchanged "no impact" answer.

## 1. Hardware and workload identity

| | Cloud (this run) | i7-4790 (repo evidence) |
|---|---|---|
| CPU | Intel Xeon @ 2.10 GHz, family 6 model 207 (Emerald Rapids), 4 vCPU, 1 thread/core, KVM | Haswell 4C/8T, 4.0 GHz turbo |
| Measured clock | 2.36–2.73 GHz effective (fluctuating, dependent-loop probe) | — |
| PMU | Not exposed (perf software `cpu-clock` only) | — |
| Memory | 15 GiB | ~16 GB |
| GPU | None — Mesa llvmpipe software GL on Xvfb | GTX 970 |
| Compiler | GCC 13.3 and GCC 14.2 | GCC 15.2 |
| OS | Ubuntu 24.04, kernel 6.18 (Firecracker/KVM) | Linux desktop |

- Built `be1b7d63` (the commit described by `docs/PREALPHA_DENSE_SUPPORT_FOLLOWUP.md`) from a scratch clone.
  All 510 frozen engine/build/fixture/benchmark inputs are byte-identical to the i7 receipt
  (`docs/evidence/prealpha-dense-support-20261019/final/FINGERPRINTS.json`); the 4 extra entries there are
  unit-test sources not linked into the benchmark.
- HEAD `3ecf26cd` differs from `be1b7d63` only by a whitespace reflow in `RuntimeWorld.cpp` and a
  Windows-only exporter string. No physics source changed.
- Release, `-O3 -DNDEBUG` (same as i7 CMakeCache). Fixture `room1024.judas` SHA-256 `a2caf62a…05bf` matches.
- 60 Hz, 8-step catch-up cap, `--profile 0`, 270 steps via `scripts/physics_dense_measure.py`.
- Ryzen 5 5600 figures were operator-supplied and are not in the repository; their revision, digest
  and OS could not be verified.

## 2. Matched 270-step benchmark (fixed-step ms)

| | Cloud GCC13 (3 runs) | Cloud GCC14 (3 runs) | i7 | Ryzen |
|---|---|---|---|---|
| Mean | 56.26 / 59.98 / 59.39 | 53.68 / 55.56 / 58.25 | 46.83 | 30.20 |
| Median | 28.26 / 29.74 / 29.80 | 29.64 / 29.46 / 30.06 | 22.23 | — |
| p95 | 182.06 / 206.63 / 197.43 | 163.77 / 189.68 / 184.53 | 158.74 | 105.49 |
| Worst | 301.00 / 333.95 / 353.38 | 286.21 / 314.45 / 376.55 | 387.28 | 178.17 |

Physical correctness (proven): all six runs end at digest `d4261f69366bc1d4`, identical to the i7;
all 270 rows (trajectory digests, contacts, impact events/queries/iterations) match the i7 exactly
(`compare/`). Endpoint: 1,024 pieces, 9,269 candidate pairs, 2,529 contact points, 0 uncertain/
unsupported, deepest penetration 7.187 mm.

Controls:
- `LP_NUM_THREADS=1` and `=2` (software renderer threads) leave fixed-step cost unchanged
  (57.2–59.6 ms mean, same digest): renderer contention does not inflate the physics numbers.
- Physics is effectively single-threaded: three job workers received ~1 s CPU each versus ~38 s on
  the main thread over a run.
- Profiled run (`--profile 1`): mean 60.45 ms, same digest. Not mixed into the table above.

## 3. Long-run / live slowdown (35 s real-time, two runs)

- 336 steps, 44 rendered frames; 1.19 mean FPS; frame intervals median ~0.5 s, max 2.78 / 2.83 s.
- 5.6 simulated seconds in 36 s wall: 0.155× real time (i7 evidence: 0.170×).
- 42 of 43 frames hit the 8-step catch-up cap → frame ≈ 8 × step cost. Two-second frames are
  8 × ~230–250 ms steps.
- Physics = 89.6% of wall time even with ~65 ms/frame software rendering; rendering 9.3%.
- Real-time stepping is bit-identical to exact mode for all 270 overlapping steps (proven):
  the stalls are the same deterministic physics work, not cadence-induced.
- Cost keeps rising with the pile (traced 400-step run; trace overhead included):

| Steps | Mean fixed ms | Events/step | CCD queries/step |
|---|---|---|---|
| 1–50 | 24.8 | 25.0 | 1,307 |
| 51–150 | ~26 | 1.6–3.0 | 258–621 |
| 151–200 | 65.6 | 21.3 | 19,872 |
| 201–250 | 90.5 | 7.1 | 30,366 |
| 251–300 | 226.4 | 9.6 | 89,739 |
| 301–350 | 289.7 | 13.3 | 112,736 |
| 351–400 | 362.0 | 15.8 | 134,655 |

All 1,024 loose pieces remain awake through step 400.

Conclusion: stalls originate in physics stepping, specifically continuous-impact (CCD event)
handling; fixed-step catch-up multiplies them into multi-second frames; rendering is not the cause.

## 4. Profiler evidence

Engine profiler (last 120 steps, dense phase): "Physics continuous impacts" 9,006 ms of 11,989 ms
rigid physics (75%). Narrowphase 1,128 ms, broadphase 428 ms, ordinary velocity solve 560 ms.
Totals: 1,529 events, 4.01 M CCD queries (≈2,600/event), 3.77 M rejected by conservative bounds.

perf (DWARF call graphs, debug-info build with identical codegen flags plus `-g`, 330 steps):
`AdvanceImpacts` = 72.9% of `PhysicsWorld::Step`. Inside it:

| Share | Work |
|---|---|
| 22.2% | Island discovery and event rows (`PairAt`, `ComputeContacts`, `QuerySlots`, `Observe`) |
| 16.8% | `ImpactSolver::Solve` (10-sweep PGS over island rows; `SolveVelocities` 54% of it) |
| 16.5% | CCD re-queries (`ImpactTime`) |
| 18.2% | Schedule bookkeeping (unordered_map lookups 8.8%, key sort/binary search 9.4%) |
| 10.2% | Event loop body (linear earliest-event scan, dirty marking, assign/clear) |
| 6.5% | Trajectory/proxy refresh (`CoverStepReach`, motion append) |
| 4.2% | Support continuation / spin checks |

Linear regression over 336 real-time steps: solver_ms ≈ 3.84 ms + 2.25 µs × CCD queries, r = 0.988;
versus impact events r = 0.14.

Per-event trace (read-only observer in a scratch copy; `trace/read-only-event-trace.patch`;
0 mismatches against the unmodified binary over 336 steps):

| | Steps 1–150 | Steps 151–270 | Steps 271–400 |
|---|---|---|---|
| Events | 859 | 1,502 | 1,701 |
| ms per event | 0.17 | 5.09 | 20.34 |
| Island bodies (mean / max) | 4 / 12 | 198 / 975 | 802 / 974 |
| Contact rows (mean) | 8 | 524 | 2,098 |
| Bodies with changed velocity (mean) | 4 | 187 | 737 |
| … of which Δv < 1 cm/s | 30% | 50% | 77% |
| Pair queries re-scheduled per event (mean) | 80 | 2,689 | 10,100 (of ~11,850 scheduled) |
| Re-scheduled queries returning identical "no impact" | 30.1% | 81.9% | **94.7%** |
| Re-scheduled queries becoming imminent (≤ dt) | 0.03% | 0.06% | **0.04%** |

Required vs repeated work: the ~13 events/step are genuine contacts. The amplification is the
per-event global treatment — whole-island narrowphase and solve, plus re-scheduling every pair of
every body whose velocity changed by any bit (`SamePosition` is exact float equality).

Secondary observation: `ImpactSolver::Solve` deduplicates bodies with a linear `find_if` per contact
row (O(rows × bodies)); measured ≤13.7% of solve, ~2% of event time. Not a primary cause.

## 5. Hardware scaling

| Metric | Cloud ÷ i7 | i7 ÷ Ryzen |
|---|---|---|
| Mean | 1.250 | 1.551 |
| Median | 1.317 | — |
| p95 | 1.231 | 1.505 |
| Worst | 0.851 | 2.174 |

- Proven: identical work everywhere; the per-step cloud/i7 ratio is similar for light and heavy
  steps (≈1.26 vs 1.14–1.29), consistent with single-thread throughput rather than memory/cache effects.
- Inference: i7→Ryzen 1.55× matches the single-thread Haswell→Zen 3 gain; the cloud's 0.8× fits a
  ~2.5 GHz shared vCPU with newer IPC. More cores do not help (parallel phase ≈7% of a step).
- Compiler: GCC 13 vs GCC 14 within run-to-run noise, bit-identical results. GCC 15 not tested.
- Worst-step values are single-sample and noisy (repo records a 262.6 ms worst for the same physics on the i7).
- Projection (inference): late steps (271–336) average 254 ms on the cloud → ~203 ms i7 → ~131 ms
  Ryzen. 60 Hz real time needs ≈15 ms/step: roughly 9× short of Ryzen, and cost is still rising.

## 6. Root causes and recommended corrections

Proven root cause: each time-of-impact event is processed globally. A local impact grows an island
through all touching contacts (up to the whole pile), recomputes its contact geometry, solves it,
then re-schedules CCD for every pair touching any body whose velocity changed bitwise. Per-step cost
≈ events × pile size, and the pile keeps growing and never sleeps.

Recommendations (architectural direction; untested):
1. Re-schedule only when the change can affect the answer — invalidate a pair's cached no-impact
   result only if the velocity change breaks its conservative bound (targets the ~95% redundant re-queries).
2. Localise impact islands — bound impulse propagation (graph depth / impulse threshold) or treat
   resting stacks with a resting-contact/shock-propagation method rather than full PGS per event.
3. Batch near-simultaneous events within a time window into one solve.
4. Replace the linear earliest-event scan and per-event hash/sort bookkeeping with a priority queue
   and incremental indices (~28% of event time; secondary).
5. Investigate why the dense pile never sleeps.
6. Catch-up policy: an 8-step cap with expensive steps produces multi-second frames; fewer steps
   per frame with slow-motion fallback keeps responsiveness (symptom treatment only).

Items 1–3 change event chronology and require separate physical qualification. The repository's
rejected experiments (BFS ordering, parallel TOI, faster primitives) are consistent with this:
the fix must change event scope, not primitive speed.

Untested hypotheses: how many events/step remain after island localisation; whether retaining only
the ~5% of re-queries whose answer changed preserves exact trajectories.

## Bundle index

| Path | Contents |
|---|---|
| `runs/exact-unprof-{1,2,3}` | GCC 13 matched 270-step runs (RESULTS/CONTEXT/AS_RUN JSON, steps/frames CSV, quality) |
| `runs/gcc14-exact-unprof-{1,2,3}` | GCC 14 matched runs |
| `runs/ctl-lp{1,2}-{1,2}` | Renderer-thread controls |
| `runs/exact-prof-1` | Profiled 270-step run incl. engine profile-final.json (last 120 frames) |
| `runs/rt35-unprof-{1,2}` | 35 s real-time runs |
| `compare/` | Repo compare tool output vs i7 evidence run (all exact passes) |
| `snapshots/snap-gcc{13,14}` | FROZEN.json manifests, build info, CMakeCache, source-input archive |
| `trace/` | Per-event trace CSV, 400-step steps CSV, and the read-only observer patch |
| `perf/` | Inclusive-by-function report and gzipped folded DWARF stacks (perf script output) |
| `host/` | lscpu/memory/OS/compiler capture and clock-probe source |
| `build-logs/` | Configure/build logs |

## Reproduction

```sh
apt install libsdl2-dev libglm-dev xvfb time g++-14   # plus linux-tools for perf
git clone <repo> judas && cd judas && git checkout be1b7d63
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target judas_physics_stress_live_benchmark judas_physics_scaling_diagnostics
python3 scripts/physics_dense_measure.py freeze --build build --output snap
Xvfb :99 &   # only on a headless machine
DISPLAY=:99 python3 scripts/physics_dense_measure.py run --snapshot snap --output runs/exact-1 \
  --mode exact --steps 270 --seconds 100 --timeout 200 --profile 0
DISPLAY=:99 python3 scripts/physics_dense_measure.py run --snapshot snap --output runs/rt35-1 \
  --mode realtime --seconds 35 --timeout 120 --profile 0
python3 scripts/physics_dense_measure.py compare \
  --baseline docs/evidence/prealpha-dense-support-20261019/final/runs/final-support-unprofiled-A1 \
  --candidate runs/exact-1 --output compare.json
```

Event trace: apply `trace/read-only-event-trace.patch` to a separate copy of `be1b7d63`, build, and run
the benchmark with `JUDAS_EVENT_TRACE=/path/events.csv`. Verify trajectory digests against an
unmodified run before using the data. The patch is diagnostic only and is not intended for production.
