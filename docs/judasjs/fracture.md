# Fracture

M63 adds an optional material partition to an ordinary `.judasdeform` asset.
Judas owns physical demand, irreversible connectivity and material ownership.
Project JS chooses loads, sounds, scoring and explicit cleanup; it does not need a
health counter or a scripted collapse sequence.

## Access and lifetime

`entity.fracture` returns a `Fracture` handle once its cooked resource is ready,
otherwise null. A malformed cook is reported by `entity.deformable` / normal asset
loading. `id` is the logical family entity, `epoch` the instance generation.
`valid` becomes false after destruction, scene reload or Play/Stop. Other stale
operations throw `ReferenceError`. The owner remains nonphysical; it has no
collider or copied material mass. Its original transform is placement, not a
fragment root. Reacquire parts from `state`, not from old body-slot numbers.

## Fracture.state

A snapshot, not mutable storage:

- `revision`: topology revision, incremented on publication/removal.
- `rigid`: ordinary M45-connected rigid cells, or M62 cohesive deformable cells.
- `error`: bounded-topology diagnostic, empty on success. Also inspect
  `entity.deformable.state.error` for preparation/solve failures.
- `tensileStrength`, `shearStrength`: cooked thresholds in Pa.
- `parts`: `{key,index,component,removed,entity,mass}`. Stable source keys/indices;
  `component` is the minimum surviving part index in its connected component,
  or null for removed material. `mass` is source material mass in kg (including
  the historical amount on a tombstone). `entity` is an ordinary safe rigid
  part entity or null for deformable material, removed/disabled/not-yet-published
  rigid cells. Several cells can belong to one connected component.
- `interfaces`: `{key,a,b,broken,cause,tension,shear}`; `a/b` index source parts;
  `cause` is `physical`, `explicit`, or null. Demand numbers are observed peak
  tractions in Pa, not current health or a calibrated engineering damage measure.

M79 additionally offers an opt-in rigid **structural section stress** cook with
geometry-integrated bending, combined axial/bending stress and approximate torsion.
Per-interface strengths can be authored in the editor or partition source; the
state's top-level strength fields remain family defaults. Existing methods,
callbacks, safe handles and legacy asset behaviour are unchanged. See
[structural authoring, equations, limitations and lab](../M79_STRUCTURES.md).

Assets/strengths are authored offline. Elastic compliance and plastic yield are
separate. Default compression does not crush; an optional positive cooked
compression threshold enables brittle compression failure. No fatigue accumulation.

## Fixed-update operations

All mutation below requires a fixed-boundary callback (`fixedUpdate`,
`onFracture`, or a collision/sensor callback); frame/UI mutation throws `TypeError`. Vectors are finite world/simulation-frame
values: metres, seconds, newtons, kg·m/s. No universal world-up is assumed.

| Method | Result and meaning |
|---|---|
| `force(part, vector)` | Boolean; total one-step force, mass-distributed over a soft part or applied once to its ordinary body |
| `impulse(part, vector)` | Boolean; one physical impulse, never replicated over the whole family |
| `release(interfaceKey, revision)` | Queue an **explicit** interface cut; false for unknown/already broken/stale revision |
| `remove(part, revision)` | Queue explicit material removal; false for unknown/removed/stale revision. This is game debris policy, not physical failure |

Disabled/missing physical parts reject loads. Reads and queries are allowed in
other callbacks. Existing `entity.deformable.force/impulse/impulseAt` work too;
rigid `impulseAt` forwards exactly once to the owning physical body at the actual
hit point. For a rigid cell, use `part.entity.applyImpulseAtPoint` / ordinary
physics/joint APIs for precise loading or attachment. Rigid material and support
configuration are authored; runtime `setMaterial` / `attach` reject changes.
`release(group)` still releases an authored support. Soft attachments remain M62.
`reset()` rejects fracture healing: use a normal scene reload.

## onFracture(event)

Called on the logical owner's enabled scripts after all fixed-step topology
publication, never inside a solver loop. `{revision, interfaces:[{key,cause}]}`
is bounded by the cook's interface count; reacquire current parts through `state`.
Candidates are deduplicated and committed in source interface order. Physical
failure and explicit cuts remain distinct. Destroying an owner in a callback is
safe; later callbacks do not access it. No events replay on save restoration.
Removal of an already isolated part changes revision but may have no new broken
interface and hence no interface notification. Loading from a callback affects a
subsequent physical step; requests are not recursive fracture transactions.

## Geometry, persistence and residency

M44 queries, CharacterMotor, M42 contacts and audio obstruction see ordinary rigid
cell colliders; they never hit a duplicate intact-parent AABB. Soft fragments keep
M62 surface contacts and per-instance `deformable.raycast`; general M44 shape
casts/motor blocking/M42 soft-surface callbacks are still unsupported.
`DeformableHit.location.revision` makes pre-cut picks reject after topology change.

M61 saves persist broken interfaces, material removal, deformed/plastic state,
rigid transforms/motion and owned support/bond solver history. Source assets must
match. Pending requests are not saved: capture records the last coherent published
state. Live region-owned families explicitly pin their source region, with a
visible reason; `scenes.adopt(owner, "root")` can adopt the logical owner
before source suspension. Rigid material descendants are root-owned. Root removal
removes the whole family; `remove` or ordinary rigid `Entity.destroy` removes one
piece. `deformable.enabled=false` retains state and disables all family colliders;
individual `setColliderEnabled` retains the physical cell but disables its collider.
Liquid-bearing fractured owners are unsupported and rejected; no conserved water
is deleted by this feature.

See [M63 architecture/authoring](../M63_FRACTURE.md), [deformables](deformables.md),
[physics/joints](physics.md), [saves](saves.md), [streaming](streaming.md), and the
[executed example](examples/fracture.js).

Pending topology requests on a disabled family are deferred until it is enabled.
Required material/owner entities marked transient cannot be silently omitted from
an M61 save; capture fails with a diagnostic before replacing a good slot.
