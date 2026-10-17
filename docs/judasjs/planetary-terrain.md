# Planet terrain and local physics origins (M85 / M90)

This optional component describes an absolute binary64 cube-sphere. Ordinary
`Entity.transform`, physics queries, CharacterMotor and camera positions still
use **local simulation metres**. A JS `number` preserves binary64; converting a
planet-scale number through an ordinary float transform does not.

## Coordinates

| Public API | Contract |
|---|---|
| `world.origin` | Copy of the current absolute binary64 origin, metres. |
| `world.toAbsolute(local)` | Add origin in binary64; returns `{x,y,z}`. |
| `world.toLocal(absolute)` | Subtract origin in binary64; returns `{x,y,z}`. Only then pass a nearby result to a float transform/query. |
| `world.requestOrigin(absolute)` | Queue an absolute anchor rounded to 512 m. Returns true or throws TypeError on invalid/unsupported state. It does not move immediately. |
| `world.originState` | `{pending,revision,error}`. Commit occurs before the next authoritative `fixedUpdate`. A subsequently blocked request remains pending and reports the reason. A new accepted request replaces it. |

An origin shift translates existing bodies, query proxies, broadphase, contact
points, world joint anchors, gravity regions, motor support origins and rigid
presentation history together. It does not reset momentum, sleep, orientation,
constraints or time. Support normal remains independent from gravity direction.

**M90 scope:** live shifts require script-owned execution and an 8 km active
physics chart. Spatial actors must be streamed out/retired before moving to a
distant chart. Wheels and scalar powertrain state, navigation frames/corridors,
ragdolls/physical animation, shared and legacy IK targets, sockets, deformables,
audio and particles participate. Body/joint/entity handles retain their identities;
the operation translates positions and preserves directions, velocities, loads,
sleep, commands and local attachment frames. Detour tiles and immutable mesh/rest
assets remain in their own local frames.

**Pre-Alpha extension:** conserved liquid owners and detached parcels translate
within the same 8 km chart. Owner-local capacity geometry, surface waves/history,
quantity, velocity and handles remain intact; derived optical planes rebuild.
An owner whose cavity has not yet published refuses nonzero shifts with a
prepare/retry diagnostic; a no-op origin request still succeeds. This prevents
liquid-only actors from bypassing geometry bounds during asynchronous startup.
Analytic atmosphere translates its frame; combustion retains its thermal scalars
and carrier handles. This does not add regional liquid suspension or modern-slot
atmosphere/combustion persistence. See the [composition matrix](../PREALPHA.md).

**Remaining exclusions:** legacy PBF and emitted legacy fluid reject shifts.
An in-flight M59 residency transaction defers the shift
until it reaches a safe boundary. A live articulation still pins its streamed
source region under the existing suspension rules; translating it does not add
articulation suspension support. This is one shared local physics chart, not
simultaneous distant physics islands or universal whole-engine rebasing.
Scenes that never request a shift retain their existing path.

## `onOriginShift(event)` — explicit script caches (M90)

An optional synchronous script callback runs **after** a nonzero origin change
has fully committed and **before** the next ordinary `fixedUpdate`:

```js
onOriginShift({delta, origin, revision}) {
  // Only a cache this script knows to be local position data.
  if (this.smoothedCamera) {
    this.smoothedCamera.x -= delta.x;
    this.smoothedCamera.y -= delta.y;
    this.smoothedCamera.z -= delta.z;
  }
}
```

`delta = newOrigin - oldOrigin`, in binary64 metres. `origin` is the new absolute
origin; `revision` is the current world's monotonic origin-transition count.
Revisions are runtime observations, not durable identities. All vectors are
object snapshots, not mutable native storage. Subtract delta from **local points**;
never from directions, velocities, local offsets or absolute locations.

Only already-started, live, healthy slots receive the hook. Rejected requests and
zero shifts emit nothing. A callback error faults that slot under the usual
isolation/budget rules; it cannot roll back a completed native chart change.
This notification is not a fixed-update callback: fixed-only movement/impulse
writes remain forbidden. Use it for owned caches, then submit motion in
`fixedUpdate`. See [lifecycle](lifecycle.md).

The engine never guesses which numbers in arbitrary JSON are positions. Store
long-lived locations in absolute space (`world.toAbsolute`), or retain their
capture origin and explicitly reproject when reacquiring them. Suspended scripts
receive no callbacks while absent; on `restore`/resume, rebuild local caches from
absolute data, rather than replaying missed origin events.

Scene reload resets to the authored origin. Full saves preserve the active chart.
M59 retained records preserve capture origins and decode kinematic targets,
deformed nodes and owned material sections in the new chart before publication.
Powertrain spin/commands are scalar and are not translated. Derived contact and
presentation data reconstruct through their existing lifecycle. See the
[M90 implementation and limitations](../M90_ASSEMBLY_TRAVEL.md).

## Entity planet methods

These require a live entity with the optional **Planet terrain** component.
Stale/non-planet handles throw ReferenceError; invalid positions throw TypeError.

| Method/property | Result |
|---|---|
| `entity.planetSample(absolute)` | `{position,normal,height,distance}`. Position is absolute, normal is outward world direction, height is radial elevation above reference radius, distance is signed displacement projected onto that sampled normal (not an exact closest-point distance). This is source sampling, **not** a physics hit. |
| `entity.preparePlanetCollision(absolute)` | Prepare up to two missing cooks for the bounded nearby collision interest. Returns false while preparing, true only when all required immutable meshes are installed. Throws on budget/precision/cook failure; existing support is retained. Call before moving/spawning into a newly requested area. |
| `entity.clearPlanetCollision()` | Retire that planet's current collision interest. Do not clear underneath active actors. |
| `entity.planetState` | Read-only snapshot: `radius`, absolute `center`, visual `leaves`, `resident`, `triangles`, `colliders`, target-selection `maximumPixelError`, `budgetLimited`, last `prepareMs`, `collisionMs`, `collisionReady`, `error`. |

One physical interest per planet is supported in this version. Its radius must
cover all interacting bodies; moving it elsewhere can retire previous support.
Each call has a bounded synchronous preparation cost; retry once per fixed update until ready. It is neither a global
collision mesh nor an implicit promise that distant terrain is queryable.
Generated collision tiles are ordinary transient static triangle meshes with
owner tags, default collision layer zero/mask all. Query hits have body handles
but no separate tile entity. Handles retire safely when residency changes.
Contact notifications against these derived tiles are reconstructed after load;
exact warm-contact/callback continuity across cold load is not claimed.

## Copyable pattern

```js
import { world } from 'judas';
export default class {
  constructor({entity}) { this.entity = entity; }
  fixedUpdate() {
    const planet = world.entity('1'); // project-authored reference in this example
    const absolute = world.toAbsolute(this.entity.transform.position);
    if (!planet.preparePlanetCollision(absolute)) return;
    if (Math.hypot(...Object.values(this.entity.transform.position)) > 1500 &&
        !world.originState.pending) world.requestOrigin(absolute);
  }
}
```

Game-side traversal/camera policy is shown in the ordinary
[planetary lab](../../projects/planetary_terrain_lab/README.md). See also
[CharacterMotor](character.md), [physics/gravity](physics.md),
[streaming](streaming.md), [saves](saves.md) and the
[representation, authoring and measured limits](../M85_PLANETARY_TERRAIN.md).

## M92 multi-view render selection

The world submits main/local-player and enabled secondary camera interest together.
The selector takes the maximum requested screen error per tile, under the existing
shared leaf/residency/build budgets. Opposite cameras therefore cannot inherit only
the main eye's detail. Shared seams and morph state remain world-owned. More views
can exhaust that budget (`budgetLimited`); they do not create independent collision
interest or extra simulated planets. See [rendering limits](rendering.md).
