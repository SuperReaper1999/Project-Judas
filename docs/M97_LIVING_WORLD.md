# M97 — Living World

Human-accepted M97 implementation; current platform qualification remains separate.

Open `projects/living_world_lab/living_world_lab.judasproj` in the editor. The normal
project contains three independently retained quarries, an editable finite rock,
construction stock and a joint-supported cantilever. The third quarry is rotated
90 degrees and has its own ordinary gravity region. The root starts at an absolute
Earth-radius offset (6,371,000m); geometry remains in the existing local chart.
This is a large-origin/local-region demonstration, not a whole-planet excavation store.

## Controls

- WASD/mouse, Space: move/look/jump.
- Click: one terrain dab at the crosshair (within 10m). Q: Remove → Add → Smooth.
- B: spawn a physical construction block and adopt it into the selected region.
- Delete: release a targeted beam's physical joint, otherwise remove a dynamic body.
- E: throw a ball (root-owned diagnostic prop; not durable regional construction).
- F3: travel to the next quarry; F1: first quarry; F2: rotated quarry.
- F4: request an advance on the 512m origin grid without physically moving the world
  (the first shift is 328m because the initial absolute origin is not grid-aligned).
- F6/F7: save/load `terrain-round`. R: discard runtime state and reload authored scene.
- Escape: pause/resume. HUD shows publication error, native preparation time,
  region states, dependency pins and retained bytes.

Walk between the two flat quarries to exercise position-interest streaming, or use
travel keys. Traveling waits for the destination to be active before positioning
the character. This project convenience does not implement regional physics.

## Ownership and publication

`RuntimeTerrain` is allocated only when used. It owns density and immutable prepared
products per static entity. M84 extraction and brush stencils are shared source;
there is no second terrain solver or edited-content file mutation. A single world
job prepares a bounded mesh, cooked collision and optional Recast navigation from
one field revision. The owner thread stages native navigation and changes static
collision **in place**, preserving the body generation, classification, materials,
constraints and query identity. Nearby sleeping bodies wake; obsolete geometric
contact history is retired. Renderer uploads only committed mesh revisions.

Failure retains all previous products. There is no rendering/collision mismatch
while work is pending. Newly streamed surfaces remain private until region
publication. Pending edits defer snapshots and unload rather than silently losing
a dab. Saves already requested during pause need resume to publish pending work.

Navigation is local to this volume, using its explicit local +Y navigation frame
and registered agent profile; it is independent of sampled gravity. Adjacent
external scene geometry is not silently included. Recast itself is now a shared
geometry-bake library; existing scene authoring still uses the same wrapper.

## Regional suspension

The existing WorldStreaming coordinator retains authored rigid bodies, joint
settings, physical pose/velocities, script JSON and its previously supported
animation/kinematic/wheel/powertrain/weather/deformable records. M97 adds the
committed terrain field/settings and primitive runtime construction records.

Adopted root rigid/render/script/joint members receive region-local identities and
are reconstructed by the existing staged installation path. Destroyed members
retain tombstones. Their motion does not advance while unloaded. This is suspension,
not coarse off-screen simulation. Cross-region hierarchy, joints, script references,
active character support, selected gravity and other existing hard dependencies
remain explicit residency pins. Transfer an entire supported assembly, release its
external dependents, or keep it resident. Budget refusal retains live objects.

Navigation intent, active ragdoll authority, conserved liquid groups, legacy fracture
families and newly adopted complex/resource-backed subsystems retain existing pins
or adoption restrictions. No universal arbitrary-assembly suspension is promised.
Joint configuration and velocities resume, not a guarantee of bit-identical warm
contact/constraint continuation across unloading. Full cold saves retain the
existing physics continuation records separately.

Streaming archive v9 adds the optional terrain payload; older archives keep their
existing readers and contracts. Full saves have an optional `runtime-terrain` chunk.
Ordinary-joint saves now use articulation v5 to retain exact native anchors/frames
before restoring the existing strict continuation records. This avoids float
inverse/forward frame loss when a joint owner itself moves or rotates. Older
versions retain their original validation; no continuation tolerance was weakened.

Cold restore reconstructs actual collision/navigation before publishing the world,
not merely script-visible terrain status. Retained-region state counts toward the
manifest's retained-byte budget. Limits are in the [Terrain API](judasjs/terrain.md).

## Editing authored content

The initial rock is an ordinary M84 source, model, collision and receipt. The startup
`workshop.judas` is the small resident bootstrap. Stop Play, open `Scenes/quarry.judas`
(or `quarry_rotated.judas`) as the content scene, select the rock and use
View → Volumetric terrain to edit/cook as before. Return to `Scenes/workshop.judas`
to play the complete composed project. After changing
its source, refresh the gameplay module explicitly:

```sh
build/judas_volume_author cook projects/living_world_lab/living_world_lab.judasproj Imports/quarry.judasvolume
python3 tools/EmbedVolumeSource.py projects/living_world_lab/Imports/quarry.judasvolume projects/living_world_lab/Assets/scripts/source.js
```

This explicit small content step ensures standalone has the initial scalar field
without requiring source-file access or a new resource type. Runtime edits never
rewrite the authoring source. Play/Stop discards unsaved gameplay changes; use slots
for persistent play state. Each region instance gets independent mutable density.

## Human checklist

1. Enter the quarry entrance; excavate its back wall and walk into the new cavity.
2. Add a blockage, throw a ball at it, then remove/smooth it.
3. Build several blocks; release the cantilever joint and watch it fall.
4. Leave for another quarry; confirm the old region becomes unloaded.
5. Return: check excavation, blocks and broken support remain changed.
6. F6 save; restart the application and F7 load; inspect actual geometry/state.
7. Visit the rotated quarry: movement, digging and props follow its local gravity.
8. F4 shift origin; confirm geometry and queries stay aligned; repeat save/return.
9. Pause/resume and Editor Stop/Play; distinguish saved state from authored reset.
10. Repeat in the relocated standalone.

Windows/Android functional validation is outstanding/nonblocking. No acceptance of
unrelated deferred physics/fluid issues is implied.

### Private physical staging

Private streamed rigid/kinematic actors are now explicitly simulation-suspended
until atomic publication. Collision disabling alone is intentionally insufficient:
gameplay may disable a collider while keeping a body moving. This separate internal
lifecycle flag prevents asynchronous preparation time from becoming hidden fall
time or consuming kinematic intent. It is not a JS pause mode or a saved physical
quantity. Ten seconds of private-step regression preserves pose and both velocities
exactly; slot reuse starts unsuspended. Unloaded regions still remove their bodies.
