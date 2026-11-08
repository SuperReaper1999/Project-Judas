# Terrain V2 — Experimental implementation

This is source-build development on Experimental. One source owns ordinary
heightfields, local excavation, materials, collision, navigation and persistence.
Fine-grained source/collision paging and independent GPU residency are now
implemented and automatically qualified, including the matching SDK, relocated
exports and completed Experimental/Alpha CI. Exact scopes and exclusions are in
the qualification record. Human terrain
acceptance is pending. Existing project assets are not replaced automatically.

The current architecture and budgets are described in
[Terrain V2 residency](TERRAIN_V2_RESIDENCY.md). The consolidated checkpoint,
SDK, measured results and human checklist belong in
[the qualification record](TERRAIN_V2_QUALIFICATION.md). Named measurements later
in this document describe their original checkpoints; they are not automatically
measurements of the latest build.

## Try the current implementation

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

For human review, use copies or save your authored changes deliberately:

1. Open the Highlands Workshop, press T in Play, wait for the queue to finish,
   then walk through both ends of the actual cross-chunk tunnel. Try manual dig,
   add, raise and paint; save, quit and load to check both edits and progress.
2. In Edit mode, create a landscape, sculpt a gesture, undo/redo, change a surface
   material, then **Save and update playable terrain** and save the scene. Play,
   Stop and reopen it. Check the source picker and Landscape Inspector selection.
3. Open the separate Planetary Workshop. Try T and O, walk through the excavation,
   save and cold-load. Check that the base planet does not block the cave and that
   the source remains attached through the origin change.
4. Add a Particle Emitter in a throwaway project. Under **Collision response**,
   enable world collision and optional **Use local collision coverage**. Confirm
   that its offset/half size persist and remain accessible through JudasJS.
   Cover all required paths; a box that is too small reports failed queries.

Automated controls and screenshots do not substitute for this visual/gameplay
acceptance. Existing games, installed SDK selections and the fluid lock remain
untouched. Linux desktop qualification does not certify this new terrain work on
Windows, Android, browsers or older Linux distributions.

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

## Residency and support boundaries

Runtime native samples use bounded pages rather than complete source arrays.
Heavy collision data is loaded independently of rendering; small body descriptions
and exact bounds remain resident so arbitrary off-camera queries remain correct.
A leased product cannot be evicted while a consumer uses it. Independent GPU
pages retain native painted detail without coupling it to geometric LOD.

Planetary excavation uses bounded projected native patches, described below.
Source discovery covers up to512editable sources under Authoring. The current
native source limit remains1,024chunks per owner and8owners; paging does not make
an unlimited terrain owner or remove existing regional/save limits. See the
residency reference for cache quotas, transient preparation memory and failure
policy. Final SDK/export/platform claims are recorded with their exact revisions
in the qualification report, not inferred from these capabilities.

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
required; competing geometry, motion and fluid owners are rejected explicitly. An optional
source-owned Navigation Surface is supported as described below. The seed supplies identity/material/filter settings. It is
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
collision and visual residency were still open at that checkpoint. Regional
streaming and subsequent memory results below supersede that historical scope.

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

At that checkpoint, temporal transitions and fine-grained source/collision paging
were still outstanding. Both are implemented in the current source. The old
full-source GPU image restriction has been replaced with checks on bounded
render pages. Historical desktop measurements do not certify Android/browser
terrain rendering or constitute human acceptance.

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
coordinate frames and the explicit changed-sample save budget. Source-owned navigation and water occupancy are qualified below. This does not
qualify planetary surface sculpting.

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
solver or liquid research. Planetary excavation and native SDK qualification have
subsequently been exercised below. The per-chunk physical residency restriction
recorded at this checkpoint has since been removed; see the residency reference.
Source-owned navigation publication is described below.

## Native regional streaming checkpoint

The following measurements describe the earlier region-granular implementation.
Current sample/collision/GPU paging and final costs are in the residency and
qualification references above; whole-region retention below is historical.

Native sources now use the existing region preparation worker to decode samples,
extract chunks, cook collision BVHs and build height LOD before registration.
The completed product is moved into one world-owned runtime; it contains no live
world or graphics handles. Registration still creates native physics handles on
the frame thread. A single source's registration is indivisible, so the 2 ms
streaming budget is a scheduling target rather than a hard deadline.

Saved revisions reuse verified base samples, check the current native header
identity, and rebuild only changed-height and excavation dependencies. Reverting
an override reads and verifies the required original native pages; paint-only
restores preserve all collider identities. Cold initial source preparation still
reads and verifies the whole native source. No permanent second base array or
unbounded geometry cache is introduced.

Region admission accounts for source samples, mesh, BVH and LOD payload, instead
of only scene text. Committed runtime edits refresh this estimate. Excavation can
increase occupancy beyond the original admission estimate; it does not silently
unload occupied ground, and subsequent admission observes the increased total.
These are payload/admission estimates, excluding mutable Detour caches, allocator
overhead, physics-world storage and GPU residency. Worker preparation's temporary allocations are not a
hard-capped heap. Use suitably sized authored regions and sensible pending/live
budgets; the original kilometre fixture retained about77MB of accounted payload.
The unused ordinary CPU draw streams have since been released: the same excavated
fixture retains54.25MB without navigation or57.32MB with full-kilometre navigation.
All1,024collision chunks and their original cooks remain. Preparation still needs
the extracted streams temporarily. These are owned payload estimates; process RSS
and peak memory are substantially larger. See runtime-memory evidence below.

The preserved five-visit 513×513 fixture keeps all 1,024 collision chunks, sculpts
and excavates, unloads everything and reconstructs exact saved source state in a
new process. The baseline's cumulative largest integration unit was 516.6 ms;
the first corrected qualification measured 3.41 ms. Total load/revisit wall times
were 478–516 ms for the first three visits and 471–479 ms for the last two,
including background preparation. Five visits are not independent benchmark
percentiles, and these unit times are not FPS. Raw scope and before/after results
are under `docs/evidence/terrain-v2/regional-streaming`.

At this earlier checkpoint, per-chunk source paging was still absent. Current
paging is described in the residency reference; camera-driven physics eviction
remains deliberately avoided.
Regional suspension remains the existing coordinator's responsibility, and native
terrain collision products then remained resident with the region. Current
products are leased and paged independently of visual LOD. Particle
snapshot limits also remain: default whole-chart capture can exceed the body or
triangle budget on a large landscape and reports that explicitly. An authored
local effect can now opt into the Particle Emitter's **Collision response → Use
local collision coverage**, or the equivalent JudasJS settings described in
[effects and cameras](judasjs/effects-camera.md). The declared box follows the
emitter; include every required path and collision radius, including older live
particles. Leaving its coverage remains an explicit failed query, not empty space.
The large-source integration controls compare bounded contacts with a complete
kilometre reference while retaining all1,024physical terrain chunks.

## Source-owned navigation

A landscape can own a live Navigation Surface using the existing component and
project agent profiles. Its baked-asset field must be empty, dynamic source
collection must be disabled, and its physical layer must remain included. The
landscape Inspector offers **Live terrain navigation**, configuring initial
coverage from the source. The Navigation Surface card exposes the profile,
coverage, resolution and advanced tile controls. **Preview live terrain
navigation** shows source geometry without writing a stale static bake asset.

This mode includes this landscape's actual collision surface and cave floors,
not its identity Box, render LOD or unrelated scene props. Use normal navigation
obstacles for props, and existing links/separate surfaces where needed. The
landscape's local +Y defines traversal up; gravity remains independently owned.
Navigation coverage is a declared local box, not a promise of planet-wide routing.

Collision and navigation preparation share immutable chunk geometry. Dirty terrain
rebuilds affected navigation tiles in the worker; both publish at the same boundary.
Old complete support/routes remain while work is pending. Paint-only changes
reuse navigation. A fully excavated coverage can publish an empty navmesh rather
than preventing the excavation. Rigid placement, regional publication/removal,
body enable/disable, origin changes and native save restoration use the same
world-owned lifetime. Navigation itself never moves characters; ordinary JudasJS
agent steering and CharacterMotor still control gameplay.

Coverage remains bounded by the existing 4,096-tile limit, the pinned Recast
height-span range, and a native-runtime preparation budget of 16 million raster
columns including tile borders. Oversized requests reject with a useful error;
coverage or collision quality is never silently reduced. Navigation rebakes its
configured coverage, not yet individual changed Recast tiles. Choose regional
coverage and cell sizes appropriate to gameplay.

Ordinary offline bakes now collect native terrain triangles too. Existing bakes
created against a native landscape's placeholder Box become correctly stale and
need an explicit rebuild. A separate static bake that includes a native terrain
cannot follow its runtime edits automatically: geometric edits are explicitly
rejected until that dependency is removed/excluded or replaced by source-owned
navigation. Painting remains available. Existing V1 bakes and projects without
native landscapes retain their usual workflow.


## Navigation cache handoff

Native terrain preparation builds both compressed Recast layers and a private
Detour cache in the existing worker job. Publication moves that cache into the
existing NavigationSystem; it does not reconstruct tiles on the frame thread.
The cache has no world/entity/graphics handles before installation, is consumed
once, and is validated against its immutable source identity. Cancellation or a
failed replacement retains the previous complete collision and routes. Explicitly
re-enabling an already disabled surface still reconstructs its cache synchronously;
ordinary native construction, regional staging and edits use worker preparation.

Ten warm-file runs per variant, ordered before/after then after/before in groups
of five, used the same 1 km source, 1,024 collision chunks and full 1,024 m
navigation bounds at 1 m cells. Median dirty publication fell from 26.38 ms to
0.495 ms; sampled worst fell from 28.12 ms to 0.616 ms. Complete edits remained
297.43 versus 298.04 ms: this removes a frame-thread pause, not total work.
At that checkpoint, whole-coverage navigation still rebuilt. The subsequent
incremental preparation below removes that rasterization work while preserving
the private cache handoff.
Peak process RSS was 230,880–231,276 KiB before and 231,512–231,980 KiB after;
there is no memory-reduction claim. These ten-sample grouped diagnostics do not
establish general percentile distributions or FPS. Raw output and commands are
in `docs/evidence/terrain-v2/navigation-cache`.


## Planetary patch core (internal qualification)

`TerrainPlanetChart` maps the existing sampled/volume geometry to bounded
cube-face patches using the same binary64 rays as `PlanetSurface`. Both collision
and rendering receive the mapped mesh. Patch and chunk origins retain local
float32 geometry even at high elevation. `PlanetTiles` can suppress replaced
base tiles and protect the native-resolution rim during ordinary multi-camera
LOD. Disabled base collision backing is retained so removing an override can
restore support immediately. Its allocated collider count includes that backing;
submitted triangle diagnostics exclude hidden base tiles.

The internal composition controls exercise actual MK2 pit/tunnel geometry,
original-ground negative controls, geographic rim support and restoration.
Modern/portable desktop framebuffer and shadow checks pass; ordinary no-cutout
selection matches a frozen pre-change implementation exactly. This is core
geometry qualification, not final visual or human gameplay acceptance.

## Geographic native terrain in ordinary projects

The current integration adds Source3 chart metadata and native envelope2. Ordinary
Source2/native1 files retain their layout and remain readable. Older readers
explicitly reject envelope2 instead of treating curved terrain as flat geometry.
The stored chart includes geographic footprint, base elevation, immutable parent
geometry identity and the protected edge samples. Binary64 absolute placement is
separate from source metres and local float32 chunk vertices.

In World → Terrain tools → Create Terrain V2 landscape, choose **Planetary patch**,
select a planet and centre the scene view on the desired surface area. Sample
pitch is derived from the parent's physical rim. The workflow generates editable
source, recipe, native data and previews, inserts/selects the root object in an
undoable transaction, and opens the existing sculpt/paint/excavation tools.
Scene view and Play both suppress the underlying planet footprint. The outer
rim cannot be sculpted/excavated away from its parent; painting is allowed.

A CLI counterpart uses the serialized PlanetSettings descriptor and a direction
in the planet's model frame:

```sh
build/judas_terrain_author v2-planet planet.json Authoring/cave.judasterrain2 0.09375 1 0.09375 128
build/judas_terrain_author v2-native Authoring/cave.judasterrain2 Assets/Terrain/cave.judasmodel
```

Register native/preview assets normally. Scene key `terrain.planet 3` references
the existing planet object3, alongside `terrain.asset`. The root's pose is derived
from its chart and the scene's world origin, with unit scale. The editor computes
this placement; native tools may use `TerrainV2::PlanetTerrainPlacement`. Do not
manually move the geographic object or overlay a flat source over intact ground.
The older full-mesh/bootstrap cook is rejected for projected sources.

Source preparation projects both render and collision meshes. Brushes retain
world-metre radii and solid occupancy maps through the same chart. Runtime and
cold saves preserve chart, surface paint and the excavation journal. Regional
`terrainPlanet` qualified references remap the parent identity; the manifest must
keep that producer resident as a dependency. Private staging reserves coverage
without removing live ground. Publication activates it; hiding/removing a patch
restores retained planetary support. Overlaps and invalid provider/pose/rim
combinations reject explicitly. Absolute planetary descriptors keep their frame
through regional placement, origin transactions and cold restoration.

Try `projects/terrain_v2_planet_workshop/terrain_v2_planet_workshop.judasproj`.
Its ordinary scripts support radial-gravity walking, world-space sculpt/paint,
a cross-chunk tunnel, origin changes and cold saves. The automated relocated
export walks through the real tunnel and restores its progress in a separate
process. This is engineering verification, not human visual/gameplay acceptance.

Boundaries: geographic patches are bounded interior cube-face footprints, with
32..512 cells per side matching the parent's physical grid. They are scene-owned,
not movable prefab instances. Overlapping patches and cross-face excavation are
not supported. Changing parent source geometry or incompatible collision/visual
sampling requires regeneration; custom callback providers cannot be serialized
as native projected assets. Each active patch retains its physical descriptions;
heavy collision and mapped render products use the current independent caches.
Mapped geometry renders at native detail, while the surrounding planet retains
its existing view-union LOD. This is not planet-wide volumetric excavation. See
the residency reference for current runtime ownership.

## Continuation size policy

Runtime-created native landscapes and asset-reference journals share an8MiB
continuation limit across capture, direct restoration, whole-world saves and
inactive regions. A real one-kilometre513x513 runtime source produces a5.27MB
record and cold-restores through both world and regional paths. The corrected
reader no longer applies the older2MiB limit to successfully captured native
state. Source/geometry, resident-chunk, whole-save and regional memory bounds
still apply. Asset-backed terrain remains preferable for large authored worlds
because saves retain references and edits rather than duplicating base arrays.

## Native SDK and retained-memory checkpoint (28984e7a)

This section preserves the earlier SDK/memory result. Its retention description
and package are superseded by the current paged implementation and final matching
SDK identified in [the qualification record](TERRAIN_V2_QUALIFICATION.md).

The fresh17-tool local SDK includes both editable Workshops and its own
`tools/validation/qualify_terrain_v2.py`. Standard editor/project/tool operations,
native terrain cooking, relocated exports and independent cold continuation pass
using that packaged content. This host-native preview requires GLIBC2.43; it is
not a published package or an Ubuntu24.04-qualified SDK. Existing installed game
SDK selections remain unchanged. See [SDK qualification](https://github.com/SuperReaper1999/Project-Judas/blob/28984e7a04db2ae1bcb07dbca2f25b598263c850/docs/evidence/terrain-v2/sdk-qualification/REVIEW.md).

Ordinary live chunks now discard unused CPU draw meshes after preparation;
collision owns its unchanged immutable cooks and rendering uses the shared grid.
Excavations and curved planetary patches retain their unique render meshes.
Matched kilometre-scale fixtures retain22.86MB less payload, with identical
collision certificates, collider counts and generated triangle counts. Peak
memory changes much less because extraction still constructs those meshes.
The 300-step physical seam trajectory remains exactly equal to the monolithic
reference. No load-time or FPS speedup is claimed by this memory correction.
See [raw measurements and controls](https://github.com/SuperReaper1999/Project-Judas/blob/28984e7a04db2ae1bcb07dbca2f25b598263c850/docs/evidence/terrain-v2/runtime-memory/REVIEW.md).

## Incremental navigation tiles

Native source edits now invalidate navigation through BOTH old and replacement
collision bounds, including each tile's complete erosion/filter border. Untouched
compressed layers are reused exactly, including empty tiles. Removed geometry
cannot lose its old dependency extent. Changes to bake settings/profile fall back
to a complete bake. The normal full operator, collision precision, atomic support
publication, path refresh and native navigation save format remain unchanged.

The 67-check reuse target compares results byte for byte with full bakes, including
the measured kilometre fixture. Its local excavation rebuilds16dependent tiles
and reuses1008. Matched10runs per side reduce complete edit/publication median
295.345 to75.691ms and sampled worst303.981 to82.351ms. This is asynchronous
worker completion latency, not frame cost or FPS. Full initial setup remains
about0.7seconds; geometry enumeration/binning and private Detour-cache construction
still run. Existing terrain/navigation/world/cold/editor/export controls pass.
See [raw comparisons and scope](https://github.com/SuperReaper1999/Project-Judas/blob/28984e7a04db2ae1bcb07dbca2f25b598263c850/docs/evidence/terrain-v2/navigation-tile-reuse/REVIEW.md).

The shared native baker's final optional update argument is source-compatible
for existing callers; C++ consumers must rebuild with the matching preview SDK.
Its rebuilt/reused tile counters are preparation diagnostics only, not serialized
gameplay state. JudasJS APIs were unchanged by that checkpoint. It preceded
the current independently paged source/collision products.

## Bounded native-heightfield LOD transitions

Flat native heightfields now cross-fade between complete, independently stitched
detail selections over 180 ms. `TerrainLodTransition.*` owns presentation history;
the terrain pass supplies complementary screen coverage to the ordinary colour
and shadow shaders. Unchanged patches render once. Changed seam rules count as
changed geometry even if the patch key is unchanged. No extra height textures,
mesh uploads or physical collision changes are involved.

The selected detail still uses all relevant cameras. During rapid changes the
latest request waits for the current fade boundary; a large camera teleport can
therefore take roughly 360 ms to settle. The 2-pixel error target describes the
newly selected endpoint, not every outgoing pixel during such a teleport.
Source publication resets history immediately, so an old intact surface cannot
fade over a newly excavated cave. Zero-time observer calls select their own union
immediately in a separate selection, preserving the game view's running fade.
History is bounded to two runtime selections plus one immediate observer list;
opening the editor Scene and Game views together cannot restart the fade every
frame. Mapped planetary patches still render full detail; this does not
replace the surrounding planet's existing LOD system.

Native modern/portable tests verify actual intermediate colour/shadow depth,
draw-order independence, unchanged terrain heights/shadows and normal renderer
state restoration. Cold saves, origin/streaming controls, editor creation and
both relocated consumer games pass. Cross-fades have a measurable cost: on the
two-camera kilometre colour-render benchmark, settled GPU elapsed was about
0.60 ms versus 0.50–0.58 ms in the preserved renderer; an exaggerated whole-view
transition measured about 0.96 ms. Ordinary camera movement reused 39 of 43
previous patches and measured about 0.62 ms. These scopes include submission
bubbles and are not whole-game FPS or an optimization claim. See
[commands, raw results and visual scope](https://github.com/SuperReaper1999/Project-Judas/blob/28984e7a04db2ae1bcb07dbca2f25b598263c850/docs/evidence/terrain-v2/temporal-lod/REVIEW.md).

This remains a source-build preview. Native consumers must relink with matching
headers/libraries; JudasJS APIs and terrain save formats did not change.
That checkpoint preceded the current data/collision paging and source material
detail work; see the current residency reference and material section.

## Responsive large-landscape brush previews

Editor preview preparation now produces one complete, read-only snapshot on the
worker: changed chunk geometry, the compatibility mesh and cached picking bounds.
The editor publishes them together and retains unchanged GPU chunks. Rejected,
cancelled or stale work cannot replace the current geometry/query pair. Retired
mesh readers keep safe shared ownership; source switches and shutdown release
both views together.

Brush hovering tests only triangle ranges whose conservative chunk bounds meet
the ray, using the same two-sided primitive and flattened triangle order as the
original picker. This is not an approximate terrain-height query: cave walls and
mapped planetary geometry remain pickable. The old `PickTerrain` native API stays
available. Internal editor preview mesh views are now const so readers cannot
invalidate cached bounds accidentally. JudasJS and saved terrain are unchanged.

On the kilometre fixture, three matched 120-ray runs reduced median query cost
from about 2.43 ms to 0.0053–0.0056 ms, with exact original hit positions. The
original 29–39 ms mesh assembly runs in the worker now, rather than during editor
publication. Exact reservation also saves 14.1 MiB of retained mesh capacity,
including the new 64 KiB query bounds. These are query/buffer measurements, not
whole-editor FPS, a measured peak-RAM reduction or a hard publication deadline.
Generic scene selection and legacy terrain/volume brush paths are not claimed
to have this acceleration. See [controls and raw evidence](https://github.com/SuperReaper1999/Project-Judas/blob/28984e7a04db2ae1bcb07dbca2f25b598263c850/docs/evidence/terrain-v2/editor-picking/REVIEW.md).

## Material detail authoring

**World → Terrain tools → Surface materials** exposes four collapsible layers.
Each has a colour texture, tint, tile size in metres and PBR roughness multiplier.
The first layer is expanded initially. **Shared normal detail** optionally adds
one linear RGB tangent-space normal texture, its metre scale and strength.
Textures can be selected or dragged from Assets. Apply, undo/redo and **Save and
update playable terrain** cover these settings together; ordinary scene Save
alone does not publish a modified terrain source.

Albedo and normal projections use the rigid source frame, including cave walls
and ceilings. Origin shifts do not reset their phase. The normal detail is visual:
it changes lighting, not the collision surface, water ownership or navigation.
RG8 neutral is `(128,128)`; use OpenGL-style tangent-space normal maps with positive
green. It is one shared detail texture, not four independent layered normal maps.
Roughness multiplies the landscape owner's PBR material roughness using the same
layer weights as albedo. It has no effect on Legacy or Unlit material lighting.

The optional source metadata is:

```json
"materialDetail": {
  "tileMetres": [2, 3, 6, 4],
  "roughness": [1, 0.9, 0.8, 1],
  "normalTexture": "registered-texture-asset-id",
  "normalMetres": 0.8,
  "normalStrength": 0.7
}
```

Tile sizes are 0.125–1024 metres, roughness multipliers 0–1 and normal strength
0–2. Unknown detail keys, malformed vectors and non-finite values are rejected.
Defaults remain four-metre albedo tiles, multiplier 1 and no normal texture;
default metadata is omitted to preserve existing source/asset identities.
Nondefault detail survives native cooking, reference saves, regional restoration
and export. New-source features require a matching Experimental runtime; older
runtimes do not implement these optional visuals. Existing assets are not upgraded
automatically. The ordinary compatibility preview atlas contains albedo only.

The portable shader adds three normal-texture samples when enabled and uses
texture unit 12, retaining the existing height, shadow, weather, lighting and
skinning bindings. No compute, array-texture extension or virtual-texturing system
is required. With no normal texture, its sampling branch is skipped. The new
`TerrainMaterialShader.h` owns terrain material GLSL; ordinary mesh materials keep
their existing path.

For an independent, disposable consumer test (never updates the authored Workshop
or its saves):

```sh
python3 tools/validation/qualify_terrain_v2.py --build build \
  --output /tmp/judas-terrain-material-consumer --material-detail
```

Add `--planet` to exercise the same settings on the geographic Workshop. Use a
fresh output directory each time. The driver authors an original periodic normal
texture, checks that export includes it, then excavates, traverses and cold-loads
the game from a genuinely relocated package.

## Exact heightfield collision preparation

Unedited ordinary heightfields now use a structured collision cook after proving
that the vertices form a strictly monotone X/Z lattice and that the entire triangle
index pattern matches. Their first-use vertex order, source feature numbers,
normals, adjacency, crease activity and shared deterministic BVH are identical to
the generic cook. The proof replaces repeated position-welding maps, edge maps
and boundary T-junction searches; it does not simplify the collision surface.
Caves, projected planetary patches and any mesh failing the proof retain the
strict generic cooker. Failed preparation never replaces previous support.

A matched native game with 1,024 terrain chunks, 1,025² samples and one-metre
spacing loads its scene in roughly 1.03 seconds versus 2.14 seconds before this
change on the Ryzen 5 5600. Warm peak process memory falls by about 24 MiB.
Steady simulation remains about 0.37–0.39 ms; this improves initial preparation,
not solver performance or the unrelated dense-body physics pile. Full results,
unchanged physical controls and method limitations are in
`docs/evidence/terrain-v2/structured-collision/REVIEW.md`.

This structured-cook checkpoint preceded demand paging. The current runtime
retains small physical descriptions but pages heavy collision/source products;
see [the residency architecture](TERRAIN_V2_RESIDENCY.md). Its startup/memory
measurements above remain historical evidence for the cooker itself.

A native landscape is a physical triangle surface and cannot also be a volume
sensor. Authoring and runtime creation reject that combination before publishing
any terrain. Use a separate box, sphere or capsule trigger for region events;
rejection leaves an existing ordinary sensor unchanged. This avoids the previous
mismatch where a sensor seed could produce blocking terrain triangles.
