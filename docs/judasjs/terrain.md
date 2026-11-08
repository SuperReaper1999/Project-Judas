# Terrain — live bounded volumes

Current M97 API. `entity.terrain` is a handle even before initialization; `state`
is null until `create`. A stale entity throws ReferenceError. Invalid operations,
unsupported owners, exhausted budgets throw TypeError. `create`/`brush` return true when queued, or
false while another volume owns the single preparation lane; retry later.

```js
const terrain = rock.terrain;
if (!terrain.state) terrain.create({
  resolution: {x:21,y:17,z:21}, extent:{x:16,y:10,z:16}, navigation:true
});
// Later, after state.ready && !state.busy:
terrain.brush(hit.point,{kind:'remove',radius:2,strength:1});
```

`create(options)` creates a bounded solid grid, or accepts `source` containing a
serialized M84 `JudasVolumeTerrain` source instead of resolution/extent. This allows
project-owned authored caves to start editable in a standalone without filesystem
writes. Per-instance density is independent; original assets remain immutable.
`navigation` defaults false; `profile` defaults 0 and must be registered.
Creation requires a root, unit-scale static body without a planet, deformable or
baked navigation-surface component. Orientation is arbitrary. Do not combine with
another component that owns the same collision geometry.

`brush(point, options)` uses M84's `add`, `remove`, or `smooth` field operation.
Point is **current simulation-frame** world position, radius is local metres
(.05–1024, with brushes constrained to the finite source), strength defaults 1
and must be 0–1. A brush is one dab, not a held-tool stroke; game JS owns cadence,
target queries, rewards and construction rules. Prefer authoritative fixedUpdate
for input-driven edits. No universal world-up is assumed.

`state` reports ready/busy/navigation, committed revision, samples/triangles,
estimated geometry bytes, last prepareMs/publishMs and error. Creation and brushes
prepare asynchronously. Only one preparation per world is accepted; false signals
normal backpressure. Retry creation from fixedUpdate until a state record exists. Failed extraction/cook/navigation keeps the last committed geometry.
An entirely empty extracted volume is rejected. No resolution reduction occurs.

All three representations publish between steps. Until then queries/rendering/nav
continue using the last complete product. CPU preparation never mutates physics or
GL. Native navigation is rebuilt only for this volume's triangles, in its authored
local +Y navigation frame, using the selected agent profile. It does not rebake
nearby independent surfaces, obstacles or arbitrary unions automatically. Use links
for traversal between separately owned surfaces. Support orientation and sampled
[gravity](physics.md) remain separate.

Limits: 8 volumes/world, 32768 samples/volume, 262144 total samples, 64m per axis,
16384 triangles/product. M84's finite air boundary remains. This is neither soil
simulation nor planetary terrain LOD editing. Small rotated volumes can be placed
in planetary environments, including live-origin worlds. Adding solid around an
occupied body uses ordinary penetration recovery; scripts should avoid entombment.

Committed density and navigation settings survive cold saves and snapshot regions.
Pending edits defer saves/unload. Restore prepares collision/navigation before
publishing the replacement world. Stop discards runtime edits; explicitly save to
retain them. Source authoring files are never changed by gameplay.

See [scenes/streaming](scenes-state.md), [queries](physics.md),
[Living World Lab](../M97_LIVING_WORLD.md).

## Terrain V2 landscape preview (Experimental)

`entity.terrain.createLandscape(sourceJson)` initializes an opt-in sampled
landscape on a root, unit-scale static owner. `sourceJson` is a serialized
`JudasTerrainV2` source, not a model asset ID. Its regular heightfield and local
excavations share one source and one collision publication boundary. Use
`terrain.brush(point, {kind:"remove"|"add", radius, strength:1})` to excavate or
add material. Points use the current simulation frame, including after an origin
shift. The V2 landscape path does not currently implement the legacy volumetric
`smooth` brush; it rejects it explicitly. Existing bounded volume `create` and
`smooth` behaviour is unchanged.

`terrain.state` additionally reports `chunks`, `volumeChunks`, and
`pendingChunks`. `bytes` is a CPU geometry/source estimate, **not** total process,
BVH or GPU memory. `prepareMs` is worker preparation elapsed time;
`publishMs` is owner-thread publication time. Do not interpret either as FPS.
A pending edit is explicit backpressure; check `busy` before another edit.
Unlike the older bounded-volume preparation lane, independent V2 owners can
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

This API does not yet rebuild independent authored navigation surfaces or update
a fluid solid-volume bake. Those integrations remain explicit Terrain V2 work;
do not infer a successful terrain publication also rebuilt those systems.

Native landscapes can opt into source-owned navigation through the existing
Navigation Surface component, using an empty baked-asset reference. The editor's
**Live terrain navigation** option configures it. This does not change the legacy
bounded-volume `create({navigation,profile})` contract. See [navigation](navigation.md)
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
native landscapes; the legacy bounded volume component retains its grid estimate.

Caches allocate on demand. A pending edit retains old collision until complete.
Resource pressure or damaged pages report errors rather than becoming empty
terrain. The existing native save identities, sparse override limits and project
save workflow are unchanged. See [terrain residency](../TERRAIN_V2_RESIDENCY.md)
for cache budgets, working allowances and ownership.
