# Terrain V2 runtime editing

`entity.terrain.createLandscape(sourceJson)` initializes an opt-in sampled
landscape on a root, unit-scale static owner. `sourceJson` is a serialized
`JudasTerrainV2` source, not a model asset ID. Its regular heightfield and local
excavations share one source and one collision publication boundary. Use
`terrain.brush(point, {kind:"remove"|"add", radius})` to excavate or
add material. Points use the current simulation frame, including after an origin
shift. Volumetric smooth is unsupported; surface smoothing uses `surfaceBrush`.

`terrain.state` additionally reports `chunks`, `volumeChunks`, and
`pendingChunks`. `bytes` is a CPU geometry/source estimate, **not** total process,
BVH or GPU memory. `prepareMs` is worker preparation elapsed time;
`publishMs` is owner-thread publication time. Do not interpret either as FPS.
A pending edit is explicit backpressure; check `busy` before another edit.
Independent V2 owners can
prepare concurrently. A busy owner returns false without submitting another
edit. Translation and rotation update all derived colliders; live scaling and
motion-authority changes are rejected explicitly.

The ordinary runtime-terrain save/streaming participant captures the landscape
source and excavation journal. An authored `terrain.asset` component initializes
native landscape data before scripts start; it does not require
`createLandscape`. Do not initialize that owner a second time. Asset-backed saves
store the base asset identity/fingerprint and journal; procedural sources store
a complete native snapshot (8 MiB save-string limit). A changed base requires an
explicit content migration. Derived chunk body handles are transient. Query
results resolve to the owning terrain entity, and ignored-terrain query filters
include its generated chunk bodies. Save/load restores native geometry before
publication. Graphics material state does not own physical terrain quantity.

Editable example: `projects/terrain_v2_workshop`. These APIs are Experimental
source-build work, not a published supported-release update. Planetary
excavation, large-world physical residency and full cross-system qualification are
still being developed; the initial flat landscape slice must not be treated as
completion of those capabilities.

### Runtime landscape sculpting and painting

`terrain.surfaceBrush(point, options)` applies a deterministic surface dab to an
initialized V2 landscape. `kind` is `raise`, `lower`, `flatten`, `smooth` or
`paint`. Radius is source-local metres, from sample spacing through 128 m.
Strength defaults to 0.25 and must be finite, 0–10; raise/lower use metres per
dab, while flatten/smooth/paint blend with a falloff and saturate their blend at
one. `height` defaults to zero and specifies the flatten target in **source-local
height**, even when the landscape is rotated. `layer` defaults to zero and must
be an integer 0–3. Paint uses the same four material layers as the editor.

```js
if (landscape.terrain.state?.ready && !landscape.terrain.state.busy) {
  landscape.terrain.surfaceBrush(hit.point, {
    kind: 'raise', radius: 3, strength: 0.15
  });
}
```

Points use the current simulation frame. Source rotation/translation is applied
by the engine. One busy owner returns false; invalid/no-effect/out-of-bounds dabs
throw. Painting queues no collision extraction and preserves collider identities.
Sculpted geometry publishes atomically with its source and visual revision; old
support remains usable while preparation is pending. Excavation and surface
changes coexist, including at chunk corners. This edits the height raster under
the excavation journal; it is not a smooth 3D SDF sculpting operation.

Native asset-backed saves now preserve exact changed height/material samples as
a versioned sparse override, alongside the base fingerprint and volume journal.
Repeated dabs replace sample values rather than growing a brush-history log.
The current bound is 65,536 distinct changed samples per native landscape; an
edit exceeding it is rejected before publication, retaining the previous source.
Procedural sources continue to use complete native snapshots within the existing
8 MiB save-string limit. Existing journal-only reference saves remain readable.
Invalid sample indices, weights and changed base assets are rejected.

Independent baked navigation surfaces require explicit rebaking. Source-owned live navigation follows terrain publication. Advanced Fluids V2 uses its own current collision preparation; no retired liquid bake is produced.

Native landscapes can opt into source-owned navigation through the existing
Navigation Surface component, using an empty baked-asset reference. The editor's
**Live terrain navigation** option configures it. See [navigation](navigation.md)
for coverage, actor steering and publication details. Geometric changes update
both support and routes; material painting preserves both collider and navigation
identities. Native navigation remains optional.

## Native Terrain V2 residency counters

Native landscapes page source samples and exact collision separately from visual
LOD. Camera visibility never removes physics support. The state object exposes
`sourcePageBytes`, `sourcePageBudget`, `sourcePages`, `sourceEvictions`,
`collisionPageBytes`, `collisionPageBudget`, `collisionPages`, `collisionEvictions`,
`gpuPageBytes`, `gpuPageBudget`, `gpuPages` and `gpuEvictions`. `pagePrepareMs` is
the last GPU page preparation/upload; `worstPagePrepareMs` is its high-water mark,
not a sum for the current frame. `bytes` is a current CPU payload estimate for
landscapes.

Caches allocate on demand. A pending edit retains old collision until complete.
Resource pressure or damaged pages report errors rather than becoming empty
terrain. The existing native save identities, sparse override limits and project
save workflow are unchanged. See [terrain residency](../TERRAIN_V2_RESIDENCY.md)
for cache budgets, working allowances and ownership.
