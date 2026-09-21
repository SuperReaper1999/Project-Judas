# Architecture

This document explains the technical decisions behind Project Judas and how
they relate to the engine's long-term purpose. It is meant to be readable by
someone with no prior context on this project. It is updated in place as
milestones land, rather than kept as a per-milestone snapshot — see
"Milestone history" below for how to recover an earlier milestone exactly.

## What exists right now (Milestone 5)

Open a window. A large static sphere exists in 3D space with **radial**
gravity pulling toward its center. A Judas-owned player (not a Jolt
character controller of any kind — see "Player/controller ownership") falls
onto the sphere, stands on its curved surface, and can walk all the way
around it — including onto what would have been "the side" or "the
underside" from the spawn point's perspective — because the player's own
sense of "up" continuously reorients to match whichever way gravity is
currently pulling. `Space` jumps away from the local surface; gravity
brings the player back and Jolt's own collision queries detect the landing.
`R` resets the player. Nothing else. See the root `README.md` for build/run
instructions and controls.

## Milestone history

Each milestone is tagged in git so it can be recovered exactly, rather than
preserved as parallel runtime code:

- `milestone-1` — window creation, a single 2D box, keyboard movement.
- `milestone-2` — 3D rendering, perspective, depth testing, a free-flight
  camera, three static cubes.
- `milestone-3` — Jolt Physics integration, `GravityField`/`FaithfulGravity`,
  fixed-timestep simulation, a dynamic cube falling onto a static floor.
- `milestone-4` — a player standing on a flat floor, driven by Jolt's
  `CharacterVirtual`; WASD/mouse/jump; the Milestone 3 cube retained
  alongside it.

Each milestone's demo content has been replaced (not extended) by the next;
check out a tag to see or run an earlier milestone as it was.

## Language: C++

The long-term engine needs tight control over large-world coordinate math,
and integrates physics middleware (rigidbody dynamics, collision detection)
and possibly a low-level graphics API like Vulkan. The mature options in
that space (Jolt Physics, PhysX, Vulkan itself) are C or C++ libraries with
the most direct, lowest-overhead integration path in C++. Choosing C++ now
avoids introducing an interop layer later for problems we already know
we'll have.

C# was considered and is workable (e.g. via Silk.NET), but it buys nothing
here and adds friction for physics/graphics integration. There was no
technical reason to prefer it.

## Dependencies

**SDL2** — window creation, OS event handling, keyboard/mouse state, and
OpenGL context creation. A solved, commodity problem; SDL2 is mature,
widely shipped, and its scope (window/input/context, plus audio/controller
support not yet used) matches this engine's needs without pulling in
anything unnecessary.

**GLM** (added in Milestone 2) — header-only C++ `vec3`/`mat4`/`quat` math
matching GLSL's own conventions. MIT-licensed. Installed via the system
package manager (`libglm-dev`).

**Jolt Physics** (added in Milestone 3) — see "Physics middleware" below.
Milestone 5's player work is a narrower use of Jolt (low-level shape
queries instead of its character controller), not a new dependency.

**stb_image_write** (added in Milestone 5, `third_party/stb_image_write.h`)
— a single-header, dependency-free PNG/BMP/TGA/JPG/HDR writer, public
domain/MIT dual-licensed. Used only by the opt-in test harness (see
"Automated testing") to save screenshots; the normal game never calls it.
Vendored directly (one file, ~1700 lines, no transitive dependencies)
rather than fetched via CMake — writing an image file from raw pixels is
as commodity a problem as they come, and a package manager or
`FetchContent` step would add process for a single self-contained header.

## Physics middleware

**Selected: [Jolt Physics](https://github.com/jrouwe/JoltPhysics), pinned
to tag `v5.6.0`, MIT license.** Chosen in Milestone 3 over PhysX 5 mainly
for first-class large-world/double-precision support and much lower CMake
integration cost; see the `milestone-3`-era history in this file's git log
for the full comparison, unchanged by this milestone.

Rigid-body dynamics, collision detection, contact resolution, and
constraint solving are a solved, extremely hard problem that every shipped
physics-using game either licenses or spends years building. Project Judas
has no reason to re-derive any of that — see "Ownership boundary" below for
exactly what Judas keeps for itself, which is now a larger share of player
behavior than it was in Milestone 4, but still none of the underlying
collision math.

Jolt is fetched at CMake configure time via `FetchContent`, pinned to the
`v5.6.0` tag (see `CMakeLists.txt`) — not packaged by apt, not vendored
into this repository. Unchanged since Milestone 3.

## Player/controller ownership

Milestone 4 built the player on Jolt's `CharacterVirtual` — a reasonable,
purpose-built choice *for that milestone*, but one where Jolt itself
decided what "grounded" meant, drove the character's movement resolution,
and held the character's velocity as its own state. Milestone 5 moves all
of that to Judas. This section documents the resulting boundary; see
"CharacterVirtual removal" below for the before/after mapping the milestone
brief specifically asked for.

**Judas (`PlayerController`, `src/PlayerController.h/.cpp`) now owns:**

- the player's entire state — position, velocity, and orientation are
  plain `glm` fields on `PlayerController`, not a Jolt body or character
  controller of any kind;
- desired locomotion (turning WASD + look direction into a velocity);
- gravity response (integrating a sampled `GravityField` acceleration into
  vertical velocity, exactly as `PhysicsWorld::ApplyLinearAcceleration`
  does for ordinary bodies);
- local up/down (see "Orientation" below);
- jump behavior (when it's allowed, and in which direction);
- support interpretation (turning a raw geometry query into "am I
  grounded?" — see "Support");
- how movement intent is resolved against collision (a small move-and-slide
  loop it runs itself — see "Locomotion").

**Jolt (via `PhysicsWorld`) now owns only:** the underlying shape-query
math — "if this capsule moved this far in this direction, what would it
hit, and what's the surface normal there?" That's it. `PhysicsWorld`
exposes exactly this as `SweepPlayerShape` (see `PhysicsWorld.h`), plus
`CreatePlayerShape`/`DestroyPlayerShape` to manage the capsule `Shape`
itself. The player capsule is **never added to `PhysicsSystem` as a body**
— it has no `BodyID`, no layer membership, no activation state, and never
appears in a broadphase pass. It exists purely as a `JPH::Shape` that
`NarrowPhaseQuery::CastShape` is asked to sweep, on demand, whenever
`PlayerController` wants an answer.

As before, no Jolt type appears in `PhysicsWorld.h` — `ShapeSweepHit`
(the query result) is plain `glm` data.

### Why `CharacterVirtual` was right for Milestone 4 but isn't the player architecture now

`CharacterVirtual` earned its place in Milestone 4 honestly: it is Jolt's
own purpose-built, battle-tested answer to "move a character and resolve
its collisions," and using it there avoided reinventing sweep-and-slide
resolution for a milestone that wasn't testing that. What it does *not*
do is leave "what counts as gravity," "what counts as up," or "what counts
as support" as decisions outside itself — those were baked into Jolt's own
class, which Milestone 5's purpose (proving Judas, not Jolt, owns player
semantics under changing gravity) specifically requires moving out.

A dynamic rigid body (the other Jolt option) was rejected again for the
same reason as Milestone 4: ordinary rigid-body dynamics (friction/
restitution/torque from glancing contacts) are the wrong tool for
player locomotion, producing unpredictable sliding and orientation drift.

The replacement is not a general character-controller library — it is the
minimum needed to satisfy this milestone: one shape, one query function
(`SweepPlayerShape`), and a bounded (4-iteration) move-and-slide loop
inside `PlayerController::FixedUpdate`. Nothing about stairs, ledges,
multiple character shapes, or crouching was built, because nothing in this
milestone needed it.

## Ownership boundary

The foundational rule from Milestone 3, unchanged and now demonstrated more
completely by the player:

```
Judas owns gravity, reference frames, and world/large-world coordinates.

The physics middleware (Jolt) owns collision detection, contact
generation, rigid-body integration, and constraint solving — nothing more.
```

### Gravity: one interface, interchangeable implementations

`GravityField` (`src/GravityField.h`) is an abstract interface — a single
pure-virtual method, `Sample(worldPosition) -> acceleration` — and is
Judas's *only* contract for gravity. Everything that needs gravity talks to
a `GravityField&` and stays completely agnostic about which concrete
implementation is behind it: neither `PlayerController` nor `PhysicsWorld`
nor `Renderer` has ever heard of `FaithfulGravity` or `RadicalGravity` by
name.

Milestone 5 adds the second canonical implementation:

```
GravityField (interface)
    |
    +-- FaithfulGravity   (uniform/constant-direction; Milestone 3)
    |
    +-- RadicalGravity    (radial, constant-magnitude; Milestone 5)
```

**`RadicalGravity`** (`src/RadicalGravity.h/.cpp`) supplies acceleration
directed toward a configured center point, at a configured constant
magnitude:

```cpp
towardCenter = center - worldPosition
direction = normalize(towardCenter)
acceleration = direction * magnitude
```

This is deliberately not physically realistic — no inverse-square falloff,
no gravitational mass, no orbital mechanics. Milestone 5 tests that gravity
*direction* can change and everything downstream still works; it is not an
astrophysics milestone. `magnitude` is explicit per-instance configuration
(the demo uses `9.81`, comparable to `FaithfulGravity`'s constant, per the
brief), not a universal constant.

`Application::Run` (the composition root) is the one place that constructs
a concrete gravity implementation and binds it to a `GravityField&` — for
this milestone's demo, a `RadicalGravity` centered on the demo sphere. This
is a direct, deliberate test of the interface built in the Milestone 3
follow-up correction: swapping which concrete class gets constructed there
is the *entire* change needed to move the whole demo from flat, constant
gravity to radial gravity around a sphere. `PlayerController`, `PhysicsWorld`,
and `Renderer` needed zero changes to support this — only `Application.cpp`
and the two gravity implementation files changed for this to work, exactly
as the interface promised. `FaithfulGravity` itself is untouched and still
compiled into the binary — it simply isn't the implementation the composition
root selects for this milestone's active demo.

Concretely:

- Jolt's own built-in global gravity remains explicitly disabled:
  `PhysicsSystem::SetGravity(Vec3::sZero())` in `PhysicsWorld::Init`. The
  player, not being a Jolt body at all anymore, was never at risk of
  receiving Jolt gravity in the first place — but the same rule still
  governs any future dynamic body (`CreateDynamicBox` remains available and
  unchanged).
- Every fixed step, `PlayerController::FixedUpdate` samples the active
  `GravityField` at the player's own current position and integrates the
  result into its own vertical velocity — the same
  `velocity += acceleration * dt` pattern `PhysicsWorld::ApplyLinearAcceleration`
  uses for ordinary bodies, just computed inside `PlayerController` since
  there's no Jolt body for `PhysicsWorld` to apply it to.

## Support

**Gravity direction and supporting-surface normal are different concepts,
even though they coincide on today's sphere almost everywhere.** This
distinction is deliberately kept as two separately-computed values, never
collapsed into one:

- **Support/grounded state comes only from an actual geometry query.**
  Every fixed step, `PlayerController::FixedUpdate` sweeps the player's
  capsule a short distance (`kGroundProbeDistance`, `0.15m`) opposite the
  *current* local up via `PhysicsWorld::SweepPlayerShape`, and treats the
  player as grounded only if that sweep hits something whose normal is
  within Jolt's conventional walkable-slope threshold
  (`dot(normal, localUp) > 0.643`, ≈50°, matching Milestone 4's limit).
  Nothing anywhere in `PlayerController` compares the player's position or
  distance to the sphere's known center or radius — `Application.cpp` knows
  there's a sphere; `PlayerController` only ever knows "a shape query says
  I am/am not supported, and here's the normal it found."
- **Jump direction comes only from the active `GravityField`**, resampled
  fresh every fixed step (`localUp = -normalize(gravity.Sample(position))`),
  never from the ground-probe's contact normal. On this milestone's sphere,
  "away from gravity" and "the surface's contact normal" point the same way
  almost everywhere (both point radially outward from the center) — that's
  a property of a sphere with gravity centered on that same sphere, not an
  engine law. The two are computed through entirely separate calls
  (`ComputeLocalUp` from `GravityField::Sample`; `groundHit.normal` from
  `SweepPlayerShape`) specifically so a future surface where they disagree
  (an overhang, a wall, uneven terrain under radial gravity) is handled
  correctly by construction rather than by coincidence.

## Orientation

There is no universal world up. Per the active `GravityField`:

```
localUp = -normalize(gravity.Sample(playerPosition))
```

`PlayerController` keeps one persistent quaternion, `m_frameOrientation`,
whose local `+Y` axis is defined to be "the player's current up" —
`m_frameOrientation * (0, 1, 0)` — and updates it once per fixed step by
applying the *minimal* rotation that takes its current up axis to the newly
sampled `localUp` (`RotationBetweenUnitVectors`, a small shortest-arc
quaternion helper local to this file). This is applied as a rotation
*composed onto* the existing orientation, not a reset — so the player's
accumulated facing direction (which way it's been walking/turning) carries
through smoothly as it walks around the sphere and `localUp` continuously
changes, the same way a small-planet platformer character's whole body
reorients as it walks over the horizon. `+Y` here is a **local body-space
axis convention** (every 3D object needs *some* axis to call "its own up"
in local space) — it is not a claim about world space, and is exactly
mirrored by `m_frameOrientation` mapping that local axis onto whatever
`localUp` currently is in world space.

Mouse-driven yaw/pitch (`m_yaw`, `m_pitch`) are kept as separate scalars,
applied on top of `m_frameOrientation` only at the point of use (building
the look direction for the camera, and the tangent-plane forward/right
vectors for movement) — never folded into `m_frameOrientation` itself. This
keeps the two update frequencies (gravity realignment every *fixed* step;
mouse look every *render* frame) from fighting over the same piece of
state, and is why "mouse look must remain usable while local up changes":
yaw/pitch are always relative to whatever the current frame is, so they
never need to know what that frame currently equals.

The player's rendered box uses `m_frameOrientation` directly (no yaw/pitch)
— see "Player visual representation."

## Locomotion

**Movement** (`W`/`Up`, `S`/`Down`, `A`/`Left`, `D`/`Right`) is computed
each fixed step by `PlayerController::ComputeTangentVelocity`: the look
direction (frame orientation + yaw, pitch excluded so looking up/down
doesn't tilt movement off the surface) is projected onto the tangent plane
perpendicular to `localUp`, giving forward/right vectors that lie *along
the curved surface* rather than a permanently fixed world XZ plane. Walking
speed is a constant **4 m/s**.

**Jumping** (`Space`) preserves the Milestone 4 rule exactly: direction is
`-normalize(gravity.Sample(position))` (never a hard-coded axis, never
derived from the sphere), magnitude a constant **5 m/s**, permitted only
when this step's own support query says grounded (see "Support"). No
double jump, coyote time, jump buffering beyond the input latch described
below, variable height, or wall jumping — a jump attempted while airborne
is discarded, not queued for the next landing.

**Collision-aware movement**: `PlayerController` never writes `m_position`
directly from a desired velocity. Each fixed step, the desired displacement
(`velocity * fixedDeltaTime`) is resolved by a small move-and-slide loop
(up to 4 iterations): sweep the capsule along the remaining displacement
via `PhysicsWorld::SweepPlayerShape`; if nothing is hit, apply the full
displacement and stop; if something is hit, advance up to just short of it
(a `0.02m` skin margin, to avoid ending each step already touching/
overlapping a surface), remove the component of the *remaining* 
displacement that points into the hit surface (an ordinary vector
projection against the contact normal — "slide"), and repeat with what's
left. This is Judas's own resolution of the conceptual pipeline the brief
described (desired displacement → shape query → allowable movement →
contact information → Judas's interpretation → resulting position) — not a
general physics solver, just enough iterations to handle "hit one surface,
then slide into a second" without visibly sticking.

**Reset** (`R`) restores the player entirely from data `PlayerController`
already holds (`Reset()`): spawn position, zero velocity, identity frame
orientation (which the very next fixed step immediately realigns to
whatever gravity says at the spawn point, so this is not a visible
discontinuity), spawn yaw, zero pitch, and clears any pending jump request.
There is no physics-side state to reset separately — unlike Milestone 4's
Jolt-backed player, all state lives in `PlayerController` itself. Demo
`Reset` logic is allowed to know the spawn point is "above the sphere";
`PlayerController`'s own code does not know a sphere exists.

Tuning values (`src/PlayerController.cpp`, anonymous namespace): capsule
radius `0.3m`, capsule cylinder half-height `0.6m` (total capsule height
`1.8m`), eye height `0.7m` above the capsule center, move speed `4 m/s`,
jump speed `5 m/s`. All explicit, chosen for a readable demonstration, not
tuned for feel.

## Simulation timing

Physics is stepped on a **fixed timestep of 1/60 second**
(`kFixedTimestep` in `src/Application.cpp`), unchanged since Milestone 3.
`Application::Run` uses the same accumulator as before:

```
accumulator += (clamped) render-frame delta time
while accumulator >= fixedTimestep and steps-this-frame < cap:
    physicsWorld.Step(fixedTimestep)          // advances any ordinary Jolt bodies
    player.FixedUpdate(..., fixedTimestep)    // gravity, support, movement, jump
    accumulator -= fixedTimestep
```

The same two guards against an unbounded catch-up backlog remain: the
render-frame delta fed into the accumulator is clamped to `0.25s`
(`kMaxFrameDeltaTime`), and a hard cap (`kMaxPhysicsStepsPerFrame`, `8`) on
fixed steps per render frame, past which the remaining accumulated time is
dropped rather than carried forward.

### Player input vs. the fixed step

Unchanged in mechanism from Milestone 4: mouse look and the jump key are
read from `Window` at **render-frame** frequency
(`PlayerController::UpdateFrameInput`), while movement/jumping/support are
only ever resolved inside a fixed step (`PlayerController::FixedUpdate`).
The same one-shot latch prevents a `Space` tap from being lost on a render
frame that completes zero fixed steps:

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
                                      never buffered until a later landing)
```

## 3D rendering pipeline

`Renderer::DrawBox`/`Renderer::DrawSphere` implement the conventional model
→ world → view → clip-space pipeline via matrices, computed with GLM and
uploaded as uniforms to one simple shader (`src/Renderer.cpp`) — unchanged
since Milestone 3:

```glsl
gl_Position = uProjection * uView * uModel * vec4(aLocalPos, 1.0);
```

**`DrawSphere`** (new in Milestone 5) draws a second, separate mesh: a
conventional UV sphere (16 latitude × 24 longitude segments), generated
once at `Renderer::Init` time as a flat, non-indexed, position-only vertex
list — like the existing cube mesh, deliberately not using an index buffer,
so drawing it needs no GL surface beyond what `DrawBox` already uses (no
`glDrawElements`, no element buffer). Its model matrix is `translate *
scale(radius)` — no rotation parameter, since a sphere looks identical
under any rotation.

The demo sphere is drawn with the exact position and radius its Jolt
`SphereShape` was created with (`Application.cpp`'s `kSphereCenter`/
`kSphereRadius`) — there is no physics-driven transform to read back for a
static body a game never queries, so this is the one place in the renderer
that isn't reading a `PhysicsWorld::GetTransform` result, simply because
nothing in this milestone ever needs the sphere's transform to change.

### Coordinate conventions (current, local to this renderer/physics setup)

- World space is a conventional right-handed 3D space in engine-defined
  "world units," used directly as Jolt's own simulation space.
- `+Y` is used as a **local body-space** axis convention by
  `PlayerController` (see "Orientation") and as `FaithfulGravity`'s
  constant test direction — **neither is an engine-wide law**. Nothing in
  `PhysicsWorld` or `Renderer` treats world `+Y` as special; `RadicalGravity`
  demonstrates this directly by pointing in a continuously different world
  direction depending on where it's sampled.
- There is still no distinction between authoritative large-world
  coordinates and local rendering/physics coordinates — see "Future
  constraints preserved."

### Player visual representation

Unchanged reasoning from Milestone 4: the player is rendered as an
axis-aligned box sized to the capsule's bounding dimensions
(`PlayerController::GetRenderHalfExtents`), not a faithful capsule mesh —
primitive geometry is explicitly sufficient for this milestone's purpose.
Its orientation now uses `PlayerController::GetRenderOrientation()`
(`m_frameOrientation`) rather than a fixed identity, so the box visibly
reorients as the player walks around the sphere — matching the physical
capsule's own up-alignment, even though the capsule shape itself (radially
symmetric) would look the same in physics regardless.

## Depth handling

Unchanged since Milestone 2: `Renderer::Init` enables depth testing once at
startup, a 24-bit depth buffer is requested at context creation, and
`BeginFrame` clears both the color and depth buffers every frame.

## Projection handling

Unchanged in mechanism: `PlayerController::GetProjectionMatrix(aspectRatio)`
rebuilds the perspective projection every frame from the window's current
width/height, so resizing is handled automatically with no dedicated
resize-event code path.

## Camera and look controls

Unchanged in shape from Milestone 4, adapted for arbitrary orientation:
`PlayerController::GetViewMatrix` computes a fixed **third-person** offset
— an eye point `0.7m` above the player's capsule center, then a camera
position `4m` behind that eye point along the current look direction plus
`1m` extra height along `localUp`, looking in that same direction. The one
change from Milestone 4: `glm::lookAt`'s up vector is now the player's
*live* `localUp` (`m_frameOrientation * (0,1,0)`), not a fixed world `+Y` —
this is the specific change that keeps the camera from rolling incorrectly
or flipping as the player walks over the sphere's horizon. Still no
smoothing, no lag, no collision check against the world, and no
interpolation.

No free-fly debug mode exists (unchanged reasoning from Milestone 4: it
would need a mode switch, which is more camera-system scope than either
milestone calls for).

## Main loop structure

`Application::Run()` (`src/Application.cpp`) owns the loop:

```
Init Window, load GL functions, Init Renderer
Init PhysicsWorld (registers Jolt types, zeroes Jolt's own gravity)
Construct a RadicalGravity centered on the demo sphere, bound to a GravityField&
Create the static sphere body
Create PlayerController, Spawn() it (creates its capsule Shape via PhysicsWorld — no body)

while (!window.ShouldClose()):
    window.PollEvents()              // close request, Escape toggle, R/Space one-shot flags
    frameDeltaTime = measured elapsed time since last frame, clamped

    player.UpdateFrameInput(window)  // mouse look + latch jump request; every frame

    if window.ConsumeResetRequest():
        player.Reset()

    accumulator += frameDeltaTime
    while accumulator >= fixedTimestep and steps < cap:
        physicsWorld.Step(fixedTimestep)
        player.FixedUpdate(window, physicsWorld, gravity, fixedTimestep)  // see "Locomotion"
        accumulator -= fixedTimestep

    renderer.BeginFrame(...)
    renderer.SetCamera(player.GetViewMatrix(), player.GetProjectionMatrix(aspectRatio))
    renderer.DrawSphere(sphereCenter, sphereRadius, sphereColor)
    renderer.DrawBox(player.GetRenderCenter(), player.GetRenderOrientation(),
                      player.GetRenderHalfExtents(), playerColor)
    renderer.EndFrame()
    window.SwapBuffers()

player.Destroy(physicsWorld)
physicsWorld.DestroyBody(sphere); physicsWorld.Shutdown()
renderer.Shutdown()
// Window's destructor tears down the GL context, the window, and SDL itself.
```

The Milestone 3/4 flat floor and dynamic cube are not part of this
milestone's active demo (recoverable via the `milestone-4` tag) — a flat
floor under `FaithfulGravity` and a sphere under `RadicalGravity` active in
the same scene would need two different simultaneous gravity fields, which
contradicts "the composition root selects one implementation," so keeping
them would have meant either an incoherent scene or building
multi-source/composite gravity that this milestone explicitly excludes.
`PhysicsWorld::CreateStaticBox`/`CreateDynamicBox` remain in the engine,
unused by this milestone's demo but not removed — general capability, not
demo-specific.

The engine-level split, updated for this milestone:

- `Window` — window/input.
- `Renderer` — graphics, now including sphere geometry.
- `PhysicsWorld` — physics: ordinary Jolt bodies (unused by this
  milestone's active demo but retained) plus the player's collision-query
  surface (`CreatePlayerShape`/`SweepPlayerShape`/`DestroyPlayerShape` —
  replacing Milestone 4's `CharacterVirtual`-backed methods).
- `GravityField` / `FaithfulGravity` / `RadicalGravity` — Judas's gravity
  interface and its two implementations.
- `PlayerController` — now owns the player's entire state and behavior
  (position, velocity, orientation, locomotion, support, jumping, camera)
  — see "Player/controller ownership."
- `Application` — wires the above together and owns the loop.

## Input handling

Unchanged since Milestone 4: the `Action` enum (`MoveForward`/
`MoveBackward`/`StrafeLeft`/`StrafeRight`, bound to `W`/`Up`, `S`/`Down`,
`A`/`Left`, `D`/`Right`) describes locomotion intent relative to the
player's current look direction; `Escape` toggles mouse capture; `R`
(`ConsumeResetRequest`) and `Space` (`ConsumeJumpRequest`) are discrete,
edge-triggered one-shot requests, not continuously-polled actions.
`R` now resets only the player (there is no cube in this milestone's active
demo to also reset).

## Frame timing

Unchanged since Milestone 1 (measured via `SDL_GetPerformanceCounter`,
clamped to 0.25s). Physics timing remains separate — see "Simulation
timing."

## CharacterVirtual removal

What `CharacterVirtual` previously provided (Milestone 4): collision-aware
movement resolution (sweep against the world and stop/slide on contact),
ground/support detection (`GetGroundState()`, `GetGroundNormal()`), slope
classification (its own `mUp`/`mMaxSlopeAngle` settings), and ownership of
the character's velocity as its own internal state.

What Judas now owns instead (Milestone 5), in `PlayerController`: the
player's position, velocity, and orientation as plain data; the decision of
what velocity the player should have each step (gravity integration,
WASD-driven tangent movement, jump); the interpretation of "grounded" from
a raw geometry query; and the move-and-slide iteration itself.

The low-level Jolt facility that replaced `CharacterVirtual`:
`JPH::NarrowPhaseQuery::CastShape`, wrapped as `PhysicsWorld::SweepPlayerShape`
— a single function answering "how far can this shape move, and what does
it touch?" `PlayerController` calls it twice per fixed step (once for the
ground probe, up to four times for move-and-slide) and makes every
interpretive decision about the results itself.

**Remaining `CharacterVirtual` dependency: none.** `grep -rn CharacterVirtual
src/` (checked as part of this milestone's own validation) finds exactly
one hit, a comment in `PlayerController.h` contrasting this milestone's
architecture with Milestone 4's for a reader's benefit — no include, no
type, no call. No Jolt character-controller header is included anywhere in
this repository as of this milestone.

## Universal-up audit

Every `+Y`/world-up-shaped assumption touched by this milestone's changed
files, reported honestly rather than selectively:

- **`PlayerController`'s local body-space `+Y`** (`m_frameOrientation *
  (0,1,0)` in several places, and the axis used to build
  `RotationBetweenUnitVectors`'s fallback for the 180°-apart case): this is
  the local-space "which way is my own model's up" convention every 3D
  object needs *some* fixed answer to — not a world-space assumption. It is
  exactly what `m_frameOrientation` exists to map onto the real,
  continuously-changing world `localUp`. Removing it isn't meaningful: some
  local axis has to represent "up" in the player's own body space.
- **`PlayerController::ComputeTangentVelocity`'s yaw rotation axis**
  (`glm::angleAxis(yaw, (0,1,0))`, applied to `m_frameOrientation`, not to
  world space directly): rotates *within* the current local frame, so it
  correctly turns the player relative to whatever `localUp` currently is —
  not a hard-coded world axis.
- **The arbitrary perpendicular-axis pick used only for the 180°-apart edge
  case in `RotationBetweenUnitVectors`**: never triggered by this
  milestone's gradual gravity changes (it would require gravity to reverse
  completely within a single 1/60s step), but present as a defensive
  fallback so the function never returns a NaN rotation. It's an arbitrary
  perpendicular choice, not an "up" assumption.
- **Not present, checked for explicitly**: no `player.y <= floorHeight`-style
  height comparison; no `distance(player, sphereCenter) <= radius +
  margin`-style grounding; no reference to the sphere's center/radius/any
  demo-specific geometry anywhere in `PlayerController.h/.cpp` or
  `PhysicsWorld.h` (confirmed by `grep`); no capsule axis permanently fixed
  to world `+Y` (the capsule shape itself is created once and never
  rotated in Jolt's own frame — `PlayerController` supplies its *current*
  `m_frameOrientation` to every `SweepPlayerShape` call, so the query
  itself always uses the up-to-date orientation).
- **`Renderer`'s model-space `+Y`** (cube/sphere mesh vertices, `DrawBox`'s
  scale/rotate/translate order): ordinary modeling-space convention,
  transformed by whatever rotation the caller supplies — not a world-up
  assumption, unrelated to gravity or locomotion, out of this audit's scope
  because it isn't a gravity-aware code path.

## Automated testing

Claude Code (the AI agent developing this engine alongside its human
maintainer) has no way to see Judas's real window or send it keyboard/mouse
input in this environment — there is no desktop-control tool available, and
the window's pixels aren't reachable through the usual screen-capture route
under a Wayland session either. Milestone 5's validation split accordingly:
Claude verified the build, the architecture, and gravity's own behavior in
isolation, but every claim about how the player actually behaves on screen
had to wait on the human operator.

`src/TestHarness.h/.cpp` closes most of that gap, permanently, for every
milestone after this one — it is developer/automation tooling, not part of
the game itself, and the normal interactive loop never touches it.

**What it does:** when the `JUDAS_TEST_SCRIPT` environment variable is set
to a script file's path, `Application::Run` creates the window *hidden*
(`Window::Init`'s `visible` parameter — a real GL context, just not shown
on screen) and hands off to `RunTestHarness` instead of the interactive
loop. The harness runs a fixed number of physics steps as fast as
possible — no real-time pacing, no vsync wait — driving `Window`'s input
from the script (`Window::SetTestActionState`/`QueueTestMouseDelta`/
`RequestTestJump`/`RequestTestReset`, all gated behind
`SetTestInputMode(true)` so they touch nothing in the normal SDL input
path) instead of a real keyboard/mouse. It prints one CSV line of player
state (position, local up, grounded, velocity) per step to stdout, and, at
any step the script names, renders the actual scene (through a `drawScene`
callback `Application` supplies, since the harness itself has no idea what
a "sphere" or a "player box" is) and writes it to a PNG via
`Renderer::CaptureFrame` + the vendored `stb_image_write.h` (public
domain/MIT, `third_party/stb_image_write.h` — writing images is a solved
commodity problem, and this single dependency-free header was preferred
over inventing a PNG encoder or adding a fetched dependency for something
this small).

The script format (one directive per line, `#` for comments) is
deliberately minimal — `STEPS`, `LOG_EVERY`, `HOLD <key> <from> <to>`,
`LOOK <dx> <dy> <atStep>`, `TAP <SPACE|R> <atStep>`, `SCREENSHOT <atStep>
<file>` — exactly the primitives needed to script a walk/jump/look
sequence, not a general input-recording or automation language.

**It already found a real bug the first time it was used.** Milestone 5's
initial jump implementation set the player's vertical velocity to
`kJumpSpeed` on a grounded step, but the very next fixed step's ground
probe (`kGroundProbeDistance = 0.15m`) reached further than that single
step's outward travel at jump speed (`kJumpSpeed * fixedDeltaTime ≈
0.083m`), so the probe immediately "found" the surface again and the
grounded logic reset vertical speed back to zero — the jump collapsed into
a barely-visible ~0.08m hop instead of a real arc. The state log made this
unambiguous in a way a screenshot alone would not have (a 0.08m hop and a
1.3m jump can look similar from some camera angles): `grounded` never
dropped to `0` after the tap, and the distance-from-surface for the next
several logged steps was far smaller than a 5 m/s launch should produce.
The fix — a ground hit only counts as support if the player wasn't already
moving away from the surface as of last step's velocity
(`PlayerController::FixedUpdate`'s `wasAscending` check) — is the standard
solution to this class of bug in any step-based character controller. This
matched what the operator had independently noticed by hand, but the
harness found and pinpointed it first, without anyone needing to watch the
window.

**What it deliberately is not:** a general test framework, a CI system, a
replacement for the operator's own interactive pass, or a visual-diffing
tool. It has no assertions of its own — reading the CSV output and looking
at the PNGs is still a human (or Claude) judgment call, the same as
reading any other diagnostic log. Extending it (new script directives,
automatic pass/fail checks against expected values) should wait until a
specific future milestone actually needs that, not be built speculatively
now.

## FUTURE CONSTRAINTS PRESERVED

This section explains only how the current design avoids *unnecessarily*
blocking known future requirements. None of these are implemented yet.

- **Multiple/composite gravity sources, true planetary systems** — the
  `GravityField` interface and its two current implementations
  (`FaithfulGravity`, `RadicalGravity`) already prove that a consumer
  needs zero changes when the concrete gravity implementation changes. A
  future composite field is a third implementation, not a rework of the
  interface or its consumers.
- **No universal up, confirmed under an actually-changing gravity
  direction** — Milestone 3/4 only asserted this was possible; Milestone 5
  demonstrates it: `PlayerController` visibly reorients as `RadicalGravity`'s
  sampled direction changes continuously while walking around the sphere,
  using no hard-coded axis for that reorientation.
- **Moving spacecraft reference frames** — unchanged reasoning from
  Milestone 4: `PhysicsWorld` bodies are addressed by opaque handles in one
  shared world space; a future frame concept can sit between "an object's
  position" and "the position Judas hands to physics" without requiring
  today's code to be undone.
- **Large-world rebasing** — unchanged: Jolt's optional double-precision
  build mode remains available if/when coordinates grow past what
  single-precision floats represent well.
- **Terrain, many collision objects, raycasts/shape queries, constraints**
  — Milestone 5 is itself evidence this is practical: the player's entire
  support/movement system is built from exactly one Jolt query primitive
  (`CastShape`) used twice differently. Terrain collision or more query
  types are a matter of calling more of what Jolt already provides through
  the same `PhysicsWorld` boundary, not new architecture.

## DELIBERATELY NOT IMPLEMENTED

Explicitly deferred, not forgotten:

- A custom general collision engine or replacement rigid-body solver —
  `PhysicsWorld::SweepPlayerShape` is a thin wrapper around one Jolt query
  function; Jolt still does 100% of the actual collision math
- Multiple gravity sources, composite gravity, gravity blending,
  gravity-source registration, sphere-of-influence systems
- Inverse-square/realistic gravity, gravitational mass, orbital mechanics,
  celestial simulation — `RadicalGravity` is constant-magnitude by design
- Planets as an engine subsystem, planetary rotation, spherical terrain,
  terrain chunks, LOD, procedural terrain, oceans, atmosphere, Terrain-ML —
  the Milestone 5 sphere exists to prove architecture, not as the start of
  a planet system
- Moving reference frames, spacecraft, floating origin, astronomical
  coordinates
- A general gameplay/entity framework, ECS, or scene graph — one
  `PlayerController` for one player is enough
- Physics interpolation between fixed steps
- Character physics beyond the move-and-slide loop this milestone needed:
  no ragdolls, no skeletal physics, no complex constraint systems,
  vehicles, or destructible physics
- Stair-climbing, ledge grabs, or any locomotion beyond walking/jumping on
  a convex surface — not intrinsically required by this milestone
- Sprinting, crouching, stamina, acceleration curves, movement states,
  character models, skeletal animation, or any animation system
- Double jump, coyote time, jump buffering beyond the render-frame-to-
  fixed-step input latch, variable jump height, air dashing, wall jumping
- Cinematic cameras, camera collision, camera shake/smoothing frameworks,
  multiple gameplay camera modes, or a free-fly debug camera
- Jolt's debug renderer / any physics-debug-drawing
- Lighting, shadows, textures, materials, model loading
- Audio, networking, NPCs, AI, inventory, weapons, health, interaction
  systems
- Editors, scripting, UI frameworks
- Vulkan (unchanged reasoning from Milestones 1–2)
- Gameplay of any kind
- Controller input (the `Action` boundary exists for this, but only
  keyboard + mouse are wired up)
- A generated OpenGL loader (glad/GLEW) — the GL surface didn't grow this
  milestone; `DrawSphere` reuses the exact same GL calls `DrawBox` already
  used
- A physics material/property system beyond per-body friction/restitution —
  the sphere gets explicit, sensible values; the player, having no Jolt
  body, has none to configure
