# Weather — profiles, sampled wind and transitions (M86)

[Index](../JUDASJS.md) · [Physics](physics.md) · [Effects/view](effects-camera.md) · [Planetary terrain](planetary-terrain.md) · [Saves](saves.md)

Weather is optional prescribed atmosphere presentation plus a bounded wind field.
It does not solve atmospheric fluid equations, select gravity, create liquid,
or give rain/snow particles individual rigid bodies. Project scripts decide what
weather means for gameplay.

## `Entity.weather` and `Weather`

Import `weather` for spatial queries; obtain a region handle through
`entity.weather`. An entity without a weather component returns `null`.
Snapshots are detached objects; changing a returned profile does not apply it.

| API | Result / contract |
|---|---|
| `region.valid` | Whether the handle still resolves to a live weather owner. |
| `region.enabled` | Read/write enable flag. Set in `fixedUpdate`. Disabling does not remove the authored component. |
| `region.state` | `{current,target,time,transitionElapsed,transitionDuration,tornadoPosition,lightningSequence,lightningPosition,lightningFlash}`. Time values are seconds, positions current-local metres, sequence an opaque decimal string. |
| `region.transition(patch, duration=1)` | Validate/apply a partial `WeatherProfile` target; returns `true`. Call in `fixedUpdate`. Finite duration 0–86,400 seconds. Omitted fields retain the current profile's values. |
| `region.strike(point?)` | Request an ordinary weather strike at a current-local world point; returns `true`. Without a point, use the configured frame's ground anchor. Call in `fixedUpdate`. |
| `weather.sample(point)` | Detached `WeatherQuery` or `null` outside applicable active weather. The query contains `wind`, `up`, `altitude`, `airDensity`, `rain`, `snow`, `fog`, `cloudCoverage`, `tornadoPosition`, `lightningFlash`. |
| `weather.quality` | `0`: visuals off, `1`: low, `2`: higher. Changing it does not change the CPU wind/force model. |

Query altitude is signed metres relative to the weather frame; air density is
kg/m³. Wind is m/s. `up` is a normalized frame direction, independent of gravity
and support. Precipitation, fog and lightning flash are artistic intensities.
Invalid values, unknown profile keys and mutations in the wrong phase throw;
expired handles fail safely rather than addressing a reused component.

```js
import { weather } from 'judas';

export default class {
  constructor({entity}) { this.entity = entity; this.state = {started: false, windSpeed: 0}; }
  fixedUpdate() {
    const region = this.entity.weather;
    if (!this.state.started && region?.valid) {
      region.transition({ rain: 0.8, cloudCoverage: 0.95,
        wind: { x: 8, y: 0, z: 2 }, gust: 5 }, 12);
      this.state.started = true;
    }
    const air = weather.sample(this.entity.transform.position);
    // A project may inspect air.wind; ordinary opted-in bodies need no JS force loop.
    if (air) this.state.windSpeed = Math.hypot(air.wind.x, air.wind.y, air.wind.z);
  }
}
```

The example's vector is a game-authored direction, not an engine world-up rule.
For arbitrary gravity, derive project policy from sampled frame/gravity data.
[Copyable script](examples/weather.js).

## Authoring and coordinates

Add **Weather region** in the editor inspector. Its profile contains cloud,
precipitation, fog, wind, lightning and tornado parameters. The preset selector
copies explicit profile values; it does not install a weather mode or automatic
sequence. Multiple scenes, prefab instances and streamed regions use the same
ordinary component/serialization path.

| Authored setting | Coordinate meaning |
|---|---|
| Global, sphere or box bounds | A global region is spatially unrestricted. Sphere/box centres, box rotation and bounds follow the owning entity. Boundary distance controls spatial fade. |
| Flat frame centre, up and north | Owner-local ground offset in unscaled metres and directions. Owner translation/rotation resolve them into the world. No global up direction is implied. |
| Radial frame centre and radius | Absolute binary64 planet centre and reference radius, metres. These stay independent of local simulation origin and prefab placement. |
| Radial frame up and north | Owner-local directions defining the starting patch/track orientation. Sampled up follows the planet at the queried point. |
| Wind and tornado travel direction | Owner-local authored vectors, rotated into world space on construction. Runtime wind intent then remains world-space. |
| JS sample/strike position | Current local simulation metres, matching ordinary physics queries. |
| JS transition wind | World-space metres/second. Returned wind is also world-space. |

Weather keeps absolute positions internally. M85 origin shifts translate
the local JS/query frame without moving the weather field. Terrain, weather and
gravity are separate; a weather region never overrides Judas gravity.

Region bounds follow owner translation/rotation/scale; a sphere uses the largest
scale axis for its radius. Flat-frame offsets and profile altitudes are physical
metres and do not scale with the owner. Moving/rotating a running region rebinds
its spatial frame, preserving the clock, transitions and already emitted strike
locations. Existing runtime wind vectors retain their world-space direction;
this preserves explicit script intent rather than rotating it implicitly.

### Profile values

| Field | Unit / supported range |
|---|---|
| `cloudCoverage` | Artistic coverage, 0–1. |
| `cloudDensity` | Optical density multiplier, 0–2. |
| `cloudBase` | Altitude in metres, 0–50,000. |
| `cloudThickness` | Metres, 10–50,000. The authored maximum atmosphere altitude must contain the cloud layer. |
| `rain`, `snow`, `fog` | Artistic intensities, each 0–1; rain and snow may coexist. |
| `wind` | Vector in m/s, magnitude at most 100. |
| `gust` | Peak additional gust speed, 0–100 m/s. |
| `lightningRate` | Expected strikes per minute, 0–120. Not an exact interval guarantee. |
| `tornado` | Funnel/wind intensity, 0–1. |
| `tornadoRadius` | Metres, 1–1,000. |
| `tornadoHeight` | Metres, 10–10,000. |
| `tornadoSpeed` | Track speed, 0–200 m/s. |

Spatial region fade and time interpolation are separate. Changing the profile
does not resize authored region bounds or change its flat/radial frame.

## Physical wind response

Add **Weather wind response** to an ordinary dynamic rigid body to opt in.
Author an effective area (m²), drag coefficient, maximum wind acceleration
(m/s²), and shelter collision-layer mask. The existing body force accumulator
owns integration. No extra body or second gravity solver is created.
World construction rejects this response without an ordinary dynamic Body.
Resident budgets are 16 weather regions and 64 opted-in response bodies; these
are bounded services rather than unbounded per-droplet simulation.

The force is centred pressure drag from sampled air/body relative velocity,
with bounded acceleration and bounded shelter queries. It does not resolve
wing aerodynamics, distributed lift, flexible cloth, or wind-turbine mechanics.
Do not apply equivalent atmospheric drag twice. Graphics quality does not alter
the CPU wind sample, clock, strike sequence or physical drag.

The magnitude is `0.5 * airDensity * dragCoefficient * area * relativeSpeed² * exposure`,
capped at `bodyMass * maxAcceleration`, directed along relative wind. Applying
this force does not make the weather itself conserve air momentum; the field is
an authored environment, not a finite atmospheric reservoir.

## Lifetime and state

The component owner, not a camera, owns the weather clock and transition.
Use fixed updates for gameplay changes and wind-dependent decisions; use
presentation updates only for view-facing effects. Pausing stops its fixed clock.
Disabling a region suppresses its sampled contribution and new automatic strikes;
its clock, transition and cloud/tornado travel still advance while resident.
An explicit zero-duration transition is an immediate replacement, and an
interrupted transition starts from its current interpolated profile.

`lightningSequence === '0'` means no strike has occurred; its reported position
is not a usable strike location until the sequence becomes nonzero.

Region unloading and whole-scene replacement retire the owning handle normally.
Reacquire a fresh handle after a cold load, reload or region reactivation; do not
persist the native handle in script state. Save/load preserves the meaningful
weather clock, interpolation endpoints, integrated cloud advection, tornado
travel and lightning history.
GPU presentation resources are reconstructed and are not numerical save state.

## Scope and approximations

This is convincing prescribed weather, not a climate simulator. No flooding,
physical snowpack, droplet rigid bodies or storm-scale fluid simulation is
provided. The cloud/precipitation renderer has bounded visual quality and local
effect ranges; rendering and shadow approximations are separate from CPU force
semantics. See the M86 implementation report for measured costs and the current
renderer limitations.

Clouds sample actual three-dimensional density in a flat slab or spherical
shell. Quality 1 uses quarter-resolution cloud pixels and 28 primary steps;
quality 2 uses half resolution and 44. Targets are capped at 960 × 540. Secondary
and portal views use a cheaper capped cloud pass without local precipitation.
There is no temporal history to reuse across unrelated camera views. Low-resolution
cloud rays interpolate smooth opaque depth at their own screen position, with
conservative rejection at large depth discontinuities.
The 64³ noise volume is mipmapped, with explicit ray-footprint filtering and a
smooth coarse regional cover for orbital views. Fine cloud detail is intentionally
filtered out when a pixel spans kilometres. This avoids far-distance aliasing;
it does not provide a simulated global climate or externally sourced cloud map.
The moving funnel uses a tight cylinder intersection and 24–64 radius-aware
samples, with procedural canopy/dust and decorative GPU debris.

Rain/snow are bounded local GPU animation. A coarse, staggered 16 × 16 physics
shelter-height grid rejects covered precipitation; small overhangs can leak.
Splashes use sampled surfaces. Ground cloud shadows and wet/snow material tint
are inexpensive visual approximations; tint responds to weather, without water
film thickness or accumulated snow mass. Material tint is region-wide and is not
masked by the precipitation roof atlas; an interior region can author a different
appearance. Turning visuals off stops draws but retains the bounded two-size
renderer cache until shutdown. Estimated noise payload is 299,593 bytes; scratch
storage depends on camera resolution and quality and is separate from scene HDR
targets and driver overhead.

## Scene lighting and dusk/night

Weather's procedural sky blends its daylight gradient according to the current
scene sun/ambient illumination, using the authored background colour in dim
conditions. Cloud, fog, funnel and precipitation lighting also follow scene
lighting. A weak above-horizon moon light therefore does not force a daylight
sky. Set scene background, ambient colour and sun colour/intensity together with
`world.setAppearance` when authoring dusk/night; weather is not an automatic
astronomical day/night clock. Lightning remains a transient illumination source.

Legacy and linear/HDR colour paths, arbitrary-up frames and secondary views use
the same lighting inputs. With weather disabled, ordinary scene rendering is
unchanged. The active weather sky remains procedural rather than compositing a
custom environment background image; environment lighting of objects is separate.
Do not disable the weather region merely to darken the sky: disabling it also
removes its gameplay wind/query contribution.
