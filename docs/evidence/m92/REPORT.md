# M92 — Judas Learns to Render More

Status: Linux automated candidate; **human review pending**. Experimental only.
Main remains the accepted M91 checkpoint
`61a9df6acaa102c0a9fcf1006866a01fc3b33a89`. No physics cadence, collision,
simulation quality, fluid implementation or game rules changed.

## What changed and why

Source inspection and frozen-baseline native GL profiling showed repeated
per-object draw/uniform/material work dominating the repeated-geometry workload.
The original shader also accepted the first five registered dynamic lights and
two fixed spot-shadow identities, and the main camera's terrain/shadow choices
were reused by other cameras. Increasing constants alone would not address these
costs or resource-selection errors.

- **Opaque instancing:** pass-scoped groups share mesh, texture, exact material
  state and mirrored handedness. Model transform and tint become GPU instance
  attributes. Layer/frustum acceptance happens first. Groups flush at 16,384
  instances. World and editor use the same generic submission machinery; rigid
  imported meshes and primitives participate. Skinning, hidden parts, masked
  materials and sorted transparency retain their existing paths. Groups never
  outlive a pass or retain resources across asynchronous replacement.
- **Tiled dynamic lighting:** conservative CPU projections build 64-pixel
  screen tiles; a single RGBA32F texture supplies fragment lists. This works
  without compute shaders or desktop storage buffers. Up to 1,024 visible
  sources/view and 128 references/tile; stable relevance selection and overflow
  counters make overload explicit. No sources means no initial grid allocation
  or upload. The same lighting path serves legacy and PBR shading.
- **Selected shadows:** one directional depth-array layer plus up to eight
  spot layers replaces dedicated maps. The ordinary world selects four visible
  requesting spots per actual camera by priority/relevance. Storage grows lazily;
  non-selected sources still illuminate. Framebuffer and viewport are restored
  after every shadow pass. Legacy torch-only player-shadow exclusion is preserved
  from source request metadata, independently of selected slot.
- **Per-view resources:** main, secondary targets and local-player views each
  select light tiles and shadow coverage for their own camera. Planetary LOD takes
  the maximum requested screen error across those views, under one shared
  residency/build budget; rendering does not relocate collision interest. Planet
  drawing clears prior object materials instead of inheriting unrelated state.
- **Public light controls:** copied `Entity.light` and strict
  `Entity.setLight(patch)` modify an existing authored light. Kind, enabled,
  colour, intensity, range, cones, shadow request and priority are authorable.
  Validation is atomic; missing/stale owners fail through normal safe-handle
  rules. Definitions carry changes through modern saves and streaming. GPU maps
  and batches are transient. Types, inventory, reference and example are updated.
- **Diagnostics:** M56 light selection/upload and opaque-instance scopes, actual
  instance/draw counts, rejected/overflow light references and estimated owned
  GPU payload bytes. No parallel renderer or scene-name branch was introduced.

Implementation: `src/RenderLightGrid.*`, `RendererScale.inl`, `Renderer.*`,
`WorldPresentation.cpp`, `WorldPlanets.cpp`, `PlanetTerrain.*`, `PlanetTiles.*`,
ordinary scene/runtime/script/editor light integration. See
[exact changed files](CHANGED_FILES.txt) and [current contracts](../../judasjs/rendering.md).

## Measurements

See [PERFORMANCE.md](PERFORMANCE.md) for final-linked observations, median/p95/max,
GPU elapsed times, process CPU, RSS and payload estimates. The raw archive retains
individual frame CSVs, M56 GPU/CPU profiles, pixels, commands and process/GPU
observations. Earlier alternating-order pairs are retained as repeatability
evidence rather than discarded in favour of the fastest result.

Hardware: native Linux, i7-4790 (4 cores/8 threads), GTX 970 4 GB,
NVIDIA 580.178.04, GL 3.3 context. Release `-O3 -DNDEBUG`, serial native runs,
960×540 benchmark window; the second view is a 480×270 target. Each case has
12 warm-up frames and 60 measured frames. Observer source is identical for both
builds except the candidate's explicit batch scope and counters.

Comparisons with 0/5 lights or two shadowed spots preserve geometry, settings and
output. The **128-light baseline truncates to five**: this demonstrates expanded
capability, not equal-quality speedup. Per-case images and triangle counts are
compared. GPU timestamp intervals include CPU command-stream bubbles; they do
not isolate pure shader execution. Wall submission excludes buffer swap; process
CPU includes the swap call. Peak RSS includes the observer and profiler, not just
renderer allocations. Payload estimates exclude driver overhead and are not
total VRAM usage.

The other Judas game was initially left running at the operator's request.
After explicit permission, only its verified PID/executable was closed. Final
comparisons have no competing Judas process; normal desktop applications remain.
The initial sandbox GPU-query failure is preserved and is not presented as
hardware data. Native logs and GPU observations establish the hardware used.

## Focused validation

[RESULTS.json](RESULTS.json) identifies each receipt and final binary. A Release
incremental build covered the engine, editor, exporter, new observers and focused
consumers. This was **not** a clean rebuild or a production-suite run.

- 44 M92 architecture checks: real framebuffer contribution beyond five lights;
  registration-order negative control; conservative culling; bounded overflow;
  shadow-priority/zero-budget controls; actual spot shadows; layer-five target
  restoration; per-camera shadow passes; mirrored/nonuniform/PBR pixel equivalence;
  accepted non-default render layers; opposing planetary views; unrelated-material
  isolation; authored preview shadow lifetime; scene/save/light API validity.
- 14 ordinary application checks: registered lab scripts, actual instanced draws,
  logical light/shadow input, independent split views, pause/resume, saved light
  restoration after a distinct later edit and fault-free lifecycle callbacks.
- Camera (52), local-player rendering (14), planetary terrain (10,074), planetary
  world (220), planetary streaming (20), material rendering (40), scripts (35),
  runtime render controls (54), application (40 + 13 fresh-process), weather
  rendering (39), plus historical UI/lighting/interaction renderer consumers.
  These are focused regression receipts, not complete engine certification.
- JudasJS drift/type/live enumeration: 424 public symbols, 37 exports, 288 native
  operations, 17 callbacks, 46 checked example sources, eight result shapes and
  four negative drift controls; TypeScript 5.9.3. Surface runner: nine checks.
  The new light example is typechecked; native script probes and the ordinary
  lab execute the corresponding real API calls.
- Editor duplicate/Undo and Play/Stop preserve authored scene equality; save
  succeeds. Export includes three scenes and their registered resources. A byte-
  verified moved package launches from an unrelated temporary working directory,
  toggles split/pause, transitions small→dense→gallery, then reloads without script
  faults. No engine-root environment fallback is supplied.

The final narrow edit-shadow correction refreshed architecture, lab, camera,
local-player, editor and export checks. Broader focused receipts predate that
edit-preview-only change; no unrelated suite was rerun. Development warnings
already present in RuntimeWorld's older formatting remain; no zero-warning clean
build claim is made. A trivial new test-initializer warning was removed.

## Genuine development failures and corrections

- [DEVELOPMENT_FAILURE.log](DEVELOPMENT_FAILURE.log): SetLight initially accepted
  a destroyed entity's retained definition. The generic setter now checks the
  live RuntimeDefinition guard before mutating either definition/cache.
- [DEVELOPMENT_LAB_FAILURE.log](DEVELOPMENT_LAB_FAILURE.log) and
  [follow-up](DEVELOPMENT_LAB_FOLLOWUP_FAILURE.log): one Escape edge opened and
  immediately closed modal UI; then focusing Resume was mistaken for clicking it.
  Project JS now honours UI event type and the opening edge. The same pause
  assertion passes without weakening it.
- [DEVELOPMENT_EDIT_SHADOW_FAILURE.log](DEVELOPMENT_EDIT_SHADOW_FAILURE.log):
  an edit-only receiver inherited the previous runtime camera's directional map.
  Authored preview boundaries now invalidate maps before drawing, including
  terrain. Edit preview has no shadow pass of its own. A pixel negative control
  fails before this correction and passes afterward.
- Development also exposed batch flush rechecking the default layer after
  individual instances had already passed another layer mask. Submission now
  preserves that prior acceptance; the negative control includes a closer,
  rejected object so merely drawing everything cannot pass.

## Lab and limitations

[The Light Factory](../../../projects/rendering_lab/README.md) is ordinary editable
content: 256/1,024/4,096 repeated modules, 128 independently scriptable lights,
32 shadow-requesting spots, two live camera screens, split views, watch tour,
free flight, pause and modern save/load. The default scene intentionally has no
physics workload: this isolates rendering and does not alter physics quality.
The exported package is `.cache/m92/delivery/package/` (ignored build output).

Limits: no point-light cubemap shadows, cascaded sunlight, deferred renderer,
clustered depth slices or GPU-driven indirect culling. Directional shadow coverage
retains its finite 50 m local window. Each shadow/view still traverses casters.
Unique meshes/materials may not benefit from grouping; batching has a diagnostic
opt-out. Grid and shadow storage retain their high-water allocation. Dense
overlapping light lists omit lower-relevance sources with visible counters.
Planet views share residency budgets and a float render chart; this does not
introduce separate binary64 camera charts for arbitrarily separated eyes.
Off-axis planetary selection is conservative rather than perfect occlusion.
Skinned/blended geometry does not gain instancing. No Windows or Android M92
execution was performed; portability is an implementation intent, not a platform
acceptance claim. Human visual acceptance remains pending.

## One human checklist

1. Launch the lab; watch the coloured gallery, shadows and live camera screens.
2. Press F5; fly with WASD/QE and mouse. Check lighting while moving close.
3. L toggles sources; H toggles shadow requests. Inspect changing light pools.
4. V switches to independent split views. Check each eye's lighting/shadows.
5. Try 1/2/3 densities; inspect responsiveness and any visible stalls.
6. F6 saves; change lights/shadows/view; F7 restores. Pause/resume with Escape.
7. Open in editor, change a lamp's intensity/range/priority, Play/Stop, and verify
   edit mode does not retain runtime shadow artifacts.
8. Run the exported package and confirm equivalent controls and scene changes.

Protected historical evidence, independent research, accepted fluid work and
existing games are unchanged. No M93 work, main promotion or human certification.
