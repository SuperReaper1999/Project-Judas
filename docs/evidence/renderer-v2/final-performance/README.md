# Renderer V2: final matched shared-shader measurements

Recorded from **2026-11-05 00:19:32 UTC**, Linux x86-64, Ryzen 5 5600,
GTX 970 (4 GiB), NVIDIA 580.178.04, GCC 15.2.0 Release. This is automated
renderer qualification, not human acceptance or a release.

All **20 fresh-process runs passed**, in **A/B/B/A order per workload**.
All **50,000 measured frames** have complete whole-scene GPU timestamps,
zero dropped GPU records and zero truncated profiler frames. All **28 captured
images**, including eight secondary-view images, are **exactly pixel-identical**
to the first baseline image of their respective workload/view. Images show one
canonical presentation sample; this is not a claim that every timed frame was
read back or every possible scene is equivalent.

## Source and observer identities

- **A:** pre-renderer Experimental checkpoint
  `d7bee296e86dc3f01e401d69512e0c1522aeb525`, using its frozen engine, headers,
  GLAD and build-identity archives. Benchmark SHA-256:
  `bf6340fff09cd8401b89e5f0ee446a06fbff07ae21526d6f321f5ce418ca4998`.
- **B:** dirty Experimental based on
  `6ac95d7ba6e95cc7f12be4f586c8a297637a4ba9`. Embedded tracked-source
  fingerprint:
  `7ff8f7e086cdb08cc22731af1a5a2b0494df4b960aaadca0dbce0b662f160d99`.
  Benchmark SHA-256:
  `710e20f72ce3296866307f98f60dd7e85920e91a6628749aba8173fc1109de89`.
- **Identical observer source:** `tests/RendererV2Benchmark.cpp`, SHA-256
  `5ea7dda731dccd2097c8a9e63bdd9403e4b691c15b6d3c5dd9f1dd5a01543c6e`.
  Each observer links against its own engine headers/ABI. All three hashes
  were independently rechecked after the runs.

`PROVENANCE.json` retains embedded identity interpretation, frozen baseline
hashes and analysis-time renderer file hashes. The latter are explicitly not
a fabricated pre-build source snapshot. A subsequent conservative shadow/LOD
reuse guard correction is outside these **shadow-disabled** workloads; final
checkpoint qualification must identify that later source separately.

## Matched workloads and method

All main views are **1920×1080**; secondary views are **960×540**. Each run
warms 120 frames before measurement. Mixed and repeated cases record 4,000
frames per process; the other cases record 1,500. Scene counts, material
equations, camera positions, alpha ordering and light lists match between A/B.
No physics simulation is included. All five cases request **zero shadow maps**;
shadow correctness/reuse controls are separate and these numbers do not measure
shadow-pass performance.

| Workload | Authored content | Views | Frames per binary, pooled |
|---|---|---:|---:|
| Mixed PBR | 2,048 objects; distinct normal-mapped PBR materials; 8 lights | 1 | 8,000 |
| Repeated | 4,096 repeated objects; no dynamic lights | 2 | 8,000 |
| Ordered sprites | 4,096 alpha sprites with ordering/pivot/flip/filter variation | 1 | 3,000 |
| General particles | 100,000 CPU-presented particles per view | 2 | 3,000 |
| Overlapping lights | 256 objects; 128 overlapping dynamic lights | 1 | 3,000 |

The corrected observer completes secondary views before the main view, matching
production world presentation. CPU submission excludes swap, collection, file
I/O and capture. Process CPU, outer frame intervals and cold initialization are
recorded separately. There is **no per-frame GPU finish/readback**; final query
drain and canonical-image capture are outside measured frames. GPU intervals
can include submission starvation, especially in sprite/particle cases; they
are not independent shader-efficiency measurements and are not additive with
CPU time.

Tables pool raw samples from both runs of each binary and use nearest-rank
percentiles. CSV values retain the observer's original stream precision;
`per-run.json` preserves the original full-precision per-process summaries.
This is a shared desktop, without fixed clocks or a continuous host-load trace.
ABBA ordering and repeat agreement support the observed substantial changes;
individual worst samples remain workload/host observations, not hard bounds.

## CPU submission costs

Milliseconds, **A → B**. All p99 values and process/outer-frame summaries are in
`SUMMARY.json` / `SUMMARY.csv`.

| Workload | Median | p95 | Worst observed | Median cost reduction |
|---|---:|---:|---:|---:|
| Mixed PBR | 2.177 → 1.748 | 2.526 → 1.830 | 9.641 → 2.709 | 19.7% |
| Repeated, two views | 3.538 → 2.012 | 3.703 → 2.050 | 6.666 → 3.540 | 43.1% |
| Ordered sprites | 19.177 → 12.881 | 19.352 → 13.096 | 30.018 → 16.231 | 32.8% |
| General particles | 27.913 → 11.153 | 29.014 → 11.428 | 36.008 → 18.793 | 60.0% |
| Overlapping lights | 0.700 → 0.600 | 1.002 → 0.721 | 2.182 → 1.032 | 14.2% |

## Whole-scene GPU timestamp intervals

Milliseconds, **A → B**. The lighting workload is GPU dominated. The final
shared shader skips Legacy lighting work that PBR did not consume, preserving
the existing PBR output. No reduction of light count, BRDF quality or shadow
filtering is used.

| Workload | Median | p95 | Worst observed | Median cost reduction |
|---|---:|---:|---:|---:|
| Mixed PBR | 2.803 → 1.967 | 3.130 → 2.209 | 9.760 → 3.116 | 29.8% |
| Repeated, two views | 3.637 → 2.111 | 3.954 → 2.203 | 7.095 → 3.783 | 42.0% |
| Ordered sprites | 19.223 → 13.017 | 19.598 → 13.466 | 30.627 → 16.288 | 32.3% |
| General particles | 28.087 → 11.257 | 29.234 → 11.595 | 36.113 → 17.416 | 59.9% |
| Overlapping lights | 21.215 → 14.886 | 21.400 → 15.340 | 22.053 → 16.301 | 29.8% |

## Physical work, uploads and memory

The following counters are constant across every timed sample in each group.
Triangle counts are unchanged; sprite batching only combines consecutive
compatible entries after sorting. It never moves a sprite across an
incompatible intervening draw. Particle count includes both actual views.

| Workload | Draws A → B | Triangles, both | B measured upload bytes/frame | B upload calls/frame |
|---|---:|---:|---:|---:|
| Mixed PBR | 129 → 129 | 24,588 | 246,560 | 131 |
| Repeated, two views | 6 → 6 | 98,328 | 656,960 | 8 |
| Ordered sprites | 4,096 → 1,697 | 8,192 | 328,400 | 1,698 |
| General particles | 2 → 2 | 400,000 | 6,400,000 | 2 |
| Overlapping lights | 3 → 3 | 3,084 | 1,086,240 | 5 |

Baseline actual-upload counters are **unavailable**, not zero. Candidate upload
counters cover view/light/instance/billboard operations, excluding asset
residency, UI, skin palettes and transient water meshes. `instanceBytes` remains
the largest individual instance upload, not total bandwidth. The complete
counter distributions, instance/particle counts and allocated light/shadow
payloads are retained in `SUMMARY.json`.

Peak process RSS across the two fresh runs, MiB:

| Workload | A range | B range |
|---|---:|---:|
| Mixed PBR | 105.3–105.7 | 111.9–171.6 |
| Repeated, two views | 145.7–146.0 | 136.2–136.3 |
| Ordered sprites | 114.9–115.4 | 118.7–119.2 |
| General particles | 164.0–166.3 | 112.7–113.6 |
| Overlapping lights | 114.1–115.8 | 114.3–115.4 |

Memory did **not** uniformly improve. Mixed B's first run reached 171.6 MiB
and 367.4 ms host initialization, compared with 111.9 MiB and 172.1 ms in
the second. The cause of that high-water variation is not established. These
are whole-process peaks including driver allocations and capture, not measured
VRAM or steady-state allocator growth. Renderer-owned appearance payload is
unchanged: 22,809,600 bytes for one-view cases and 28,512,000 for two-view cases.

Cold first-completed-frame ranges, milliseconds, are separately retained:
mixed A 7.38–8.27 / B 6.37–7.06; repeated A 7.02–7.12 / B 7.47–8.95;
sprites A 22.55–23.22 / B 18.08–19.10; particles A 61.30–63.68 / B 21.01–21.62;
lights A 25.94–30.58 / B 19.30–19.97. The repeated case's first completed
frame is slower even though its steady distribution improves. Cold-process
measurements do not imply cold OS/driver caches; none were forcibly flushed.

## Reproduction and retained artifacts

- `RUNS.json`: exact twenty commands, ABBA order, recorded binary/observer
  hashes and per-process execution status.
- `baseline-compile-command.json`: exact observer compilation against frozen A.
- `run-matched-original.py`: verbatim runner snapshot. It uses the historical
  fixed output location; **do not rerun it into existing evidence**. Use new
  output directories for the commands and retain their original arguments/order.
- `per-run.json`: original full `RESULTS.json` metadata, cold timings and
  summaries for all twenty processes.
- `SUMMARY.json` / `SUMMARY.csv`: pooled timing distributions, counter
  distributions, memory, identity and exact image comparison records.
- `RAW_SHA256.json`: byte lengths and hashes for every raw result, CSV, GPU
  history, image and process log under `.cache/renderer-v2/final-matched/`.
  The original cache files remain retained; this compact evidence does not
  duplicate all raw frames.
- `PROVENANCE.json`: source/binary/environment boundaries described above.

Recalculate these summaries without running or rebuilding the engine:

```sh
python3 docs/evidence/renderer-v2/final-performance/analyze.py \
  --raw .cache/renderer-v2/final-matched --out /tmp/new-renderer-v2-analysis
```

The analysis requires Pillow for PNG decoding. It asserts the twenty successful
runs, complete timestamps, untruncated frames, identical workload/build metadata
within each group and exact image equivalence. It does not change raw evidence.

## Rejected experiments and remaining coverage

These final numbers use the **single shared shader**, not the rejected
multiple-program specialization. That earlier prototype's source/binaries,
measurements and one unexplained failed HDR/Unlit invocation remain under
`.cache/renderer-v2/rejected-specialization/` and preceding qualification folders.
Its approximately 23% light-case improvement must not be mixed into these final
ABBA results. Earlier observer runs with an unfinished main HDR pass around a
secondary view are diagnostic failure evidence, not performance acceptance.

This data establishes measured improvements in five fixed native workloads on
one hardware/driver configuration. It does not certify arbitrary cameras,
shadow scaling, weather, skeletal scenes, maximum resource counts, Android,
browser or Windows performance. Portable GL and multi-view shadow correctness
are qualified by their separate tests. No simulated GPU particles, fluid unlock,
Vulkan backend, automatic frame-rate guarantee or human acceptance follows.
