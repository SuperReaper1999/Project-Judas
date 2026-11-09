# Editor terrain performance follow-up

Experimental engineering qualification, 2026-11-09. Baseline:
`f243e8320779db45892cb77fa4ce0c7639de11e6`.
This work is awaiting operator review; it is not a main promotion, SDK update,
release or change to the Advanced Fluids lock.

The earlier comparison with `2ee4203a` did not reproduce the originally reported
severe slowdown or establish that the purge resolved it. This follow-up corrects
two demonstrated costs instead of retroactively declaring that investigation fixed.

## Presentation ownership

A retained Game render target no longer makes a hidden Game tab render.
`EditorViewportPresenter::ShouldRenderGame` owns that decision.

- Visible Game: ordinary runtime presentation, authored camera textures and UI.
- Scene only: one primary observer presentation owns authored camera cadence,
  terrain visual time and Scene rendering. It does not render the Game display
  or collect visual interests from its hidden gameplay camera.
- Both visible: Game owns presentation cadence; Scene is a secondary observer.
- Neither visible: explicit auxiliary presentation can update authored camera
  textures without drawing the main/split-screen display.

Simulation, networking, scripts, audio and input continue through the same
`InteractivePlay::Frame`. Skipping rendering retains its existing default
meaning for service waits; auxiliary rendering is an explicit editor choice.
Primary Scene presentation opens its destination after auxiliary camera passes,
so it does not nest render targets. Secondary observers never advance cadence.

## Editing preview LOD

`TerrainV2PreviewRenderer` owns editor GPU presentation. It reuses `HeightLod`,
stitched height grids and `RenderPages`, rather than adding a second terrain LOD
algorithm. Ordinary heightfields use the runtime's two-pixel geometric error
criterion. Volumetric/cave chunks and planetary chart meshes retain their real
geometry. Brush interests pin affected heightfield chunks to full resolution.

Picking and the shared authored mesh retain full-resolution geometry. Physics,
runtime collision, saves and source brush operations are unchanged. Rendering
reads an immutable worker-published source snapshot; it cannot combine new
heights with an old hierarchy during asynchronous preparation. Excavation
shares the unchanged height hierarchy; sculpting replaces it. Changed chunk
bounds invalidate relevant GPU pages, including normal/stitching halos.

The GPU page cache is bounded to 32 MiB, separately from the shared material
textures and CPU authoring products. Editor pages borrow the current full-detail
material weights, so painting does not leave stale colours or rebuild heights.
Shutdown and project changes explicitly release owned pages.

## Matched measurements

Ryzen 5 5600, GTX 970, NVIDIA 580.178.04, native X11/XWayland OpenGL 4.6.
Three fresh processes per variant, identical 513x513 / two-metre source,
1280x800 rendering, fixed simulation cadence and synchronous GPU queries.
NVIDIA presentation was explicitly uncapped for both variants. These are
controlled frame costs, not a normal interactive FPS certification.

| Workload | Before p50 / p95 | After p50 / p95 |
| --- | ---: | ---: |
| Camera movement with editing draft | 3.671 / 4.271 ms | 1.354 / 1.551 ms |
| Sculpting | 3.411 / 4.710 ms | 1.507 / 2.108 ms |
| Painting | 3.353 / 8.359 ms | 1.461 / 6.409 ms |
| Excavation | 3.531 / 4.777 ms | 1.522 / 2.525 ms |
| Scene selected during Play | 2.569 / 2.936 ms | 1.802 / 2.086 ms |

Camera traversal reduces median frame cost by approximately 63%; terrain draw
submission falls from approximately 1.9 ms to 0.17 ms. The representative view
uses 10 draws instead of 276. Scene-during-Play frame cost falls approximately
30%, with the hidden Game display removed. Full means, p99, worst cases and
sample counts are in [the evidence summary](evidence/editor-terrain-performance-20261109.json).

Each editing run applies all 48 identical dabs: 16 sculpt, 16 paint, 16 excavation.
The faster variant waits for worker readiness instead of silently skipping
frame-numbered edits. Capture/PNG-encoding frames are excluded from full-frame
timing summaries; their raw rows are retained. Raw data, source overlays,
commands and GPU captures remain under `.cache/terrain-editor-gap/`.

There is a worker tradeoff: initial hierarchy preparation is approximately
72 -> 108 ms, and dirty sculpt preparation approximately 14 -> 46 ms on this
fixture. These are asynchronous worker times, not frame stalls. Excavation
reuses the hierarchy and remains approximately 19 -> 20 ms. Improving sculpt
hierarchy preparation and full material-weight uploads remains future work;
this change does not claim to eliminate every editing cost or startup spike.

## Repeated-session memory investigation

Eight repeated baseline sessions were traced with allocator accounting.
Comparable live allocation samples were approximately 136-138 MiB, while freed
heap retained by the allocator grew by hundreds of MiB. A diagnostic-only
`malloc_trim` returned RSS from 754,577,408 to 326,148,096 bytes without changing
live allocations: approximately 409 MiB reclaimed.

The dominant observed RSS growth is allocator retention, not evidence of the
same quantity of live leaked objects. The corrected editor was also exercised
through eight repeated sessions. RSS can still remain high; it is not a complete
process-memory budget or an unlimited leak-free guarantee. Small live/cache
changes and long-duration behaviour remain distinct from this measured cause.

No periodic trimming, allocator replacement, worker-count reduction or forced
collection is shipped. The preload observer and explicit trim exist only in
the disposable investigation directory and are not referenced by builds.

## Qualification and human review

- Actual camera textures, hidden main-framebuffer preservation and unchanged
  physical state: 65 camera checks.
- Scene/Game visibility, primary/secondary ownership, two-frame camera cadence,
  origin changes and native target restoration: 89 viewport checks.
- Full-mesh versus LOD pixels, sculpted/pinned depth, caves above/below, painting,
  immutable hierarchy reuse, exact picking and GPU-page release: 24 checks each
  on modern OpenGL 4.6 and portable OpenGL 3.3.
- Actual editor open/sculpt/undo/redo/save/update and disk persistence: 15 checks
  using a fresh committed Workshop copy, leaving operator edits untouched.
- Existing LOD, picking, runtime-world and GPU-page controls remain authoritative.
  Registered terrain/camera gates include genuine cold persistence processes.

Human checklist:

1. Orbit/fly through the Workshop with its terrain source open, near and far.
2. Sculpt, paint and excavate; inspect brush contact, cave entrances, seams and
   colours; exercise undo/redo and Save and update.
3. Play, select Scene instead of Game, split both views, and hide/reopen each.
   Camera-backed displays and terrain transitions should keep updating normally.
4. Repeat Play/Stop and return to editing. Distinguish process RSS high-water
   retention from steadily rising live allocations or degraded frame times.

Main, the operator's saved terrain edits, RPG SDK and Advanced Fluids lock are
outside this checkpoint's modification set. No new Windows/Android hardware
qualification is claimed by these desktop measurements.
