# M44 — runtime raycasts and shape casts

Current public JavaScript signatures, examples and lifetime rules: [JudasJS reference](JUDASJS.md). This document retains milestone architecture and evidence context.

Judas answers geometric questions; project JavaScript gives the answers meaning.
These are **read-only queries of resolved colliders**, separate from M42 events,
player locomotion sweeps and the impact solver.

## Current extensions through M69

M69 adds `physics.raycastMany`, `sphereCastMany` and `capsuleCastMany`: at most
256 requests, one shared normal filter and ordered scalar-style hit/null results.
See [current batch contracts](judasjs/physics.md). Existing geometry is unchanged.

M64 extends these existing queries to actual cooked hulls, static triangle surfaces
and oriented compound children. It adds `physics.closestPoint` and read-only
`Entity.collider` metadata. See [current geometry contracts](M64_COLLISION.md) and
[JudasJS physics](judasjs/physics.md) for surface/containment, sidedness, feature keys
and bounded convergence. The M44 sections below retain their milestone context.

## API

C++ `PhysicsWorld` exposes `Raycast`, `SphereCast`, `CapsuleCast`, `BoxCast`.
Each takes an origin/pose, direction, maximum distance, existing
`PhysicsQueryFilter`, and optional `PhysicsCastStats*`. Capsule `halfHeight` is
half the straight core segment along pose-local Y; radius adds the hemispherical
ends. Box dimensions are positive half extents. Rotations are quaternions.
Direction is normalized internally; maximum distance is in local simulation
metres. Cast orientation stays fixed throughout translation.

```js
import {physics, world} from 'judas';
const hit = physics.raycast({x:0,y:2,z:5}, {x:0,y:0,z:-1}, 100, {
  includeLayers:['Default'], excludedTags:['IgnoreQuery'],
  ignored:[world.entity('5')], includeSensors:false
});
if (hit && hit.entity?.valid) console.log(hit.point, hit.normal, hit.distance);

physics.sphereCast(origin, radius, direction, maximum, filter);
physics.capsuleCast({position:origin, rotation:{w:1,x:0,y:0,z:0}},
                    radius, halfHeight, direction, maximum, filter);
physics.boxCast({position:origin, rotation:{w:1,x:0,y:0,z:0}},
                halfExtents, direction, maximum, filter);
```

The filter is shared with M39 queries: `includeLayers`, `excludeLayers`,
`requiredTags` (all required), `excludedTags` (any rejects), `ignored` entity
handles, `includeSensors` (default false). Query policy is independent of a
body's physical collision mask. Unknown category names/invalid numeric inputs
produce a useful JS exception; they do not become an unfiltered query.

Miss: C++ `hit=false` / JS `null`. Hit: generation-aware C++ body / normal M40
safe JS entity wrapper, point on the target surface, outward target normal,
distance, fraction, `initialOverlap`, target `shape`, and `primitiveIndex`
(compound child index). JS `bodyId` is an opaque generation-bearing identifier,
not a second body-control API. A retained hit is a historical observation;
check `entity.valid` before later use. Equal-distance hits choose lowest body
handle, then lowest primitive index. There is no all-hits API in M44.

Zero distance may report initial overlap. Deep initial overlaps have no unique
surface normal; a deterministic nearest feature is returned, not a penetration
resolution or physical impulse. Cast convergence tolerance is 1e-5 m;
box/contact witness calculations retain the existing float/local-coordinate
limits. Nonconvergence after 256 convex iterations reports an error.

## Geometry and ownership

The existing dynamic AABB tree provides candidates. Its conservative swept
AABB is the union of start/end shape bounds. The existing M39 body filter then
rejects candidates before geometry work. Static, dynamic, enabled, compound,
sensor and runtime-prefab colliders use the same body entries and identities.
No query changes poses, velocities, forces, contacts, warm starts or events.
The tree's reusable query scratch is used on the authoritative thread, as with
existing queries; these calls are not a concurrent worker-query interface.

Rays use analytic sphere and oriented-box intersections. Translating convex
casts use separating-plane advancement. Sphere/capsule-to-box distance uses
an exact piecewise-quadratic segment/AABB closest pair; box-to-box reuses
Judas's robust signed-separation/contact manifold. Compound children use
parent-local geometry rather than a competing mesh representation. Render
meshes without colliders are not physics targets. The custom player controller's
separate capsule is not a registered rigid-body query target.

## Terrain and deliberate limits

Ray, sphere and capsule queries sample the **actual `RadialTerrain` surface**.
Terrain supplies an approximate radial signed distance rather than a certified
Euclidean distance bound. Crossings are bracketed with 512 intervals within its
conservative bounding sphere, then bisected; capsules use nine core samples,
matching the existing player surface convention. Very thin terrain features or
grazing crossings can be missed; this is not exact arbitrary-terrain CCD.
Box casts against radial terrain are explicitly unsupported and report an
error if that terrain is among the permitted candidates. Filter terrain out
or use ray/sphere/capsule queries. No fake demo terrain path exists.

These APIs do not cast rotational trajectories, predict target motion, query
render-only geometry or redesign the player's bespoke sweep. They operate in
the existing fixed-origin local simulation coordinates, not absolute doubles.

## Demo

`projects/query_demo/query_demo.judasproj`: red ordinary box, green moving
runtime-prefab sphere, yellow explicitly ignored box, blue excluded layer/tag.
Aim with mouse; **G** toggles ray / radius-0.4 sphere; **P** spawns another prefab.
New P-spawned targets appear three metres along the view; the HUD shows the
spawn count plus entity, distance, point and normal. White marker = hit point;
magenta marker = endpoint of a 0.5 m outward normal. Escape opens authored pause.
Project JS owns all these reactions. `world.viewRay` is the latest completed
active presentation-camera snapshot (null before the first view), hence aiming
has a one-render-frame snapshot delay. It does not control physics or camera
ownership; arbitrary supplied origins/directions remain the primary API.

Human review: aim at red/green; aim into empty space; confirm blue/yellow are
ignored; spawn a green target; compare ray and sphere near edges; confirm the
same behaviour in an exported standalone project. Automated/headless checks
are not human visual acceptance.

## Validation

`python3 scripts/m44_validation.py` performs one clean Release build, existing
production/async suite, focused geometry + real Application/JS checks, editor
Play/Stop, standalone and moved-package smoke. Persistent results and source
fingerprints: `docs/evidence/m44/`. It refuses to overwrite existing evidence.
