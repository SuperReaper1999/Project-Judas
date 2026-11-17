> Stage A historical evidence used the original dirty physics experiment.
> The approved production reconciliation and current qualification are in
> [RECONCILIATION.md](RECONCILIATION.md) and [SOURCE_SELECTION.md](SOURCE_SELECTION.md).

# Native desktop whole-application baseline

Started 2026-11-16; captures completed 2026-11-17. This is an initial measured baseline, not a release, sustained-stability or universal performance qualification. No engine implementation was changed to obtain these results.

## Configuration and scope

- Ryzen 5 5600, 6 cores/12 logical processors, approximately 15 GiB physical RAM; GTX 970 4 GiB, NVIDIA 580.178.04, native OpenGL 4.6 on Kubuntu/Ubuntu 26.04.1.
- Exact executable identity: `2.0.0-alpha.1-Experimental-ca0d437-dirty-0e93fdf8cbbf-ud1f3a815687b`. Every trial records executable/project/workload hashes and embedded identity in its invocation/run receipts.
- 1280x720 drawable, modern graphics policy, ordinary project quality/shadow policy, uncapped, achieved swap interval 0. All nineteen profiled captures report zero inactive sample frames, zero resource failures and no script fault.
- Thirteen frozen public workloads, ten measured seconds per trial after two seconds of warmup and the existing resource-quiet startup boundary. Animation, vehicle and combined have three trials; others have one. No compilation or concurrent test/benchmark ran beside capture.
- The source generator froze ca0d437 projects; the unmodified advanced-water project was added from that same Git commit before any captures. Final workload SHA256: `7d23a9a86eee2b31eab12e223b510fb01927b766a27d94f6998a4326414c9fea`.
- External OS/NVIDIA sampling, the five-second repository guard and two-second per-capture size monitor are included diagnostic overhead. GPU telemetry includes the desktop, startup and warmup and is not per-process GPU utilization.
- Loading screenshots/preparation are separate from measured gameplay; startup receipts retain loading-frame/preparation timings. This does not measure cold-boot shader compilation or every exceptional collision.

## Profiled frame intervals

Values are p50 / p95 / p99 / maximum in milliseconds, measured from actual Application frame boundaries. A range spans trial results; maximum is the worst individual frame across those trials. RSS is the highest sampled gameplay resident set, not a whole-system memory figure. These traced timings include profiling overhead.

| Workload | Trials | p50 ms | p95 ms | p99 ms | Max ms | Frames >16.67 ms | Longest such sequence | RSS MiB |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| lightweight | 1 | 0.954 | 1.056 | 1.481 | 4.982 | 0 | 0 | 142.3 |
| town | 1 | 1.233 | 2.020 | 2.876 | 5.666 | 0 | 0 | 180.7 |
| populated-rendering | 1 | 8.250 | 9.253 | 14.266 | 18.409 | 5 | 4 | 224.0 |
| animation | 3 | 10.964–11.414 | 12.033–12.520 | 17.492–19.693 | 26.341 | 39 | 13 | 191.0 |
| animation-ik | 1 | 14.572 | 16.600 | 26.098 | 56.406 | 32 | 9 | 193.2 |
| particles | 1 | 3.222 | 4.016 | 4.377 | 5.836 | 0 | 0 | 152.4 |
| basic-water | 1 | 1.158 | 1.455 | 1.611 | 3.951 | 0 | 0 | 142.5 |
| terrain-excavation | 1 | 0.948 | 1.425 | 1.614 | 3.578 | 0 | 0 | 206.2 |
| terrain-large | 1 | 0.754 | 1.204 | 1.389 | 2.998 | 0 | 0 | 213.6 |
| vehicle | 3 | 1.195–1.197 | 2.525–2.556 | 2.938–3.069 | 6.317 | 0 | 0 | 186.3 |
| fleet | 1 | 1.209 | 3.726 | 4.192 | 6.769 | 0 | 0 | 186.3 |
| combined | 3 | 2.903–2.929 | 7.343–7.466 | 8.050–8.944 | 15.344 | 0 | 0 | 226.5 |
| advanced-water | 1 | 1.371 | 10.883 | 16.091 | 20.414 | 14 | 7 | 225.6 |

All nineteen traced runs report zero incoming-delta clipping, zero discarded physics seconds and zero catch-up-cap frames. That is observed for these short workloads, not proof for arbitrary stalls. The animation-IK capture has 702 truncated detailed records; its frame intervals remain recorded, but detailed attribution is incomplete. Other captures report no detail truncation or dropped events.

## Profiler-disabled controls

One fresh ten-second control per case uses the same executable, content, quality, resolution and wall-clock simulation with profiling disabled. These single controls are not an interleaved regression campaign. They show that traced tails cannot be equated with shipping profiler-off behavior. Simulation-loss counters are unavailable in this mode and are **unknown**, not zero.

| Workload | p50 ms | p95 ms | p99 ms | Max ms |
|---|---:|---:|---:|---:|
| animation | 10.539 | 11.019 | 11.332 | 14.029 |
| combined | 2.761 | 6.621 | 6.847 | 9.928 |
| populated-rendering | 7.810 | 7.910 | 7.984 | 8.645 |
| animation-ik | 13.638 | 14.602 | 15.223 | 15.571 |
| advanced-water | 1.365 | 10.319 | 15.089 | 16.644 |

The 129-character and IK controls exceed the 8.33 ms / 120 FPS target even at the median. The populated-rendering control has p99 7.984 ms; the combined control has p99 6.847 ms. These five single controls show no >16.67 ms frame, but they do not certify sustained 60 Hz, exceptional-impact latency, platform coverage or lost simulation time. The aggressive 5 ms completion goal remains unqualified.

## Rendered content and simulation verification

- Native screenshots were visually inspected. Large terrain shows the landscape, with 256 chunks, 524288 triangles and about 50 MiB geometry in its HUD; it is not a sky-only timing.
- Combined shows the full actor group and live particles. Its final public state oracle reports 32 animation-ready actors, all 32 supported at y=2.9973, 5000 GPU particles and 46 contacts.
- Particle stress screenshot reports 100000 live GPU particles, actual contacts and zero query failures.
- Animation has 129 character-motor instances and 136 physics bodies in frame counters, with the crowd visibly rendered; submitted triangles are nonzero.
- Vehicle telemetry shows the selected car genuinely driving, braking and reversing while others remain at rest. Fleet telemetry shows all four authored vehicles moving, with support counts 4/6/8/4. Initial zero-speed screenshots were not used as the motion oracle.
- Advanced-water screenshot shows the active pool, floating/submerged props, 181.528 cubic metres of water and active grid cost. Counters include 23 physics bodies and more than 62000 visible water-surface vertices.
- Terrain-excavation and combined satisfy the existing workload-state gates; no fixture was weakened to obtain a fast run.

## CPU/GPU interpretation and decisions

The existing calibrated report tool computes max(CPU end, GPU end) minus CPU frame start. It does not add CPU and GPU durations. In the first traced combined trial, estimated same-frame completion p99 is 12.192 ms, with median GPU queue lead 3.135 ms and calibration half-bracket about 14 microseconds. This estimate includes diagnostics and queueing and excludes compositor/scanout; it is not inverted into FPS. The profiler-off control has no comparable completion trace.

The first animation trace averages 8.198 ms in the enclosing World render submission scope, including 5.595 ms in Lighting shadow planning. Source inspection shows that PrepareShadows includes actual shadow rendering via its draw callback: that scope is not solely key construction or planning. Its children/parents overlap and must not be summed. The sampled process uses about one CPU core equivalent; the resource-worker busy fraction is zero in the steady interval. Neither result proves that more workers or a new physics solver would improve whole-frame latency.

Prioritize a bounded rendering/shadow submission and instrumentation investigation for the animation workload before choosing a physics replacement. Separate preparation, actual shadow draws, driver work and profiler collection with lean counters. Preserve the same lights, cascades, poses, physical behavior and asset fidelity. A passing relative gate alone would not establish the absolute product budgets.

## Editor overhead and visual limitation

The supported SDL/X11 observer measured Willow Town in the unmodified editor, 1280x800 primary window, fresh preferences, ten seconds per mode. Profiling is enabled to verify the requested mode. The game viewport is smaller than the standalone 1280x720 surface, so these are not matched-resolution GPU comparisons.

| Mode | p50 ms | p95 ms | p99 ms | Max ms | RSS MiB |
|---|---:|---:|---:|---:|---:|
| edit | 0.612 | 0.671 | 0.801 | 2.385 | 194.2 |
| play | 1.463 | 2.144 | 2.929 | 4.647 | 215.7 |

The observer mode checks pass, but X11 screenshot readback returns error 8. The existing native EditorAutomation screenshot path succeeds and confirms actual town content. It also reveals overlapping/clipped text in the default small Game view. A standalone 681x386 harness run reproduces the layout issue, so this is an application responsive-layout problem rather than evidence of an editor/runtime render mismatch. The source shrinks panel rows while retaining text sizes and clamps its layout reference to at least 640x480. Fast presentation does not qualify this UI as usable.

## Evidence ownership

Compact current results: `evidence/performance-results.json`, three selected `*-analysis.json` files, final build/input identities and the audit launchers. Raw frames, hardware samples and native screenshots belong only to the current bounded `.cache/platform-audit` campaign and may be discarded after review; do not archive them indefinitely. Hitches, sequence lengths, memory, diagnostics and simulation-loss details remain in the compact results.
