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

## 4. Not measured

- Any hardware GPU or vendor driver (NVIDIA/AMD/Intel), Windows, Android, browser.
- Frame time, hitching, memory or any budget in `AGENTS.md`. Software
  rasterizer timings were deliberately not recorded as performance evidence.
- Long sessions, window resize while rendering, device loss, multi-monitor.
- Human play-testing of any project or the editor.
