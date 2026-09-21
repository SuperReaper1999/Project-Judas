# Architecture

This document explains the technical decisions behind Project Judas and how
they relate to the engine's long-term purpose. It is meant to be readable by
someone with no prior context on this project. It is updated in place as
milestones land, rather than kept as a per-milestone snapshot — see
"Milestone history" below for how to recover an earlier milestone exactly.

## What exists right now (Milestone 4)

Open a window. A static floor and a Milestone 3 dynamic cube still exist
and still fall/settle under real physics. A controllable player — a
capsule-shaped character controller — stands on the floor: WASD walks it
(relative to where it's looking), the mouse looks around, `Space` jumps
(only while grounded, in a direction derived from the active gravity, not
a hard-coded axis), and gravity brings it back down to a real physical
landing. `R` resets both the cube and the player. Nothing else. See the
root `README.md` for build/run instructions and controls.

## Milestone history

Each milestone is tagged in git so it can be recovered exactly, rather than
preserved as parallel runtime code:

- `milestone-1` — window creation, a single 2D box, keyboard movement.
- `milestone-2` — 3D rendering, perspective, depth testing, a free-flight
  camera, three static cubes.
- `milestone-3` — Jolt Physics integration, `GravityField`/`FaithfulGravity`,
  fixed-timestep simulation, a dynamic cube falling onto a static floor.

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

## Player/controller representation

**Selected: Jolt's `CharacterVirtual`**, wrapped by a handful of methods on
`PhysicsWorld` (`CreatePlayer`, `SetPlayerVelocity`, `UpdatePlayer`,
`GetPlayerPosition`, `GetPlayerGroundContact`, `ResetPlayer`), driven by a
new `PlayerController` (`src/PlayerController.h/.cpp`).

### Why `CharacterVirtual` and not a dynamic rigid body

Jolt offers two ways to represent a controllable character: a full dynamic
`Body` (like the Milestone 3 cube, just capsule-shaped and rotation-locked),
or `CharacterVirtual` — a kinematic controller that isn't tracked by
`PhysicsSystem` at all and instead does its own collision-aware sweep-and-
resolve each time the caller explicitly calls `Update()`.

A dynamic rigid body was rejected for the player specifically because
ordinary rigid-body dynamics (friction, restitution, torque from
glancing contacts) are the wrong tool for player locomotion — they produce
exactly the kind of unpredictable sliding, snagging, and orientation
drift that makes rigid-body characters feel bad to control, which is a
solved problem `CharacterVirtual` exists to avoid: the caller sets a
*desired* velocity every step and `Update()` moves the character that far
unless collision blocks it, reporting clean ground-contact state
(`GetGroundState()`, `GetGroundNormal()`) instead of leaving the caller to
infer support from contact/collision callbacks. This is Jolt's own
purpose-built answer to exactly what this milestone needs (physical
collision, grounded locomotion, falling, jumping, stable floor contact) —
not a generic framework being built for its own sake, and not a choice
made because "mature engines commonly do this": it was chosen because
reading its actual API (`Jolt/Physics/Character/CharacterVirtual.h`) shows
it solves this exact problem with less code and fewer footguns than
driving a dynamic body would.

Only the plain `Update()` call is used, not `ExtendedUpdate()` (which adds
stair-stepping and floor-sticking on top). Milestone 4's flat floor has no
stairs or steps to climb, so that extra machinery would be speculative.

### What Judas owns vs. what Jolt owns, for the player specifically

This mirrors the general ownership boundary below, applied one level
deeper:

- **Jolt (`CharacterVirtual`) owns**: collision-aware movement resolution,
  contact/ground detection, and slope classification (its own `mUp` /
  `mMaxSlopeAngle` settings — used only to decide "is this surface a floor
  or a wall," a controller implementation detail, not gravity).
- **`PhysicsWorld` owns**: translating that Jolt-specific API into plain
  `glm` types and semantic calls, exactly like it does for ordinary bodies.
  No Jolt type appears in `PhysicsWorld.h`.
- **`PlayerController` owns**: input intent (WASD, mouse, jump key),
  deciding what velocity the player *should* have this step, and reading
  back where physics put it for the camera/render. It never touches Jolt
  directly and never contains a gravity constant of its own — every
  acceleration it uses comes from a `GravityField&` passed in.
- **`Application::Run`** remains the composition root wiring all of the
  above together, exactly as for the cube and floor.

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
(`Application::Run`'s cube-gravity step, and — new in Milestone 4 —
`PlayerController::FixedUpdate`) talks to a `GravityField&` and stays
completely agnostic about which concrete implementation is behind it.
`PlayerController` never queries `FaithfulGravity` directly, never hard-codes
`(0, -9.81, 0)`, and receives the interface reference as a parameter rather
than owning or constructing one itself.

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

## Ground/support semantics

**Gravity direction and supporting-surface normal are different concepts,
even though they coincide on today's flat floor.** This distinction is
deliberately kept visible in the code rather than collapsed into one
vector:

- **Support/grounded state comes only from the physics controller's own
  contact information** — `PlayerGroundContact` (`PhysicsWorld.h`), backed
  by Jolt's `CharacterVirtual::GetGroundState()`/`GetGroundNormal()`. A
  jump is permitted only when `PlayerGroundContact::isGrounded` is true.
  Nothing in `PlayerController` or `PhysicsWorld` ever asks "is the
  player's Y coordinate close to some known floor height" — that would
  hard-code knowledge of this one demo floor's placement into player logic,
  which is exactly what's avoided (moving `kFloorPosition` in
  `Application.cpp` requires touching nothing about the player).
- **Jump direction comes only from the active `GravityField`**, resampled
  fresh every fixed step in `PlayerController::FixedUpdate`
  (`up = -normalize(gravity.Sample(position))`), not from
  `PlayerGroundContact::normal` and not from a hard-coded `+Y`. On a flat
  floor with `FaithfulGravity`, "away from gravity" and "the floor's
  contact normal" happen to be the same direction — that's a property of
  today's test geometry, not an engine law, and the code computes them
  through entirely separate paths so that remains true even though the
  numbers currently agree.
- The one place a fixed `+Y` axis *is* used as a convenience is
  `PlayerController::ComputeHorizontalVelocity`'s yaw/pitch-to-movement
  math (matching Milestone 2/3's `Camera` precedent) — documented there as
  this milestone's convention, not reused for anything support- or
  gravity-related.

## Locomotion

**Movement** (`W`/`Up`, `S`/`Down`, `A`/`Left`, `D`/`Right`) is expressed
each fixed step as a desired horizontal velocity
(`PlayerController::ComputeHorizontalVelocity`, walking speed **4 m/s**,
relative to the player's current look yaw) and handed to
`CharacterVirtual` via `PhysicsWorld::SetPlayerVelocity` before
`PhysicsWorld::UpdatePlayer` resolves it against the world — never a direct
transform write, so it can't bypass collision.

**Jump** (`Space`) only takes effect when
`PlayerGroundContact::isGrounded` is true at the moment a fixed step
consumes the request, imparting **5 m/s** away from the gravity direction
sampled at that instant (see "Ground/support semantics"). No double jump,
coyote time, jump buffering, or variable height: a jump attempted while
airborne is simply discarded, not queued for the next landing.

**Reset** (`R`) restores both the Milestone 3 cube (`PhysicsWorld::ResetBody`)
and the player (`PlayerController::Reset` → `PhysicsWorld::ResetPlayer`,
which sets the feet position and zeroes velocity, plus resets the
controller's own yaw/pitch/pending-jump state) to their spawn conditions.

Tuning values (`src/PlayerController.cpp`, anonymous namespace): capsule
radius `0.3m`, capsule cylinder half-height `0.6m` (total height `1.8m`),
mass `70kg` (also `CharacterVirtualSettings`' own default), move speed
`4 m/s`, jump speed `5 m/s`, eye height `1.6m`. All explicit, chosen for a
readable demonstration, not tuned for feel.

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

### Player input vs. the fixed step

Mouse look and the jump key are read from `Window` at **render-frame**
frequency (`PlayerController::UpdateFrameInput`, called once per iteration
of the loop, outside the accumulator's `while`), because input should feel
responsive regardless of how physics happens to be paced that frame.
Movement and jumping themselves, however, are only ever resolved inside a
fixed step (`PlayerController::FixedUpdate`, called once per accumulator
iteration) — so the player's motion stays governed by the same trustworthy,
frame-rate-independent timestep as every other physics body, and input
sampling frequency never leaks into how far or how fast the player actually
moves.

This split creates one real hazard: a render frame can complete zero fixed
steps (if it runs faster than 1/60s and the accumulator hasn't filled up
yet), so a `Space` tap sampled that frame could otherwise vanish before any
`FixedUpdate` call ever sees it. The fix is the smallest mechanism that
solves exactly this and nothing more — no generic input-command buffer:

```
Window::ConsumeJumpRequest()       — one-shot: true on the render frame the
                                      key transitioned down, then clears
        |
        v
PlayerController::UpdateFrameInput — drains it into m_jumpRequested, which
                                      persists across render frames if needed
        |
        v
PlayerController::FixedUpdate      — consumes m_jumpRequested unconditionally
                                      (grounded: jumps; airborne: discards —
                                      see "Ground/support semantics" — never
                                      buffered until a later landing)
```

## Current gravity

`FaithfulGravity::Sample` returns a constant `(0, -9.81, 0)` for every
position. **This is configuration/test data for `FaithfulGravity`, not an
engine-wide definition of gravity or of "down."** Nothing about the
vector's direction or magnitude is assumed anywhere outside
`FaithfulGravity.cpp` — `PhysicsWorld` takes whatever acceleration the
active `GravityField` implementation produces and applies it, without
interpreting it, and (new in Milestone 4) `PlayerController` does the same
for the player's vertical velocity and jump direction. A future
radial/planetary gravity implementation is expected to be a new class
implementing `GravityField` (see "Gravity: one interface, interchangeable
implementations" above) that actually uses its `worldPosition` argument,
without changing anything about how a physics body — or the player —
*receives* gravity.

## 3D rendering pipeline

`Renderer::DrawBox` implements the conventional model → world → view →
clip-space pipeline entirely via matrices, computed with GLM and uploaded
as uniforms to a single, simple shader (`src/Renderer.cpp`):

```
local (unit cube, [-0.5, 0.5] per axis)
    --(uModel: translate * rotate * scale, from position/rotation/halfExtents)-->
world coordinates
    --(uView: PlayerController::GetViewMatrix(), a glm::lookAt)-->
camera/view coordinates
    --(uProjection: PlayerController::GetProjectionMatrix(), a glm::perspective)-->
clip space / screen
```

The vertex shader is exactly:

```glsl
gl_Position = uProjection * uView * uModel * vec4(aLocalPos, 1.0);
```

Milestone 3 added rotation and non-uniform scale to the model matrix
(`glm::translate(position) * glm::mat4_cast(rotation) * glm::scale(halfExtents * 2)`)
so different boxes can have different sizes and so a body's rendered
orientation can come from physics — both were previously translation-only.
The floor and cube are both drawn with the **same transform their physics
body reports** (`PhysicsWorld::GetTransform`); there is no separate
rendering-side position/rotation for either of them, and no duplicated
movement logic between simulation and rendering. The player (see "Player
visual representation" below) is the one exception to exact transform
matching, for a documented reason.

### Coordinate conventions (current, local to this renderer/physics setup)

- World space is a conventional right-handed 3D space in engine-defined
  "world units," used directly as Jolt's own simulation space (no unit
  conversion between Judas and the physics middleware yet).
- `+Y` is used as "up" by `PlayerController` (its camera math and its
  horizontal-movement basis — see "Ground/support semantics") and by
  `FaithfulGravity`'s constant test vector. **Neither is an engine-wide
  law** — see "Current gravity," "Ground/support semantics," and "Future
  constraints preserved."
- There is still no distinction between authoritative large-world
  coordinates and local rendering/physics coordinates — see "Future
  constraints preserved."

### Player visual representation

The player is rendered as an axis-aligned box sized to the capsule
collider's bounding dimensions (`PlayerController::GetRenderHalfExtents`:
`(radius, halfCylinderHeight + radius, radius)`), **not a faithful capsule
mesh** — unlike the floor and cube, this is an approximation of the actual
collision shape, not an exact match. Building real capsule geometry
(hemispherical end caps, tessellation) is real extra code for a
demonstration that only needs a visible, debuggable stand-in for "where the
physical player is"; primitive geometry is explicitly sufficient for this
milestone's purpose. The box uses the player's actual physics position
(feet position + half-height offset) and an identity rotation — the capsule
itself never visibly rotates, so there is nothing to lose by not tracking
orientation here.

## Depth handling

Unchanged from Milestone 2: `Renderer::Init` enables depth testing once at
startup (`glEnable(GL_DEPTH_TEST)`, `glDepthFunc(GL_LESS)`), a 24-bit depth
buffer is requested at context creation, and `BeginFrame` clears both the
color and depth buffers every frame.

## Projection handling

Unchanged in mechanism from Milestone 2 — now on `PlayerController`:
`GetProjectionMatrix(aspectRatio)` rebuilds the perspective projection
every frame from the window's current width/height, so resizing is handled
automatically with no dedicated resize-event code path.

## Camera and look controls

Milestone 2's standalone free-flight `Camera` has been removed (recoverable
via the `milestone-2`/`milestone-3` tags) in favor of a camera attached to
the player, since flying the camera independently of the character it's
meant to help you inspect would need a mode switch — additional
camera-system scope this milestone deliberately avoids.

`PlayerController::GetViewMatrix` computes a fixed **third-person** offset:
an eye point `1.6m` above the player's feet, then a camera position `4m`
behind that eye point along the current look direction plus `1m` extra
height, looking in that same direction
(`glm::lookAt(cameraPosition, cameraPosition + front, worldUp)`). This is
recomputed directly from the player's live physics position and the
current yaw/pitch every frame — no smoothing, no lag, no collision check
against the world, and no interpolation. It was chosen over a strict
first-person view specifically so the player's own capsule (box
approximation) stays visible, which is what makes it useful for seeing the
physical body actually fall, land, and stand rather than only inspecting
the world from inside it.

Mouse movement changes yaw/pitch (`PlayerController::UpdateFrameInput`),
which drives both this camera's look direction *and* which way WASD
currently walks (see "Locomotion") — but looking around never writes to
the player's physics position or velocity; it only changes numbers read
back when computing the view matrix and the movement basis. A minimal
free-fly debug mode was not retained: it would need input to route to two
different consumers (camera vs. player) with a mode toggle, which is
already more camera-system machinery than this milestone calls for.

## Main loop structure

`Application::Run()` (`src/Application.cpp`) owns the loop:

```
Init Window, load GL functions, Init Renderer
Init PhysicsWorld (registers Jolt types, zeroes Jolt's own gravity)
Create a FaithfulGravity, bound to a GravityField& (see "Ownership boundary")
Create the static floor body and the dynamic cube body
Create PlayerController, Spawn() it (creates its CharacterVirtual via PhysicsWorld)

while (!window.ShouldClose()):
    window.PollEvents()              // close request, Escape toggle, R/Space one-shot flags
    frameDeltaTime = measured elapsed time since last frame, clamped

    player.UpdateFrameInput(window)  // mouse look + latch jump request; every frame

    if window.ConsumeResetRequest():
        physicsWorld.ResetBody(cube, initialPosition, initialRotation)
        player.Reset(physicsWorld)

    accumulator += frameDeltaTime
    while accumulator >= fixedTimestep and steps < cap:
        acceleration = gravity.Sample(cube's current position)  // through the GravityField interface
        physicsWorld.ApplyLinearAcceleration(cube, acceleration, fixedTimestep)
        physicsWorld.Step(fixedTimestep)

        player.FixedUpdate(window, physicsWorld, gravity, fixedTimestep)  // see "Locomotion"

        accumulator -= fixedTimestep

    floorTransform = physicsWorld.GetTransform(floor)
    cubeTransform  = physicsWorld.GetTransform(cube)

    renderer.BeginFrame(...)
    renderer.SetCamera(player.GetViewMatrix(physicsWorld), player.GetProjectionMatrix(aspectRatio))
    renderer.DrawBox(floorTransform..., floorHalfExtents, floorColor)
    renderer.DrawBox(cubeTransform...,  cubeHalfExtents,  cubeColor)
    renderer.DrawBox(player.GetRenderCenter(physicsWorld), identity, player.GetRenderHalfExtents(), playerColor)
    renderer.EndFrame()
    window.SwapBuffers()

player.Destroy(physicsWorld)
physicsWorld.DestroyBody(cube); physicsWorld.DestroyBody(floor); physicsWorld.Shutdown()
renderer.Shutdown()
// Window's destructor tears down the GL context, the window, and SDL itself.
```

The engine-level split, updated for this milestone:

- `Window` — window/input.
- `Renderer` — graphics.
- `PhysicsWorld` — physics, including the player's `CharacterVirtual` (new
  player surface in Milestone 4; body-based physics unchanged since
  Milestone 3).
- `GravityField` / `FaithfulGravity` — Judas's own gravity interface and
  its uniform-gravity implementation (Milestone 3).
- `PlayerController` — input intent, locomotion decisions, and the
  player's camera (new in Milestone 4; replaces `Camera`).
- `Application` — wires the above together and owns the loop, including
  the physics accumulator.

## Input handling

The `Action` enum (`Window.h`) now describes grounded locomotion, not
free-flight: `MoveForward`/`MoveBackward`/`StrafeLeft`/`StrafeRight`, bound
to `W`/`Up`, `S`/`Down`, `A`/`Left`, `D`/`Right` — unchanged bindings, but
now interpreted relative to the player's look direction (see "Locomotion")
rather than a flying camera's. `Ascend`/`Descend` (`Space`/`Left Ctrl`) were
removed: nothing consumes them any more now that the free camera is gone,
and a grounded player doesn't have a "descend" concept.

`Escape` still toggles mouse capture, unchanged from Milestone 2/3.

Two **discrete, one-shot** requests exist alongside the continuously-polled
`Action`s, each its own edge-triggered flag on `Window` (drained by
`Consume...Request()`, matching the existing `ConsumeResetRequest`
pattern) rather than folded into the action-state model, because they're
one-time events, not held directions:

- **`R`** — `ConsumeResetRequest()`. `Application::Run` resets both the
  cube (`PhysicsWorld::ResetBody`) and the player
  (`PlayerController::Reset`).
- **`Space`** — `ConsumeJumpRequest()`. Latched by `PlayerController` and
  consumed by the next fixed step — see "Simulation timing," "Player input
  vs. the fixed step."

## Frame timing

Render-loop timing is unchanged from Milestones 1–3 (measured via
`SDL_GetPerformanceCounter`, clamped to 0.25s). Physics timing remains
separate from it — see "Simulation timing" above, including this
milestone's addition of how player input specifically interacts with the
fixed step.

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
  `PlayerController` (its camera math and horizontal-movement basis,
  documented as local convention) and `FaithfulGravity`'s constant test
  vector (also documented as temporary, scoped to that one implementation).
  Nothing in `PhysicsWorld` or `Renderer` treats any axis as special, and
  the parts of `PlayerController` that *can* correctly derive direction
  from live gravity (vertical integration, jump) already do — see "Ground/
  support semantics."
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
  minimal surface (create/destroy/step/query-transform, plus the player
  surface added this milestone) is intentionally small because that's all
  each milestone needed, not because the underlying middleware can't do
  more.
- **Planetary character locomotion** — `PlayerController` never conflates
  "opposite gravity direction" with "ground contact normal" (see "Ground/
  support semantics"); a future planet's radial gravity and a slope's
  actual contact normal disagreeing with each other is exactly the case
  this milestone's code already keeps as two separate values, even though
  today's flat floor makes them numerically identical.

## DELIBERATELY NOT IMPLEMENTED

Explicitly deferred, not forgotten:

- Planets, spherical/radial gravity, composite/multi-source gravity, or
  any gravity-source registration system — the `GravityField` interface is
  shaped to allow these as future implementations (see "Ownership
  boundary"), but only `FaithfulGravity` exists today
- Terrain (including Terrain-ML)
- Moving reference frames, floating origin, astronomical coordinates,
  spacecraft
- A general gameplay/entity framework, ECS, or scene graph — one
  `PlayerController` for one player is enough
- Physics interpolation between fixed steps (the render frame currently
  just reads the latest stepped transform; not visibly necessary yet at
  1/60s physics with vsync-capped rendering)
- Character physics beyond `CharacterVirtual`'s own collision/ground
  detection: no ragdolls, no skeletal physics, no complex constraint
  systems, vehicles, or destructible physics
- Stair-climbing / `CharacterVirtual::ExtendedUpdate` — plain `Update()`
  is sufficient for a flat floor; adding it isn't intrinsically required by
  anything this demonstration proves
- Sprinting, crouching, stamina, acceleration curves, movement states,
  character models, skeletal animation, or any animation system
- Double jump, coyote time, jump buffering, variable jump height, air
  dashing, wall jumping — one reliable jump is the whole requirement
- Cinematic cameras, camera collision, camera shake/smoothing frameworks,
  or multiple gameplay camera modes (see "Camera and look controls")
- Jolt's debug renderer / any physics-debug-drawing (the rendered floor,
  cube, and player box are enough to prove the simulation is real; adding
  a debug wireframe overlay was judged not worth the scope for this
  milestone)
- Lighting, shadows, textures, materials, model loading
- Audio, networking, NPCs, AI, inventory, weapons, interaction systems
- Editors, scripting, UI frameworks
- Vulkan (see "Windowing & Graphics API" — unchanged reasoning from
  Milestones 1–2)
- Gameplay of any kind
- Controller input (the `Action` boundary exists for this, but only
  keyboard + mouse are wired up)
- A generated OpenGL loader (glad/GLEW) — the GL surface didn't grow this
  milestone (no new GL calls were needed for the player itself, only new
  Jolt API surface), so there was nothing to re-evaluate.
- A physics material/property system beyond per-body friction/restitution/
  mass — the floor, cube, and player each just get explicit, sensible
  values.
