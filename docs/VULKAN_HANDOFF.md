# Vulkan-Exp handoff: what to test next and how to work on it

For whoever (person or agent) continues the Vulkan experiment on a machine
with a real GPU. Read this first, then [VULKAN_BACKEND.md](VULKAN_BACKEND.md)
(design, controls, limitations) and
[evidence/VULKAN_EXP_PROOF.md](evidence/VULKAN_EXP_PROOF.md) (what was measured).

## State at handoff

- Branch `Vulkan-Exp`, rebased onto Experimental `65a0400b` (originally based on
  `d9a80f6c`). Not merged anywhere.
  Do not push this work to `Experimental` or `main` without the operator asking.
- `JUDAS_GRAPHICS=vulkan` runs the runtime and editor on Vulkan. OpenGL is the
  default and its code path is unchanged apart from the small host edits listed
  in VULKAN_BACKEND.md.
- Everything so far was measured on **software rasterizers** (llvmpipe OpenGL,
  lavapipe Vulkan) in a headless container. On those: 182/184 gates pass on
  each backend, 0 validation messages, 24/24 sample projects run and match
  closely.
- **Update:** steps 1–3 and 5 below have since been run on a GTX 970 (NVIDIA
  580.178.04). Five hardware-only defects were found and fixed, the gates and
  image comparison are clean, and Vulkan matches or beats OpenGL in 10 of 11
  gated workloads; GPU particles remain about 10–20 % slower. See section 5 of
  [evidence/VULKAN_EXP_PROOF.md](evidence/VULKAN_EXP_PROOF.md). Still **never
  human-tested** (step 4), and no AMD/Intel or Windows results exist.

## How it works, in one page

The renderer (`src/Renderer*.{cpp,inl}`, `src/render/*`) calls OpenGL-named
functions through GLAD's function-pointer table. `EngineHost::Init` fills that
table by calling `gladLoadGL` with a loader:

- OpenGL: `SDL_GL_GetProcAddress` (the driver).
- Vulkan: `JudasVulkan::ProcAddress` (`src/vulkan/`), which returns Judas
  functions that keep OpenGL-style state and record Vulkan commands.

So a call such as `glDrawArrays` in `Renderer.cpp` lands in
`DrawArrays` in `src/vulkan/VkDraw.cpp`. To find the Vulkan implementation of
any call, search `src/vulkan/` for `table["glTheName"]`. An entry point that is
not in a table is null: calling it crashes with a null function pointer, which
is the usual signature of "this path uses a call the backend lacks".

| File | Owns |
|---|---|
| `VulkanBackend.h` | Public interface used by `Window`, `EngineHost`, `GraphicsApi.h`. |
| `VkCore.h` | All internal types: `Context` (global `g`), GL-style state `GlState`, object tables. Start here. |
| `VkCore.cpp` | Instance/device selection, memory blocks, command submission (`Flush`, `PollCompleted`), swapchain, `Present`. |
| `VkShader.cpp` | GLSL → SPIR-V (glslang), binding/varying unification, SPIR-V reflection, uniform storage, program introspection. |
| `VkTexture.cpp` | Textures (created lazily in `RealizeTexture`), samplers, framebuffers, readback, blits, pixel conversion. |
| `VkDraw.cpp` | Buffers (copy-on-write in `PrepareBufferWrite`), vertex arrays, `BeginPass`/`EnsurePass`, pipeline cache (`GetPipeline`), descriptor sets (`BindDescriptors`), draws, clear, compute. |
| `VkState.cpp` | Enable/disable state, `glGet*` answers, timestamp queries, sync objects. |

Concepts needed to debug it:

- **One command buffer is recorded continuously** and submitted at `Present`,
  `glFlush`/`glFinish`, or when a readback needs results. `g->serial` numbers
  submissions; `g->completedSerial` is the last one the GPU finished. Objects
  are destroyed only after the serial that last used them completes
  (`Retire*`).
- **Render passes are lazy.** `EnsurePass` begins one for the bound framebuffer
  at the first draw/clear; `EndPass` is called before anything that cannot
  happen inside a pass (uploads, blits, layout changes, compute).
- **Image layouts** are tracked per image (`Image::layout`) and changed with
  `Transition`, which is fatal if called inside a pass.
- **Orientation**: images use OpenGL's row order (row 0 = bottom). Nothing is
  flipped except the final blit in `Present`. Front-face winding is inverted in
  `ApplyDynamicState` to compensate.
- **Uniforms** live in `Program::defaults` (a CPU copy of the shader's default
  block) and are copied to a per-frame ring buffer when dirty.

## Build and first run on hardware

```sh
sudo apt install libvulkan-dev glslang-dev spirv-tools vulkan-validationlayers vulkan-tools
vulkaninfo --summary        # confirm the GPU is listed and reports Vulkan >= 1.3
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUDAS_BUILD_CHANNEL=Experimental -DJUDAS_ENABLE_VALIDATION=ON
cmake --build build --target judas judas_editor -j
JUDAS_GRAPHICS=vulkan JUDAS_VULKAN_VALIDATION=1 build/judas projects/renderer_v2_lab/renderer_v2_lab.judasproj
```

Configure must print `experimental Vulkan backend enabled`. At startup the log
must contain `[Graphics] Vulkan backend: <your GPU name> ...`. If it names
`llvmpipe`, the software device was chosen: set
`JUDAS_VULKAN_DEVICE=<part of the GPU name>`. Record that startup line in any
report: it states the device, depth format and whether device clip control or
the shader depth remap is in use.

Windows is not wired on this branch (see "Open work").

## Test plan, in priority order

Record for every step: commit hash, the `[Graphics] Vulkan backend` line, GPU
and driver version, and whether validation printed anything. Keep failures;
do not weaken a check to obtain a pass (repository rule, `AGENTS.md`).

### 1. Does it start and present on a real driver?

Run the command above. Untested risks specific to hardware:

- Swapchain creation/resize/minimise and vsync (`CreateSwapchain`, `Present` in
  `VkCore.cpp`). Resize the window, minimise and restore it, toggle fullscreen.
- Device selection and missing optional features (`Init` in `VkCore.cpp`).
- Depth format: devices without 24-bit depth (AMD) take the 32-bit float path;
  devices without `VK_EXT_depth_clip_control` take the shader remap path.
  Force each on any GPU with `JUDAS_VULKAN_DEPTH32F=1` and
  `JUDAS_VULKAN_SHADER_DEPTH=1` and re-run step 2.
- Any `[Vulkan validation]` line is a defect to fix, not noise. The count is
  printed at exit as `[Vulkan] validation messages: N`.

### 2. Automated image comparison against OpenGL on the same GPU

```sh
python3 tools/vulkan/compare_backends.py --build-dir build --no-xvfb \
    --output /tmp/judas-vk-parity projects/*/*.judasproj
```

It runs each project on OpenGL twice and Vulkan once and prints per-project
pixel differences plus the OpenGL-vs-OpenGL baseline. Captures and
`RESULTS.json` are in the output directory (which must not already exist).
Expect small edge differences; investigate anything well above the GL-vs-GL
column. On hardware the two backends are different drivers, so differences
will be larger than the software results; judge by looking at the captures.
Add interaction with `--script-line` (see `docs/TEST_HARNESS.md`), for example
`--script-line "CONTROL key:2 1 5 6"` to switch the renderer lab's case, and
`--shot N` for more frames.

Known items to look at specifically:

- `projects/basic_water_lab`: 2.2 % of pixels differed in software, as a band
  of z-fighting stripes at the shoreline that OpenGL also shows with a
  different pattern. Cause not established. Check whether it is visible and
  objectionable on hardware.
- `projects/terrain_v2_workshop`, `terrain_v2_planet_workshop`: about
  0.23–0.30 % differed versus about 0.05 % between two OpenGL runs. Not traced.
- `projects/renderer_v2_lab`: exercise all six cases (keys 1–6), shadows (`H`),
  live monitors (`C`) and split views (`V`). Only the default view was compared.

### 3. Existing gates on Vulkan

```sh
JUDAS_VULKAN_VALIDATION=1 python3 scripts/prealpha_validate.py --build-dir build \
    --group rendering --group render --group particles --group water --group game-water \
    --group ui --group editor --group terrain --group 2d \
    --graphics vulkan --output /tmp/judas-vk-gates
```

Build the gate targets first (`cmake --build build --target judas_prealpha_tests`).
Run the same command without `--graphics vulkan` for the OpenGL baseline on the
same machine and compare the two `RESULTS.json` files. Expected from the
software run: `renderer-shader-library` fails on both backends (pre-existing);
`renderer-compute` fails 2 checks on Vulkan that release an OpenGL context
(not applicable to Vulkan). Anything else that fails only on Vulkan is a
backend defect. The runner refuses binaries that do not match the source, so
rebuild after every edit.

### 4. Human pass

Play at least Willow Town, the renderer lab, a terrain workshop, a water
project and the particle lab on Vulkan, and open a project in
`JUDAS_GRAPHICS=vulkan build/judas_editor` (edit, Play, stop, resize panels).
Things automated comparison cannot see: stutter when new materials first
appear (pipelines are compiled on first use), flicker, tearing, input latency,
anything wrong only while moving. Only the operator can grant human acceptance.

### 5. Performance

Nothing is known. Follow `docs/WHOLE_ENGINE_PERFORMANCE.md` and the rules in
`AGENTS.md` (whole-game evidence, p50/p95/p99/max, same workload on both
backends, same machine). `tests/RendererV2Benchmark.cpp`
(`judas_renderer_v2_benchmark`) and the whole-engine tools go through
`EngineHost`, so `JUDAS_GRAPHICS=vulkan` selects the backend for them. Both
call `SDL_GL_SetSwapInterval(0)` directly, which does nothing on Vulkan, so
they would run vsync-capped: change those calls to `GraphicsSetSwapInterval(0)`
(`src/GraphicsApi.h`, already used by the test harness) before measuring. Note
`tools/benchmarks/PresentCapture.cpp` hooks `SDL_GL_SwapWindow`, which Vulkan
never calls; it needs a Vulkan path before its numbers mean anything.
`JudasVulkan::Stats()` exposes draws, render passes, pipelines and descriptor
sets for diagnosis.

Likely hot spots, unmeasured guesses to check with a profiler rather than
assume: per-draw work in `PrepareDraw`/`BindDescriptors`/`ResolveSamplers`;
render-pass splits when a texture is first sampled after being rendered to;
host-visible vertex/index buffers on discrete GPUs; first-use pipeline
creation hitches; run-time shader compilation at startup.

## Debugging recipes

- **Validation**: `JUDAS_VULKAN_VALIDATION=1`. Fix messages first; later
  symptoms are often consequences.
- **Compute cost**: `JUDAS_VULKAN_DISPATCH_TIMING=1` times every compute
  dispatch on the GPU and prints totals per program and grid at exit.
- **Same-binary benchmark**: `tools/benchmarks/run_matrix.py` with
  `--baseline-graphics opengl --candidate-graphics vulkan` and the same binary
  for both sides. `run_whole_engine.py` records `graphics_backend`.
- **Shader/layout trace**: `JUDAS_VULKAN_TRACE=1` prints each linked program's
  reflected blocks and writes its SPIR-V to `/tmp/judas-vk-N.spv`
  (`spirv-dis`, `spirv-val`).
- **Null-pointer crash in a `gl*` call**: the entry point is not implemented.
  Add it next to its relatives and register it in that file's `Install*Entries`.
- **Wrong picture on Vulkan only**: get a capture from both backends with
  `compare_backends.py`, then bisect by pass. Useful engine switches that work
  on both backends: `JUDAS_SHADOWS=0`, `JUDAS_RENDER_BATCHING=0`,
  `JUDAS_PERSISTENT_UPLOADS=0`, `JUDAS_GL_PROFILE=portable`.
- **Unimplemented behaviour** is reported as `[Vulkan] ...` warnings on stderr
  (first 200). A clean run prints none.
- **RenderDoc** captures Vulkan directly and shows the real passes/pipelines.

Bugs found so far all came from places where the backend answered an OpenGL
query differently from a driver (bindings, introspection, state queries), not
from drawing itself. Suspect the same class first.

## Open work, not started

1. Hardware bring-up and fixes from the test plan above.
2. Windows: add a glslang dependency to `cmake/JudasWindowsCrossDependencies.cmake`
   and enable the backend for MinGW/MSVC in `cmake/JudasVulkan.cmake`. The
   backend code itself uses only Vulkan, SDL and glslang.
3. Explain or eliminate the `basic_water_lab` and terrain differences.
4. Not implemented (warn when used; the engine's own passes do not use them):
   `GL_FRAMEBUFFER_SRGB`, stencil, multisampling, line width ≠ 1,
   non-interleaved transform feedback, 8-bit indices, detached editor windows.
5. Only after hardware results exist: decide whether to keep this layer and
   optimise it, or migrate passes to explicit Vulkan ownership (see the last
   section of VULKAN_BACKEND.md). That is an operator decision.
