# Navigation

Current navigation API (introduced in M53). Navigation proposes traversable routes and steering;
[CharacterMotor](character.md) resolves authoritative collision-aware movement.
Destination choice, chase, link traversal, animation and combat belong to project JS.
Navigation does not read input, gravity or player state, and never writes entity transforms.

## Obtaining navigation

```js
import {navigation} from 'judas';
const point = navigation.sample({x: 2, y: 0, z: 4});
const route = navigation.path({x: 0, y: 0, z: 0}, {x: 8, y: 0, z: 4});
```

An enabled authored `NavigationSurface` loads a registered `.judasnav` asset through
the ordinary asynchronous ResourceManager. Initial queries can return null/failed
while resources load. `navigation.errors` reports missing, stale or invalid bakes.
No Recast bake runs in standalone. Each RuntimeWorld owns its own mutable Detour
mesh/cache; asset layer bytes are immutable and shareable.

## Coordinates, profiles and areas

All query points/results and guidance vectors use Judas **local world-space metres**,
not huge absolute coordinates. M23 provides a fixed double-precision absolute origin;
no live rebasing exists. Each surface object's position/rotation supplies a navigation
frame, converting world geometry to local +Y traversal up and back. This internal
axis does not imply world up or gravity: CharacterMotor independently samples Judas gravity.

Project profiles have stable numeric IDs and names, radius/height/slope/climb.
A query selects one profile, default ID 0. Incompatible profile IDs do not route
through another profile's bake. Project navigation areas (IDs 0..61) are separate
from tags, collision layers and render layers. Deleted IDs are retired; default ID 0
is retained. Up to 64 lifetime profile IDs. Changing settings requires rebaking.

`NavigationFilter` accepts `profile` (name or ID), `includeAreas` and `excludeAreas`
(name arrays), and `costs` (object keyed by area name, finite values >= 1).
An absent include list permits all areas; an empty list permits none. Exclusions win.
Unknown profile/area filter names or invalid values throw TypeError. Unknown cost
object keys are ignored; use names from `navigation.areas`. Costs apply to Detour
polygon routing and explicit link distance; changing cost does not alter physics.

## `navigation.sample(point, range = 2, filter = {})`

Returns `{position, surfaceId, surface, area}` or null. Range is a positive Euclidean
search limit (<=100 m in JS); Detour first searches a local bounding box, then Judas
checks actual distance. Ties are deterministic by surface iteration order. `surface`
is a normal safe entity wrapper, `area` a stable project area ID.

## `navigation.path(start, end, filter = {})`

Returns `{status, corners, distance, areas, revision}`. Status is `complete`, `partial`
or `failed`. Endpoints are snapped within a 2 m search box on compatible surfaces;
first/last corners are resolved positions, which can differ from requested points.
A partial path ends at the reachable boundary; it must not be treated as arrival.
`distance` sums corner segments and link gaps, in metres. `areas` lists traversed
area IDs. `revision` identifies the world cache revision, not a persistent handle.

Each corner has `{position, linkId, link, linkEnd}`. Ordinary corners have linkId
`"0"`, link null and a zero linkEnd. A special corner identifies an explicit
link and its world-space exit. Disconnected surfaces/regions can connect through
these authored endpoints; Detour supplies each on-surface segment. Cross-region
endpoint search is bounded by authored content and uses deterministic order.

## `navigation.raycast(start, end, filter = {})`

A navigation traversal test, **not** a [physics raycast](physics.md). Returns a
NavigationLocation at the traversable endpoint/boundary, or null if no valid start.
Compare the returned position with the requested endpoint to identify blockage.
Detour tests in the surface's local horizontal plane: it does not answer aerial,
vertical clearance or visual line-of-sight questions. Returned height is interpolated
from the query endpoints; sample the result if you need surface height. Use physics
casts for clearance/visibility.

## Registry/status snapshots

- `navigation.areas`: `[{id,name}]`.
- `navigation.profiles`: `[{id,name,radius,height}]`; metres, immutable snapshots.
- `navigation.errors`: `[{entityId,message}]`; diagnostic snapshots, not callbacks.

## `entity.navigation` / `NavigationAgent`

Returns a wrapper if the entity owns an agent, otherwise null. A disabled agent
still has a wrapper so `agent.enabled = true` can re-enable it; state/control calls
on disabled agents throw ReferenceError. All calls reacquire the entity definition;
stale/destroyed entity handles throw ReferenceError. No native pointer is exposed.

- `setDestination(point)`: finite world point; marks route for recomputation, returns bool.
- `clear()`: forget route/destination, zero guidance; returns true.
- `stopped`: read/write bool; suppress guidance without forgetting destination.
- `enabled`: setter-only bool; changes component state, reads yield undefined.
- `configure({profile,includeAreas,excludeAreas,costs,speed,arrival,repathSeconds,avoidance})`:
  partial settings patch. Speed m/s; arrival m; cadence seconds; positive finite values.
- `state`: snapshot with destination, hasDestination, stopped, path, steering,
  remainingDistance, reached, nextCorner, onLink, linkId/link, linkEnd.
- `steering` / `remainingDistance`: conveniences returning snapshot values.
- `completeLink()`: acknowledge script traversal, advance route corner; false unless onLink.

`state.path` contains native linkId fields in its corners; use `world.entity(linkId)`
if needed. Top-level `navigation.path` additionally constructs link wrappers.

Navigation updates before scripts' `fixedUpdate`, reading actual prior motor/body
poses. Motor foot sampling includes the rotated shape offset; other movers use
their observed root point, projected onto nearby navigation space for progress.
Destination/configuration changes request the next update; cache revisions are
observed at the configured repath cadence;
no path search happens for an idle agent. Destination arrival uses resolved route
and tolerance; no movement is enforced. Local avoidance uses Detour's adaptive
velocity sampler with observed nearby agents. It is advisory, not guaranteed crowd
separation, motor-to-motor collision or a second positional simulation.

At a special link, guidance stops and `onLink` becomes true. JS decides whether
and how to cross. Call `completeLink()` only after performing traversal. There is
no automatic jump, ladder behaviour or teleport.

## Obstacle and link control

- `entity.navigationObstacle`: null or `{enabled,cylinder,halfExtents,radius,height}`.
- `entity.navigationLink`: null or `{enabled,bidirectional,start,end,area}`; endpoints world-space.
- `entity.setNavigationEnabled("agent" | "obstacle" | "link", enabled)`: bool;
  false for an absent component. Ordinary entity transform changes move an obstacle.

TileCache rebuilds affected tiles incrementally. Installation/removal can take several
fixed updates; path revision changes after updates finish, then agents repath. Boxes
use conservative surface-frame AABBs; cylinders align to the navigation frame's up.
Use these for doors/crates/modest slow blockers, not every fast-moving rigid body.
Changing a navigation obstacle never adds/removes its separate physical collider.
Links follow authored/explicit transform writes; physics-driven moving link endpoints
are not synchronized automatically in M53. Surface enabling is authored; runtime
setters cover agents, obstacles and links, not moving/enabling surfaces.

## Lifetime and limitations

Scene replacement, reload and Stop discard routes, agents, links and mutable caches.
Snapshots are detached values; entity wrappers remain subject to normal safe-handle
rules. Session JSON can store game intent, not a live navmesh/path pointer.
[M61 slots](saves.md) preserve destination/settings, stop and link-progress state;
restoration rebuilds corridors against ordinary baked resources, not saved Detour pointers.

Recast is locally 2.5D; arbitrary **orientation** is supported, whole-planet continuous
curvature/streaming is not. Use separate locally oriented surfaces and explicit links.
Current budgets: 4096 bake grid tiles, 16384 serialized layers, 256 obstacles per
surface, 512 polygon/corner query buffers, 32 avoidance neighbours. Buffer-limited
paths are partial. M64 bake extraction uses authoritative cooked mesh/hull faces
and independently oriented box/sphere/hull compound children. Boxes use exact
collider faces; spheres/terrain use finite tessellations of real collision surfaces.
Changed physical geometry invalidates the bake fingerprint; render meshes are not
substitutes for collision sources. Modifier boxes
are conservatively projected. No moving navmesh surface, live rebasing or runtime bake.

See [authoring/baking](../NAVIGATION.md), [worked navigation script](examples/navigation.js),
[input](input.md), [motor](character.md), [physics](physics.md), and [animation](animation-ragdolls.md).
