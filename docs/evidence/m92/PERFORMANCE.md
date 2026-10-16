# M92 measured rendering costs

Final-linked paired dataset: `.cache/m92/checkpoint-measure`, 2026-10-16 UTC.
Raw observations and exact binary hashes are in the archive and manifest. Native
Linux i7-4790 / GTX 970, Release, 960×540 main + 480×270 second target, 60 measured
frames after 12 warm-ups. Earlier two alternating-order rounds and a later pair
are also retained; these are repetitions of a bounded workload, not confidence
intervals across every game. No concurrent build or other Judas game ran during
these final measurements. Ordinary desktop load remains.

## Equal-quality wall submission (ms)

Each cell is **median / p95 / maximum**. Swap wait is excluded. The repeated-object
case uses five identical point lights. The shadow case adds a receiving floor,
two ordinary spot sources and two real depth passes per view. These workloads
isolate rendering and contain no simulated bodies.

| Repeated objects | Workload | Views | M91 baseline | M92 candidate | Draws before → after |
|---:|---|---:|---|---|---:|
| 256 | 5 lights | 1 | 1.732 / 1.922 / 2.111 | 0.125 / 0.150 / 0.196 | 256 → 1 |
| 256 | 5 lights | 2 | 3.491 / 4.150 / 4.771 | 0.254 / 0.387 / 0.443 | 512 → 2 |
| 2048 | 5 lights | 1 | 13.626 / 14.726 / 15.050 | 0.526 / 0.605 / 0.788 | 2048 → 1 |
| 2048 | 5 lights | 2 | 27.668 / 28.265 / 28.632 | 1.062 / 1.190 / 1.421 | 4096 → 2 |
| 256 | 2 shadowed spots + floor | 1 | 3.138 / 3.343 / 3.630 | 0.335 / 0.378 / 0.526 | 643 → 3 |
| 256 | 2 shadowed spots + floor | 2 | 6.286 / 6.546 / 6.657 | 0.633 / 0.762 / 0.972 | 1286 → 6 |
| 2048 | 2 shadowed spots + floor | 1 | 24.457 / 25.217 / 26.833 | 1.425 / 1.661 / 1.726 | 5129 → 3 |
| 2048 | 2 shadowed spots + floor | 2 | 49.149 / 50.262 / 51.356 | 2.838 / 3.216 / 3.390 | 10258 → 6 |

All compared per-frame triangle totals agree. Sixteen equal-quality framebuffer
pairs have maximum mean absolute 8-bit channel error
0.0000019290123456790124; the maximum single-channel difference is one.
This is pixel evidence for these views, not a universal image-equivalence proof.

## CPU work and GPU timestamps

The GPU interval spans the whole scene and includes CPU submission bubbles.
Reduced elapsed GPU time therefore does **not** mean an equivalent reduction in
shader ALU/fragment cost. Process CPU is measured with `std::clock` through the
swap call; it is shown separately from wall submission.

| 2,048 objects, two views | Stage | Process CPU ms (median / p95 / max) | GPU elapsed ms (median / p95 / max) |
|---|---|---|---|
| 5 lights | baseline | 27.682 / 28.335 / 28.704 | 27.698 / 28.401 / 28.682 |
| 2 shadowed spots | baseline | 49.190 / 50.350 / 51.449 | 45.370 / 47.217 / 47.723 |
| 5 lights | candidate | 1.117 / 1.277 / 1.478 | 1.091 / 1.243 / 1.339 |
| 2 shadowed spots | candidate | 2.910 / 3.238 / 5.021 | 2.378 / 3.032 / 4.670 |

## Expanded lighting, not a matched-quality speed comparison

M91 only shades the first five sources even when given 128. M92 shades all
visible sources below its documented limits. For 2,048 objects and 128 submitted
sources, candidate wall submission is:

| Views | CPU wall ms (median / p95 / max) | GPU interval ms (median / p95) |
|---:|---|---|
| 1 | 0.629 / 0.898 / 1.582 | 1.644 / 1.675 |
| 2 | 1.423 / 1.682 / 1.704 | 2.410 / 2.667 |

## Memory

Peak process resident memory reported by `/usr/bin/time -v` across all cases in
one observer process (includes driver/profiler/host allocations):

| Process workload | Baseline KiB | Candidate KiB |
|---|---:|---:|
| objects | 92668 | 102148 |
| shadows | 92080 | 104064 |

Owned GPU payload estimates: original fixed three D24 shadow maps ~12 MiB;
candidate starts at one ~4 MiB layer, grows to ~12 MiB for the same two spot
shadows, ~20 MiB for the lab's sun/four spots, and at most ~36 MiB for nine layers.
High-water storage is retained. Final direct observers use at most 48 KiB light
grid storage and ~160 KiB instance upload; lab grid high water is 176 KiB.
These estimates are not measured total VRAM, allocation peaks during growth or
driver bookkeeping. No-light startup avoids allocating a grid; a formerly lit
view retains its high-water grid rather than freeing it every frame.

## Ordinary lab watch tour

Final wall-clock application run: default 1,024 modules, 128 authored sources,
sun + four selected spot shadows per actual view, two 384×216 camera screens
updated every second frame, scripts, HUD, 1,024×768 main window. 360 frames with
30 discarded warm-up frames; swap interval one. There is no deterministic frame
advance in this measurement. The first observed interval was 299.556 ms (including startup before the first
frame hook), with 45.762 ms submission; both remain in the raw CSV and are
excluded explicitly from the warm summary.

| Metric | Median | p95 | Maximum |
|---|---:|---:|---:|
| Wall application submission (ms) | 6.622 | 17.115 | 20.226 |
| Observed frame interval (ms) | 17.120 | 17.885 | 20.363 |
| Process CPU (ms) | 6.622 | 17.082 | 20.227 |
| Last fixed step (ms) | 0.064 | 0.131 | 0.191 |
| Draw calls, all passes | 8.000 | 25.000 | 25.000 |
| Instances submitted, all passes | 1594.000 | 4818.000 | 4837.000 |

Effective rate over the 330 measured intervals: **58.40 FPS**.
Zero script faults. The cadence alternates costly secondary-view frames with
main-only frames. Shadows still submit caster geometry for each view; this is
visible in the tail rather than obscured by an average. The fixed-step entry is
the last completed step observation, not an aggregate across multiple catch-up
steps. This lab has no physics bodies and is not a physics performance result.
