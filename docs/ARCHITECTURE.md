# Architecture

This document explains the technical decisions behind Project Judas and how
they relate to the engine's long-term purpose. It is meant to be readable by
someone with no prior context on this project. It is updated in place as
milestones land, rather than kept as a per-milestone snapshot — see
"Milestone history" below for how to recover an earlier milestone exactly.

## What exists right now (Milestone 3)

Open a window. A static floor and a dynamic cube exist in 3D space. Judas
samples its own gravity and hands it to the physics middleware, which
integrates the cube's motion, detects its collision with the floor, and
resolves it — the cube falls, lands, and settles under real rigid-body
simulation, not scripted motion. The rendered floor/cube use the transform
the physics simulation produced. The Milestone 2 free-flight camera still
works, purely for observation. `R` resets the cube. Nothing else. See the
root `README.md` for build/run instructions and controls.

## Milestone history

Each milestone is tagged in git so it can be recovered exactly, rather than
preserved as parallel runtime code:

- `milestone-1` — window creation, a single 2D box, keyboard movement.
- `milestone-2` — 3D rendering, perspective, depth testing, a free-flight
  camera, three static cubes.

Each milestone's demo content has been replaced (not extended) by the next;
check out a tag to see or run an earlier milestone as it was.

## Language: C++

The long-term engine needs tight control over large-world coordinate math,
and integrates physics middleware (rigidbody dynamics, collision detection)
and possibly a low-level graphics API like Vulkan. The mature options in
that space (Jolt Physics, PhysX, Vulkan itself) are C or C++ libraries with
the most direct, lowest-overhead integration path in C++. Choosing C++ now
avoids introducing an interop layer later for problems we already know
we'll have — Milestone 3's direct Jolt integration is a concrete case of
this paying off.

C# was considered and is workable (e.g. via Silk.NET), but it buys nothing
here and adds friction for physics/graphics integration. There was no
technical reason to prefer it.

## Dependencies

**SDL2** — window creation, OS event handling, keyboard/mouse state, and
OpenGL context creation. A solved, commodity problem; SDL2 is mature,
widely shipped, and its scope (window/input/context, plus audio/controller
support not yet used) matches this engine's needs without pulling in
anything unnecessary.

**GLM** (added in Milestone 2) — header-only C++ `vec3`/`mat4` math
matching GLSL's own conventions. MIT-licensed. Installed via the system
package manager (`libglm-dev`).

**Jolt Physics** (added in Milestone 3) — see "Physics middleware" below.

## Physics middleware

**Selected: [Jolt Physics](https://github.com/jrouwe/JoltPhysics), pinned
to tag `v5.6.0`, MIT license.**

### What it provides / why we need it

Rigid-body dynamics, collision detection, contact resolution, and
constraint solving are a solved, extremely hard problem (robust narrow-phase
collision, stable contact manifolds, a working constraint solver) that
every shipped physics-using game either licenses or spends years building.
Project Judas has no reason to re-derive any of that — see "Ownership
boundary" below for exactly what we do keep for ourselves.

### Jolt vs. PhysX 5

Both were evaluated against this engine's long-term requirements (arbitrary
gravity, no universal up, large spherical worlds, moving reference frames,
many terrain collision objects, raycasts/shape queries, rigid
bodies/constraints, Linux + Windows, performance, clean integration,
MIT-compatible licensing):

|  | Jolt Physics | PhysX 5 |
|---|---|---|
| License | MIT | BSD-3-Clause |
| Integration into a small CMake project | `add_subdirectory`/`FetchContent`, produces one `Jolt` target, no code generation step | Own Python-based project generator + platform-specific build scripts; heavier to embed in a foreign build system |
| Large-world support | First-class: an optional `DOUBLE_PRECISION` build mode for large-coordinate worlds, explicitly designed for — and shipped in — Guerrilla Games' open-world *Horizon* titles | Single-precision floats; the usual advice is to keep the simulation origin near the camera (no first-class large-coordinate mode) |
| Multithreading/perf | Built job-system-first, SIMD, used in shipped AAA titles | Also strong; both are production-grade |
| Platform support | Windows, Linux, macOS, consoles, ARM | Windows, Linux, consoles |
| API shape | Explicit body/shape/constraint API; middleware doesn't assume ownership of "the world" | Similar shape |

Both licenses are compatible with this MIT-licensed public repository, and
both are technically capable rigid-body engines — this was not a
performance or correctness gap. The deciding factors were **large-world
support** (a direct match for this engine's enormous-planet/large-world
future, and something Jolt treats as a first-class use case rather than an
afterthought) and **integration cost** (a `FetchContent` + `add_subdirectory`
away vs. a separate generator toolchain), which matters for a small,
solo-maintained public repository where every added build-system moving
part is a maintenance and reproducibility cost. Popularity was not the
deciding factor, though it's a supporting signal: Jolt is also the physics
engine Unreal Engine added as a selectable backend from 5.4 onward, for
similar large-world/performance reasons.

### Integration

Jolt is not packaged by apt (or any mainstream Linux distro package
manager), so it's fetched at CMake configure time via `FetchContent`,
pinned to the `v5.6.0` git tag for reproducibility (see `CMakeLists.txt`).
This means a first configure needs network access to clone it; nothing
about Jolt is vendored into this repository's own git history, and nothing
about it is assumed pre-installed on a developer's machine. Jolt's own
`Build/CMakeLists.txt` is used directly (via `SOURCE_SUBDIR Build`), which
produces exactly one `Jolt` static-library target and — because it detects
it's being used as a subdirectory dependency rather than built standalone —
automatically skips building its own unit tests, samples, and viewer.

## Ownership boundary

This is the foundational rule Milestone 3 establishes, and the most
important thing to preserve in every milestone after this one:

```
Judas owns gravity, reference frames, and world/large-world coordinates.

The physics middleware (Jolt) owns collision detection, contact
generation, rigid-body integration, and constraint solving — nothing more.
```

### Gravity: one interface, interchangeable implementations

`GravityField` (`src/GravityField.h`) is an abstract interface — a single
pure-virtual method, `Sample(worldPosition) -> acceleration` — and is
Judas's *only* contract for gravity. Everything that needs gravity
(currently just `Application::Run`) talks to a `GravityField&` and stays
completely agnostic about which concrete implementation is behind it.

`FaithfulGravity` (`src/FaithfulGravity.h/.cpp`) is the first, canonical
implementation: conventional, uniform, constant-direction gravity suitable
for an ordinary/local environment with a single fixed "down." For
Milestone 3 it returns `(0, -9.81, 0)` everywhere — see "Current gravity"
below for why that constant is not an engine-wide law. The name is
intentional and permanent: `FaithfulGravity` names *this specific
implementation* (faithful to conventional, ordinary gravity), not the
concept of gravity in Judas generally — that's what the `GravityField`
interface is for.

This shape exists specifically so that later implementations —
radial/planetary gravity, a composite field blending multiple sources —
are new classes implementing `GravityField`, sitting next to
`FaithfulGravity`, not modifications to it or to anything downstream:

```
GravityField (interface)
    |
    +-- FaithfulGravity            (uniform/constant-direction; Milestone 3)
    |
    +-- (future) radial/planetary gravity
    |
    +-- (future) composite/multi-source gravity
```

`Application::Run` is the composition root: it's the one place that
constructs a concrete `FaithfulGravity` and binds it to a `GravityField&`.
Every line of code after that construction — including the fixed-step
physics loop — only ever calls `gravity.Sample(...)` through the interface
reference.

Concretely, as of this milestone:

- Jolt's own built-in global gravity is explicitly disabled:
  `PhysicsSystem::SetGravity(Vec3::sZero())` in `PhysicsWorld::Init`
  (`src/PhysicsWorld.cpp`). Jolt defaults this to `(0, -9.81, 0)` applied
  automatically to every dynamic body — that default is never used here.
- Every fixed physics step, `Application::Run` samples the active
  `GravityField` at the dynamic body's current position and hands the
  resulting acceleration to `PhysicsWorld::ApplyLinearAcceleration`, which
  integrates it into the body's velocity
  (`velocity += acceleration * fixedDeltaTime`) via Jolt's
  `BodyInterface::AddLinearVelocity`. Jolt never computes gravity; it only
  receives the result of a `GravityField` implementation having already
  computed it.
- `PhysicsWorld` (`src/PhysicsWorld.h/.cpp`) is the only file that includes
  a Jolt header. Its public interface (`PhysicsWorld.h`) exposes an opaque
  `BodyHandle`, plain `glm` types, and semantic operations
  (`CreateStaticBox`, `CreateDynamicBox`, `ApplyLinearAcceleration`,
  `Step`, `GetTransform`, `ResetBody`) — no Jolt type is visible outside
  `PhysicsWorld.cpp` (it uses the pImpl idiom specifically for this). No
  other engine file needs to know Jolt exists, which is what makes the
  physics middleware itself swappable in principle, even though swapping
  it isn't a goal right now.

**Why this matters:** if a future radial-gravity implementation required
touching how `PhysicsWorld` integrates a body's motion, `Application`'s
physics loop, or Jolt's own gravity settings, that would mean this
boundary was drawn in the wrong place. It shouldn't: a new `GravityField`
implementation is a new class plus swapping which concrete type
`Application::Run` constructs — everything downstream
(`ApplyLinearAcceleration`, `Step`, the render read-back) stays exactly as
it is.

## Simulation timing

Physics is stepped on a **fixed timestep of 1/60 second**
(`kFixedTimestep` in `src/Application.cpp`), not the variable render-frame
delta time — a common, well-tested rate for rigid-body simulation, chosen
for that reason rather than anything specific to this scene. Running
physics directly off render delta time would make the simulation's
behavior (and therefore whether the cube's resting state is actually
stable) depend on frame rate, which defeats the purpose of proving the
integration works.

`Application::Run` uses a conventional accumulator:

```
accumulator += (clamped) render-frame delta time
while accumulator >= fixedTimestep and steps-this-frame < cap:
    sample gravity at the body's current position, apply it
    step physics by exactly fixedTimestep
    accumulator -= fixedTimestep
```

Two independent guards prevent a stall (e.g. dragging the window) from
causing an unbounded catch-up backlog ("spiral of death"):

1. The render-frame delta time fed into the accumulator is itself clamped
   to `kMaxFrameDeltaTime` (0.25s), same as Milestones 1–2.
2. A hard cap, `kMaxPhysicsStepsPerFrame` (8), on how many fixed steps a
   single render frame will run. If it's hit, the remaining accumulated
   time is **dropped** rather than carried into the next frame — the
   simulation loses a little wall-clock accuracy after a severe stall
   rather than trying to fully catch up and risking never recovering.

The rendered transform is simply whatever `PhysicsWorld::GetTransform`
returns after the accumulator loop for that frame — there is currently no
interpolation between physics steps and the render frame. At 1/60s physics
alongside vsync-capped rendering this isn't visually necessary yet; it's a
known, explicitly deferred refinement (see "Deliberately Not
Implemented").

## Current gravity

`FaithfulGravity::Sample` returns a constant `(0, -9.81, 0)` for every
position. **This is configuration/test data for `FaithfulGravity`, not an
engine-wide definition of gravity or of "down."** Nothing about the
vector's direction or magnitude is assumed anywhere outside
`FaithfulGravity.cpp` — `PhysicsWorld` takes whatever acceleration the
active `GravityField` implementation produces and applies it, without
interpreting it. A future radial/planetary gravity implementation is
expected to be a new class implementing `GravityField` (see "Gravity: one
interface, interchangeable implementations" above) that actually uses its
`worldPosition` argument, without changing anything about how a physics
body *receives* gravity.

## 3D rendering pipeline

`Renderer::DrawBox` implements the conventional model → world → view →
clip-space pipeline entirely via matrices, computed with GLM and uploaded
as uniforms to a single, simple shader (`src/Renderer.cpp`):

```
local (unit cube, [-0.5, 0.5] per axis)
    --(uModel: translate * rotate * scale, from position/rotation/halfExtents)-->
world coordinates
    --(uView: Camera::GetViewMatrix(), a glm::lookAt)-->
camera/view coordinates
    --(uProjection: Camera::GetProjectionMatrix(), a glm::perspective)-->
clip space / screen
```

The vertex shader is exactly:

```glsl
gl_Position = uProjection * uView * uModel * vec4(aLocalPos, 1.0);
```

Milestone 3 added rotation and non-uniform scale to the model matrix
(`glm::translate(position) * glm::mat4_cast(rotation) * glm::scale(halfExtents * 2)`)
so the floor and cube can be different sizes and so the cube's rendered
orientation can come from physics — both were previously translation-only.
The floor and cube are both drawn with the **same transform their physics
body reports** (`PhysicsWorld::GetTransform`); there is no separate
rendering-side position/rotation for either of them, and no duplicated
movement logic between simulation and rendering.

### Coordinate conventions (current, local to this renderer/physics setup)

- World space is a conventional right-handed 3D space in engine-defined
  "world units," used directly as Jolt's own simulation space (no unit
  conversion between Judas and the physics middleware yet).
- `+Y` is used as "up" by the camera (see "Camera orientation") and, as of
  this milestone, by `FaithfulGravity`'s constant test vector. **Neither is
  an engine-wide law** — see "Current gravity" and "Future constraints
  preserved."
- There is still no distinction between authoritative large-world
  coordinates and local rendering/physics coordinates — see "Future
  constraints preserved."

## Depth handling

Unchanged from Milestone 2: `Renderer::Init` enables depth testing once at
startup (`glEnable(GL_DEPTH_TEST)`, `glDepthFunc(GL_LESS)`), a 24-bit depth
buffer is requested at context creation, and `BeginFrame` clears both the
color and depth buffers every frame.

## Projection handling

Unchanged from Milestone 2: `Camera::GetProjectionMatrix(aspectRatio)`
rebuilds the perspective projection every frame from the window's current
width/height, so resizing is handled automatically with no dedicated
resize-event code path.

## Main loop structure

`Application::Run()` (`src/Application.cpp`) owns the loop:

```
Init Window, load GL functions, Init Renderer
Init PhysicsWorld (registers Jolt types, zeroes Jolt's own gravity)
Create a FaithfulGravity, bound to a GravityField& (see "Ownership boundary")
Create the static floor body and the dynamic cube body
Create Camera (positioned to see the whole scene at launch)

while (!window.ShouldClose()):
    window.PollEvents()          // close request, Escape toggle, R (reset) flag
    frameDeltaTime = measured elapsed time since last frame, clamped

    camera.Update(window, frameDeltaTime)      // keyboard movement + mouse look

    if window.ConsumeResetRequest():
        physicsWorld.ResetBody(cube, initialPosition, initialRotation)

    accumulator += frameDeltaTime
    while accumulator >= fixedTimestep and steps < cap:
        acceleration = gravity.Sample(cube's current position)  // through the GravityField interface
        physicsWorld.ApplyLinearAcceleration(cube, acceleration, fixedTimestep)
        physicsWorld.Step(fixedTimestep)
        accumulator -= fixedTimestep

    floorTransform = physicsWorld.GetTransform(floor)
    cubeTransform  = physicsWorld.GetTransform(cube)

    renderer.BeginFrame(...)
    renderer.SetCamera(camera.GetViewMatrix(), camera.GetProjectionMatrix(aspectRatio))
    renderer.DrawBox(floorTransform..., floorHalfExtents, floorColor)
    renderer.DrawBox(cubeTransform...,  cubeHalfExtents,  cubeColor)
    renderer.EndFrame()
    window.SwapBuffers()

physicsWorld.DestroyBody(cube); physicsWorld.DestroyBody(floor); physicsWorld.Shutdown()
renderer.Shutdown()
// Window's destructor tears down the GL context, the window, and SDL itself.
```

The engine-level split is unchanged in kind, with one addition:

- `Window` — window/input.
- `Renderer` — graphics.
- `PhysicsWorld` — physics (new in Milestone 3).
- `GravityField` / `FaithfulGravity` — Judas's own gravity interface and
  its uniform-gravity implementation (new in Milestone 3).
- `Camera` — the observational free-flight camera (no longer "the demo
  content" on its own — the floor/cube pairing in `Application.cpp` is now
  the demo content, driven by physics rather than by `Camera`).
- `Application` — wires the above together and owns the loop, including
  the physics accumulator.

## Input handling

Unchanged from Milestone 2 (`MoveForward`/`MoveBackward`/`StrafeLeft`/
`StrafeRight`/`Ascend`/`Descend`, mouse look, `Escape` to release/recapture
the mouse), plus one addition:

**`R` resets the dynamic cube.** Handled as a discrete key-down event in
`Window::PollEvents` (like `Escape`, not like the continuously-polled
movement actions), setting a flag that `Window::ConsumeResetRequest()`
returns once and clears. `Application::Run` calls
`PhysicsWorld::ResetBody(cube, initialPosition, initialRotation)`, which
sets the body's pose and zeroes both linear and angular velocity — a
minimal debug control, not a general save/restore or replay system.

## Camera orientation

Unchanged from Milestone 2: `Camera` keeps a fixed world `(0, 1, 0)`
reference axis for mouse look and vertical movement, documented there and
in `Camera.h` as a convention scoped to that one class, not an engine-wide
definition of "up." The camera remains purely observational in Milestone 3
— it has no physics body, is not affected by gravity, and does not control
anything with a rigid body. There is still no player controller.

## Frame timing

Render-loop timing is unchanged from Milestones 1–2 (measured via
`SDL_GetPerformanceCounter`, clamped to 0.25s). Physics timing is now
separate from it — see "Simulation timing" above — which is itself an
application of the same "trustworthy elapsed time, not an assumed rate"
principle Milestone 1 established, applied to a second, independently-paced
system.

## FUTURE CONSTRAINTS PRESERVED

This section explains only how the current design avoids *unnecessarily*
blocking known future requirements. None of these are implemented yet.

- **Radial gravity / multiple gravity sources / planetary physics** — the
  `GravityField` interface already takes a `worldPosition` and its result
  is already delivered to bodies through one path
  (`PhysicsWorld::ApplyLinearAcceleration`) with no assumption about the
  vector's direction. Radial or multi-source gravity is a new class
  implementing `GravityField` alongside `FaithfulGravity`, not a change to
  `FaithfulGravity`, `PhysicsWorld`, or `Application`'s physics loop. See
  "Ownership boundary" and "Current gravity."
- **No universal up** — the only places world `+Y` means anything are
  `Camera` (observational, documented as local convention) and
  `FaithfulGravity`'s constant test vector (also documented as temporary,
  and scoped to that one implementation). Nothing in `PhysicsWorld` or
  `Renderer` treats any axis as special.
- **Moving spacecraft reference frames** — `PhysicsWorld` bodies are
  addressed by an opaque `BodyHandle` and positioned in one shared world
  space; nothing about that prevents a future frame concept from sitting
  between "a body's position" and "the position Judas hands to physics."
  It just isn't built yet.
- **Large-world rebasing** — Jolt was specifically chosen (see "Physics
  middleware") because it has a first-class path to large-coordinate
  worlds (its optional double-precision build mode) if/when this engine's
  coordinates grow past what single-precision floats represent well.
  Nothing about today's integration (plain `glm::vec3` positions in and
  out of `PhysicsWorld`) forecloses adopting that later.
- **Many terrain collision objects, raycasts/shape queries, constraints** —
  not built, but Jolt provides all of them; `PhysicsWorld`'s current
  minimal surface (create/destroy/step/query-transform) is intentionally
  small because that's all this milestone needs, not because the
  underlying middleware can't do more.

## DELIBERATELY NOT IMPLEMENTED

Explicitly deferred, not forgotten:

- Planets, spherical/radial gravity, composite/multi-source gravity, or
  any gravity-source registration system — the `GravityField` interface is
  shaped to allow these as future implementations (see "Ownership
  boundary"), but only `FaithfulGravity` exists today
- Terrain (including Terrain-ML)
- A player controller or any character physics
- Moving reference frames, floating origin, astronomical coordinates,
  spacecraft
- Physics interpolation between fixed steps (the render frame currently
  just reads the latest stepped transform; not visibly necessary yet at
  1/60s physics with vsync-capped rendering)
- Complex constraint systems, vehicles, ragdolls, destructible physics
- Jolt's debug renderer / any physics-debug-drawing (the rendered floor and
  cube are enough to prove the simulation is real; adding a debug
  wireframe overlay was judged not worth the scope for this milestone)
- Lighting, shadows, textures, materials, model loading
- Audio, networking
- Entity/component systems, scene graphs
- Editors, scripting, UI frameworks
- Vulkan (see "Windowing & Graphics API" — unchanged reasoning from
  Milestones 1–2)
- Gameplay of any kind
- Controller input (the `Action` boundary exists for this, but only
  keyboard + mouse are wired up)
- A generated OpenGL loader (glad/GLEW) — the GL surface didn't grow this
  milestone (no new GL calls were needed for physics itself), so there was
  nothing to re-evaluate.
- A physics material/property system beyond per-body friction/restitution/
  mass — the floor and cube each just get explicit, sensible values.
