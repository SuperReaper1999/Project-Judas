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
strain, named `groups` and per-attachment observations. These are snapshots, not mutable solver storage.

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

## M80 elastic/plastic structural solids

[M80 bending](../M80_BENDING.md) reuses this API for physically deforming
tetrahedral solids and optional soft section-based failure. Solid elastic strain
uses `shearCompliance`; `volumeCompliance` controls volume preservation.
`stretchCompliance`/`bendCompliance` remain cloth parameters. Yield/rate/capped
isochoric rest flow produces permanent deformation; the plastic cap is not a
failure threshold. Optional cooked interface strength determines physical fracture.
`state.maximumStrain` is a diagnostic strain magnitude, not accumulated damage.
Current node geometry still supplies contact, rendering and `raycast`; no new
M44 cast, CharacterMotor blocking or M42 soft-event contract is implied.

Without finite attachment strengths, new saves use deformables participant 3 and versioned retained streaming state to
preserve the original material basis independently of a moved owner/support frame.
Older records still decode, but cannot promise historical owner-rotation
continuation when that reference was absent. These are internal persistence
versions; the JavaScript handle API and cooked asset format are unchanged.
Within a still-connected cohesive component, self-contact is suppressed;
separated components use normal deformable surface contact. Extreme intact folds
remain outside the supported envelope.

## M83 material/body connection strength

`attach` accepts optional `forceLimit` (N) and `torqueLimit` (N m), each zero or
omitted for unlimited strength. Finite limits require an ordinary **dynamic body**
target. World, bone, static and kinematic prescribed targets have no measured
finite-mass reaction in this solver and reject finite strength. Rigid fracture
families retain their M79 section-bond failure path; these limits apply to soft
material/body patches, including cloth and tetrahedral solids.

The actual signed impulses applied to the body are summed within each substep.
Force is the resultant divided by substep duration; moment uses the attachment
patch centroid as its origin. Exceeding either authored limit releases that patch
at the successfully completed fixed-step boundary. It does not delete the body,
reset its velocity, teleport a wheel, or apply an additional break impulse.
Three or more non-collinear attachment nodes can constrain a physical carrier's
orientation as well as position; deformation therefore changes its actual pose.
Collision between carrier and material follows ordinary authored layer/mask rules.

`state.attachments` is an array of `{group, enabled, released, active, forceLimit,
torqueLimit, force, moment, peakForce, peakTorque}`. `force`/`moment` are detached
world/simulation-space vector snapshots from the last substep. Peaks are maximum
**signed resultant** demand in the last evaluated fixed step, not sums of individual
node magnitudes or accumulated gameplay damage. Failed solves can retain partial
evaluated-substep diagnostics while withholding pending release. `active` means
both component and attachment enabled and not released; it does not independently
certify the target's readiness. Reactions are
reconstructed after load and are zero until reevaluated. Released state, material
geometry, velocity and plastic rest persist normally. Explicit `reset`/`attach`
are project-directed reconstruction operations, not automatic healing.

Saves containing finite strengths use deformables participant 4; retained region
state uses `Judas.DeformableState.2`. Default unlimited content retains participant
3 / retained-state 1 and unchanged authored attachment rows. Older formats decode
with unlimited historical strength; no absent strength history is invented.

A [Powertrain connection prerequisite](powertrains.md#physical-connection-prerequisite)
can require this patch and an intact M79 material path. The next mechanical fixed
interval observes committed release/fracture. See [M83 model and bounds](../M83_MECHANICAL_DAMAGE.md).
