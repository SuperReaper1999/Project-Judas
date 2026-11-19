# Vulkan and OpenGL parity in V2 Alpha

OpenGL is the default. Use `JUDAS_GRAPHICS=vulkan` on a Vulkan-enabled Linux
build to select Vulkan. See the [setup and SDK requirements](VULKAN_INTEGRATION.md).
The experimental native mesh path is outside this assessment. It is a separate
opt-in and does not own the ordinary Vulkan renderer.

## What the backends share

`EngineHost` and `Window` select the device and install graphics entry points.
The scene, animation, camera, Lucifer, terrain, particles, water and UI owners
submit to the same renderer on both backends. OpenGL resolves the entry points
from its driver. Vulkan supplies the engine's OpenGL-shaped dispatch functions
and records native Vulkan commands. `VkShader.cpp` compiles the shared GLSL to
SPIR-V, `VkDraw.cpp` owns pipeline state and bindings, `VkTexture.cpp` owns image
operations, and `VkCore.cpp` owns queues, image synchronization, resource
retirement and swapchain presentation. Vulkan uses an offscreen default target
and blits it to the swapchain with the required orientation. Physics, save formats,
navigation, JudasJS gameplay and game-side contracts stay shared.

Parity below applies to the demonstrated feature and test conditions. Native
tests used Xvfb and Mesa llvmpipe. Hardware image comparisons used the GTX 970
with NVIDIA 580.178.04. Neither substitutes for human editor acceptance.

## Capability matrix

| Area and status | Evidence and scope | Remaining work and priority |
| --- | --- | --- |
| **Core renderer — parity confirmed for exercised paths** | `VkDraw.cpp`, `VkTexture.cpp`, `VkShader.cpp`. Renderer submission, pass-state, compute and renderer-lab checks cover materials, texture reads, target state, depth, blending, culling, instancing, indirect draw, uploads and readback. Explicit portable compute rejection and modern GPU ownership remain separate paths. | The bridge implements engine-used graphics operations. Legacy stencil state, non-default depth range, scaled depth blits and generic framebuffer sRGB encoding are outside the present renderer contract. No shipping renderer call site uses these missing operations for its supported effect. |
| **Same-layout image writes — functional gap fixed** | `VkCore.cpp`, `VkTexture.cpp`. On baseline `cbe17127`, synchronization validation found an unordered depth copy after a clear in `portal-camera`, an atlas upload after a clear in `lighting-lists` and repeat atlas uploads in `runtime-ui`. Pixel assertions passed despite the hazards. The bridge now issues image dependencies for writable layouts even if the layout stays the same. | High-severity ordering defect, with a targeted synchronization gate in Linux CI. Keep broader draw, compute and WSI checks separate from prospective barrier optimization. |
| **Lucifer — parity confirmed for exercised paths** | `src/lighting`, `RendererScale.inl`. Native tests exercise shadow sampling and cache reuse. Camera tests exercise view-local directional and local lighting policy. `lighting-lists` checks clustered GPU lists against the CPU layout and rendered output. Exposure queries use shared simulation transforms and collision, with the standalone `lighting-exposure` suite. | Additional projects, overlapping local shadow budgets and new vendors expand scope. No measured Vulkan-specific regression established in this audit. |
| **Terrain V2 — scoped parity, small image differences** | `src/terrain`, `src/editor`, shared render passes. Transition, planet rendering, and editor preview tests exercise coverage, height, material, LOD and target readback. The Terrain V2 workshop had up to 0.044% of pixels with a channel difference above 8/255 under the matched hardware captures. The repeat OpenGL run also varied. | Extend qualification to long streaming sessions and live authoring sequences that paint, excavate, undo and reload. |
| **Particles V2 — scoped parity confirmed** | `src/particles`, `ParticlePass.cpp`, `GpuParticlePass.cpp`. `particle-gpu-resident` exercises actual compute, collision, lifetime and draw on the modern tier. The particle lab rendered on both APIs. Unsupported GPU combinations use the shared capability policy and explicit CPU path by design. | Long-duration GPU memory and user-driven editor preview coverage remain open. |
| **Basic Water and Advanced Fluids V2 — scoped parity, small image differences** | `src/fluid` owns Advanced Water simulation and coupling on the CPU. `WaterPass.cpp` consumes its presentation data with ordinary colour and depth transfers. Water workshop and native water visuals run the expected passes. Matched Advanced Water samples had at most 0.025% of pixels above 8/255. | Extended live editor preview, long visual runs and combined cross-backend persistence stress are unverified. The shared numerical tests cover the solver independently of image comparisons. |
| **Animation and entity visibility — scoped parity confirmed** | Shared pose and visibility preparation supplies the mesh pass. Animation studio fixed-tick captures and recorded simulation rows matched to within one image channel value and exact telemetry. The shared glTF importer rejects morph targets on both backends. | Animated structural fracture and physical animation under varied scenes need matched GPU captures. The native physics tests alone establish no visual parity for those cases. |
| **Cameras and composition — scoped parity confirmed** | `Renderer.cpp`, `RendererMaterials.inl`, shared camera projection. Native camera and portal cases exercise target readback, per-view shadow policy, off-axis projection, oblique clipping, origin rebasing, target recreation and clean ownership. The renderer lab and UI showcase compare fixed frames before and after two SDL resizes. | More combined portals and render-to-texture chains remain unverified. General scaled depth blits are outside the supported camera contract. |
| **Runtime UI, text and maps — scoped parity confirmed** | `RuntimeUI.cpp`, shared UI shaders. `runtime-ui` exercises text and texture ownership, clipping paths and frame capture. UI showcase pixels were exact in all three matched hardware frames. The accepted `6d85a304` map qualification separately covered hardware bake, crop, marker alignment, clamp, resize and relocated exports on both backends. | More font/locale combinations and long texture-atlas churn are unverified. |
| **Editor — single-window and Linux X11 detachment qualified** | `EditorVulkanViewports.cpp` installs ImGui renderer callbacks over the shared renderer. `editor-v2-detached-viewports` tests native Scene and Game windows, shared texture pixels, move, resize, close, three detach/reattach cycles and Play/Stop. GTX 970 and llvmpipe additionally passed minimize and restore with Openbox on an isolated X display and synchronization validation enabled. | Vulkan detachment requires the [present-fence extensions](VULKAN_INTEGRATION.md#detached-editor-windows). [Operator confirmation](#editor-detachment-evidence) covers Scene and Game detach and re-dock in KWin/XWayland. Snapping, monitor changes, fractional scaling and sustained editing in that session remain unverified. |
| **Device and presentation — scoped parity and unverified recovery** | `Window.cpp`, `VkCore.cpp`. Device recreation, application restart, resize, resource retirement and owner rejection have executable coverage. Vulkan skips swapchain work while minimized and recreates on out-of-date events. Device loss currently requires restart. A bounded retry covers initial device creation, and explicit Vulkan selection rejects a missing driver. | Minimize/restore beyond the tested X11 environment, live device-loss injection/recovery and prolonged WSI stress are unverified. Do not infer hot recovery from a successful recreate after normal shutdown. |
| **Platforms and distribution** | `JudasVulkan.cmake` enables Vulkan only on desktop Linux. Hosted Ubuntu 24.04 checks both backends in relocated SDK and standalone exports and enforces the glibc 2.39 ceiling. The Vulkan-capable SDK links the Vulkan loader even when OpenGL is selected. | Windows is unsupported project-wide. Android and Web use their own GLES and WebGL paths and have no Judas Vulkan backend. Other platforms and other physical GPU vendors have no parity qualification in this assessment. |

The matching GTX 970 workload reports are correctness checks, even though they
run on hardware. The established comparison uses a second OpenGL run to
measure repeatability, a 0.1% limit for pixels more than 8/255 apart in a channel,
and exact recorded simulation telemetry. A few edge pixels can exceed 8/255.
The visual threshold applies only alongside scene content and the feature's
numeric assertions. Queue pacing and GPU elapsed time are separate measurements.
Earlier hardware evidence at `f3c3e6cb` applies to its measured game and driver.

A separate GTX 970 check compared `cbe17127` with the synchronization repair
in the renderer lab at 1280×720. Three interleaved runs per build used the
uncapped hidden-window real-time harness, identical content, and shallow
profiling. The last 120 frames per run gave these median run statistics:

| Vulkan build | p50 | p95 | p99 | Worst |
| --- | ---: | ---: | ---: | ---: |
| Before | 1.309 ms | 1.675 ms | 1.697 ms | 1.752 ms |
| After | 1.327 ms | 1.691 ms | 1.797 ms | 1.835 ms |

The sampled runs recorded no discarded simulation time. The observed p99
ranges overlap (1.688–1.840 ms before and 1.742–1.864 ms after). This short
instrumented test cannot establish a performance gain or rule out a small
cost. It measures the application frame interval and does not measure physical
presentation latency.

The same control on the frozen combined terrain, actor and particle fixture
measured Vulkan p50/p95/p99/worst of 1.017/4.460/4.519/4.538 ms before and
1.026/4.512/4.584/4.604 ms after, with zero recorded discarded simulation time.
The combined p99 trials ranged from 4.517 to 4.537 ms before and from 4.525
to 4.666 ms after. These short checks are evidence about the dependency
change in the selected workloads. Broader backend performance comparisons
need independently controlled captures.

## Editor detachment evidence

On Experimental [`eecef3a3`](https://github.com/SuperReaper1999/Project-Judas/commit/eecef3a31ccd323d749bf176937e0e90bb05e72b),
the operator confirmed that both the Scene and Game panels could be detached and
docked again on the Kubuntu Plasma desktop with SDL X11 through XWayland, using
the GTX 970 and NVIDIA 580.178.04. This acceptance covers those two interactions
on that setup. Native Wayland uses single-window docking.

The [Ubuntu 24.04 Vulkan/SDK run](https://github.com/SuperReaper1999/Project-Judas/actions/runs/44632228483)
on that commit exercised both APIs' detached-window tests and the full registered
suite. That run used the software graphics CI environment.

## Reproduce

Use a Vulkan-enabled Release build with the Khronos layer and
`JUDAS_ENABLE_VALIDATION=ON`. Run the registered cases through the same runner
used by [Linux CI](../.github/workflows/linux-vulkan.yml).

```sh
cmake --build build --target judas_portal_camera_tests judas_lighting_lists_tests judas_runtime_ui_tests -j2
VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT \
  xvfb-run -a python3 scripts/prealpha_validate.py --graphics-backend vulkan \
  --graphics-profile modern --test portal-camera --test lighting-lists --test runtime-ui
```

Use the same tests with `--graphics-backend opengl` for the control. Select
additional cases through the [validation manifest](../tools/validation/suites.json).
The [comparison utility](../tools/vulkan/compare_backends.py) captures OpenGL,
repeat OpenGL and Vulkan from one runtime. Use a fresh output directory and
the same driver, tier, project, resize schedule and frame list across runs.
