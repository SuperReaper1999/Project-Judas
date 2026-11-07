# Terrain V2 — Experimental implementation

This is active source-build development on Experimental. The integrated
checkpoints provide editable sampled landscapes with genuine local excavation,
shared-grid rendering and native terrain assets.
It does not complete the approved Terrain V2 programme or replace existing
project assets automatically.

## Try the current slice

Build `judas`, `judas_editor` and `judas_terrain_author`, then open
`projects/terrain_v2_workshop/terrain_v2_workshop.judasproj` in the editor or
runtime. In Play, press T to excavate a tunnel, wait for the queue, then walk
through the hill. WASD/mouse move/look; Space jumps; left/right click remove/add
terrain; F6/F7 save/load; Escape pauses. Quit and reload to verify persistence.

For a new project, open World → Terrain tools → Create Terrain V2 landscape.
Choose Flat, Highlands, Island or Import heightmap, then Create landscape. The
workflow writes source, native data, preview and recipe, adds/selects the object
through normal scene undo and opens its brushes. No bootstrap script or hand-made
scene keys are needed. Add component → New landscape opens the same workflow.

If replacing an existing floor, select it first and explicitly choose Replace
selected ground. This disables its old appearance/collision in the same undo
transaction, preserving its identity, gravity, scripts and children. The new
landscape is placed at the old top surface. Gravity keeps its authored bounds;
enlarge its region if your new landscape extends beyond them. Leaving an old
floor enabled underneath an excavation will still block that excavation.

In the editor choose World → Terrain tools, Edit landscape, then Select landscape.
Raise/lower, flatten, smooth, paint, dig and add use the actual viewport. A gesture
is one undo item. Save and update playable terrain publishes the editable source
and cooked products; ordinary scene Save remains separate. Pending or unsaved
terrain prevents starting Play with stale products. Changes made in Play remain
runtime state and do not rewrite the authored source.

The Source chooser switches between authored landscapes and selects their actual
scene owners. The Inspector's Landscape card and double-clicking a native terrain
asset open the corresponding source. Project-relative provenance connects assets
to sources; the older unique-filename fallback remains, but ambiguous matches are
rejected. Legacy terrain tools remain available without discarding a dirty draft.

Surface materials offers four texture/tint layers, including asset drag/drop.
Apply previews material changes; Save and update also applies pending choices.
Painting changes material samples without rebuilding geometry. Undo/redo retains
up to 16 snapshots and a 64 MiB sample/journal payload budget across both stacks,
retaining the immediate snapshot if a single allowed source exceeds that budget.
This is history payload, not total editor RAM.

Heightmaps retain grayscale 16-bit samples, deliberately resampled to the chosen
grid with image top mapping to negative local Z. Height values map to the chosen
minimum/range in metres. The file picker accepts PNG; the importer also handles
other grayscale formats supported by the existing decoder. Colour images are
rejected. Original input and recipe are retained under Authoring. This is not a
GIS/GeoTIFF pipeline. The text-source budget remains 64 MiB.

## Ownership and implementation

- `src/terrain/TerrainSource.*` owns heights, four material layers and the
  excavation journal. Heightfields and excavations share one source.
- `TerrainMesher.cpp` retains ordinary heightfield patches, converting affected
  chunks to local volumetric surfaces. Sparse vertical bands keep excavation
  cost independent of mountain elevation. Canonical edge construction and
  `TerrainTopology.*` join the two representations before strict native cooking.
- `TerrainRuntime.*` coordinates immutable preparation jobs and derived bodies.
  `TerrainPresentation.cpp` owns views/GPU products; `TerrainPersistence.cpp`
  owns asset-reference and complete-source snapshots. Complete
  dirty products replace old support between steps. The uncut seed collider is
  disabled after publication; it cannot secretly block the tunnel.
- Existing `RuntimeTerrain` persistence and regional publication own the lifetime.
  Physics queries return the logical terrain owner; save restoration resolves
  generated support bodies through the common world query interface.
- `render/TerrainPass.cpp` reuses ordinary lighting, shadows and material sampler
  slots. Source-local triplanar detail remains anchored through origin changes.
- `editor/TerrainV2Editor.*` owns the workspace and viewport workflow. Existing
  terrain tools route to it without adding terrain-specific frame-loop logic.
- `TerrainAsset.*` owns bounded, indexed native sample pages with per-page
  checksums. `TerrainComponent.*` connects authored sources to ordinary scenes.
- `TerrainCook.*` uses the shared guarded authoring transaction. Native mode
  publishes a source asset, adaptive editor preview, texture and receipt. Runtime
  collision comes from that source, independently of the preview mesh. The older
  full-model/full-collision cook remains available for the first-slice workflow.

Source commands in `judas_terrain_author` are `v2-validate SOURCE`,
`v2-native SOURCE OUTPUT.judasmodel`, `v2-pack SOURCE OUTPUT.judasterrain`,
`v2-cook SOURCE OUTPUT.judasmodel`, and `v2-import V1_SOURCE NEW_SOURCE`.
Import preserves the original and rejects unsupported resampling or material
tiling explicitly. Do not overwrite an old project to try V2.

Additional source-generation commands:

```sh
build/judas_terrain_author v2-new Authoring/hills.judasterrain2 highlands 129 1 77 48
build/judas_terrain_author v2-heightmap heightmap.png Authoring/imported.judasterrain2 129 1 0 64
```

New-source commands never overwrite existing files. Both V1/V2 writers share
published-kit/dependency write protection; fork those sources into owned locations.
Generated landscapes are ordinary editable data, with no procedural simulation
cost during gameplay. Flat starts with one clear material; other presets seed
slope/shore materials. Sculpting retains the source's normal vertical headroom
rather than limiting a new flat source to a short decorative slab.

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

The flat runtime still keeps all physical chunks resident (maximum 1024). Shared
render grids, camera-union LOD and indexed native files now exist. Runtime still
loads complete source arrays; independent physical residency and budgeted
multi-view streaming remain work. Planet
precision and existing LOD remain intact, but planetary
excavation is not implemented by this slice. Broader PBR material authoring, incremental navigation,
fluid/particle integrations, representative consumer scenes and final SDK
qualification remain. Source discovery currently covers up to 512 editable
sources under Authoring; large-data demand paging remains separate work.

Runtime landscapes require a unit-scale root static owner. Translation and
rotation move derived support; live scaling and authority changes are rejected.
Runtime add/remove is supported; runtime volumetric smoothing is explicitly
unsupported. Entirely empty chunks are valid, but cooking a wholly empty source
to an ordinary model remains unsupported. Source bounds, journal and extraction
budgets fail explicitly, keeping the previous complete revision.

Main, supported releases and the Advanced Fluid lock remain unchanged.

## Native scene assets and persistence

The owned Workshop uses `Assets/terrain/highlands.judasterrain`, registered with
normal `.judasmeta` type `terrain`, and this scene key:

```text
object 1 "Landscape"
 terrain.asset "8a22000000000000000000000000000e"
 position 0 0 0
 rotation 1 0 0 0
 scale 1 1 1
 body static box
end
```

Use an actual registered ID for your asset. A root unit-scale static Box seed is
required; competing geometry, motion, navigation-surface and fluid owners are
rejected explicitly. The seed supplies identity/material/filter settings. It is
disabled once the real chunk colliders publish. Initial collision is ready before
scene gameplay starts, without a JavaScript bootstrap. Render components may refer
to the cooked preview for ordinary editor display; Play draws the actual source.

The version-1 native file stores exact binary32 heights and four normalized
material weights in indexed 64×64-sample pages, with smaller rectangular edge
pages. Opening the header/index does not read sample payloads. Requested pages
are checked before publication; a failed read leaves the previous result intact.
This is corruption detection, not asset authentication. Native source size is
bounded to 96 MiB and the existing source sample budget still applies.

Asset-backed saves keep the immutable base ID/fingerprint, the actual edit
journal and sparse changed height/material samples; unchanged arrays are not copied. A changed/missing base fails
with an explicit migration diagnostic. Runtime-created sources use complete
native snapshots within the existing 8 MiB save-string budget. Earlier JSON
snapshots remain readable. Restore preserves old support until the entire new
collision revision succeeds, including when the replacement source is smaller.
Regional staging, private placement, publication and removal use that same owner.
The exporter follows native terrain and all material texture dependencies through
ordinary asset IDs, so relocation does not depend on authoring paths.

Current native integration checks cover 32 asset-format cases, 35 component/
regional cases, 416 geometry cases, 93 world cases and genuine separate-process
restoration. The real exported Workshop excavates, traverses, saves, quits and
restores after relocation. These remain automated checks, not human acceptance.

The one-kilometre native benchmark (513×513 samples, 2 m spacing, five runs)
loads all 1,024 physical chunks. Complete scene setup is about 0.50 s median;
a local four-chunk excavation completes in about 29 ms, with 0.15–0.28 ms
publication. Its asset is 5,271,331 bytes and the edited-terrain snapshot is 594
bytes. Peak benchmark-process RSS is 148,420 KiB, including input/output arrays,
physics, repeated construction and allocator retention. These are warm-file
measurements, not cold-disk timings or a total-game memory budget. Actual source,
collision and visual residency streaming remains the next engineering work.

## Shared-grid rendering checkpoint

Ordinary terrain renders from one reusable grid per cell resolution and a
lossless binary32 height image. It no longer allocates a separate GPU vertex
buffer for every ordinary chunk. Excavated chunks retain their actual volume
meshes. Camera interests are collected before shadow and colour passes, including
split-screen, secondary and orthographic cameras. Collision remains at its
authored resolution and does not depend on camera visibility or visual LOD.

`TerrainLod.*` owns a worker-built geometric-error hierarchy. Selection uses a
two-pixel projected-error target; this is a distance-based error estimator, not
a universal pixel-difference guarantee for arbitrary projections or materials.
Fine edges interpolate against coarse neighbours. The excavation seam and its
neighbour ring remain full detail. No skirts cover tunnel openings. Colour and
shadow shaders share the same portable height/edge code. Runtime batch extraction
validates immutable source arrays once, and collision identities hash dirty
geometry rather than serializing the complete landscape after each local edit.

The desktop geometry tests compare actual framebuffer depth against CPU source
reconstruction and actual shadow depth against independently built CPU-height
geometry, in ordinary and rotated/translated frames. Maximum height error is
0.0000072 m; shadow depths match exactly at tested samples. Both OpenGL 4.6 and
portable 3.3 pass. A real motor traversing 20 metres across chunk seams/corners
matches the monolithic-collider trajectory exactly across 300 supported steps.

The one-kilometre landscape benchmark renders two independent cameras from the
same 513×513 source, alternating paths over 30 pairs:

| Camera case | Full-mesh CPU median / p95 | Shared-grid CPU median / p95 | Submitted triangles before → after |
| --- | --- | --- | --- |
| Distant | 11.632 / 11.702 ms | 0.063 / 0.067 ms | 1,048,576 → 4,096 |
| Closer | 5.858 / 6.347 ms | 0.364 / 0.370 ms | 526,848 → 28,672 |

Mesh/height payload is 22,863,872 → 1,075,004 bytes. Common material images are
excluded on both paths. These are terrain submission/rendering measurements,
not total game FPS, simulation performance, or a RAM reduction. GPU elapsed
queries also include bubbles while the CPU submits work, so their large
reduction must not be described as a pure GPU-arithmetic speedup. The small
completed-tunnel scene remains approximately 0.70 ms for its main-camera GPU
scope; its cost is essentially unchanged. Raw paired results and controls are
under `docs/evidence/terrain-v2/stage2`.

Still pending: temporal LOD transitions, physical streaming/demand paging and
larger integrated consumer scenes. A source page exceeding the device's texture
size fails before allocation with an actionable request to split its regions.
This checkpoint does not certify Android/browser terrain rendering or qualify
the complete five-stage Terrain V2 delivery.

## Native authoring checkpoint

Fresh-project qualification exercises the real creation controls, file browser,
16-bit image import, palette, material-only painting, save/update, scene undo,
multiple sources, Inspector entry point and explicit old-ground replacement.
The 46-check workflow and 23 generation/policy controls pass on Linux, together
with the existing editor and runtime controls. A relocated standalone still
excavates, traverses and cold-restores the tunnel. Human acceptance remains pending.

The editor worker previously revalidated all source samples for every chunk.
Thirty alternating pairs with identical geometry measure these preparation costs:

| Samples | Former median / p95 / worst ms | Batch median / p95 / worst ms |
| --- | --- | --- |
| 129² | 4.564 / 5.167 / 5.647 | 1.904 / 2.044 / 2.080 |
| 257² | 50.048 / 58.184 / 64.768 | 7.815 / 8.604 / 9.084 |
| 513² | 727.846 / 797.213 / 797.228 | 32.823 / 35.776 / 36.673 |

These cover geometry worker preparation only, excluding JSON, merged mesh,
collision cooking and GPU work. No builds or other qualification ran during
the final timing series. Raw results and preserved failed test-oracle attempts
are under `docs/evidence/terrain-v2/stage4-authoring`.

Runtime `terrain.surfaceBrush` now shares the editor's raise/lower/flatten/smooth/
paint implementation. Surface changes coexist with excavation and persist through
versioned native references. Paint leaves real collision handles intact; sculpt
replaces affected support atomically. See the scripting reference for defaults,
coordinate frames and the explicit changed-sample save budget. This does not yet
qualify navigation/fluid publication or planetary surface sculpting.

## Source-owned solid integration

`TerrainSolid` provides immutable, current-frame solid snapshots from committed
heights and the excavation journal. Grid water uses those to distinguish earth
from air; generated triangle meshes remain collision boundaries and are not
misinterpreted as closed volumes. Paint preserves the geometry epoch, while
sculpting, excavation, replacement and placement changes invalidate occupancy.
Region visibility, origin changes and cold restoration use the same ownership.
The query follows the source field at the water backend's voxel resolution; it
is not a promise of microscopic equivalence to every extracted triangle.
Source queries currently scan the bounded edit journal, so heavily edited large
water regions still need representative cost qualification.

The integrated native control passes85checks, including real Particles V2 sweeps
through the newly excavated opening, coordinated terrain/water saving, origin
shifts and a rotated radial-gravity chart. The old triangle-shell interpretation
reported240m³ free capacity in an independent192m³ layout; source occupancy now
reports192m³ and retains64m³ of water during excavation and flow. Existing water
and particle suites pass unchanged. The new body index checks world ownership
as well as handles, preventing another world's ID from aliasing a terrain.

This does not change the Alpha fluid lock, default water policy, pressure/transport
solver or liquid research. It does not yet complete native navigation publication,
planetary excavation, physical residency or final Terrain V2 qualification.
