# Entities, transforms, tags and prefabs

[Index](../JUDASJS.md) · [Physics](physics.md) · [Safe lifetime](scenes-state.md)

## Entity

`import {entity, world, Entity} from 'judas'`. `entity(id)` and `world.entity(id)`
construct wrappers, **not verified lookups**. Falsy IDs and string `"0"` give null;
any other supplied ID produces a wrapper. Prefer decimal strings: runtime IDs
can exceed safe JS integers. `new Entity(id)` also exists but does not test validity.
`id` is a plain writable JS field; treat it as opaque/immutable by convention.

| Member | Arguments / result / behaviour |
|---|---|
| `id`, `valid` | String identity; validity resolves the current world's definition and returns false after destruction. |
| `presentedTransform` | Readonly detached world transform: render interpolation during `presentationUpdate`, authoritative current pose in other callbacks. Same validity/coordinate rules as `transform`. |
| `transform` | Read authoritative detached `{position,rotation,scale}`; assign a partial object to update selected fields. World transforms in fixed-origin local simulation coordinates. |
| `parent`, `children` | Safe wrapper/null; immediate authored/runtime children, ascending entity ID. |
| `destroy()` | Destroy hierarchy, true or throws on failure. Stale owner throws, not an idempotent no-op. |
| `setColliderEnabled(bool)` | Enable/disable existing body; boolean success, false when absent. |
| `hasTag(name)`, `addTag(name)`, `removeTag(name)` | Registered project tag; boolean result, unknown name throws. |
| `classification` | Detached `{renderLayer,collisionLayer,collisionMask}`. Layer IDs are numeric, 64-bit mask is a decimal string. No JS classification setters exist. |
| `scriptState(slot)` | Detached bounded JSON copy of another slot's state; null when absent/faulted. Invalid slot/state throws. |
| `velocity`, `angularVelocity` | Read actual dynamic/kinematic world COM m/s and world rad/s; writes require dynamic authority, missing/static body throws. Character velocity is instead on `.character`. |
| `applyForce(v)`, `applyImpulse(v)`, `applyTorque(v)` | N, N·s, N·m; world-space, existing dynamic body required. Return undefined. `applyImpulseAtPoint(impulse, point)` applies world-space N·s at a world-space point in metres through the existing rigid-body operation (M52). It produces both linear and angular motion; stale/missing/static body throws, and invalid vectors throw before mutation. |
| `audio`, `animation`, `ragdoll`, `character`, `camera` | Component facades/snapshots or null when component absent; see subsystem pages. Stale entity usually throws; `.character` specifically returns null. |
| `playAudio/stopAudio/pauseAudio/resumeAudio`, `setAudioEnabled`, `burst`, `setParticles`, `setCameraEnabled` | See [effects/cameras](effects-camera.md). |

Use `transform` for physics intent, shooting and game rules. Use `presentedTransform`
in [presentationUpdate](lifecycle.md#callbacks-and-order) for cameras and body-free
cosmetic followers. It reuses motor/body interpolation, including orientation and
authored visual-child hierarchy. It does not advance simulation or interpolate
arbitrary script teleports. Never assign a presented pose back to a physical body.

Returned transforms/vectors are snapshots. `e.transform.position.x += 1` only
edits the temporary object. Assign it back, e.g. `const t=e.transform;
t.position.x+=1; e.transform=t`. Quaternion components are `{x,y,z,w}`; finite
nonzero rotations normalize on write. Supplied position/scale must have all xyz
components; scale is positive. Pose writes teleport, not collision-aware movement.
Visual scale does not resize authored collider dimensions. Parent-local authoring
is resolved before these world-space reads; scripts do not set a parent through this API.
The [M70 batch-configuration tuple format](animation-ragdolls.md#configuration-tuples-versus-live-vector-objects)
is an explicit exception: its `PoseVector`/`PoseQuaternion` arrays are not a replacement
for these ordinary object `Vec3`/`Quat` transform fields.
Disabling a collider does not invalidate its entity/body identity: these dynamic-body
operations remain callable while disabled, although physics participation stops.

## world

| Member | Arguments / result |
|---|---|
| `entity(id)` | Alias above. |
| `queryTags(required=[], excluded=[])` | All required, no excluded project tags; sorted live Entity wrappers. Unknown tags throw. |
| `spawnPrefab(assetID, transform)` | Partial placement defaults to identity; normal hierarchy/components, independent runtime IDs. Returns root Entity or throws; no half-valid success. |
| `overlap(min,max,filter={})`, `sweepCapsule(...)` | Read-only existing queries; [physics reference](physics.md). |
| `viewRay`, `setView`, `clearView`, `fluidSample` | [Camera/liquid reference](effects-camera.md). |

Use stable asset IDs, not filenames, for prefab/resource APIs. Runtime prefab
support is the existing ordinary component subset, not every imaginable engine
component. Nested prefab variants are not introduced here. Source data is not
modified by runtime edits. See [prefab architecture](../M36.md).

Tags are semantic labels (many per entity); collision layers/masks are physical
policy; render layers/camera masks are independent presentation policy. Registries
and default relations are project authoring, not JS-created runtime namespaces.
JS currently reads classification and changes tags; it cannot rename registries,
set render masks or replace collider layers/masks on ordinary bodies. Character
query configuration is an explicitly exposed exception, not a global setter API.
An entity without a rigid body reports collision layer 0/mask "0"; this does not
create a collider or describe its CharacterMotor query settings.

## M51 rigid-body properties

`entity.mass` is a readonly mass snapshot in kg. `entity.inertiaWorld` is a
readonly world-space inertia tensor snapshot in kg m², with columns `x`, `y`, `z`
(each a Vec3). Both require a live active dynamic body, otherwise throw TypeError
(after normal entity validity checks). Multiplying this matrix by an angular
acceleration gives a world torque; it does not add a controller or holding policy.

## Navigation components

`Entity.navigation`, `navigationObstacle`, `navigationLink` and `setNavigationEnabled` are described in [Navigation](navigation.md). They remain distinct from character movement and physical collision.

## `Entity.liquid` (M54)

Nullable generation-safe [LiquidVolume](liquid.md) owner after resource loading and fixed-step registration. It exposes conserved quantity/capacity/material snapshots and paired transfers. It is separate from the legacy PBF `world.fluidSample` API.

## `Entity.deformable` — M62

Returns a [Deformable](deformables.md) handle for an authored component after its
normal asynchronous resource publishes. Absent/loading returns null; failed or
stale source bakes throw a diagnostic TypeError. Mutable nodes remain native engine
state. Force/impulse/attachment/material/reset mutations belong in fixedUpdate.
Deformable-local picking queries the current surface, separate from rigid M44 casts.

## Entity.collider

M64 read-only detached snapshot, or null without a living physics body. Stale
Entity reads throw `ReferenceError`. Fields: `type`, `key`, `position`,
`rotation`, `halfExtents` (box, otherwise null), `radius` (sphere or capsule,
otherwise null), `halfHeight` (capsule cylinder half-height, otherwise null), `asset` (cooked asset ID or null), `vertexCount`, `triangleCount`,
`twoSided`, `centerOfMassOffset`, `enabled`, `sensor`, and `children`. Child
metadata uses the same geometry fields plus stable authored key and local pose;
maximum 64 children, no full mesh/native pointers. Root local pose is identity;
COM offset is in authored pivot-local metres, distinct from a child position.

```js
const collider = rail.collider;
for (const child of collider?.children ?? []) {
  if (child.type === 'box') {
    const length = 2 * child.halfExtents.x; // No duplicate game-side rail dimensions.
  }
}
```

See [queries](physics.md#physicsclosestpoint) and [collision assets](../M64_COLLISION.md).

## M65 references and physical/presentation consumers

`setSocket(target,joint,offset={})` assigns a visual attachment and `clearSocket()`
removes it; null is not a valid `setSocket` target. See
[resolved pose/socket semantics](animation-ragdolls.md#resolved-joint-reads-visual-sockets-and-limb-ik-m65).
`physicalMaterial`, `setPhysicalMaterial` and read-only `sleeping` use
[normal physical state](physics.md#gravity-physical-materials-and-runtime-joint-configuration-m65).

Inspector schemas now accept `{type:"entity",default:null}`. Authored properties
store null or `{entity:"decimal-stable-id"}`; scripts receive null or normal safe
Entity wrappers. Existing primitives retain their meanings. References remap through
prefab/duplication/region/save paths; a missing target is an invalid safe wrapper,
not a guessed entity. Never store raw body handles in authoring data.

## `Entity.modelParts` and `Entity.setPartVisible(identity, visible)`

`modelParts` returns a detached array of `{identity,materialSlot,triangles,visible}`
for a ready rendered model, otherwise null. Identity is the cooked stable part key;
`materialSlot` selects the existing per-instance `entity.material(slot)` facade.
`setPartVisible` changes this runtime instance only and returns false for a missing
part/unready mesh. Stale entity handles throw through normal validity checks.
Hidden parts are omitted from ordinary camera, secondary-camera and shadow draws;
this does not disable physics or remove skeleton joints. Editor assignments serialize
as render hidden-part keys. Stop restores authored visibility.

## world.spawnPrefab construction options (M67)

`world.spawnPrefab(asset, transform={}, options={})` keeps ordinary spawn behaviour when options are absent. Options accept only `velocity`, `angularVelocity` and `scripts`. Initial velocity is world-space m/s and angular velocity is world-space rad/s; both require a dynamic or kinematic root body. Kinematic initialization starts persistent prescribed velocity control. The whole hierarchy, resource/shape references and initialization are validated before any hierarchy member is created.

`scripts` contains at most 256 unique `{source?,slot,properties?,state?}` records. `source` is a decimal source-prefab entity ID (omitted/0 selects the root); `slot` is its authored script slot ID. `properties` replaces that slot's authored property object and is checked against its existing exported schema. Use normal typed entity-reference values (`{entity:"ID"}`); the engine accepts only live published external entities or members of the new hierarchy. Construction state is bounded plain JSON (64 KiB per slot), passed as `context.initialState` to the constructor. There is no module-global pending map. Unknown/disabled/duplicate slots, stale refs and malformed data throw before publication.

Construction data and body motion are applied before publication/first integration. The constructor sees final properties and initialState; `start` follows normal resource readiness/synchronization, before the instance's first script fixedUpdate. Physical integration uses the ordinary fixed-step boundary; spawning inside a fixed callback can integrate the body before its next script synchronization, so do not assume its velocity remains numerically equal to the initial velocity in a later callback. Instances are independent. Capture values you need into your script's existing serialized state. On M61 restore, `context.restored` is true, `initialState` is null, existing saved properties/motion/script state are restored, and fresh initialization is not reapplied or start called twice. There is no arbitrary async JS constructor.

## Kinematic motion (M71)

`Entity.motionType` reads `"static"`, `"dynamic"`, `"kinematic"`, or null without a
live rigid body. Static authority holds placement; dynamic authority integrates
forces and contact impulses; kinematic authority advances an externally commanded
trajectory and gives dynamic neighbours ordinary contact response. An externally
driven body can supply work to the world.

| Member | Contract |
|---|---|
| `setMotionType(type, {preserveVelocity:false})` | Safe authority change on a live ordinary actor. Uses authored `body.mass` on entry to dynamic authority; preserves the body handle. A change clears velocity/old commands by default; repeating the current type preserves state. Explicit preservation transfers actual COM/angular velocity to dynamic state or persistent kinematic control. Static entry stops. |
| `moveKinematic({position,rotation}, seconds=0)` | Complete world authored-pivot target using normal `Vec3`/`Quat` objects. Zero consumes the next physics interval; a positive duration up to 60 seconds spans fixed intervals. The current pose remains unchanged until physics advances it. |
| `setKinematicVelocity(linear, angular={x:0,y:0,z:0})` | Persistent world COM velocity in m/s and world angular velocity in rad/s. Both vectors are a single atomic replacement command. |
| `stopKinematic()` | Replaces the command with stopped control. Actual movement stops at the next physics publication boundary. |
| `kinematicMotion` | Detached durable intent `{control,target,linearVelocity,angularVelocity,remainingSeconds,targetNextStep}`, or null outside kinematic authority. `control` is `"stopped"`, `"target"` or `"velocity"`. Velocity fields describe the command; use `Entity.velocity`/`angularVelocity` for the actual last resolved interval. |
| `pointVelocity(worldPoint)` | Actual world velocity at a point: COM linear velocity plus angular velocity crossed with the offset from COM. Static, dynamic and kinematic bodies use the same engine query. |

A target interpolates COM linearly between the start and target COM positions,
including the collider's authored pivot-to-COM offset. Rotation follows the
shortest quaternion arc with constant world angular velocity. Therefore an offset
pivot may curve while the COM follows its line; both endpoint pivot poses are
exact. Sign-equivalent quaternions mean the same orientation. An unchanged or
unrepresentable angular update retains its represented rotation. Targets cannot
express whole extra turns: use angular velocity for continuous spins.

Each complete valid write replaces the previous target, velocity or stop command;
the last valid write before publication wins. Invalid data leaves the previous
command intact and throws. Position and rotation must be complete finite objects,
rotation must be nonzero, and physics rejects unusable coordinates or unsupported
shapes. A completed target holds on the following interval. Without a replacement,
velocity continues; stopped control and completed targets stay still. Commands
cannot be issued during `presentationUpdate`; issue repeatable intent from
`fixedUpdate`, using its fixed interval rather than render delta time.

| Scheduler phase | Owner and readable sample |
|---|---|
| JS `fixedUpdate` | Script publishes bounded intent. Transform/actual velocity reads still show the previous resolved physics interval; command readback shows the latest queued request. |
| `PhysicsWorld::Step` starts | Physics accepts the latest command, constructs prescribed motion and uses the existing motion ledger, broadphase and continuous contact processing. |
| Physics contacts and ordinary motor resolution | Dynamic bodies receive impulses; prescribed bodies retain commanded trajectories. Normal motors consume the resolved support interval through existing support history. M70 motors running before physics retain their established preceding-interval support contract. |
| Authoritative state and events | Physics owns actual pose/COM/angular velocity; owner identities and existing filtered contact callbacks remain unchanged. |
| Presentation | Existing moving-body interpolation reads previous/current fixed poses. It creates no physical motion or collision history. |

Targets are prescribed trajectories, so static/kinematic obstacles do not
unconditionally stop the mover. Dynamic bodies can be trapped by a commanded
trajectory. Use normal contacts, sensors and casts to decide gameplay obstruction
behaviour. Supported moving shapes are box, sphere, convex hull and supported
convex compound children. Concave triangle meshes and terrain remain static.
Masks, sensors, enable state and tags retain their ordinary meanings.

Authority changes reject managed/reduced bodies and actors whose other components
own motion or fixed geometry: character motors, animation/ragdolls, historical
vehicles/doors/switches, gravity/celestial sources, atmosphere/particle-fluid,
baked liquid/nav surfaces, deformables and active joint participants. This keeps
unsupported ownership changes explicit. A normal prop can change among all three
modes with its existing body identity. Native runtime consumers use
`RuntimeWorld::SetRuntimeMotionType` for the same definition, presentation and
lifetime bookkeeping; raw `PhysicsWorld::SetMotionType` is the low-level owned-body
operation and does not update scene ownership. Transform assignment remains explicit
placement/teleport; it clears kinematic intent rather than replaying a target from
the new position. Physical shape dimensions still come from collider authoring.

Author `body kinematic box` through ordinary named scenes, prefabs or the body's
Motion inspector. `body.initial-velocity` is world COM m/s and optional
`body.initial-angular-velocity` is world rad/s; nonzero values start persistent
velocity control. The inspector uses existing undo. Instances own independent
commands. Modern save slots retain current motion type, pose/physical velocities
and durable command state; region suspension uses the same command participant. Private staged actors retain
validated initial intent until normal publication; retained snapshot commands
replace that initial intent.
The separate legacy F6 `.judasstate` pose-delta save rejects kinematic intent or
changed authority with guidance to use modern save slots.
Restoration reinstates remaining intent after current physical state and rebuilds
disposable history without replaying the last movement. Removal, region unload
and Stop retire normal handles and pending commands. Legacy default authoring and
fingerprints omit the optional angular extension.

```js
// An ordinary authored body with kinematic authority; no native route policy.
fixedUpdate(dt) {
  const pose = this.entity.transform;
  this.entity.moveKinematic({
    position: {x: pose.position.x + 2 * dt, y: pose.position.y, z: pose.position.z},
    rotation: pose.rotation
  });
  // These reads still describe the preceding resolved interval.
  const command = this.entity.kinematicMotion;
}
// Alternatives replace the target atomically:
// mover.setKinematicVelocity({x:2,y:0,z:0}, {x:0,y:0.5,z:0});
// mover.stopKinematic();
// mover.setMotionType('dynamic', {preserveVelocity:true});
```

The ordinary [kinematic lab](../../projects/kinematic_lab/) executes these public
VM calls with dynamic crates and motor supports. Its retained evidence records
measured bounds and validation separately from this API contract.
