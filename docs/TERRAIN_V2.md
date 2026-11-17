# Terrain authoring, runtime and persistence

Terrain V2 has one editable source for heightfields, local sparse excavation,
materials, collision, navigation and saves. Native assets and runtime procedural
sources share the terrain owner. The retired bounded-volume and terrain-to-static-
mesh authoring pipelines are not alternate V2 backends.

## Create and edit a landscape

Open `projects/terrain_v2_workshop/terrain_v2_workshop.judasproj` with the editor.
For a new project choose **World → Terrain tools → Create Terrain V2 landscape**:
Flat, Highlands, Island or Import heightmap. The transaction writes source/native
asset/preview/recipe, creates the scene owner, selects it and opens brushes.
Add component → New landscape uses the same workflow.

To replace a floor, explicitly choose **Replace selected ground**. Undo preserves
its identity, scripts and children while restoring the prior appearance/collision.
An old enabled floor underneath a landscape still blocks an excavated tunnel.
Gravity keeps its authored bounds; enlarge them for a larger landscape.

Use **Edit landscape → Select landscape** for raise/lower, flatten, smooth, paint,
dig and add. One gesture is one Undo item. **Save and update playable terrain**
publishes source and cooked products; ordinary scene Save is separate. Pending
or unsaved terrain work remains visible and must be completed/discarded before
conflicting publication. The editor never applies a worker result to a different
project or source revision.

The Workshop's project-owned controls are WASD/mouse, Space, left/right remove/add,
T tunnel tour, F6/F7 save/load and Escape pause. Wait for the queued tunnel before
walking through it. Use the [planet workshop](../projects/terrain_v2_planet_workshop/README.md)
for geographic charts; its source descriptor owns the absolute frame.

## Source, native products and tools

`JudasTerrainV2` source describes regular height samples and local volume edits.
`v2-pack` produces a native `.judasterrain` asset. `v2-native` creates its model
preview/receipt through the shared authoring publication service; it is not the
retired full-mesh runtime bootstrap. Inspect a tool's `--help` from the matching SDK.

From the source checkout (replace `build/` with the installed SDK directory
when using its matching tools):

```sh
build/judas_terrain_author v2-new /tmp/landscape.source flat 65 1 123 2
build/judas_terrain_author v2-validate /tmp/landscape.source
build/judas_terrain_author v2-pack /tmp/landscape.source /tmp/landscape.judasterrain
build/judas_terrain_author v2-native /tmp/landscape.source /tmp/landscape.judasmodel
```

Use caller-owned new outputs; valuable source files are not disposable. Heightmap
input uses `v2-heightmap IMAGE OUTPUT SAMPLES SPACING MIN_HEIGHT MAX_HEIGHT`.
Planet generation uses `v2-planet`; [planet authoring](PLANETARY_TERRAIN.md)
specifies its separate descriptor. The editor and CLI call `TerrainCook` and the
same source codecs/validators, not independent implementations.

A native `terrain.asset` component initializes the landscape before scripts start.
The owner is root, unit scale and static. Translation/rotation update derived
collision; live scale or incompatible motion authority rejects. Generated chunk
bodies retain terrain-entity query ownership rather than becoming game entities.

## Runtime edits and publication

[The scripting reference](judasjs/terrain.md) is authoritative for exact options.
`createLandscape(sourceJson)` is for a procedural owner; do not initialize an
already asset-backed owner again. `brush` adds/removes local volume material;
`surfaceBrush` raises, lowers, flattens, smooths or paints sampled surfaces.
Volumetric smoothing is not supported.

Check `state.ready` and `state.busy`. A busy owner returns false instead of
accepting another edit; invalid/no-effect/out-of-bounds edits report an error.
Points use the current local simulation frame. Replacement source, collision,
queries and navigation publish coherently; failed preparation retains the last
complete revision. Painting does not replace physical colliders. Localized
invalidations preserve unrelated GPU pages and unchanged collision geometry.

The editor reuses `HeightLod` and stitched runtime grids with a two-pixel geometric
error target. Brush interests pin affected chunks to full resolution. Cave and
planetary chart meshes retain their real geometry. Picking remains full resolution;
render LOD never becomes physical collision. Preview pages have their own bounded
32 MiB cache and borrow current material weights.

## Residency and failure semantics

[Residency](TERRAIN_V2_RESIDENCY.md) separates sampled-source access, exact leased
collision products and GPU pages. Per-owner caches are independent; camera
visibility is not permission to remove physical support. Physics, motors, wheels,
casts, navigation and particle snapshots acquire actual geometry. Grid-water
occupancy reads the immutable source, including excavation air.

Edits spill private checked products before publication. Spill data is disposable
preparation storage, not an automatic game save. Missing/corrupt required pages
produce errors rather than empty collision. Budgeted caches cannot silently evict
pinned products; excessive simultaneous demand reports a resource failure.

Declared bounds include eight owners, 1,024 chunks per owner, 65,536 durable raster
changes and an 8 MiB continuation string. Split large editable worlds into regions.
The cache limits exclude shared assets, navigation, driver allocations and temporary
worker products; use whole-process and GPU accounting alongside terrain counters.

## Save, reload, export and composition

Use ordinary SaveService slots to retain gameplay edits. Asset-backed saves record
base identity plus sparse changes/journal; procedural saves include a complete
native source. A changed base requires explicit compatibility policy. Cold load
reconstructs physical terrain before publishing the replacement world. Derived
body handles and prepared-page files are not durable identities.

Native terrain assets follow normal asset dependency closure and moved exports.
Source authoring files are not hidden runtime dependencies. Regional suspension
and origin shifts have explicit participant admission rules; see
[world streaming](WORLD_STREAMING.md), [assembly travel](ASSEMBLY_TRAVEL.md) and
[planetary terrain](PLANETARY_TERRAIN.md). A single nearby physics chart is not
simultaneous planet-wide dynamics or arbitrary-world streaming.

## Implementation and checks

Source ownership is under `src/terrain`: source/codecs, surface and volume edits,
`TerrainCook`, `HeightLod`, `RenderPages`, collision preparation and persistence.
`TerrainV2PreviewRenderer` owns editor presentation; `WorldTerrain` integrates
normal world ownership; the terrain scripting module delegates to those services.
The registered terrain gates cover editor image/depth parity, edits, collision,
queries, regional lifecycle, planet charts, residency and fresh-process saves.
Actual author/cook/export/relocate/cold workflows are recorded in
[development readiness](DEVELOPMENT_READINESS.md). Functional checks are not a
promise that every landscape/device/workload meets a frame budget.
