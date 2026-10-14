# Wheel — suspension, rolling and tyre contact

[Index](../JUDASJS.md) · [Physics](physics.md) · [Entities/prefabs](entities.md) · [M81 authoring and limitations](../M81_WHEELS.md)

An authored Wheel component references an ordinary dynamic rigid-body chassis.
Each wheel has its own suspension, rotational inertia, torque/brake commands and
contact state. It does not require a vehicle controller, wheel count, gearbox or
engine. Project JavaScript supplies the control policy.

An optional [Powertrain](powertrains.md) can supply mechanical shaft impulses and
receive real axle load. Direct wheel commands remain supported independently.

## `entity.wheel`

Returns a `Wheel` handle when the entity owns a Wheel component, otherwise `null`.
Retaining the handle does not retain the native component. `valid` becomes false
after its owner is destroyed or unloaded; other accesses throw `ReferenceError`
when the owner/component is unavailable. A missing or disabled chassis leaves a
valid wheel component whose state reports `available: false`.

`available` describes the chassis; it does not replace the independent wheel
`enabled` gate. A disabled wheel applies no suspension or tyre impulses.

| Member | Meaning |
|---|---|
| `valid` | Read-only component lifetime check. |
| `enabled` | Read/write wheel simulation gate. |
| `settings` | Detached snapshot of authored/current physical parameters and chassis reference. |
| `state` | Detached snapshot of the last completed wheel evaluation. |
| `set(commands)` | Replace supplied torque/brake/steering fields; omitted fields retain their values. |
| `configure(settings)` | Change the supported generic physical parameters atomically. |

`enabled`, `set` and `configure` mutations belong in `fixedUpdate`. Invalid
options or non-finite/out-of-range values throw `TypeError`; an invalid request
does not partly change the wheel. Reads may be used in other callbacks.

## `Wheel.set(commands)`

Accepts optional `driveTorque` (signed N m), `brakeTorque` (nonnegative N m) and
`steeringAngle` (radians). Commands persist until replaced, including across
updates with no new command. Explicitly send zero to release a drive or brake.
There is no built-in forward, reverse, handbrake or steering-input policy.

```js
// Attach to a wheel with an authored chassis reference.
import {input} from 'judas';
export default class {
  constructor({entity}) { this.entity = entity; }
  fixedUpdate() {
    this.entity.wheel?.set({
      driveTorque: 140 * input.axis('drive'),
      brakeTorque: input.held('brake') ? 240 : 0,
      steeringAngle: .4 * input.axis('steer')
    });
  }
}
```

The logical action/axis names above are project content. Wheels do not poll input
or choose a camera. See the [complete copyable example](examples/wheel.js).

## Snapshots

`settings` contains `enabled`, `chassisId`/`chassis`, `mount`, `orientation`,
`radius`, `minLength`, `restLength`, `maxLength`, `springStiffness`, `damping`,
`spinInertia`, `friction`, `rollingResistance`, `collisionLayer`, `collisionMask`,
`requiredTags` and `excludedTags`. The latter arrays use project category names.
`chassis` is an ordinary Entity wrapper; check its `valid` property before using
a reference after destruction. Geometry/reference/filter changes are authored;
`configure` accepts only friction, spring stiffness, damping and rolling resistance.

`state` contains `available`, `contact`, `initialOverlap`, `contactEntityId`/
`contactEntity`, `point`, `normal`, `groundVelocity`, `center`, `rotation`,
`length`, `normalForce`, `angularVelocity`, `angle`, `longitudinalSlip`,
`lateralSlip`, `longitudinalImpulse` and `lateralImpulse`. Contact witness data
is meaningful when `contact` is true; a miss gives `contactEntity: null`.
Slip values describe the relative tyre/ground motion before the last friction
impulse, while impulses describe the applied response. `angularVelocity` is
absolute axle spin; `angle` is spin relative to chassis, wrapped into a finite
2π interval for presentation.

`configure` returns true after validation/publication; unavailable or invalid
chassis/configuration throws `TypeError`. Omitted fields stay unchanged.

## Geometry, coordinates and units

Suspension mount and orientation are expressed in chassis-local coordinates.
The component's local suspension axis and rolling/axle axes are authoring
conventions, not a world-up assumption. Chassis orientation carries them into
the current world; contact normal and ground-point velocity come from Judas's
authoritative collision geometry. Gravity remains the normal selected Judas
field acting on the chassis.

Positive wheel spin is rotation about mount-local **-X**, producing rolling
forward **-Z** on a level contact. Steering is rotation about mount-local +Y.

State points/vectors and wheel centre/orientation are world-space values.
Lengths are metres, angular velocity rad/s, wheel angle radians, spring
stiffness N/m and damping N s/m. Read `entity.presentedTransform` in
`presentationUpdate` for an interpolated visual wheel pose; use authoritative
state for physical/gameplay decisions.

The contact proxy is a sphere swept along the suspension travel, separate from
the visual wheel mesh. Normal impulses support/compress the chassis, and bounded
longitudinal/lateral tyre impulses affect its real velocity and wheel spin.
Dynamic ground receives the opposite impulse; moving/kinematic ground supplies
its actual point velocity. Collider classification uses the existing collision
layers/masks and tags, with the chassis ignored and sensors excluded.

## Current modelling limits

This is a query-contact wheel with one independent rotational degree of freedom,
not a separate rigid wheel body. Author the chassis mass to include unsprung
translational mass. The sphere proxy has no tyre width, tread, deformable tyre,
mesh-rim collision or full wheel-body gyroscopic model. See
[M81 limitations](../M81_WHEELS.md#limitations) before using very steep faces,
very thin/high-speed terrain or full-contact bicycle balancing.

Save/load retains independent commands and rotational state. Scene replacement,
component destruction and region lifetime use normal world ownership. Prefab
chassis references are remapped through the existing entity reference system;
scripts must not keep a pre-reload handle as though it were the new instance.

The strict save path requires a surviving, non-transient chassis reference.
Destroy the wheel owners with their required chassis (normally one authored
prefab hierarchy); retaining dangling wheel references causes explicit save
rejection instead of silently dropping physical state.

## Rigid material section mounts (M83)

An optional authored `wheel.part` names a stable rigid-fracture part key.
`wheel.chassis` then identifies the partition owner. Its mount/frame follow that
part's ordinary physical body, including plastic displacement and detachment.
`wheel.settings.part` returns this string (empty for an ordinary chassis).
Missing/unpublished/removed parts make the wheel unavailable; no body-slot alias
is substituted. This is an authored source selection, not a new gameplay controller.
See [physical section authoring](../M83_COARSE_CRASH.md).
