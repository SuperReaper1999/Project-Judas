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
