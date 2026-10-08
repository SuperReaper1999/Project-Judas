# M37 — Frustum culling and visual particle emitters

**Status through M69:** M37 is operator-accepted and checkpointed. Candidate/pending
statements and measurements below record the original milestone review state,
not a current outstanding acceptance gate. Later contracts take precedence;
start with [Architecture](ARCHITECTURE.md) and [JudasJS](JUDASJS.md).

## Visibility

`VisualBounds` is a reusable world AABB; `Frustum` extracts six planes from the
actual OpenGL clip matrix. Plane/support-point tests use a conservative floating
point margin. Unknown/invalid bounds fail open. Perspective and orthographic
matrices use the same path.

Renderer caches local mesh bounds on upload, refreshes them when dynamic mesh
vertices change, and transforms their eight corners for each draw. All ordinary
DrawMesh/Box/Sphere consumers share this gate. Every SetCamera supplies that
camera's frustum; shadow passes use their own light clip matrix; ending a render
target restores the previous camera/frustum. No global visibility list exists.
Physics, audio, gameplay, residency and fidelity do not depend on visibility.

`Renderer::SetCullingEnabled(false)` is the reference/diagnostic switch.
`RenderStats` exposes considered/visible/culled mesh and emitter counts plus
submitted particle counts. Editor Profiler shows aggregate counts across passes.
Debug lines and UI are deliberately outside mesh culling.

## Visual particles

The ordinary `particle-emitter` component is serialized by the existing scene
serializer, so prefab property overrides/source resolution work without prefab
special cases. The Inspector's Add Component list includes Particle emitter.
It exposes enabled/continuous emission, startup burst, capacity, lifetime,
position spread, velocity/variation, acceleration, start/end size and RGBA,
texture AssetId, local/world space, gravity and seed.

RuntimeWorld owns a bounded CPU pool per emitter. Simulation updates it once
per ordinary fixed step, irrespective of cameras/visibility. There is no
per-particle allocation per frame. Disabled emitters stop emission/drawing;
existing particles age out. Non-looping emitters emit their startup burst only;
`RuntimeWorld::EmitParticleBurst(id,count)` supports subsequent gameplay bursts.
At capacity, excess new emission is discarded rather than growing the pool.
World/Play destruction releases the pools. Runtime creation/spawn and ordinary
entity destruction support the component.

Emission vectors are emitter-local and rotated into world space for world
particles. Local-space particles retain local coordinates and follow the
emitter's presented TRS. Optional gravity samples Judas's actual GravityField
at each particle's world position, converting acceleration back to local
coordinates when necessary. No world-down assumption or particle collision is
present. This visual pool is independent of production liquid/PBF particles.

The current billboard union provides conservative per-emitter bounds, including
size and camera-independent corner radius. The **same** Renderer visibility API
culls the whole emitter before GPU upload/draw. No per-particle frustum test.
Culled particles continue ageing/moving and are not reset on visibility changes.

Renderer lazily owns one OpenGL 3.3 billboard shader/VAO/VBO and reusable CPU
scratch buffers. One draw per visible nonempty emitter. Billboards are unlit,
texture-modulated RGBA, depth-tested, alpha-blended with depth writes off.
Particles sort back-to-front **within each emitter, per camera**. Ordering
between emitters/other transparent geometry is not globally sorted; overlapping
systems can show transparency artifacts. No shadow casting or lighting model.
Missing/loading textures use the normal white fallback. All GL is in Renderer.

## Persistence and formats

Scene grammar remains 3; canonical fingerprint schema remains 5. A tagged
`Judas.VisualParticleEmitters.1` canonical extension is emitted only when this
new component exists and includes stable entity IDs and every authored field.
Old scenes without particles retain byte-identical fingerprints; no historical
scene could already contain this formerly unsupported component. Adding,
removing or changing emitters changes baseline identity. Pool ages, random
state and live particles are transient visual state, not saved gameplay state.

## Demo

Open `assets/scenes/particle_demo.judas` in the editor; Play, then move/look
using the existing project inputs. It contains ordinary props, a soft-textured
looping plume, a gravity-driven spark emitter, a finite startup burst and a
rear-view M33 camera surface. Stop/Play restarts the startup burst. The particle
mask is a small authored procedural radial-alpha texture, not external art.

Future work: globally sorted transparency, particle collisions, lighting,
trails/ribbons, GPU simulation, interpolation and visibility throttling.
Particles use current fixed-step state (local emitters follow presented poses);
there is no separate particle interpolation in M37. Human visual acceptance is
pending; automated GL/readback does not substitute for it.
