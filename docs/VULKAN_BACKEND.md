# Vulkan backend — experimental proof (branch `Vulkan-Exp`)

Baseline: Experimental `d9a80f6c`. This branch is an experiment, not a release,
not an Experimental checkpoint and not human-validated. OpenGL remains the
default backend of every build. Nothing here changes `main` or `Experimental`.

## What this is

The question was whether Judas can render through Vulkan while preserving its
OpenGL behaviour. This branch answers it with a second graphics backend which
is selected at run time:

```sh
JUDAS_GRAPHICS=vulkan build/judas projects/willow_town/willow_town.judasproj
JUDAS_GRAPHICS=vulkan build/judas_editor
```

With `JUDAS_GRAPHICS=vulkan` no OpenGL context is created, no OpenGL driver is
loaded and no translation library (Zink, ANGLE) is involved. The window is an
SDL Vulkan window, and every graphics call the engine makes is executed by
Judas-owned code in `src/vulkan/` which records native Vulkan commands.

### Design, stated plainly

The renderer was **not rewritten**. `Renderer`, the passes under `src/render/`
and every GLSL shader are shared, unmodified, by both backends. They reach the
graphics API through GLAD's table of function pointers. The OpenGL backend
fills that table from the driver; the Vulkan backend fills the same table with
its own implementations of the ~250 entry points Judas uses (`gladLoadGL` is
given `JudasVulkan::ProcAddress` instead of `SDL_GL_GetProcAddress`).

This was chosen because the renderer is written against OpenGL's state machine
throughout (about 880 call sites in 21 files: persistent program uniforms,
state save/restore through queries, texture-unit conventions). Sharing the
source is what makes "preserves the OpenGL behaviour" checkable: the same
renderer code runs on both, so a difference is a backend difference.

The consequence to be clear about: this is a Vulkan **backend beneath the
existing renderer**, not a renderer redesigned around Vulkan idioms. It proves
that everything Judas draws is expressible in Vulkan with matching output and
gives a working base to migrate from. It does not yet deliver the things a
Vulkan-first design is usually wanted for (explicit frame graph, pre-built
pipelines, multithreaded command recording). See "What this does not show".

### How the backend works

| Concern | Implementation |
|---|---|
| Device | Vulkan 1.3 required (dynamic rendering, extended dynamic state, host query reset, cube arrays). Discrete GPU preferred; `JUDAS_VULKAN_DEVICE=<name substring>` overrides. |
| Shaders | The unmodified GLSL is compiled at run time to SPIR-V by glslang (relaxed Vulkan rules gather loose uniforms into one block). Bindings and varying locations are then made program-wide by name, and the uniform layout is reflected from the generated SPIR-V itself. |
| Uniforms | Each program keeps its uniform values, as OpenGL does. A changed block is copied into a per-frame ring buffer and bound with a dynamic offset. |
| Coordinates | Images keep OpenGL's memory convention (row 0 = bottom of the picture), so texture coordinates, `gl_FragCoord`, readbacks, blits and render-to-texture are unchanged. Only presentation flips. Front-face winding is mapped accordingly. |
| Depth | `VK_EXT_depth_clip_control` gives OpenGL's `[-w, w]` clip volume on the device. Without it, each vertex shader's result is remapped after its unmodified `main()` (mathematically identical, differs in float rounding). 24-bit depth is used when the device has it, otherwise 32-bit float. |
| Passes | Dynamic rendering. A pass begins lazily for the bound framebuffer and ends when the target changes or an image needs a layout change/transfer. |
| Pipelines | Created on first use and cached by program, vertex layout, blend state and attachment formats; cull, depth, topology, viewport and scissor are dynamic state. |
| Buffers | Suballocated host-visible blocks. A write to a buffer still referenced by unfinished commands gets fresh storage (copy-on-write), which reproduces OpenGL's ordering guarantees. |
| Textures | Created when first used so the mip chain is known; uploads are recorded in command order. |
| Modern tier | Compute dispatch, storage buffers, indirect draws, persistent mapped buffers, texture views (cube-array shadow views) and timestamp queries are implemented, so `JUDAS_GL_PROFILE=auto/modern` selects the same modern renderer tier as an OpenGL 4.6 driver; `portable` selects the 3.3 tier. |
| Editor | Dear ImGui's GL3 renderer is compiled to resolve its calls through the same table, so the editor UI also draws through Vulkan. Detached (multi-window) editor panels are disabled under Vulkan. |

Source: `src/vulkan/VkCore.cpp` (device, memory, submission, presentation),
`VkShader.cpp` (programs, reflection), `VkTexture.cpp` (textures, framebuffers,
pixel transfer), `VkDraw.cpp` (buffers, pipelines, draws, compute),
`VkState.cpp` (state, queries, sync). Public interface: `VulkanBackend.h`.

Engine changes outside `src/vulkan/` are small: `Window` creates the Vulkan
window/device and presents; `EngineHost` selects the loader; `GraphicsApi.h`
gained a backend-neutral context identity and swap-interval helper which five
render owners now use instead of `SDL_GL_GetCurrentContext`.

## Building

```sh
sudo apt install libvulkan-dev glslang-dev spirv-tools   # plus a Vulkan driver
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUDAS_BUILD_CHANNEL=Experimental -DJUDAS_ENABLE_VALIDATION=ON
cmake --build build --target judas judas_editor -j
```

`JUDAS_VULKAN=AUTO` (default) builds the backend when the Vulkan headers/loader
and glslang are found, `ON` requires them, `OFF` excludes it. Configure prints
which happened. A build without the backend refuses `JUDAS_GRAPHICS=vulkan`
with an explicit error rather than silently using OpenGL. The backend is only
wired for native Linux desktop builds on this branch (see limitations).

Run-time controls:

| Variable | Effect |
|---|---|
| `JUDAS_GRAPHICS=vulkan` | Select the Vulkan backend (anything else: OpenGL). |
| `JUDAS_GL_PROFILE=auto\|portable\|modern` | Same renderer tier policy as OpenGL. |
| `JUDAS_VULKAN_VALIDATION=1` | Enable the Khronos validation layer; messages go to stderr and are counted at shutdown. |
| `JUDAS_VULKAN_DEVICE=<text>` | Prefer the device whose name contains the text. |
| `JUDAS_VULKAN_SHADER_DEPTH=1` | Force the shader depth remap even when the device has clip control. |
| `JUDAS_VULKAN_DEPTH32F=1` | Force 32-bit float depth. |

## Reproducing the comparison

```sh
# Same binary, same harness script: OpenGL, OpenGL again, Vulkan. Pixel comparison.
python3 tools/vulkan/compare_backends.py --build-dir build --output /tmp/judas-vk-parity projects/*/*.judasproj

# Existing gates against Vulkan (the runner strips JUDAS_* otherwise).
JUDAS_VULKAN_VALIDATION=1 xvfb-run -a python3 scripts/prealpha_validate.py --build-dir build \
    --group rendering --graphics vulkan --output /tmp/judas-vk-gates
```

`compare_backends.py` runs OpenGL twice so that a project's own run-to-run
variation (asynchronous loading, on-screen timings) is measured and not
attributed to the backend.

## Evidence

See [evidence/VULKAN_EXP_PROOF.md](evidence/VULKAN_EXP_PROOF.md) for the
measured results, the environment they were measured in and what they do and
do not establish.

## What this does not show

- **No hardware GPU was available.** Every measurement used Mesa's software
  rasterizers (llvmpipe for OpenGL, lavapipe for Vulkan) under Xvfb. Behaviour
  on NVIDIA, AMD or Intel Vulkan drivers is untested. Software parity is
  evidence about the backend's logic, not about any vendor driver.
- **No performance qualification.** Software rasterizers say nothing about
  frame time on a GPU. The backend creates pipelines on first use, binds
  descriptors per draw and keeps buffers host-visible; none of that has been
  profiled on hardware. The budgets in `AGENTS.md` are unqualified for Vulkan.
- **Not human-tested.** Image comparison and gates are automated.
- **Linux desktop only.** Windows (MinGW cross and native), Android and the
  browser are not wired; they keep OpenGL/GLES/WebGL. Windows additionally
  needs a glslang dependency in the cross toolchain.
- **Shaders are compiled at run time**, so glslang ships inside the engine
  binary when the backend is built.
- Known gaps: `GL_FRAMEBUFFER_SRGB` encoding, stencil operations, multisample
  targets, line widths other than 1, non-interleaved transform feedback,
  `GL_UNSIGNED_BYTE` indices and detached editor windows are not implemented.
  The engine's own passes do not use them; the backend warns when asked.
- Where coplanar surfaces z-fight in OpenGL they also z-fight in Vulkan, but
  the per-pixel winner can differ (see the evidence file).

## If this proof is accepted

Because the renderer talks to a narrow entry-point table, migration can be
incremental rather than a rewrite: replace state-machine idioms pass by pass
with explicit pipeline/descriptor ownership (starting where profiling on real
hardware points), move shader compilation to build time, then retire the
OpenGL table once no caller needs it. None of that is started here.
