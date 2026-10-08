# Physics queries, dynamic motion, joints and touch events

[Index](../JUDASJS.md) · [Entities](entities.md) · [Character](character.md) · [Ragdolls](animation-ragdolls.md)

## physics

`import {physics} from 'judas'`. Queries are explicit read-only snapshot questions,
not contact events. They use the existing AABB tree and authoritative geometry.
Directions normalize internally; maximum/distance are metres, fraction is relative
to maximum distance. No all-hits binding exists.

| Method | Signature / result |
|---|---|
| `raycast` | `(origin, direction, maximum, filter={}) → CastHit\|null` |
| `raycastMany` | `([{origin,direction,maximum}, …], filter={}) → (CastHit\|null)[]`; up to 256 |
| `sphereCast` | `(origin, radius, direction, maximum, filter={}) → CastHit\|null` |
| `sphereCastMany` | `([{origin,radius,direction,maximum}, …], filter={}) → (CastHit\|null)[]`; up to 256 |
| `capsuleCast` | `({position,rotation?}, radius, halfHeight, direction, maximum, filter={}) → CastHit\|null` |
| `capsuleCastMany` | `([{pose:{position,rotation?},radius,halfHeight,direction,maximum}, …], filter={}) → (CastHit\|null)[]`; up to 256 |
| `boxCast` | `({position,rotation?}, halfExtents, direction, maximum, filter={}) → CastHit\|null` |
| `closestPoint` | `(point, maximum, filter={}) → ClosestPointHit\|null`; bounded nearest **surface** search |
| `joint` | `(ownerEntity) → Joint\|null`; owner is the entity authoring the joint component, not automatically a connected body. |

Pose-local Y is the capsule core axis, not universal world up. Capsule full height
is `2*(halfHeight+radius)`. Boxes use positive half extents. Default cast rotation
is identity. Casts translate with fixed orientation, no rotational sweep or target
motion prediction. Invalid input throws TypeError; zero maximum can test initial
overlap. Hits choose nearest distance, then body/primitive identities for ties.

`CastHit`: `{entity,entityId,bodyId,point,normal,distance,fraction,primitiveIndex,
initialOverlap,shape,childKey,feature}`. Target surface point and outward normal; deep initial
overlap has a deterministic feature normal, not unique physical penetration data.
`physicalMaterial` is the actual body-assigned stable asset ID or null; no per-triangle
material is fabricated. `shape` is `capsule`, `sphere`, `box`, `terrain`, `hull` or `triangle-mesh`; compounds return child primitive index and authored stable `childKey`. Single shapes use key 0. `feature` identifies a cooked triangle where meaningful, otherwise null; recooking can change feature keys.
`bodyId` is an opaque generation-bearing number, not a controllable JS Body.
`entity` can be null for unassociated geometry; otherwise it may later become
invalid. Retained hit data does not update with the world.

`QueryFilter`: `includeLayers`, `excludeLayers`, `requiredTags`, `excludedTags`
are project-name arrays; `ignored` is Entity[] (up to 256), `includeSensors` defaults
false. Includes default to all, excludes to none; all required tags must match,
any excluded tag rejects. Unknown names throw. Query masks are independent of
physical body collision masks. Ignored missing bodies are skipped safely.

## Batched nearest casts (M69)

`raycastMany`, `sphereCastMany` and `capsuleCastMany` cross the JS/native bridge
**once per accepted batch**. Native code validates all requests, prepares the
one shared `QueryFilter` once, then performs the existing scalar geometry query
for each request. This saves bridge/filter marshalling work; 100 rays still mean
100 geometric queries. There is no promised machine-independent speedup.

The input must be an array of at most **256** requests. Results correspond
one-for-one in **input order**, retaining null misses and repeated requests;
there is no result sort, truncation or implicit splitting. `[]` returns `[]`.
Oversized requests throw RangeError. Malformed entries throw an indexed TypeError
before any geometry query executes, so an invalid middle entry cannot produce a
partial result. Invalid shared filter names/handles follow scalar query rules.

All scalar limits apply: finite coordinates/distance/dimensions, nonzero finite
direction, nonnegative maximum/radius/halfHeight, and nonzero valid quaternion
when supplied. Directions/rotations normalize as in scalar casts. A zero maximum
can test initial overlap. Capsule `pose` is required; omitted rotation is identity.
Pose-local Y remains the capsule axis, not world up.

Batches are synchronous read-only questions against the same authoritative world
as scalar casts. They do not advance physics, run callbacks between requests or
copy a world snapshot for a worker. Use `fixedUpdate` for gameplay queries that
should coincide with authoritative control; other phases have the same legal
read-only query behaviour as scalar calls. Retained hits are detached observations
with ordinary generation-safe Entity wrappers; they do not extend body lifetime.
Sensors, layers/tags, exclusions, ignored entities and rotated geometry use the
same `QueryFilter` and broadphase/narrowphase. Terrain inherits existing scalar
approximation/coverage. No box, overlap or all-hits batch binding is provided.

```js
const rays = [
  {origin: {x:0,y:2,z:0}, direction: {x:0,y:0,z:-1}, maximum: 12},
  {origin: {x:1,y:2,z:0}, direction: {x:0,y:0,z:-1}, maximum: 12}
];
const hits = physics.raycastMany(rays, {ignored: [this.entity]});
for (let i=0; i<hits.length; ++i) {
  const hit=hits[i]; // Null stays at the request's index.
  if (hit?.entity?.valid) console.log(i, hit.entity.id, hit.distance);
}
```

[The copyable 100-ray fan](examples/ray-fan.js) includes optional scalar-result
comparison. It only returns observations; ledge selection/climbing remain game JS.

`world.overlap(min,max,filter={})` returns entity wrappers from conservative
broadphase bounds in ascending body-slot order, NOT exact overlap penetration
tests or an entity-ID sort. `world.sweepCapsule(from,
displacement,rotation=identity,filter={})` retains the existing player-sized capsule
(radius 0.3, cylinder half-height 0.6); result always has `{hit,distance,normal,entityId}`,
including miss. It sweeps a displacement, not a unit direction/maximum pair. Use
`physics.capsuleCast` for explicit shape dimensions. Neither returns a Character.

Terrain: ray/sphere/capsule sample actual RadialTerrain, using approximate radial
surface distance/bracketing. Thin/grazing features can be missed. Box casts reject
permitted terrain candidates. Render-only meshes remain non-queryable; motors expose their query-only capsule geometry without rigid mass. No concurrent worker-query API. [Geometry limits](../PHYSICS_QUERIES.md).

All vectors/poses use local float simulation coordinates around a fixed double
absolute origin. No JS absolute-coordinate/rebasing API is exposed. No universal
world-up: supply directions/orientations from local state, support and gravity.

## physics.closestPoint

Nearest **surface**, even when the point is inside a closed shape. Maximum radius
is required, finite and nonnegative; missing/filtered/beyond-radius geometry returns
null. Uses ordinary QueryFilter, body broadphase and local cooked BVH. Result:
`{entity,entityId,bodyId,point,normal,distance,contains,primitiveIndex,childKey,feature,shape}`.
`contains` is true/false for closed primitives/hulls, null for an open mesh;
unsigned distance is not an invented mesh interior. `normal` is null when tied
surface features lack a unique normal. Surface coordinates and units match casts.
Terrain inherits approximate radial projection. Stale retained entity wrappers
follow ordinary Entity rules; detached results do not update with simulation.

```js
const nearest = physics.closestPoint(character.transform.position, 2, {
  ignored: [character], includeLayers: ['Default']
});
if (nearest) { marker.transform = {position: nearest.point}; }
```

Static cooked meshes use explicit one/two-sided contact/cast policy; closest-point
inspection sees either side. Hulls are closed convex volumes. Sphere/capsule/fixed
box casts against cooked surfaces use a bounded 256-advance policy per candidate,
1e-5 metre convergence tolerance, throwing on exhaustion instead of a clean miss.
No rotational sweep / general high-speed CCD claim. [Cooking and pair coverage](../M64_COLLISION.md).

## Joint

`Joint.id` is a plain opaque string; `valid` returns false after generation retirement.
Other stale joint calls throw ReferenceError. `state` is detached:
`{active,enabled,coordinate,motorImpulse,type}`. `type`: 0 fixed, 1 hinge,
2 ball/socket, 3 slider. Active is solver participation, distinct from enabled.

| Method | Behaviour |
|---|---|
| `setEnabled(enabled)` | Boolean authored-runtime enable. |
| `setLimits(lower,upper,limits=true)` | Hinge radians [-π,π], slider metres; ordered finite values. |
| `setMotor(speed,maxForce,motor=true)` | Hinge rad/s + N·m, slider m/s + N; finite bounded motor. |
| `setSpring(rest,stiffness,damping,spring=true)` | Free-coordinate equilibrium and implicit spring/damping: slider rest in m, stiffness N/m, damping N·s/m; hinge rest in rad, stiffness N·m/rad, damping N·m·s/rad. |

All setters return true or throw on invalid settings/stale handle. `configure(settings)` validates an atomic partial patch, including type/anchors/frames,
using JointSettings below; participants cannot be changed in place. `destroy()` retires
the component and handle. `physics.createJoint(owner, settings)` creates one normal
owner component (rejects an existing one), returning Joint. Author frames/anchors in
editor/scene: body-local frame X is hinge/slider axis; a world-side anchor/frame is
transformed through the joint owner's authored pose. Ball/socket has no angular
cone limiter. Motors do not imply a gameplay state. Use fixedUpdate for control.
[Joint geometry/solver semantics](../JOINTS.md).

## Collision and trigger callbacks

`onCollisionEnter/Stay/Exit(event)` and `onTriggerEnter/Stay/Exit(event)` receive
`{other,point,normal,relativeVelocity,normalImpulse,physicalMaterial,selfBody,
selfJoint,otherArticulation,otherJoint}` at the fixed boundary.
Normal points from other toward the recipient. Relative velocity is OTHER minus
SELF point velocity (including angular contribution), observed when contact was
recorded, not a fabricated post-solve measurement. `normalImpulse` is a reported
solved normal impulse where available, otherwise null (especially sensors/exits).
Exit retains historical contact observations, not a new geometric query.

One event per body pair/phase, deterministic body-handle ordering, then ordinary
recipient A/B and authored slots followed by opted-in articulation owners.
Sensors share normal geometry/M39 physical filtering but
produce no response; overlaps are discrete endpoint observations, not continuous
trigger trajectories. Disabled/destroyed recipients are rechecked; destroyed `other`
may still be an Entity wrapper with `valid=false`. Check validity before use.
Mapped ragdoll bodies retain per-body events. An owner script receives their external
observations only when it explicitly opts in with
[`ragdoll.receiveContactEvents = true`](animation-ragdolls.md#ragdollreceivecontactevents)
(default false, or authored `ragdoll.receive-contact-events true`). Unscripted bone
entities and ground do not prevent delivery to that subscribed owner script.
`selfBody` is the actual receiving body; `selfJoint` is its mapped stable key or null.
`otherArticulation`/`otherJoint` identify the other mapped owner/key or are null.
These wrappers may become stale: check `valid`. Same-articulation contacts are not
forwarded to its owner; individual body callbacks remain unchanged. Forwarding is
still per body pair/phase, with no aggregate articulation or injury semantics.
Toggling the subscription generates no synthetic enter/exit callbacks.
Motors now participate through massless query capsules and coalesced sweep/support
observations; endpoint sensor overlap is discrete and supplies no solved impulse. JS decides game meaning.

## Gravity, physical materials and runtime joint configuration (M65)

`physics.gravity(position) → Vec3` samples acceleration (m/s²) using the normal
position-based gravity resolver, including overriding zones, uniform and radial
fields. It is a read-only simulation-coordinate query. No field-type branch or
universal up is needed in scripts. `physics.gravity({x:0,y:1,z:0})` does not depend
on having a CharacterMotor.

`entity.physicalMaterial → {asset,friction,restitution}|null` reads effective body
coefficients, with `asset` null for legacy inline values.
`entity.setPhysicalMaterial(asset=null, parameters={}) → true` selects a registered
`.judasphysmat` resource; optional friction/restitution apply explicit instance
overrides. Missing body/stale entity throws ReferenceError; invalid resource or
coefficients throws TypeError. Resource coefficient changes do not rewrite shared
assets. Both identity and overrides persist. Runtime changes wake/invalidate body
contact state. Render materials are independent.

`JointSettings`: type fixed/hinge/ball/slider; bodyA required safe Entity, bodyB
optional Entity/null (world anchor); anchorA/B Vec3, frameA/B quaternion; enabled,
limits, lower, upper, motor, speed, maxForce, spring, rest, stiffness, damping.
Creation accepts owner references, not raw handles. AnchorA is body-local; body-side
B is body-local, world-side B is **owner-local**, composed through owner transform.
Fixed/ball translation uses metres. Existing M45 angular/slider units apply.
Configuration retains participants and invalidates cached rows; create/destroy can
replace them. Use fixedUpdate. Normal scene/prefab/M59/M61 references own lifetime.

Sleeping bodies retain geometry and queries. `entity.sleeping` reports current
physical state (false without a physical body); force/impulse/torque and meaningful
changes wake connected islands. It is not a gameplay pause or visibility policy.

### Passive joint rotational resistance

`Joint.configure({rotationalResistance: coefficient})` and
`physics.createJoint(owner, {…, rotationalResistance: coefficient})` accept a
finite nonnegative coefficient in N·m·s/rad. Default **0** preserves the previous
undamped behaviour. It opposes relative angular velocity implicitly on a ball
joint's three free angular axes, or a hinge's free angular axis. Fixed/slider
joints have no free angular axis. It does not drive a rest orientation, alter sleep
thresholds, or replace hinge motors/springs. The same setting is authored in
ordinary joints and ragdoll mappings; game scripts decide whether to use it.
