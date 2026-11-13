# Native Vulkan execution: design and status

Branch `Vulkan-Exp`. Follows [VULKAN_BACKEND.md](VULKAN_BACKEND.md) and the
hardware results in [evidence/VULKAN_EXP_PROOF.md](evidence/VULKAN_EXP_PROOF.md).
OpenGL remains the default backend and its renderer code is not rewritten.

## Why this shape

The first backend answers every OpenGL-named call individually and re-derives
the whole Vulkan state at each draw. Profiling on the reference machine
(Ryzen 5 5600, GTX 970, `perf`, user-space samples) showed where frame time
goes on Vulkan before this work:

| Scene | Frame | Backend draw path | Engine scene walk and material logic |
|---|---:|---:|---:|
| 129 animated characters (2,640 draws) | 6.8 ms | about 2.5 ms | about 3.2 ms |
| Renderer lab (206 draws, 8,500 object visits) | 7.2 ms | about 0.3 ms | about 5.9 ms |

Per draw the backend resolved every sampler of the program, hashed sampler
state, rebuilt the vertex layout, looked up the pipeline by a 100-byte key and
re-hashed all descriptors. Each skinned draw also replaced the bone buffer,
which forced a new buffer view and therefore a new descriptor set.

The renderer's shaders stay shared with OpenGL on purpose: one source of truth
is what makes the two backends comparable pixel for pixel. "Native" therefore
means the Vulkan side owns execution explicitly, not that shaders fork.

## Layers

1. **Draw-state cache (all passes).** State changes mark what they invalidate
   (textures, buffers, vertex layout, pipeline state). A draw redoes only the
   invalidated parts; an unchanged draw binds nothing but its per-draw uniform
   offset. Entry points are invalidating by default; only audited ones opt out.
2. **Stable bindings for streamed data.** Buffer textures used as float tables
   (bone palettes) are bound as dynamic-offset storage buffers, so a new palette
   is a new offset, not a new descriptor set.
3. **Ahead-of-time state.** Compiled SPIR-V (one file per program, keyed by
   the adapted sources and compiler version) and the driver's pipeline cache
   persist under `$XDG_CACHE_HOME/judas-engine/vulkan` (default `~/.cache`).
   `JUDAS_VULKAN_SHADER_CACHE=0` disables both. Creating known pipelines before
   the first frame is not implemented yet: they are still created at first
   use, from the warm cache.
4. **Memory placement.** GPU-owned buffers (`*_COPY`) live in device memory.
   Placing static geometry there too was measured and rejected (no gain).
5. **Explicit interface and native passes.** `src/vulkan/VulkanGfx.h` is an
   explicit renderer interface in Vulkan's own terms: shader objects with
   indexed parameters, vertex layouts, pipelines, passes with named
   attachments, binding groups, transient frame data and draws. It reads no
   OpenGL-style global state and involves no OpenGL-named entry point. Passes
   are moved onto it one at a time; a pass that has moved leaves the
   OpenGL-named layer entirely on Vulkan, while its OpenGL version stays the
   OpenGL backend's path, untouched.

## Pass migration status

| Pass | On the explicit interface | Notes |
|---|---|---|
| Shadow charts | yes | Explicit depth pass per chart; `src/render/NativeMeshPass.inl` |
| Mesh draws: opaque, instanced, skinned, blended, sprites, terrain grids | yes | Materials, terrain bindings, bone palettes and instance arrays; all 20 pipelines created at startup |
| GPU particles | partly | Vulkan-specific shader, still issued through OpenGL-named calls |
| CPU particles, water, weather, UI and text, debug lines, post-processing | no | OpenGL-named layer |
| Frame targets, clears, viewport | no | Chosen by frame code through the OpenGL-named layer; native draws join the current target |
| Scene-wide shader parameters (lighting, weather, water, terrain flags) | no | Written by their own passes through OpenGL-named calls into the same parameter block the native pass uses |
| Resource creation and uploads (meshes, textures, buffers), compute | no | OpenGL-named layer; referenced by handle |

`JUDAS_VULKAN_NATIVE_MESH=0` routes the mesh passes through the OpenGL-named
layer again, for bisecting. While a shadow pass is recording, the chart also
remains the current framebuffer and viewport for code that inspects it through
the graphics API (gates read its depth mid-pass); drawing does not use that.

The OpenGL-named layer can be removed only when every row above reads "yes".

## Rules

- Every stage is measured with `run_matrix.py` (same binary, OpenGL vs Vulkan)
  and checked with the gates, validation layers and `compare_backends.py`.
- A stage that does not measurably help is reverted and recorded, not kept.
- `JUDAS_VULKAN_DRAW_CACHE=0` disables layer 1 for bisecting.

## Not in this design (recorded so it is not assumed done)

- The engine's per-pass scene walk (entity lookup, material key strings) costs
  more than all graphics calls in populated scenes on both backends. It is
  engine work on `Experimental`, not a backend change.
- Multi-threaded command recording needs that walk to be restructured first.
- Windows is still not wired.

## Status

See the "Native execution" section of the evidence file for measured results
per stage.
