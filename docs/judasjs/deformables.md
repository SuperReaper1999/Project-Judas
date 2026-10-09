# Deformable

Current deformation API (introduced in M62). Judas owns deformation and contact; project JavaScript chooses
forces, attachments and game meaning. Obtain `entity.deformable`; it returns null
until the normal immutable resource is ready, or if the component is absent.
Failed/stale baked resources throw a diagnostic `TypeError`; they are not a successful empty simulation.
Cache a ready handle, check `valid` after destruction/reload, and reacquire after
scene replacement. Invalid handles throw `ReferenceError` except `valid`.

## State and picking

`state` reports enabled/sleeping/error, mass (kg), world/local-simulation-frame
AABB `minimum`/`maximum`, node/contact counts, minimum element Jacobian, maximum
strain and named `groups`. These are snapshots, not mutable solver storage.

`raycast(origin, direction, maximum)` queries this instance's **current deformed
boundary**, returning `{point, normal, distance, location}` or null. Coordinates
are metres in the normal fixed-origin simulation frame. Direction is normalized.
`location` is a checked generation/triangle/barycentric snapshot, not a node pointer.
This query is separate from M44 rigid-body casts. Shape casts, CharacterMotor
blocking and M42 deformable events are not implied.

## Fixed-update mutation

All operations below belong in `fixedUpdate`; calling them in frame/UI callbacks
throws `TypeError`. Reading state/picking is allowed outside fixed updates.

| Operation | Meaning |
|---|---|
| `enabled = boolean` | Pause/resume this instance; retained node state; wake on resume |
| `reset()` | Reset rest shape at current owner placement, velocities, plasticity, pins and interpolation |
| `force(vector, group = "")` | One-step total force in newtons, distributed by mass over group; empty means whole asset |
| `impulse(vector, group = "")` | Total impulse in kg·m/s, distributed by group mass |
| `impulseAt(location, vector)` | Barycentrically distributed impulse; stale/invalid location returns false |
| `release(group)` | Release that attachment, retaining actual motion; returns false if absent |
| `attach(group, options)` | Replace/create a group attachment and wake |
| `setMaterial(options)` | Change supplied material fields, validate ranges, wake |

Unknown force/impulse groups return false. `force` also returns false for disabled
or failed simulations, preventing queued-force accumulation. Invalid ordinary
force/impulse vectors, materials and attachment targets throw; invalid
`impulseAt` locations/vectors return false.
`attach` options: `kind: "world" | "body" | "bone"`, `target: Entity` for body/bone,
`joint: stable skeleton key` for bone, and optional local-metre `offset`.
World pins use the owner's current transform as the prescribed frame. Body/bone targets transform each
group's rest node plus offset in their local frame. Body targets exchange reaction;
world/static/bone targets prescribe motion. Bone targets consume the existing final
authoritative pose, including passive ragdoll contributions, never interpolated GL
data. Lost targets release; explicit reacquisition is project policy.

Material fields: `density` (kg/m² cloth, kg/m³ solid), `stretchCompliance`,
`shearCompliance`, `volumeCompliance` (inverse membrane/strain energy stiffness),
`bendCompliance` (distance-hinge compliance), `damping` and `airDrag` (s⁻¹),
`airVelocity` (m/s), `thickness` (metres), `friction` (dimensionless),
`yieldStrain` (dimensionless; zero disables plasticity), `plasticRate` (s⁻¹),
`maximumPlasticStrain` (dimensionless bounded rest-strain envelope).
Plastic flow preserves element rest volume. These are approximate game materials,
not a calibration promise. Density changes preserve velocity, changing momentum.

```js
import {world} from 'judas';
export default class Fabric {
  constructor({entity}) { this.entity = entity; this.state = {started:false}; }
  fixedUpdate(dt) {
    const patch = this.entity.deformable;
    if (!patch) return;
    if (!this.state.started) {
      patch.setMaterial({airDrag: 0.3, airVelocity: {x: 2, y: 0, z: 1}});
      this.state.started = true;
    }
  }
}
```

See [lifecycle](lifecycle.md), [physics](physics.md),
[animation](animation-ragdolls.md), [saves](saves.md) and [streaming](streaming.md).
M64 extends sampled rigid contact to cooked static meshes, convex hulls and
oriented compound children through the same authoritative geometry path. This
does not add exact mesh/cloth CCD, motor blocking or deformable M42 events.
See [M64 geometry coverage](../M64_COLLISION.md) and the
[M62 implementation/limits](../M62_DEFORMABLES.md) for the physical support envelope.

## M63 fracture cooks

An optional partition uses this same resource/component. See [Fracture](fracture.md)
for irreversible interfaces, rigid/deformable representation, interior materials,
topology-aware picks, support release and the restrictions on reset/material/attach.
