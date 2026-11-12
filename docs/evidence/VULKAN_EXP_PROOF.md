# Vulkan-Exp proof: measured results

Tested source: `74f2ae0d86d535cb7be751ca5bb7eab054ee6c7a` (branch `Vulkan-Exp`, based on
Experimental `d9a80f6c`). Release build, GCC, Ubuntu 24.04 container, 4 CPU threads,
Xvfb 1280x720x24. **No hardware GPU**: OpenGL ran on Mesa 25.2.8 llvmpipe
(OpenGL 4.5 core), Vulkan on Mesa 25.2.8 lavapipe (Vulkan 1.4.318) with the
Khronos validation layer 1.3.275 enabled. These are automated results only;
nothing here is human validation, hardware-driver evidence or performance
qualification. Design and limitations: [VULKAN_BACKEND.md](../VULKAN_BACKEND.md).

## 1. Focused gates, both backends, same binaries

`scripts/prealpha_validate.py` with every manifest group (184 gates), 400 s
timeout, once per backend (`--graphics vulkan` for the second run).

| Backend | Passed | Failed | Failing gates |
|---|---:|---:|---|
| OpenGL (llvmpipe) | 182 | 2 | `renderer-shader-library`, `shadow-sampling` |
| Vulkan (lavapipe) | 182 | 2 | `renderer-shader-library`, `renderer-compute` |

- 54 of the 184 gates create a graphics device; under `--graphics vulkan` all
  54 logged the Vulkan device. The other 130 do not render, so their Vulkan
  result says nothing about the backend.
- Vulkan validation messages across all gate logs: **0**. Backend warnings: 0.
- `renderer-shader-library` fails identically on both backends ("canonical
  built-in matches its reviewed portable shader identity"). It fails on
  unmodified OpenGL, so it is not caused by this branch; it was not investigated.
- `shadow-sampling` fails on OpenGL/llvmpipe ("unassigned/out-of-range shadow
  slot remains lit") and passes on Vulkan. Not investigated; it may be specific
  to the software OpenGL driver.
- `renderer-compute` fails 2 of 59 checks on Vulkan. Both release the OpenGL
  context with `SDL_GL_MakeCurrent(nullptr, nullptr)` and expect compute calls
  to be refused; the Vulkan device has no "no current context" state, so these
  two checks do not apply. The other 57, including compute-generated geometry
  drawn through an indirect command, pass.
- `tests/GpuParticlesTests.cpp` creates its own graphics context. Its host was
  extended to create the Vulkan device when `JUDAS_GRAPHICS=vulkan` is set;
  without that change the gate would have silently tested OpenGL. All 63 checks
  (resident GPU particles, collision kernels, indirect draw) pass on Vulkan.

Defects this run found and fixed in the backend before the final numbers:
per-stage shader binding collisions, varyings matched by position instead of
name, indexed sampler-binding queries returning zero (which made the water pass
drop caller sampler bindings), reporting unused interface blocks that OpenGL
hides (which broke GPU particle dispatch), std430 block sizes, decoded readback
of block-compressed textures, default point size, and missing 64-bit shader
features. They are listed because each was a real behavioural difference that
image comparison of the sample projects alone did not reveal.

## 2. Sample projects, pixel comparison

`tools/vulkan/compare_backends.py`, profile `auto` (modern renderer tier on both
backends), 40 frames, capture at frame 30, all 24 projects. Every run exited 0
with 0 validation messages and 0 backend warnings. Percentages are of pixels.
"GL vs GL" is two identical OpenGL runs: it is the project's own run-to-run
variation. The Vulkan column is the smaller of its difference to the two
OpenGL captures.

| Project | GL vs GL differing % | Vulkan vs GL differing % | > 8/255 % | Max channel delta | Mean channel delta |
|---|---:|---:|---:|---:|---:|
| 2d_traversal_workshop | 0.0 | 0.0 | 0.0 | 0 | 0.0 |
| ai_workshop | 0.0 | 0.0 | 0.0 | 0 | 0.0 |
| animation_studio | 0.0 | 0.0005 | 0.0 | 1 | 0.0 |
| audio_lab | 0.8403 | 0.0 | 0.0 | 0 | 0.0 |
| basic_water_lab | 0.0 | 2.2235 | 1.895 | 37 | 0.5073 |
| beacon_grove | 0.0 | 0.0 | 0.0 | 0 | 0.0 |
| browser_player_lab | 0.0 | 0.0003 | 0.0001 | 38 | 3e-05 |
| connected_workshop | 0.0 | 0.0131 | 0.0009 | 178 | 0.00043 |
| core_scramble | 0.0 | 0.0042 | 0.0014 | 204 | 0.00054 |
| developer_lab | 0.0 | 0.0004 | 0.0 | 1 | 0.0 |
| graffiti_workshop | 0.0 | 0.0342 | 0.0 | 1 | 0.0002 |
| lucifer_workshop | 0.0 | 0.0023 | 0.0 | 4 | 1e-05 |
| menu_workshop | 0.0 | 0.0017 | 0.0006 | 26 | 9e-05 |
| particle_v2_lab | 0.0 | 0.0366 | 0.0005 | 72 | 0.00047 |
| physics_stress_lab | 0.0 | 0.0117 | 0.0084 | 185 | 0.00404 |
| renderer_v2_lab | 0.0 | 0.0572 | 0.0046 | 227 | 0.00207 |
| sprite_animation_workshop | 0.0 | 0.0 | 0.0 | 0 | 0.0 |
| terrain_v2_planet_workshop | 0.0519 | 0.2972 | 0.0567 | 213 | 0.04526 |
| terrain_v2_workshop | 0.047 | 0.2267 | 0.0257 | 178 | 0.02055 |
| tilemap_workshop | 0.0 | 0.0 | 0.0 | 0 | 0.0 |
| ui_showcase | 0.0 | 0.0 | 0.0 | 0 | 0.0 |
| vehicle_v2_workshop | 0.0 | 0.0017 | 0.0005 | 56 | 0.0001 |
| water_v2_workshop | 0.0332 | 0.0387 | 0.0193 | 162 | 0.00973 |
| willow_town | 0.0 | 0.0088 | 0.0025 | 70 | 0.00096 |

Reading the table:

- 7 projects are bit-identical; 21 of 24 differ in under 0.06 % of pixels,
  typically single pixels on geometry edges.
- `audio_lab`, `water_v2_workshop` and both terrain projects already differ
  between two OpenGL runs (asynchronous loading/paging and an on-screen timing
  readout), so part of their Vulkan difference is not the backend. The terrain
  projects still differ more against Vulkan (about 0.23–0.30 %) than OpenGL does
  against itself (about 0.05 %); that residual was not traced to a cause.
- `basic_water_lab` is the largest difference (2.2 %). It is a band of
  horizontal stripes where a shoreline surface z-fights with the ground. The
  stripes are present in the OpenGL capture too; the per-pixel winner differs.
  Using the device's OpenGL-convention depth transform did not remove it, so the
  exact cause of the differing pattern is **not established**. It is the one
  visible difference a person could notice and should be judged on hardware.

## 3. Editor

`judas_editor` with `JUDAS_EDITOR_AUTOTEST` on Willow Town ran to completion
on both backends (exit 0, 0 validation messages), with the editor UI, scene
viewport, gizmos and Play all drawn through Vulkan. Edit-mode capture: 0.25 % of
pixels differ from OpenGL. The Play capture differs by 0.8 %, but the two runs
executed a different number of simulation steps before the capture, so that
figure is not a rendering comparison. Detached editor windows are disabled on
Vulkan and were not exercised. The editor gate group is included in section 1.

## 4. Not measured (software-rasterizer phase; section 5 covers hardware)

- Any hardware GPU or vendor driver (NVIDIA/AMD/Intel), Windows, Android, browser.
- Frame time, hitching, memory or any budget in `AGENTS.md`. Software
  rasterizer timings were deliberately not recorded as performance evidence.
- Long sessions, window resize while rendering, device loss, multi-monitor.
- Human play-testing of any project or the editor.

## 5. Hardware results: GTX 970 (after rebasing onto Experimental 65a0400b)

Machine: Ryzen 5 5600, GeForce GTX 970 4 GiB, NVIDIA 580.178.04, Vulkan 1.4.312,
Kubuntu (Wayland session). Startup line:
`[Graphics] Vulkan backend: NVIDIA GeForce GTX 970 (API 1.4.312, driver 0x912c8100), depth D24, device clip control, BC 1`.
Automated only: **not human-tested**. Raw captures stay on that machine.

### Correctness

- Gates (rendering, render, particles, water, game-water, ui, editor, terrain, 2d):
  OpenGL 117/117. Vulkan 116/117 with validation on, **0 validation messages in
  59 runs**; `renderer-compute` fails only its 2 checks that release an OpenGL
  context (not applicable to Vulkan). Experimental fixed the earlier
  `renderer-shader-library` failure.
- `compare_backends.py` on all 24 projects: all run, 0 validation messages; only
  the known `basic_water_lab` (0.16 %) and terrain workshop (0.22–0.25 %)
  differences exceed 0.05 %. Renderer lab cases 2 and 5 (with shadows), 4, and 6
  (monitors + split views), and Lucifer at frames 120/390, were also compared:
  all within 0.01 %, except case 4 (particles) at 1.6 % of pixels, almost all
  differing by at most 8/255 (blending rounding).
- Resize to 1600×900, 320×200 and back on all three depth paths (D24, D32F,
  shader depth remap): correct capture sizes, 0 validation messages.
  Minimise and fullscreen were not exercised.

### Defects found on hardware and fixed

1. Swapchain semaphores were reused while still pending (validation errors in 9
   gates). Now a pool of acquire semaphores and one present semaphore per image.
2. Tests set vsync with `SDL_GL_SetSwapInterval`, a no-op on Vulkan, so they ran
   FIFO-capped (Willow Town timed out). They now use `GraphicsSetSwapInterval`.
3. Every buffer lived in host memory: the 100k GPU particle stress ran at about
   **1 FPS**. GPU-owned buffers (`*_COPY` usage, or immutable storage without
   CPU access flags) now prefer device-local host-visible memory.
4. Latency was bounded by in-flight *submissions*: frames that flushed mid-way
   (the particle simulation's async readback) serialised CPU and GPU, costing
   about 4 ms on every simulation step. Now bounded by frames.
5. `glGet(GL_TIMESTAMP)` returned 0, breaking benchmark GPU clock calibration.

### Performance: same binary, OpenGL vs Vulkan

`run_matrix.py --baseline-graphics opengl --candidate-graphics vulkan`, with
`prepare_workloads.py` inputs at the Vulkan-Exp revision, 1280×720, VSync off,
uncapped, validation off, 3 alternating 20 s trials per backend (IK 45 s).
Medians across trials; worst is the worst of all trials. Gate = the existing 10 %
relative p99 regression check, with Vulkan as the candidate.

| Workload | Avg FPS GL → VK | p50 ms | p99 ms | Worst ms | Gate |
|---|---:|---:|---:|---:|---|
| lightweight | 1047.2 → 1141.1 | 0.95 → 0.87 | 1.10 → 1.01 | 1.65 → 5.95 | pass |
| town | 760.0 → 815.3 | 1.23 → 1.14 | 2.73 → 2.63 | 6.61 → 5.43 | pass |
| populated-rendering | 125.6 → 137.7 | 7.93 → 7.17 | 8.62 → 8.01 | 14.94 → 12.46 | pass |
| animation | 108.3 → 140.6 | 10.31 → 6.07 | 11.08 → 9.15 | 23.06 → 14.02 | pass |
| particles | 303.4 → 252.4 | 3.21 → 3.93 | 4.06 → 4.47 | 5.92 → 6.58 | **fail** |
| basic-water | 809.9 → 1027.1 | 1.20 → 0.94 | 1.41 → 1.17 | 7.56 → 2.59 | pass |
| terrain-excavation | 1005.5 → 920.7 | 0.96 → 1.06 | 1.40 → 1.44 | 14.47 → 4.75 | pass |
| terrain-large | 1328.2 → 1321.0 | 0.71 → 0.72 | 1.20 → 1.16 | 11.12 → 6.10 | pass |
| vehicle | 766.4 → 786.8 | 1.23 → 1.19 | 2.67 → 2.44 | 10.01 → 4.51 | pass |
| fleet | 715.1 → 729.2 | 1.24 → 1.21 | 3.70 → 3.63 | 5.57 → 6.48 | pass |
| combined | 273.8 → 372.9 | 2.81 → 2.05 | 6.95 → 5.98 | 12.37 → 14.10 | pass |
| animation-ik | 81.4 → 102.6 | 13.49 → 11.65 | 15.38 → 13.53 | 211.36 → 189.49 | rejected¹ |

¹ `animation-ik`: the comparison tool rejects the case because trials on
**both** backends overflowed the profiler (truncated records) during
190–210 ms hitches, so no gate result exists. Its medians are for information
only. The same hitch appears on OpenGL, so it is not a Vulkan defect; it should
be investigated on Experimental.

Reading the table:

- Vulkan is faster or equal in 10 of the 11 gated cases (largest gains:
  animation, combined, basic water).
- **Particles fails the gate**: p99 +10.1 %, median frame +22 %. Both backends
  are GPU-bound; the GPU frame is about 3.9 ms on Vulkan vs 3.2 ms on OpenGL.
  Traced (temporary GPU scopes and timestamps, all reverted): see
  "Particle draw trace" below.
- One Vulkan `lightweight` trial in an earlier run had a single 59 ms
  `Swap present wait` stall; in the final run the worst frame was 5.95 ms.
  Cause unknown.
- p99 passes do not certify product budgets: the `combined` 14.1 ms worst frame
  and the IK hitches still need the usual exceptional-frame review.

### Particle draw trace (100k GPU particle stress, 1280×720)

Measured with temporary GPU profiler scopes around the particle view
preparation and draw (same timer queries on both backends), and a temporary
timestamp pair around the Vulkan indirect draw alone. All diagnostics were
reverted; only `JUDAS_VULKAN_DISPATCH_TIMING` remains.

| GPU work per frame | OpenGL | Vulkan |
|---|---:|---:|
| View preparation (cull + 131k-key bitonic sort + gather, ~55 dispatches) | 1.81 ms | 1.81 ms |
| Particle draw (`glDrawArraysIndirect`, 100,000 instances × 6 vertices) | 0.80 ms | 1.19 ms |
| Same draw at 640×360 | 0.73 ms | 1.28 ms |

- The gap is the **draw call itself**. It does not scale with resolution, so it
  is per-vertex/per-instance work, not fill rate or blending. Both backends draw
  exactly 100,000 instances (indirect command read back on each).
- Restarting the render pass after the view compute costs nothing measurable.

Ruled out by A/B measurement on the same binary (no measurable change):

1. Narrower compute barriers (compute→all after, all→compute before) instead of
   a full barrier after every dispatch: 252 vs 252 FPS.
2. glslang SPIR-V optimisation: the system glslang ignores
   `disableOptimizer = false` (identical SPIR-V), so it could not be tested.
3. Replacing the shader's dynamically indexed local corner arrays with
   arithmetic: 1.26 → 1.21 ms (noise; OpenGL 0.82 → 0.72).
4. Omitting the backend's injected `gl_PointSize = 1.0` write: 1.19–1.25 ms
   either way.
5. Placing the per-frame uniform ring in device-local memory: 1.19 vs 1.18 ms.

Not yet tried: a vendor GPU profiler (Nsight Graphics) on this draw, and the
non-instanced formulation (6N vertices, per-particle data fetched from the
records buffer by `gl_VertexIndex / 6`), which changes engine code shared with
OpenGL.

## 6. Native execution (docs/VULKAN_NATIVE.md), GTX 970

Same machine and method as section 5. Automated only: **not human-tested**.

### Where the time went before this work (`perf`, user-space samples, Vulkan)

| Scene | Frame | Backend draw path | Engine scene walk and material logic |
|---|---:|---:|---:|
| 129 animated characters (2,640 draws) | 6.8 ms | about 2.5 ms | about 3.2 ms |
| Renderer lab (206 draws, about 8,500 object visits) | 7.2 ms | about 0.3 ms | about 5.9 ms |

The backend share was measured by making draws return early; the engine share
is what remained. In the renderer lab the top samples are `memcmp`,
`RuntimeWorld::FindEntity`, `RuntimeDefinition`, `RenderLayerOf` and material
key construction: engine code shared by both backends.

### Stages

| Stage | Measured effect | Kept |
|---|---|---|
| Draw-state cache | 129 characters: 6.5 → 4.95 ms median frame; 83 % of draws reuse state | yes |
| Bone palettes as dynamic-offset storage blocks | 4.95 → 4.2 ms; backend draw functions fall to about 3 % of samples | yes |
| Persistent SPIR-V and pipeline caches | Renderer lab, empty caches: 116 ms compiling + 250 ms for the first 5 pipelines; warm: about 5 ms + 1 ms | yes |
| GPU particle draw fetching records from storage | 100k stress: GPU frame 3.97 → 3.75 ms, p99 4.50 → 4.12 ms | yes |
| `GL_STATIC_DRAW` buffers in device-local memory | No measurable change in four scenes (A/B, two trials each) | no, reverted |

### Correctness after all stages

- Gates: OpenGL 117/117; Vulkan 116/117 with validation on, 0 validation
  messages in 59 runs (`renderer-compute`: the same 2 OpenGL-context checks).
- `compare_backends.py`, 24 projects: all run, 0 validation messages, the same
  three known differences (water 0.16 %, terrain 0.24–0.25 %). Renderer lab
  lights+shadows 0.002 %, monitors+split views 0.009 %, Lucifer 0.001 %,
  particle cases unchanged from section 5.

### Same binary, OpenGL vs Vulkan, after all stages

| Workload | Avg FPS GL → VK | p50 ms | p99 ms | Worst ms | Gate |
|---|---:|---:|---:|---:|---|
| lightweight | 1041.0 → 1152.9 | 0.95 → 0.86 | 1.04 → 0.93 | 1.81 → 3.22 | pass |
| town | 771.6 → 833.9 | 1.21 → 1.12 | 2.69 → 2.59 | 4.96 → 5.47 | pass |
| populated-rendering | 126.0 → 137.9 | 7.90 → 7.16 | 8.59 → 8.03 | 19.06 → 15.38 | pass |
| animation | 108.0 → 201.7 | 10.34 → 4.15 | 10.93 → 7.12 | 28.15 → 9.17 | pass |
| particles | 302.5 → 262.2 | 3.22 → 3.78 | 4.08 → 4.31 | 5.26 → 100.67 | pass |
| basic-water | 803.2 → 1021.7 | 1.21 → 0.95 | 1.40 → 1.20 | 111.90 → 3.07 | pass |
| terrain-excavation | 1003.0 → 894.0 | 0.96 → 1.09 | 1.40 → 1.44 | 3.34 → 4.56 | pass |
| terrain-large | 1320.4 → 1283.8 | 0.71 → 0.74 | 1.21 → 1.18 | 5.69 → 32.40 | pass |
| vehicle | 767.3 → 787.6 | 1.23 → 1.19 | 2.55 → 2.36 | 5.86 → 8.24 | pass |
| fleet | 716.8 → 732.0 | 1.24 → 1.21 | 3.62 → 3.49 | 5.99 → 6.64 | pass |
| combined | 278.7 → 492.3 | 2.76 → 1.55 | 6.69 → 5.47 | 12.00 → 11.72 | pass |
| animation-ik | 81.9 → 144.8 | 13.38 → 4.63 | 15.35 → 11.33 | 194.77 → 259.90 | rejected¹ |

¹ As in section 5: `animation-ik` trials on both backends overflowed the
profiler during 190–260 ms hitches, so the tool gives no gate result.

- All 11 gated workloads pass; particles now passes (p99 +5.6 %, median still
  +17 %: the remaining gap is the instanced draw itself, see section 5).
- Largest gains over section 5: animation 140.6 → 201.7 FPS, combined
  372.9 → 492.3 FPS, animation-ik 102.6 → 144.8 FPS.
- `terrain-excavation` is 11 % slower in average FPS on Vulkan (p99 equal).
  Not traced.
- Single slow frames on Vulkan: particles 100.7 ms (no profiled scope active)
  and terrain-large 32.4 ms (inside the SDL event pump). OpenGL had a 111.9 ms
  frame in basic-water. These are outside rendering; cause not established.

### Startup failures observed

Twice, a Vulkan run started in the same second the previous graphics process
exited failed in `vkCreateDevice` with `VK_ERROR_DEVICE_LOST`; the kernel
logged NVIDIA Xid 32 and Xid 13 at those moments. Both happened before any
rendering by the failing process. Eleven targeted repeat runs (with and without
the draw cache and disk caches) did not reproduce it, and the final 72-launch
matrix had none. Device creation now retries up to four times. Cause not
established; if it recurs, check `journalctl -k | grep Xid`.

### Not done

- Pipelines are still created at first use (from the warm cache), not before
  the first frame.
- The mesh passes still reach the backend through OpenGL-named calls; only the
  particle draw has a Vulkan-specific formulation.
- The engine scene walk that dominates populated scenes is unchanged.
