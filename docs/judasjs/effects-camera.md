# Audio, particles, camera and liquid field

[Index](../JUDASJS.md) · [Entities](entities.md) · [Character](character.md)

## Audio

Author AudioEmitter/Listener components and registered WAV/MP3/FLAC assets.
[Audio playback and acoustics](audio.md) is the current reference for emitter
controls, bounded streams, motion, obstruction, reverb, sound groups and lifetime.

## Particles

`burst(count)` queues an authored visual emitter burst; boolean success, false
when no emitter. Count is converted to uint32 and must be <=65536; use a nonnegative
integer. Capacity/lifetime remain authored. `setParticles({enabled?,rate?})`
changes runtime enable/emission rate only; boolean success, false without emitter,
invalid rate/settings throw. No JS readback, collision, arbitrary particle-position
editing or fluid-particle API exists. Pose follows entity transforms. Offscreen
emitters continue ageing; rendering uses normal per-camera culling.

## Cameras

`camera` on Entity returns the runtime render-target camera snapshot
`{enabled,width,height,near,far}`
or null. `setCameraEnabled(bool)` returns success. No JS target resize/cadence,
render-layer-mask setter or generated texture resource handle is exposed. Author
those settings through normal M33/M39 scene/editor data.

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
`{immersion,density,velocity,acceleration}` from the existing production particle
field. Point/geometry in metres, density kg/m³, velocity m/s, acceleration m/s².
Up normalizes; tangent must not be parallel to up. Nonnegative half-height and
positive radius required. Dry worlds return zero/dry data. This is approximate
field sampling, not direct water ownership, exact pressure or a new fluid solver.
Use sampled liquid acceleration with Judas gravity for project-specific swimming
behaviour; the motor provides geometric motion, not a built-in swim mode.
Existing 20 Hz demo/coarse-fluid limitations remain [documented](../FLUID_DEMO.md).

## CameraRange, world.project and world.viewport (M67)

`world.setView(pose, fov=70, range={near:0.1,far:500})` accepts a partial transform and optional clipping range. Omitted clipping members retain these defaults on that call; set both each presentation update to keep a longer range. Both values must be finite and positive with `near < far`. Unknown range keys throw `TypeError`. `entity.setCameraProjection({near,far})` changes an existing secondary/render-target camera, returning false if the entity has no camera; `entity.camera` includes the current `near` and `far`. These are runtime settings, captured by the existing camera/world save participants.

The actual view projection drives Renderer frustum culling and projection. `world.viewRay` uses the current script-owned view's origin and local `-Z` direction; it has no implicit weapon meaning. Secondary cameras retain their authored projection modes and independent masks.

`world.project(worldPoint)` returns null until a script-owned view and a positive viewport size exist. Otherwise it returns `{x,y,depth,distance,behind,inside}`: x/y are normalized top-left viewport coordinates; depth is OpenGL normalized depth mapped to 0–1; distance is signed view-axis distance in metres, not Euclidean distance. `behind` identifies a point on/behind the camera plane; `inside` includes the near/far clipping bounds. Outside coordinates are not clamped. A point exactly on the camera plane returns the safe centre coordinates with `behind=true`, `inside=false`. Results are snapshots. `world.viewport` returns pixel `{width,height}`, zero before the interactive view is established. Read it again after resize.

Use `presentationUpdate` for view/marker placement, not authoritative movement. A larger far range with a very small near plane reduces depth precision; this does not supply infinite viewing distance or live origin rebasing.
