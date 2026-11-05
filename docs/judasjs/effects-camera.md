# Audio, particles, camera and liquid field

[Index](../JUDASJS.md) · [Entities](entities.md) · [Character](character.md)

## Audio

Author AudioEmitter/Listener components and registered WAV/MP3/FLAC assets.
[Audio playback and acoustics](audio.md) is the current reference for emitter
controls, bounded streams, motion, obstruction, reverb, sound groups and lifetime.

## Particles

An authored **Particle Emitter** is optional visual simulation, separate from
Advanced Fluid Dynamics. The default remains portable CPU billboards without
collision. [Particle V2 authoring and limits](../PARTICLES_V2.md) describes the
backends, collision snapshots, bounded work and persistence contract.

`entity.burst(count)` emits up to the available capacity and returns success,
or false without an emitter. Count must be an integer number from 0 to 100000;
fractions, numeric strings, negative and nonfinite values throw. This deliberately
replaces the former uint32 coercion. Changing the authored `burst` setting does
not replay startup emission; call `burst` explicitly.

`entity.setParticles(patch)` updates an existing emitter transactionally. The
common fields are `enabled`, `loop`, `rate`, `lifetime`, `size`, `endSize`, `color`,
`endColor`, `velocity`, `velocityVariation`, `spread`, `acceleration`, `burst`,
`maxParticles`, `seed`, `textureAsset`, `localSpace` and `useGravity`. New optional
fields are:

- `backend`: `"cpu"` (default), `"auto"` or `"gpu"`.
- `collision`, `useWind`: booleans; `collisionRadius`, `restitution`, `friction`,
  `maxContacts`; `response`: `"bounce"`, `"slide"`, `"stick"` or `"remove"`.
- `collisionLayers`: registered collision-layer names, an exact unsigned decimal
  64-bit mask string, or `null` for all layers. Readback is always a decimal string.
- `impacts`: `"off"` (default), `"cosmetic"` or `"reliable"`; `impactCellSize`,
  `impactWeight` and `maxImpactCells` bound spatially aggregated contact records.

Unknown or invalid fields throw before changing the emitter; no emitter returns
false. Use a registered texture AssetId or `null` to clear it. Colours are RGBA.
`entity.particles` is a detached complete settings snapshot, or null; an
untextured emitter reports `textureAsset: null`.

Velocity, variation and spread affect future spawns. Acceleration, gravity, size,
colour and texture affect live particles. Reducing capacity trims the pool and
reducing lifetime expires older particles. Changing `localSpace` clears the live
pool; a seed change affects future emission. Disabled emitters keep legacy
behaviour: their particles continue ageing, while emission and drawing stop.
Offscreen particles still simulate; extra cameras never add simulation ticks.

`entity.controlParticles(command)` supports `"play"`, `"pause"`, `"stop"`,
`"clear"` and `"restart"`. Pause freezes simulation; stop stops future emission
while existing particles age. Destructive reset commands can return false while
native impacts (including queued cosmetic observations) or a captured blocked
step are pending: drain the events first.
Unknown commands throw. This is separate from the persisted `enabled` setting.

`entity.particleStats` returns a detached `ParticleRuntimeStats` or null: requested
and active backend, `live`, `simulated`, `contacts`, `queryFailures`,
`queuedImpacts`, `deliveredImpacts`, `cosmeticDropped`, `snapshotBodies`,
`snapshotColliders`, `snapshotTriangles`, `gpuBytes`, `geometryUploads`,
`stateTransferBytes`, `paused`, `backpressure`, `pendingStep`, `simulationMs`,
`snapshotMs` and `diagnostic`. Check the active backend and diagnostic rather than
assuming that requesting GPU made it available.

`entity.takeParticleImpacts()` publishes and drains a detached array of
`ParticleImpact` records. Consume it in `fixedUpdate`. Each contains exact string
`sequence`, `tick`, `receiver` EntityId and `geometryRevision`; `childKey`,
nullable `feature`; historical absolute `position`, body-COM `localPosition`,
world/local normals, incoming `velocity`, `surfaceVelocity`, RGBA `color`, `size`,
`age`, aggregated `count` and authored summed `weight`. These are historical
samples, not current receiver poses. Resolve `receiver` before using it; the body
may have been destroyed or streamed out. `geometryRevision` is diagnostic, not a
portable collider handle. JavaScript owns damage, sounds, decals and gameplay.

Reliable delivery uses CPU simulation and applies explicit backpressure when the
queue is full; it preserves the exact captured step instead of dropping contacts.
The queue is acknowledged only after the complete JS array is created. Cosmetic
events may be dropped within their budget and are unsuitable as gameplay truth.
Modern saves retain settings and queued durable aggregates; individual live
particles and GPU buffers remain transient. See the [example](examples/particles.js).

## Cameras

`camera` on Entity returns the runtime render-target camera snapshot
`{enabled,width,height,near,far,targetWidth,targetHeight,targetError,...}`
or null. `setCameraEnabled(bool)` returns success.
`setCameraPresentation({fov:45, updateEveryFrames:2, renderLayers:['Default']})`
patches an existing secondary camera for the current played world. FOV is vertical
degrees, cadence is a positive frame count, and render layers are registered names.
An empty array draws no entity layers and null restores the all-layer mask.
Read back `fov`, `updateEveryFrames` and `renderLayers` from `entity.camera`.
The returned layer list is detached and uses registered names.
Omitted fields retain their values. Invalid fields reject the whole patch.
`setCameraTargetSize(width, height)` requests dimensions in integer pixels from
1 to 4096. It returns false without a camera and throws for invalid dimensions.
`width` and `height` report the requested size. `targetWidth` and `targetHeight`
report the allocated size (zero before allocation). The renderer applies the
request on the camera's render cadence. If allocation fails, the previous
target continues rendering and `targetError` reports the failure. Read the
camera texture again after a successful resize because its handle can change.
Modern save slots retain requested dimensions. Render targets are recreated at runtime.

`world.setView(pose,fov=70)` publishes a separate runtime main view; partial transform
omissions default to identity (not the previous view). FOV is degrees, strictly
between 1 and 179. Rotation normalizes; invalid pose/FOV throws. Scale is not view
projection. `clearView()` restores the compatibility main-view path. Both return
true. This view is world-owned and clears on reset/destruction. It does not move a
motor or create another simulation. Scripts choose camera behaviour separately.
For a moving-body/motor camera, publish in `presentationUpdate` from
`entity.presentedTransform`, keeping position/orientation on the renderer's
interpolated timeline. Read look input in `update` and preserve authoritative
`transform` for physics queries. See [lifecycle](lifecycle.md).

`world.viewRay` is `{origin,direction}` from the current script-owned main view,
using its world origin and rotated local `-Z`. It immediately reflects a successful
`world.setView` call. Without a script-owned view, it falls back to the last main
view passed to ScriptSystem, or null before any; that compatibility sample can lag
the next draw. A view submitted from an interpolated presentation pose is still a
presentation sample, not an authoritative body pose. Choose query geometry/timing
deliberately. Main view feeds normal presentation/audio; secondary cameras stay separate.

## world.fluidSample

`fluidSample(point,up,halfHeight,radius,tangent)` returns detached
`{immersion,density,velocity,acceleration}` from basic prescribed water, or an
explicitly enabled advanced particle field. Basic water is available by default
without any fluid solver. Point/geometry use metres, density kg/m³, velocity m/s
and acceleration m/s². Up normalizes; tangent must not be parallel to up.
Nonnegative half-height and positive radius are required.

Basic water samples its mean oriented volume using bounded capsule-centre-line
quadrature; its visual waves do not change immersion. Its current is prescribed,
and acceleration is zero. Overlapping water uses greatest immersion, never summed
buoyancy. If both a basic and particle sample exist, greatest immersion wins.
Dry worlds and projects with swimming queries disabled return dry/zero data.

The Experimental particle path restricts immersion to a connected patch's
observed lateral wet footprint. `radius` selects motion donors; increasing it
does not extend wetness across an external dry gap or join disconnected pools.
Within a connected patch this remains a local convex-column approximation:
enclosed air pockets and concave shoreline details can remain unresolved.
Advanced Fluids are still unavailable in Alpha builds pending qualification;
this repair does not change that lock or Basic Water.

Use the sample with resolved Judas gravity for project-owned buoyancy, drag and
propulsion. CharacterMotor remains geometric movement, not a built-in swim mode.
See [Project Settings and water](../PROJECT_SETTINGS_AND_WATER.md) for authoring,
defaults, experimental opt-in, approximation limits and the Stillwater project.
The particle path still has its [existing limitations](../FLUID_DEMO.md).

## Portal views: off-axis frustum and oblique clip plane

`entity.setCameraProjection({near, far, frustum, clipPlane})` also accepts two optional
render-target camera settings for see-through surfaces (portals, windows, mirrors):

- `frustum: {left, right, bottom, top}` is an asymmetric (off-axis) frustum, in metres at the
  near plane in camera space (`left < right`, `bottom < top`). It replaces the authored vertical FOV
  and aspect. Aim the camera perpendicular to a planar opening and pass the opening's corners scaled
  to the near plane: the image then covers exactly that opening, so the surface showing it can use
  plain planar UVs (the image depends only on the eye position, as in a real window).
- `clipPlane: {normal, point}` is a current-local-world plane that becomes the near plane (oblique near-plane
  clipping, Lengyel). Everything on the side the normal points away from is clipped; the image
  (x/y) is unchanged. It applies only while the camera is on the clipped side; otherwise the
  ordinary near plane is used.

Omitting either key keeps its current value; `null` clears it. Both are runtime presentation
settings: they are not authored, saved or restored, so scripts set them each `presentationUpdate`
(typically right after publishing the main view). `entity.camera` reports them as `frustum` and
`clipPlane` (or null). One render target cannot show itself: a portal camera that sees its own
surface still gets the M33 white fallback, and portals seen inside another portal show that portal's
previous image (no recursive passes). Explicit M85 origin shifts translate the plane point
alongside the camera; normals and camera-space frustum extents remain unchanged.
[Copyable example](examples/portal-camera.js).

## CameraRange, world.project and world.viewport (M67)

`world.setView(pose, fov=70, range={near:0.1,far:500})` accepts a partial transform and optional clipping range. Omitted clipping members retain these defaults on that call; set both each presentation update to keep a longer range. Both values must be finite and positive with `near < far`. Unknown range keys throw `TypeError`. `entity.setCameraProjection({near,far})` changes an existing secondary/render-target camera, returning false if the entity has no camera; `entity.camera` includes the current `near` and `far`. These are runtime settings, captured by the existing camera/world save participants.

The actual view projection drives Renderer frustum culling and projection. `world.viewRay` uses the current script-owned view's origin and local `-Z` direction; it has no implicit weapon meaning. Secondary cameras retain their authored projection modes and independent masks.

`world.project(worldPoint)` returns null until a script-owned view and a positive viewport size exist. Otherwise it returns `{x,y,depth,distance,behind,inside}`: x/y are normalized top-left viewport coordinates; depth is OpenGL normalized depth mapped to 0–1; distance is signed view-axis distance in metres, not Euclidean distance. `behind` identifies a point on/behind the camera plane; `inside` includes the near/far clipping bounds. Outside coordinates are not clamped. A point exactly on the camera plane returns the safe centre coordinates with `behind=true`, `inside=false`. Results are snapshots. `world.viewport` returns pixel `{width,height}`, zero before the interactive view is established. Read it again after resize.

Use `presentationUpdate` for view/marker placement, not authoritative movement. A larger far range with a very small near plane reduces depth precision. Camera clipping does not supply infinite viewing distance or request an origin shift; use the explicit [origin API](planetary-terrain.md) within its supported bounds.

## Local viewports (M89)

[LocalPlayer.setView/project/ray](local-multiplayer.md#viewports-and-cameras)
provide independent cameras in the same world. With no dedicated player views,
the existing primary world view remains unchanged. Cameras do not create another
physics tick or private audio listener.

For phone-aspect layout/camera checks on desktop, use the startup size and
mid-run resize commands in the [application harness](../TEST_HARNESS.md#window-size-and-rotation-layout-tests).
The existing `world.viewport` values update before UI/gameplay callbacks;
no new camera or viewport scripting setter is introduced by that workflow.

## Optional orthographic and pixel-perfect lens

Main, local-player and render-target cameras now accept an optional lens.
Perspective remains the default. See [native 2D cameras](sprites-2d.md#orthographic-and-pixel-perfect-views) for fields, content rectangles, project/unproject semantics and portal restrictions.

## Basic Water regions

`entity.basicWater` returns a detached settings snapshot or null when the
component is absent. Use `entity.setBasicWater({flow:{x:2,y:0,z:0}, opacity:0.7})`
in authoritative gameplay to update an existing region. Omitted fields keep
their values. Invalid patches throw before publication. The current is in
region-local m/s and half extents are in metres (Y is depth below the plane).
Wave amplitude and wavelength are in metres and affect the visual surface.
Region extent, enable and current changes also affect the swimming query.
Project Basic Water and swimming toggles still apply. These calls do not
unlock or run Advanced Fluid Dynamics.
