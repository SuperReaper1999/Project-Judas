# Planet terrain and local physics origins (M85)

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

**Scope:** live shifts require script-owned execution and an 8 km active physics
chart. Spatial actors must be streamed out/retired before moving to a distant
chart. Active liquid/PBF, atmosphere/combustion, deformables, articulations/IK,
wheels/powertrains, navigation, audio, particles and visual sockets currently
reject live shifts. Retained opaque component continuations and pending M59
transactions also reject shifts. This is not universal whole-engine rebasing.
Scenes that never request a shift retain their existing behaviour.

Scripts own their cached coordinates. Store long-lived locations in absolute
space, or observe `revision` and translate local caches yourself. The engine
cannot infer which numbers inside arbitrary game JSON represent positions.
Scene reload resets the origin to the authored value; saves preserve the current
origin. Streamed ordinary rigid/motor records retain their capture origin so
unload/reload does not reinterpret old local coordinates in a new chart.

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
