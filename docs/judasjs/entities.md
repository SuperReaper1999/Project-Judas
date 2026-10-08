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
| `velocity`, `angularVelocity` | Read/write dynamic-body world-space m/s and rad/s; missing/static body throws. Character velocity is instead on `.character`. |
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

`setSocket(target,joint,offset={})` creates/removes a visual attachment; see
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

`world.spawnPrefab(asset, transform={}, options={})` keeps ordinary spawn behaviour when options are absent. Options accept only `velocity`, `angularVelocity` and `scripts`. Initial velocity is world-space m/s and angular velocity is world-space rad/s; both require a dynamic root body. The whole hierarchy, resource/shape references and initialization are validated before any hierarchy member is created.

`scripts` contains at most 256 unique `{source?,slot,properties?,state?}` records. `source` is a decimal source-prefab entity ID (omitted/0 selects the root); `slot` is its authored script slot ID. `properties` replaces that slot's authored property object and is checked against its existing exported schema. Use normal typed entity-reference values (`{entity:"ID"}`); the engine accepts only live published external entities or members of the new hierarchy. Construction state is bounded plain JSON (64 KiB per slot), passed as `context.initialState` to the constructor. There is no module-global pending map. Unknown/disabled/duplicate slots, stale refs and malformed data throw before publication.

Construction data and body motion are applied before publication/first integration. The constructor sees final properties and initialState; `start` follows normal resource readiness/synchronization, before the instance's first script fixedUpdate. Physical integration uses the ordinary fixed-step boundary; spawning inside a fixed callback can integrate the body before its next script synchronization, so do not assume its velocity remains numerically equal to the initial velocity in a later callback. Instances are independent. Capture values you need into your script's existing serialized state. On M61 restore, `context.restored` is true, `initialState` is null, existing saved properties/motion/script state are restored, and fresh initialization is not reapplied or start called twice. There is no arbitrary async JS constructor.
