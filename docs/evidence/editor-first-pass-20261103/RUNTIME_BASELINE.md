# Runtime baselines for the first editor pass

The engine is the copied Release Linux Experimental build from
`84b94833df172c9962c61bff6e5a9d8597c95cec`. `EXACT_BUILD.json` carries the
in-process source receipt. The adjacent baseline-bin manifest fingerprints each
copied executable. Workload input hashes are in `FIXTURE_SHA256SUMS`.

All runs use a 640 × 360 SDL offscreen surface and Mesa llvmpipe with two driver
worker threads. The fresh 2D, Lunar Island and live physics cases use the stock
application's `JUDAS_TEST_SCRIPT` with `REALTIME`: actual elapsed times feed fixed
steps and every application frame renders. That harness uses blocking resource
admission and swap interval zero. The rendering observer uses the ordinary loop
and waits for the initial load, then records all 360 rendered frames. The
separate fixed physics route also renders, with one controlled clock tick per
frame, and its per-step digests establish reproducibility independent of GPU
pacing. An observer requests a zero-clock quit frame after its counted frames.

The routes exercise the previously authored fresh 2D top-down project,
Lunar Island's opening coast after the welcome overlay, Rendering Lab's authored
watch tour, and the Physics Stress Lab's scenario 3 with injected W/A/S/D, look
and an all-shelf disturbance. The injected inputs pass through ordinary project
bindings. The last three are isolated project copies. Each process owns its own
save/data, cache and configuration roots. Audio uses SDL's dummy driver where
applicable. The separate miniaudio backend may probe and fall back to its null
backend here. These captures do not exercise physical audio output, visible
desktop scheduling, GPU hardware, or physical input devices.

The profiler retains the final bounded history. Application interval statistics
use those retained frame-start intervals. Per-step scopes are separate from
outer work. Driver work can appear in CPU submission, including software
rasterization, and GL timestamps on this machine describe the software
implementation. Report the recorded profiler drop counters alongside timing.
`/usr/bin/time -v` supplies complete process maximum RSS. Render and fixed-physics
observers also retain CSVs for earlier frames and steps. Fixture hashes cover
runtime inputs rather than generated profile and user-data files.

From the checkout root:

```sh
p=.cache/editor-first-pass-20261103
python3 "$p/baseline-runtime/measure.py" \
  --bin "$p/baseline-bin" --out "$p/baseline-runtime/second-exact"
sha256sum -c "$p/baseline-runtime/FIXTURE_SHA256SUMS"
```

Use a fresh result directory. Repeating a run on the same fixture and backend
checks measurement noise. Performance effects require an independently matched
candidate build, especially for the realtime cases whose simulation steps depend
on elapsed frame times.

## Observed at the exact baseline

Times are median / nearest-rank p95 / worst in milliseconds. The application
interval includes actual work and software-driver waits. These are single-run
diagnostic baselines.

| Workload | Samples | Outer interval | Fixed step | Peak RSS |
|---|---:|---:|---:|---:|
| Fresh 2D workflow, route complete | 119 retained frames | 4.53 / 5.48 / 7.49 | 0.054 / 0.083 / 0.086 (33 steps) | 241.5 MiB |
| Lunar Island, route complete | 119 retained frames | 111.17 / 142.27 / 150.81 | 3.09 / 6.15 / 9.05 (783 steps) | 1494.4 MiB |
| Rendering Lab, stock 50-frame route complete | 40 after warmup | 727.66 / 817.98 / 1421.89 | 0.014 / 0.090 / 0.168 (320 steps) | 266.8 MiB |
| Physics Scenario 3, movement and shelf impulse complete | 119 retained frames | 59.17 / 99.99 / 108.43 | 2.80 / 4.50 / 8.19 (498 steps) | 432.7 MiB |
| Physics Scenario 3, controlled 240-tick observer complete | 210 steps after warmup | Synthetic clock | 3.85 / 12.60 / 22.37 | 399.3 MiB |

The 360-frame Rendering Lab observer timed out at 120 seconds. Its 221
flushed CSV records survive, 191 after warmup. The final 119-frame profile
also survived. No 360-frame completion or screenshot
was available from that attempt. Its partial interval median/p95/worst was
548.84/816.08/904.25 ms. The shorter stock application route is a different
execution boundary, so these rows do not form a before/after comparison.
The short run retained zero incomplete CPU records and all 40 retained frames
hit the fixed-step cap. World render submission had median 726.92 ms, while
shadow submission's narrower owner-thread scope had median 9.27 ms.

The 2D player physically moved from (0, 2, 0) to (1.561, 3.043, 0) at frame
180. Lunar Island exited the welcome pause and the player advanced more than
17 m by frame 150. Lunar's retained window hit the fixed-step cap 36 times,
discarding 0.253 simulated seconds. It had 153 truncated profiler records,
so its phase attributions are partial. The 2D capture had two dropped GPU
queries with zero dropped CPU events or truncations.

The Scenario 3 input route verified the named scene and an active kick action.
The camera moved from (37.05, 21.35, 48.99) at frame 225 to
(17.40, 21.35, 29.03) at frame 270. Peak sampled body speed was zero at frame
225 and 0.239 m/s at frame 255 after the all-shelf disturbance. The live
retained window hit no fixed-step cap. Its median world render scope was
47.11 ms, a software result. The separate fixed route had no nonfinite fields,
1024 authored loose pieces and 0.671 mm deepest independently measured final
penetration. Its trajectory digest sequence SHA-256 is
`1329eae3fbcda8c66a6239f1701b70ece8b85dd76233b3677340664bdfce32ba`.
The fixed-clock observer's authored motor tour is not the earlier
900-step perturbed MK2 experiment.

See each run's `receipt.json` for exact commands, data, GL identity, scope
details and binary fingerprints. Raw profiles are losslessly gzipped and
each receipt records its pre-compression hash. The completed short render
route can be repeated separately with `--case rendering-harness`.

The supplementary `runtime-profile-off` logs replay the same fresh 2D and
live scenario 3 routes with `JUDAS_PROFILE=0`, using the same baseline
runtime and SDL/Mesa settings. They record whole-process wall time and
peak RSS, with 2.91 s / 266.7 MiB for fresh 2D and 20.12 s / 344.8 MiB
for scenario 3. The off runs omit per-frame profiler statistics. A first
off run selected the 128-piece project startup rather than its 1,024-piece
scene and failed the body-count assertion. Its log is retained separately.
