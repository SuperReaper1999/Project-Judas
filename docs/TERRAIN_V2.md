# Terrain V2 — Experimental implementation

This is active source-build development on Experimental. The first integrated
checkpoint provides editable sampled landscapes with genuine local excavation.
It does not complete the approved Terrain V2 programme or replace existing
project assets automatically.

## Try the current slice

Build `judas`, `judas_editor` and `judas_terrain_author`, then open
`projects/terrain_v2_workshop/terrain_v2_workshop.judasproj` in the editor or
runtime. In Play, press T to excavate a tunnel, wait for the queue, then walk
through the hill. WASD/mouse move/look; Space jumps; left/right click remove/add
terrain; F6/F7 save/load; Escape pauses. Quit and reload to verify persistence.

In the editor choose World → Terrain tools, Edit landscape, then Select landscape.
Raise/lower, flatten, smooth, paint, dig and add use the actual viewport. A gesture
is one undo item. Save and update playable terrain publishes the editable source
and cooked products; ordinary scene Save remains separate. Pending or unsaved
terrain prevents starting Play with stale products. Changes made in Play remain
runtime state and do not rewrite the authored source.

## Ownership and implementation

- `src/terrain/TerrainSource.*` owns heights, four material layers and the
  excavation journal. Heightfields and excavations share one source.
- `TerrainMesher.cpp` retains ordinary heightfield patches, converting affected
  chunks to local volumetric surfaces. Sparse vertical bands keep excavation
  cost independent of mountain elevation. Canonical edge construction and
  `TerrainTopology.*` join the two representations before strict native cooking.
- `TerrainRuntime.*` owns immutable preparation jobs and derived bodies. Complete
  dirty products replace old support between steps. The uncut seed collider is
  disabled after publication; it cannot secretly block the tunnel.
- Existing `RuntimeTerrain` persistence and regional publication own the lifetime.
  Physics queries return the logical terrain owner; save restoration resolves
  generated support bodies through the common world query interface.
- `render/TerrainPass.cpp` reuses ordinary lighting, shadows and material sampler
  slots. Source-local triplanar detail remains anchored through origin changes.
- `editor/TerrainV2Editor.*` owns the workspace and viewport workflow. Existing
  terrain tools route to it without adding terrain-specific frame-loop logic.
- `TerrainCook.*` uses the shared guarded authoring transaction. The first slice
  generates an ordinary model, collision, preview texture, registered JavaScript
  data module and receipt. Large-data native paging remains subsequent work.

Source commands in `judas_terrain_author` are `v2-validate SOURCE`,
`v2-cook SOURCE OUTPUT.judasmodel`, and `v2-import V1_SOURCE NEW_SOURCE`.
Import preserves the original and rejects unsupported resampling or material
tiling explicitly. Do not overwrite an old project to try V2.

See [JudasJS terrain](judasjs/terrain.md) and the editable project's README for
the scripting contract and controls.

## Verified first checkpoint

Raw results, executable hashes, command scripts, failed meshing experiments,
native screenshots and profiler data are under `docs/evidence/terrain-v2`.
The first checkpoint passes 215 geometry checks, 70 world/scripting checks,
separate-process terrain restoration, 15 native editor checks, the original
planetary suites and the existing Living World checks. Gameplay also excavates,
walks through, saves, quits and reloads using ordinary project code.

At 129×129 samples the matched geometry-plus-BVH preparation medians are 37.34 ms
for a full V1 rebuild and 1.56 ms for dirty V2 chunks, over 30 alternating pairs.
This is preparation work, not FPS or complete brush latency. At 1280×800 on the
GTX 970, the last 120 presented frames of the completed small tunnel scene have
a 0.699 ms median main-camera GPU scope and 0.704 ms p95. That small scene is
not a large-world scaling benchmark. Machine-readable details distinguish
geometry estimates, process memory and profiler overhead.

Automated input and screenshots are engineering qualification. Human visual and
gameplay acceptance of the new terrain remains pending.

## Remaining approved work

The initial flat runtime keeps all chunks resident (maximum 1024); native paged
sources, shared render grids, LOD, budgets and multi-view residency are not yet
implemented. Planet precision and existing LOD remain intact, but planetary
excavation is not implemented by this slice. Fresh-source creation/import UI,
high-bit-depth heightmaps, broader material authoring, incremental navigation,
fluid/particle integrations, representative consumer scenes and final SDK
qualification remain. The current workspace discovers the first V2 source in
Authoring; it is not yet a general multi-source project browser.

Runtime landscapes require a unit-scale root static owner. Translation and
rotation move derived support; live scaling and authority changes are rejected.
Runtime add/remove is supported; runtime volumetric smoothing is explicitly
unsupported. Entirely empty chunks are valid, but cooking a wholly empty source
to an ordinary model remains unsupported. Source bounds, journal and extraction
budgets fail explicitly, keeping the previous complete revision.

Main, supported releases and the Advanced Fluid lock remain unchanged.
