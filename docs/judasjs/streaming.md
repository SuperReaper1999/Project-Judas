# Additive regions — `scenes`

[Ownership/readiness walkthrough and editable example](../DEVELOPER_WORKFLOWS.md)

region describes one played world, physics world and script VM. A region is an
ownership group. `scenes.current`, `scenes.load()` and `scenes.reload()` retain
scene/session whole-world semantics. Additive operations do not change the root identity,
session store, selected locale or persistent root entities.

## Requests and readiness

```js
import {scenes} from 'judas';
const demand = scenes.requestRegion('gallery-0', {preload: true});
// In a later update, without waiting:
const status = scenes.regionStatus(demand);
if (status?.state === 'prepared') scenes.activateRegion(demand);
// Release only this consumer's demand. Other consumers remain valid.
scenes.releaseRegion(demand);
```

| Member | Result / contract |
|---|---|
| `requestRegion(name, {preload?})` | Opaque string token. Throws for unknown region, absent manifest or request limit. Same region coalesces; different tokens retain independent demand. |
| `regionStatus(token)` | Detached `RegionStatus` or `null` for a released/old-world token. Never waits. |
| `activateRegion(token)` | Converts this preload demand into activation demand. Boolean; false for stale token. |
| `releaseRegion(token)` | Removes only this demand. Boolean; false for stale token. Cancellation is cooperative and cleanup occurs at an outer boundary. |
| `unloadRegion(name)` | Requests release of unneeded content; false if unknown or still demanded. Safety pins can defer actual unloading. |
| `regions` | Detached statuses for declared regions in stable ID order. |
| `streamingStats` | Pending/live/retained estimated bytes, active/pending counts, resource resident/cache budget bytes, latest integration and largest observed unit milliseconds. |
| `owner(entity)` | Region identity or `root`; throws for stale entity. Ownership is independent of location. |
| `resolveRegionEntity(region, localId)` | Fresh safe `Entity` or `null` while absent/staged/destroyed. `root` accepts root entity identity. Cache only while valid. |
| `pinRegion(region, reason, pin=true)` | Bounded named manual pin; false for unknown region/invalid reason. Caller releases explicitly. |
| `adopt(entity, region='root')` | Transfers a complete hierarchy without changing its identity, velocity or JS instance. Throws for invalid destination, incomplete joint assembly or group-owned gravity/articulation state. |
| `setInterest(name, position, {load, retain, priority?})` | Adds/updates one of at most 32 sources. Position/distances are metres in the fixed simulation frame; `retain >= load`; priority is an integer in [-100000, 100000]. Returns true or throws. |
| `removeInterest(name)` | Removes a spatial source. Does not cancel another source/request. |

Requests made by a script instance are automatically released after its entity
or its owning script slot becomes invalid/disabled. Spatial interests and named pins are explicit session policy;
remove them in `destroy()`. Tokens are not valid in a replacement world. No
Promise or asynchronous language runtime is introduced.

`RegionStatus` exposes `id`, `state`, `error`, `pins`, `entities`, `installed`,
`bytes`, `retained`, `demands`, `preparationMs`, `integrationMs`, `largestUnitMs`,
`loadMs` and `visualReady`. State is `unloaded`, `preparing`, `prepared`, `installing`, `active`,
`unloading`, `cancelled`, `failed` or `blocked`. Counts measure completed units,
not an invented percentage. Active means required collision/baked data has been
installed; optional visual resources may still show normal loading placeholders.
`visualReady` is true once this region’s held resource references are Ready.

## Boundary and lifetime

Workers read/parse/validate immutable CPU data through JobSystem. They never
access the played VM or native simulation. At the outer boundary, bounded work
units register disabled private bodies through RuntimeWorld's normal component
constructor. Complete publication precedes script construction/start. Navigation
bake validation uses the original source geometry before remapping/placement;
native tile caches stage privately and publish with the group.

Pause permits preparation but prevents installation/publication/unload. Script
scheduling keeps the existing pause contract. Destroy callbacks run while ordinary
services/handles are valid, before the departing group becomes private. Its status already reads `unloading`; adoption from a departing group is rejected. Callbacks
may enqueue work but arbitrary callback side effects cannot be rolled back.
Validation failure precedes that no-return point and rolls staged registration back.

## Suspension is not destruction

Snapshot regions retain ordinary entity definitions, physical poses/velocities,
tombstones and the existing bounded JSON `script.state`. Script authors keep
suspendable state in `state`, and reacquire local handles in `start`; arbitrary
closures/native handles are not snapshots. Module caches remain in the one VM.
Do not put a region's unique state only in an imported module singleton.

Enabled Grid Water owners must remain in the root scene; owner suspension is unsupported.
Ordinary animated owners can suspend their playback, mixer/layers and resolved pose
through the shared bounded animation snapshot. Active ragdolls, captured-pose
returns and pending/non-animation physical authority still pin their region with
a reason. See [pose lifetime](animation-ragdolls.md) and [modern saves](saves.md).
Adopt compatible travelling actors or carried props into the persistent root
before releasing their source region; group-owned articulation cannot be
adopted separately.
Incomplete physical assemblies are rejected. Retention pressure pins content
rather than silently respawning or discarding it.

[Modern slots](saves.md) preserve supported active/suspended state, tombstones and travellers. Ordinary reload still resets world-owned region state; Load reconstructs the selected slot.

## Coordinates and authoring

A project's `world-manifest` is a stable `.judasworld` asset ID. Manifest version 1
uses unit-scale placements: an absolute **double** source-origin destination and
quaternion `x y z w`. Source object positions remain local to that origin. The
session origin is subtracted in double precision before converting to float;
planetary-terrain adds opt-in checked live-origin changes for a supported component subset; see [planetary terrain and origins](planetary-terrain.md). Bounds are oriented 3D half extents for interest.

See [the authoring/component matrix](../WORLD_STREAMING.md) and the executable
[streaming cookbook](examples/streaming.js). Related APIs:
[scene/session](scenes-state.md), [navigation](navigation.md),
[character motors](character.md), [physics](physics.md).

## Deformable residency

Independent deformables retain node/velocity/plastic/material/attachment state once
before sliced teardown, then restore privately before publication with fresh owner
IDs and handle epochs. An active attachment from another region pins its target
region; releasing it permits suspension. Manifest reference fields use
`deformable:<group>` with normal qualified region/local IDs. The selected gravity
region is retained when live simulated node positions depend on it. Existing
articulation/water-owner residency restrictions above remain; a pinned region is not
reported as unloaded. See [Deformable](deformables.md) and [save slots](saves.md).

## visual and script dependencies

## Prescribed bodies

Suspending a kinematic body retains its current authority, physical state and
remaining target/persistent velocity intent. Revisit restores that intent privately
before publication; unload time does not advance it. Fresh bodies have fresh native
handles. [Kinematic command semantics](entities.md#kinematic-motion) apply equally
to a root body and a region-owned body. This does not add physics to preparation jobs.

## absolute terrain and live origins

See [planetary terrain and local origins](planetary-terrain.md) for the new coordinate APIs, explicit collision interest, capture-origin retention and supported-component limits.

## mutable regional content
