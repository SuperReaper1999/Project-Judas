# M86 — Judas Learns to Weather

Human-accepted M86 implementation; current platform qualification remains separate. This document
describes the source contract; actual Linux measurements, focused results and
the preserved unrelated regression failure are recorded in the
[M86 evidence report](evidence/m86/REPORT.md). Windows M86 remains untested.

[JudasJS reference](judasjs/weather.md) · [Architecture](ARCHITECTURE.md) ·
[Weather Lab](../projects/weather_lab/README.md) · [M85 terrain](M85_PLANETARY_TERRAIN.md)

## Ownership and source map

Weather is optional prescribed environment state, GPU presentation and opt-in
rigid-body loading. Judas owns those mechanisms. Project JavaScript chooses
weather changes, game meaning and demonstration sequences.

| Responsibility | Source / owner |
|---|---|
| Profiles, spatial frames, interpolation, gust/vortex samples, deterministic events, explicit core continuation | `src/Weather.h`, `src/Weather.cpp` |
| Published region composition, ordinary force application, shelter queries, delayed thunder and per-world lifetime | `src/WorldWeather.cpp`, `RuntimeWorld` |
| Fixed ordering | `src/Simulation.cpp` |
| GPU resources, 3D cloud density, sky/fog, precipitation/funnels and material effects | `src/RendererWeather.inl`, included by `src/Renderer.cpp`; Renderer owns all GL |
| Main/secondary view preparation | `src/WorldPresentation.cpp` |
| Authored component properties, prefab equality and serialization | `src/Scene.h`, `src/Scene.cpp`, `src/SceneSerialization.cpp` |
| Inspector and non-playing preview | `src/editor/ComponentEditors.cpp`, `src/editor/EditorApplication.cpp` |
| Full-world continuation and retained-region archive | `src/WorldPersistence.cpp`, `src/SaveService.cpp`, `src/WorldStreaming.cpp` |
| Public script handles, types and inventory | `src/ScriptSystem.cpp`, `docs/judas.d.ts`, `docs/judasjs/` |

Existing `AtmosphereField` remains the M26 hydrostatic gas model. Weather does not
replace it, alter gravity selection, run an atmospheric fluid solve or create a
second physics world. No weather component means no weather field/force work;
renderer resources initialize lazily when an enabled weather view is prepared.

## Authoring

Add **Weather region** in the inspector. Presets copy explicit editable values;
they do not install a native state machine. A project can author global, spherical
or oriented box bounds and select different priorities. Higher priorities replace
the lower stack through the authored boundary fade; stable entity order resolves
equal priorities. The selected frame, seed and tornado/lightning identity come
from the last applicable region. Profile scalars/wind are blended, but different
flat/radial frames are not themselves interpolated. Author coherent overlap zones;
this is not simultaneous rendering of many independent storm volumes.

- Region centres, box rotations and bounds are owner-local. Translation, rotation
  and scale resolve them into absolute coordinates; spherical bounds use the
  largest owner scale axis.
- A flat frame's ground centre is an owner-local offset; up and north are
  independent directions. Flat offsets and cloud altitudes are physical metres,
  not scaled distances.
- A radial frame has an explicit **absolute binary64** centre and reference
  radius. Its up/north orient the starting patch; sampled up follows the sphere.
  A prefab placement does not silently relocate that absolute planet centre.
- Authored wind and tornado direction rotate from owner-local into world space.
  Script transition wind is world-space. Moving a running owner rebinds its
  spatial frame without discarding transitions, time or prior absolute strikes;
  current runtime wind intent remains world-space.
- Weather up is independent of the current gravity acceleration and support
  normal. A weather region does not override FaithfulGravity or RadicalGravity.

The authored descriptors are versioned `weather.region` / `weather.response`
properties through ordinary scene, prefab, resource and export mechanisms.
Non-playing editor preview samples authored weather at its initial clock; Play
owns the running clock, transitions, wind forces and thunder.

### Parameter envelope

| Setting | Supported units / bounds |
|---|---|
| Cloud coverage / density | 0–1 / 0–2 optical multipliers |
| Cloud base / thickness | 0–50,000 m / 10–50,000 m |
| Rain, snow, fog | Independent artistic intensities, each 0–1 |
| Mean wind / peak gust | Mean vector magnitude ≤100 m/s; gust 0–100 m/s |
| Lightning rate | 0–120 expected strikes/minute |
| Tornado strength / radius / height / speed | 0–1 / 1–1,000 m / 10–10,000 m / 0–200 m/s |
| Maximum atmosphere altitude | 1–1,000,000 m; must contain the cloud layer |
| Region radius / box half extents | 0.01–100,000,000 m |
| Transition duration | 0–86,400 s |
| Resident region / response limits | 16 regions / 64 opted-in rigid bodies |

Malformed descriptors, nonfinite values, unknown fields and unsupported versions
reject rather than publishing a partially decoded component.

## Authoritative update and physical approximations

At a played fixed boundary, origin commitment and navigation precede project
`fixedUpdate`; weather then advances and applies its forces before ordinary rigid
physics. It does not advance from render callbacks. Pausing therefore freezes
weather's authoritative clock. Graphics quality never enters core sampling,
interpolation, event generation or force calculation.
Disabling a resident region removes its contribution and automatic strikes, but
does not freeze its clock, transitions or integrated travel. Pausing and retained
streaming nonresidency do freeze that clock.

A transition uses `s(u) = 3u² − 2u³`, `u = clamp(elapsed/duration, 0, 1)`.
Interrupted transitions start at the current evaluated profile. Duration zero is
an immediate replacement. Mean cloud displacement and tornado travel integrate
this interpolation analytically; changing wind does not multiply a new velocity
by the entire elapsed lifetime and jump the cloud field.

### Wind and opt-in forces

The mean vector plus three coherent analytic gust modes supplies a bounded
prescribed wind. In a radial frame the background wind projects into the local
tangent plane; this is not a model of global planetary circulation. A moving
vortex adds finite rotational flow, inward flow and an updraft, with a finite
spatial envelope and no inverse-distance singularity. Its flat track is straight;
its radial track follows a great circle on the reference sphere. Motion is an
actual shared field parameter, not a separate visual funnel animation track.

The approximate reference air density is:

```
rho(h) = 1.225 exp(-max(h, 0) / 8500) * altitudeFade * regionWeight
```

`altitudeFade` smoothly reaches zero between 80% and 100% of the authored maximum
atmosphere altitude. Global weather remains available to orbital cameras for cloud
rendering even where local air/wind forces have reached zero.

Add **Weather wind response** to an ordinary dynamic Body to opt in. Author its
effective area, drag coefficient, maximum acceleration and 64-bit M39 shelter
collision mask. The ordinary force accumulator receives:

```
relative = sampledWind - bodyLinearVelocity
F = direction(relative) * min(0.5 rho Cd area |relative|² exposure,
                              bodyMass * maximumAcceleration)
```

A bounded 12 m ray opposite relative incoming air reduces exposure to 0.1 when
blocked. This is coarse shelter, not resolved indoor airflow. Spatial script wind
queries return the free-air field; they do not issue shelter casts. Force acts at
the body centre and supplies no distributed lift, aerodynamic torque, cloth or
reaction momentum into the prescribed atmosphere. Accepted area/Cd/acceleration
bounds are 0–10,000 m², 0–10 and 0–1,000 m/s².

The existing M26 celestial vehicle already receiving whole-body `AtmosphereField`
drag is explicitly skipped by weather response. Equivalent aerodynamic drag is
not applied twice. Other existing gravity, contact, wheel and mechanical owners
remain unchanged.

## GPU presentation

The current OpenGL backend ray marches actual three-dimensional density in a
flat slab or spherical shell. Camera-relative positions are formed after absolute
double subtraction; modulo reductions occur before narrowing large coordinates.
The radial altitude expression avoids direct subtraction of nearby Earth-sized
float radii. Integrated advection remains absolute across M85 chart shifts.

A lazily generated 64³ R8 noise volume with its complete mip chain (299,593
texel bytes) supplies layered cloud density. Each octave selects an explicit
mip from the current ray's pixel footprint. Once a pixel spans 256–2,048 m,
density smoothly blends toward a filtered regional cover with 16 km and 64 km
noise cells. This bounded distant representation preserves cloud regions during
ground-to-orbit travel instead of sampling unresolved small clouds. All octaves
share an exact 4,194,304 m repetition period; absolute-coordinate modulo reduction
therefore does not introduce a wrap seam. The procedural map repeats and is not
an Earth weather dataset. **The GPU
noise volume currently uses one fixed seed.** The authored seed changes CPU gusts
and lightning scheduling, not the cloud noise pattern. Three coarse sunlight
samples approximate cloud self-shadowing; directional illumination, an approximate
sky, storm darkening and flash light provide coherence. The main geometry shader
uses the same coarse noise phase for inexpensive cloud shadows and immediate
wet/snow tint. Tint is region-wide and uses surface orientation; the precipitation
shelter atlas does not mask material tint, so covered interior surfaces can also
receive it unless an authored interior weather region changes that appearance.
These are approximations, not full multiple scattering, physically accumulated
films or snow depth.

| Quality | Cloud target | Primary march steps | Local rain / snow maximum |
|---|---|---:|---:|
| 0 | No weather draws | 0 | 0 / 0 |
| 1, default | Quarter width/height, capped 960 × 540 | 28 | 3,500 / 1,800 |
| 2 | Half width/height, capped 960 × 540 | 44 | 7,000 / 4,000 |
| Secondary / portal | Eighth width/height, capped 240 × 135 | 16 | None |

Rain/snow use bounded local instanced GPU animation with world/frame wind bending
short fall trajectories. A separate bounded debris pass surrounds the rotating
volumetric funnel; those visual fragments are not rigid bodies. Funnel marching
intersects a finite cylinder, then uses 24–64 samples based on the traversed
distance and configured radius. This resolves the narrow base without marching
the mostly empty span of a tall bounding sphere. Its condensation canopy blends
above the configured core height, and a low dust skirt surrounds the base;
these visual envelopes do not extend the physical vortex's core height.
Splashes use the shelter/surface samples rather than droplet impacts.

### Shelter and camera/resource limits

The main camera maintains a 16 × 16 height atlas over a 32 m tangent patch. Sixteen
ordinary physics rays refresh it per rendered precipitation frame; after initial
creation or recentering, a complete refresh takes 16 rendered frames. The atlas
recentres after 8 m of travel or a significant up change. Rays start 80 m above
the patch and cover 160 m. This coarse, staggered approximation can briefly leak
while warming up, miss small overhangs or leave stale cover after fast changes.
Secondary views omit shelter and local precipitation.

Cloud occlusion interpolates projected triangle depth at each cloud ray rather
than borrowing a neighbouring full-resolution depth ray; this matters when an
orbital half-pixel surface gradient exceeds the cloud thickness. Large relative
depth discontinuities retain conservative nearest-surface rejection.
Cloud compositing uses current view/depth information. There is **no temporal
history**, so unrelated/portal cameras do not reuse another camera's accumulated
cloud frame. Two framebuffer-size entries form an LRU cache; many different view
sizes can cause reallocations while remaining bounded. A view is limited to
16 megapixels of scratch depth, with the separate cloud cap above. Diagnostics
report estimated texture storage, draw counts and allocations; driver overhead
and unrelated scene resources are separate. Quality zero stops draws but retains
already allocated weather caches until renderer shutdown.

At 1,280 × 720, one default-quality primary scratch allocation plus noise/cover
uses approximately 4.24 MiB of estimated texture payload. A cached 64 × 64
secondary view brings the measured allocation accounting to 4,464,713 bytes.
This excludes the existing scene HDR buffers, renderer resources and driver
allocation overhead. Rendered timings and full-lab timings are separate receipts.

## Lightning and thunder

Seeded half-second slots select lightning with probability `rate/120`, bounding
work to at most two generated events per normal ≤1 s core advance. This is a
controlled stochastic approximation, not an exact inter-strike interval. Strikes
use the configured ground reference, not terrain-aware electrical propagation.
JavaScript can request an explicit local-world strike through the same event path.

Analytic bolt/flash presentation and delayed procedural thunder share the native
event time/position. Delay uses listener distance / 343.3 m/s. The existing audio
mixer owns sound; no new audio backend is added. Pending thunder is bounded to
eight events per region and active weather voices to eight. Excess events may be
dropped. Already playing one-shots are transient audio presentation; pending
future thunder, rather than sample-by-sample voice playback, is save state.

The lab authors an ordinary follow-active-view audio listener. M85 origin support
is extended for that listener's camera-derived motion and weather's absolute
one-shot locations. Positional audio emitters/zones retain their existing live
origin restrictions; this is not general rebasing of every audio component.

## Save, streaming, lifecycle and packaging

The version-1 weather save participant records exact live owners and their
validated clock, enabled state, transition endpoints/progress, integrated cloud
advection, tornado travel, last lightning and pending thunder. Missing or mismatched
required records reject restoration. Weather owners cannot opt out as transient
entities while still contributing required world state.

M59 retained weather uses streaming archive version 8. A region contributes no
weather until atomic publication, retires its field on unload and retains its
continuation. Its clock freezes while nonresident. Reactivation rebuilds the
ordinary component and restores state before publication, including capture-origin
translation where necessary. Older archive versions remain the path for content
without this new state. Stale script handles must be reacquired after reload or
region reconstruction.

GPU textures, cover atlas, quality choice and running sound voices are not
physical continuation data. Export uses the existing project scene/asset closure;
there is no hidden engine demo mode or downloaded weather service.

## Weather Lab and controls

Open [`projects/weather_lab/weather_lab.judasproj`](../projects/weather_lab/weather_lab.judasproj).
The flat meadow supplies a solid-roof pavilion, light panels, freight and a 300 kg
crate. The Earth-sized scene uses M85 terrain plus a normally registered streamed
observatory. Project JS owns the tour, free-camera flight, ordinary CharacterMotor
locomotion and physical probe spawning.

- **1–8:** sunny, cloudy, rain, thunderstorm, tornado, snow, blizzard, fog.
- **F5:** watch tour; **F9:** switch flat/planet scenes.
- **Q:** quality; **0:** visuals off/on without changing wind forces.
- **WASD/mouse/Space:** walk/look/jump; **E:** throw a physical probe.
- **F1/F2:** ascend/land; **F3:** planetary face visit.
- **F6/F7:** save/load; **R:** reload; **Escape:** pause.

The lab's orbit travel is a project-controlled view. It retires/streams local
physical actors before changing an M85 physics chart and reconstructs a motor on
landing. It does not pretend the motor itself flew through interplanetary space.
See the [project README](../projects/weather_lab/README.md) for the consolidated
human inspection sequence.

## Deliberate limitations and validation status

- No meteorology, full atmospheric CFD, conserved air reservoir, flooding,
  physical snowpack or individually simulated drops/flakes.
- One resolved visual weather/frame stack per view, one moving funnel per region;
  this is not simultaneous independent rendering of every overlapping storm.
- Fixed cloud noise, limited low-resolution marching, coarse ground shadows and
  immediate wet/snow appearance. No temporal upscaling or accumulated surface mass.
- Local shelter has finite resolution, range and warm-up; wind shielding is a
  single bounded ray with residual exposure. Neither is an indoor air solver.
- Lightning has no electrical propagation, automatic damage, fire or material
  conduction. Visual debris does not create physical collision bodies.
- Radial weather uses a reference sphere, not a terrain-conforming cloud/funnel
  base or a planetary climate/circulation model.
- Scene/quality/resolution comparisons require separate CPU/GPU timing and memory
  receipts. The evidence report separates the matched render workload, whole-lab
  responsiveness and physical wind workload; none is a universal timing guarantee.

## Reference starting points

Technique/context references supplied with the brief; none is a new engine
runtime dependency or a claim of feature parity:

- [Guerrilla, Nubis: Authoring Real-Time Volumetric Cloudscapes (2017)](https://www.guerrilla-games.com/read/nubis-authoring-real-time-volumetric-cloudscapes-with-the-decima-engine).
- [Guerrilla/GDC, The Real-Time Volumetric Superstorms of Horizon Forbidden West (2022)](https://gdcvault.com/play/1028023/The-Real-Time-Volumetric-Superstorms).
- [Epic, Volumetric Cloud Component](https://dev.epicgames.com/documentation/unreal-engine/volumetric-cloud-component-in-unreal-engine).
- [NVIDIA GPU Gems 2, Accurate Atmospheric Scattering](https://developer.nvidia.com/gpugems/gpugems2/part-ii-shading-lighting-and-shadows/chapter-16-accurate-atmospheric-scattering).
- [Microsoft Research, Real-Time Rendering of Realistic Rain (2006)](https://www.microsoft.com/en-us/research/publication/real-time-rendering-of-realistic-rain/).
- [NOAA, Ingredients for a Thunderstorm](https://www.noaa.gov/jetstream/thunderstorms/ingredients-for-thunderstorm).
