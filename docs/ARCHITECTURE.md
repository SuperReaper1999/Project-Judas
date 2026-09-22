# Architecture

This document explains the technical decisions behind Project Judas and how
they relate to the engine's long-term purpose. It is meant to be readable by
someone with no prior context on this project. It is updated in place as
milestones land, rather than kept as a per-milestone snapshot — see
"Milestone history" below for how to recover an earlier milestone exactly.

## What exists right now (Milestone 7-Final)

Open a window. Two independent static spheres ("planets," radius `20m`
each, centers `55m` apart — see "Physics test world") exist in 3D space,
each with its own **radial** gravity pulling toward its own center. A flat
static plank connects them near their facing surfaces. A Judas-owned
player (not a physics-middleware character controller of any kind — see
"Player/controller ownership") falls onto Planet A, stands on its curved
surface, and can walk all the way around it — because the player's own
sense of "up" continuously reorients to match whichever way gravity is
currently pulling — then walks onto the plank, across it, and onto Planet
B, and back, with gravity handing off coherently at each boundary. `Space`
jumps; `R` resets the player. Ordinary locomotion is visually smooth: the
fixed-step simulation is unchanged, but what gets *rendered* each frame is
a presentation-only interpolation between two authoritative simulation
states rather than the latest one presented directly — see "Diagnosis" and
"Simulation/presentation boundary" below.

Both planets and the plank also carry ordinary dynamic bodies (cubes and
spheres) — see "Physics test world" and "Dynamic bodies." Each one samples
the same effective gravity the player does, purely from its own current
position, with no knowledge of which planet or the plank it's on, or which
`GravityField` implementation(s) are active. They fall, land, roll, and
collide with the world and with each other under ordinary rigid-body
dynamics — Judas supplies acceleration only, never orientation or resting
position. The player can walk into one and push it — see "Player-to-object
interaction."

**As of this milestone, Project Judas owns its physics engine entirely —
no third-party physics middleware is used at all.** Every prior milestone
through the first attempt at this one used Jolt Physics for collision
detection, contact resolution, and rigid-body integration; that dependency
is now fully removed (see "Physics ownership" for why, and the full
retrospective on the two gravity-model designs that failed human
validation before the operator made this call). Rigid-body state,
integration, collision detection, contact generation, and contact
resolution are all Judas's own code (`src/RigidBody.*`, `src/Contacts.*`,
`src/ContactSolver.*`, `src/PhysicsWorld.cpp`), built and verified to
contain no world-space axis assumption anywhere in that stack — see
"Physics ownership" and "Automated testing."

Gravity context — which `GravityField` governs a consumer at a given
position — is resolved by pure ownership routing (`GravityContextMap`):
each planet and the plank has its own coherent gravity, and a position
belongs to exactly one of them, never a blend of two. See "Gravity context
ownership" for the two earlier designs this replaced and why both failed
human validation despite passing every automated check. Nothing else. See
the root `README.md` for build/run instructions and controls.

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
- `milestone-5` — the player rebuilt as fully Judas-owned (no more
  `CharacterVirtual`); `RadicalGravity` added; a spherical demo world;
  the headless test harness added as permanent infrastructure.
- `milestone-6` — diagnosed and fixed the locomotion judder discovered
  during Milestone 5's human validation via presentation-only
  interpolation (`GetPresentedPosition`/`GetPresentedOrientation`), added
  `REALTIME` mode to the test harness to make the diagnosis possible; no
  demo scene changes.
- `milestone-7a` — sphere enlarged 8m to 20m; four ordinary Jolt dynamic
  bodies sharing the player's gravity (`DynamicBody`,
  `PrepareDynamicBodiesForStep`); minimal player-to-object push. Followed
  by an un-tagged bugfix (commit `f8e48ac`, "Bugfix #1") for a real
  move-and-slide contact-normal bug — see "Contact normal correctness and
  the 'already touching' case" — found during this milestone's own human
  validation; bugfixes between milestones aren't tagged the way milestones
  are.
- `milestone-7b` — a second, simultaneous `FaithfulGravity` platform
  alongside the sphere, with `GravityResolver` (falloff-weighted vector
  blending) resolving which field governed a consumer at a given position.
  Passed its own human validation at the time; later shown by Milestone
  7-Final's own validation to be built on an assumption (continuous field
  blending as the model for "gravity context") that doesn't generalize —
  see "Gravity context ownership."
- `milestone-7final` — two independent planets, a connecting plank, full
  round-trip traversal. Reached only after TWO complete gravity-model
  redesigns failed the operator's own interactive validation (not just
  automated checks) — see "Gravity context ownership" for the full,
  honest retrospective on why `GravityResolver` and the first
  `GravityContextMap` design both passed every harness check while still
  feeling physically wrong to a human standing on the plank. The second
  failure prompted a further decision unrelated to gravity semantics
  themselves: Jolt Physics was removed entirely and replaced with a
  Judas-owned rigid-body/collision/contact stack — see "Physics
  ownership." Both prior attempts' commits remain in git history
  (untagged, since neither passed human validation) for anyone who wants
  to see exactly what was tried and rejected.

Each milestone's demo content has been replaced (not extended) by the next;
check out a tag to see or run an earlier milestone as it was.

## Diagnosis

**Symptom (reported by the operator during Milestone 5 interactive
validation):** ordinary continuous locomotion around the spherical world
looked visibly jumpy/jerky, even though the underlying gameplay (walking,
collision, jumping, orientation) was functionally correct.

The brief for this milestone was explicit that this could be either of two
different bugs — a genuinely unstable simulation, or a stable simulation
being presented badly — and insisted on measuring before touching anything.
Two measurements, both taken with `src/TestHarness.h/.cpp` (see
"Automated testing" below for the `REALTIME` mode added specifically for
the second one):

**1. Is the authoritative fixed-step simulation itself smooth?** Walked in
a straight line at constant input, logging every single fixed step
(`LOG_EVERY 1`, no rendering involved at all). Across 150 consecutive
steps, the per-step position delta was `0.06667m ± 0.00004m` — a relative
standard deviation of about `0.06%`, and exactly `kMoveSpeed *
kFixedTimestep = 4.0 * (1/60) = 0.0667m`. **The authoritative simulation
was already producing a essentially perfectly uniform sequence of states.**
No repeated penetration correction, no oscillating shape sweeps, no
support-transition instability, no orientation snapping — none of the
"simulation is actually broken" candidates listed in the brief were
present.

**2. Is that smooth state being presented smoothly?** Added a real-time
diagnostic mode to the harness (`REALTIME <n>`) that mirrors
`Application::Run`'s own interactive accumulator loop exactly — same
`SimulationTiming` constants, same clamp/accumulate/catch-up-cap shape —
so it reproduces the true render-frame-to-fixed-step relationship
headlessly, logging one row per *rendered frame* (not per fixed step):
how many fixed steps ran that frame, and the position that would have been
handed to the renderer. Walking at the same constant input, over 289
render frames:

| | min | max | mean | relative stddev |
|---|---|---|---|---|
| fixed steps per rendered frame | 0 | 2 | ~0.96 | — |
| per-frame position delta (old: direct presentation) | `0.000m` | `0.180m` | `0.071m` | `21%` |
| implied instantaneous speed | `0.0 m/s` | `10.4 m/s` | `4.14 m/s` | `21%` |

The render loop was running at roughly 58–59Hz against a 60Hz fixed
timestep — close, but not an exact integer ratio (as is true of the
overwhelming majority of real monitors: 59.94Hz, 75Hz, 144Hz, 165Hz, and
even nominal "60Hz" displays with vsync-delivery jitter all fail to divide
evenly into a 60Hz simulation). Most frames got exactly one new fixed step,
but some got zero (the same authoritative position rendered twice in a
row — a visible freeze) and some got two (a render frame showing double a
normal step's worth of motion — a visible lurch). **Root cause: not
simulation instability. The fixed-step state was correct at every step;
directly presenting "whatever the latest fixed step happened to compute"
on render frames whose timing doesn't line up 1:1 with the fixed rate
produces temporal aliasing — the classic consequence of a fixed-timestep
simulation rendered without interpolation.**

**Why the chosen fix (presentation-only interpolation — see "Simulation/
presentation boundary" below) addresses this and nothing else needed to
change:** since the authoritative sequence of states was already smooth,
correct, evenly-spaced ground truth, the only thing missing was a way to
ask "what does the world look like at this render frame's actual moment in
time, which usually falls *between* two fixed steps?" instead of snapping
to whichever fixed step happened to have completed most recently.
Interpolating between the previous and current authoritative
position/orientation, weighted by how far into the next (not yet
simulated) step real time has progressed, answers exactly that question —
without touching gravity, collision, support, velocity, or any gameplay
decision, all of which remain driven by the untouched fixed-step states.
Interpolation was not assumed up front; it's what two rounds of measurement
pointed at.

**After the fix**, the same real-time measurement:

| | min | max | mean | relative stddev |
|---|---|---|---|---|
| per-frame position delta (new: presented/interpolated) | `0.065m` | `0.113m` | `0.071m` | `11%` |
| implied instantaneous speed | `4.0 m/s` | `6.5 m/s` | `4.14 m/s` | `11%` |

Freeze frames (`0.0 m/s`) and double-step lurches (`~10 m/s`, over 2.5×
target speed) are both gone. The remaining ~11% relative variation
correlates with genuine render-frame delivery jitter (this environment's
frame interval itself varies roughly 16.3–18.5ms) rather than being
decoupled from real elapsed time the way the original quantization
artifact was — proportional motion that tracks real elapsed time reads as
smooth; motion that randomly alternates between "nothing happened" and
"twice as much happened" does not. See "Human visual validation" for the
operator's judgment on whether this reads as smooth in practice.

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

**stb_image_write** (added in Milestone 5, `third_party/stb_image_write.h`)
— a single-header, dependency-free PNG/BMP/TGA/JPG/HDR writer, public
domain/MIT dual-licensed. Used only by the opt-in test harness (see
"Automated testing") to save screenshots; the normal game never calls it.
Vendored directly (one file, ~1700 lines, no transitive dependencies)
rather than fetched via CMake — writing an image file from raw pixels is
as commodity a problem as they come, and a package manager or
`FetchContent` step would add process for a single self-contained header.

## Physics ownership

**Milestone 3 through the first attempt at Milestone 7-Final used [Jolt
Physics](https://github.com/jrouwe/JoltPhysics) (pinned `v5.6.0`, MIT
license, fetched via CMake `FetchContent`)** for rigid-body dynamics,
collision detection, contact resolution, and constraint solving — a
solved, extremely hard problem that every shipped physics-using game
either licenses or spends years building, and Project Judas had no
evidence it needed to re-derive any of that.

**Milestone 7-Final removes Jolt entirely.** Not because middleware is
expensive or immature — the operator's own words: "I do not care" about
implementation cost, this "is not a question of whether doing so is more
expensive, more difficult, less mature, or duplicates middleware
functionality. The decision is made." The evidence that triggered it:
Milestone 7-Final's two-planet-plus-plank demonstration failed the
operator's interactive validation *twice* under two different gravity-
context designs (see "Gravity context ownership"), and while diagnosing
the second failure, the operator concluded the deeper problem was
architectural — every previous gravity design had been built as "Judas
logic wrapped around a conventional Y-up-shaped physics engine," which
made it too easy for a subsystem (support detection, contact response,
character movement) to quietly assume a fixed axis existed somewhere
underneath, even while `PlayerController` itself stayed scrupulously
axis-agnostic. The fix wasn't another gravity-resolution redesign; it was
making "no global up or down" a property of the physics layer itself, not
just of Judas's logic sitting on top of it.

**What Judas now owns, in full** (`src/RigidBody.*`, `src/Contacts.*`,
`src/ContactSolver.*`, `src/PhysicsWorld.cpp`): rigid-body state (position,
free-quaternion orientation, linear/angular velocity, inverse mass, full
3x3 inverse inertia tensor), force/gravity accumulation, semi-implicit
Euler integration, collision detection (sphere/box narrowphase, a proper
multi-point manifold for box-vs-box — see "Locomotion" for why a single
point wasn't enough), contact resolution (sequential impulses for the
normal and Coulomb-friction constraints, plus penetration-proportional
positional correction), and the player's own capsule sweep query (a
substep-sampled march with bisection refinement — see "Player/controller
ownership"). None of it assumes a world-space up axis: every operation is
expressed in terms of the shapes' and bodies' own positions/orientations,
verified by dedicated tests that rigidly rotate an entire scenario
(bodies, gravity, and all) and confirm the result rotates identically —
see "Automated testing."

**Why this is still "ownership," not "reinventing everything":** the
brute-force nature of this stack is deliberate, not a placeholder for
missing ambition. Broadphase is all-pairs (this demo's body count is in
the low teens — a spatial structure would be unused machinery, not a
correctness requirement); box-vs-box uses a vertex-inside-the-other-box
manifold rather than full Sutherland-Hodgman clipping; the player's sweep
is substep-sampled rather than closed-form continuous collision detection.
Each simplification is documented at its own definition, not hidden — see
"Remaining limitations." The goal was the smallest COMPLETE stack Project
Judas's own bodies need, built and verified bottom-up (state → integration
→ collision → contacts → resolution → the player's query surface), not a
feature-complete general physics engine.

`PhysicsWorld.h`'s public API — `BodyHandle`, `BodyTransform`,
`ShapeSweepHit`, `CreateStaticBox`/`CreateStaticSphere`/`CreateDynamicBox`/
`CreateDynamicSphere`, `ApplyLinearAcceleration`, `Step`, `GetTransform`,
`CreatePlayerShape`/`DestroyPlayerShape`/`SweepPlayerShape` — is completely
unchanged by this migration. Every consumer (`PlayerController`,
`DynamicBody`, `Application`, `TestHarness`) needed zero changes beyond the
demo content itself; the interface was already physics-middleware-agnostic
by construction (law: `PhysicsWorld.h` never exposes a concrete engine
type), so replacing what's behind it was exactly as contained as the
interface always promised.

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

**The physics engine (via `PhysicsWorld`) now owns only:** the underlying
shape-query math — "if this capsule moved this far in this direction, what
would it hit, and what's the surface normal there?" That's it.
`PhysicsWorld` exposes exactly this as `SweepPlayerShape` (see
`PhysicsWorld.h`), plus `CreatePlayerShape`/`DestroyPlayerShape` to manage
the capsule shape itself. The player capsule is **never added to the world
as a body** — it has no handle, no mass, no activation state, and never
appears in the contact-resolution pass. It exists purely as a shape
(radius + half-height) that `SweepPlayerShape` samples at several points
along a displacement (see "Physics ownership") on demand, whenever
`PlayerController` wants an answer.

As before, no concrete physics-engine type appears in `PhysicsWorld.h` —
`ShapeSweepHit` (the query result) is plain `glm` data. This held true
across the Milestone 7-Final physics migration specifically because it was
already true: the interface never named Jolt, so replacing what
implements it required no change here.

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

The foundational rule from Milestone 3, conceptually unchanged even though
Milestone 7-Final moved *who* sits on the physics side of it:

```
Judas owns gravity, reference frames, and world/large-world coordinates.

The physics engine owns collision detection, contact generation,
rigid-body integration, and constraint solving — nothing more.
```

**Milestone 7-B kept this boundary exactly where it was** — `GravityResolver`
was a pure addition entirely inside Judas's own existing half of the split,
nothing that touched collision, contact, or rigid-body integration; Jolt
did precisely what it always had.

**Milestone 7-Final moved who implements the physics side, not the split
itself.** The physics engine still owns exactly the same things (collision
detection, contact generation, rigid-body integration, constraint solving)
— it's now Judas's own code instead of Jolt's, but the boundary between
"gravity/reference-frame logic" and "collision/contact solving" is
unchanged; see "Physics ownership" for why the *implementer* changed and
"Gravity context ownership" for why gravity semantics were re-derived at
the same time (a related but logically separate decision — the operator
was explicit that middleware replacement and gravity-context redesign are
two different questions, addressed together only because evidence for both
surfaced in the same investigation). No physics middleware computes
gravity of its own; Judas still samples a `GravityField` and hands the
result to the physics layer as an applied acceleration, exactly as before.

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

- Judas's own physics engine has no built-in gravity of its own to
  disable — unlike Jolt (which defaulted to a global `(0,-9.81,0)` and had
  to be explicitly zeroed in `PhysicsWorld::Init` through the first
  Milestone 7-Final attempt), `RigidBody`/`PhysicsWorld` never apply any
  acceleration a caller didn't hand them. There is nothing to turn off
  because nothing was ever built in.
- Every fixed step, `PlayerController::FixedUpdate` samples the active
  `GravityField` at the player's own current position and integrates the
  result into its own vertical velocity — the same
  `velocity += acceleration * dt` pattern `PhysicsWorld::ApplyLinearAcceleration`
  uses for ordinary bodies, just computed inside `PlayerController` since
  the player has no physics-engine body for `PhysicsWorld` to apply it to.

## Multiple gravity consumers

Through Milestone 6, the player was `GravityField`'s only real consumer.
Milestone 7-A proves the interface was never player-specific: the four
dynamic test objects (see "Physics test world") sample the exact same
`GravityField&` the player does, each from its own current position, every
fixed step:

```
active GravityField
         |
         +------ player        (samples inside PlayerController::FixedUpdate)
         |
         +------ dynamic cube A
         +------ dynamic cube B    (all sampled the same way, from
         +------ dynamic sphere A   src/DynamicBody.cpp's
         +------ dynamic sphere B   PrepareDynamicBodiesForStep)
```

`PrepareDynamicBodiesForStep` (`src/DynamicBody.h/.cpp`) is the entire
mechanism — a small free function, not a framework:

```cpp
for (DynamicBody& body : bodies) {
    body.SnapshotPrevious();                                  // presentation history (see below)
    const glm::vec3 acceleration = gravity.Sample(body.GetPosition());
    physics.ApplyLinearAcceleration(body.Handle(), acceleration, fixedDeltaTime);
}
```

called once per fixed step, before `PhysicsWorld::Step`, from both
`Application::Run` and `TestHarness.cpp`'s two modes. A `DynamicBody`
itself never sees a `GravityField`, a `RadicalGravity`, a sphere center, or
a radius — it hands `PrepareDynamicBodiesForStep` its current position and
receives an acceleration back, exactly the shape of knowledge
`PlayerController` has always had. With `RadicalGravity` active, this was
verified directly with the test harness: four objects spawned at
deliberately different locations around the sphere fall along four
different straight lines, each toward the sphere's center from its own
starting point (confirmed by each object's logged velocity direction
matching `normalize(center - position)` for its own position, not a shared
direction) — see "Automated testing."

`ApplyLinearAcceleration` already existed (added in Milestone 3, unused by
the active demo since Milestone 5) and needed no changes — it was already
exactly the right shape for this.

## Gravity context ownership

Through Milestone 7-A, gravity selection happened once, at composition
time: `Application::Run` constructed exactly one concrete `GravityField`
and bound it to the `GravityField&` every consumer holds. Milestone 7-B
needed something more — two genuinely, simultaneously active environments,
and a consumer that could move between them at runtime — and Milestone
7-Final needed the same thing again, twice as hard (two planets instead of
one planet plus a directionless platform). Its own human validation
rejected TWO different designs before landing on the one this repository
now ships. Both failures are recorded in full below, not summarized away —
the reasoning behind *why* they failed is the actual content of this
section, and the project's own standing rule ("a numerically smooth wrong
physical model is still wrong; the harness is not authoritative over human
validation") only means something if the record shows exactly how that
happened twice.

### Design 1 (Milestone 7-B): `GravityResolver` — falloff-weighted vector blending

`GravityResolver` treated "which gravity governs a consumer" as a
continuous force-summation problem: every registered zone within its own
falloff radius of a position contributed, weighted by distance
(smoothstep, full weight at an inner radius to zero at an outer one), with
direction combined via spherical interpolation and magnitude via a
weighted-average-times-combined-weight formula. This passed Milestone
7-B's own human validation, and worked correctly for a sphere-plus-
directionless-platform pair specifically because `FaithfulGravity`'s raw
direction never varies with position — blending it with a radial source
could only ever rotate the result toward straight down, never introduce
genuine sideways contamination. One of the two "sources" being blended
contributed zero extra directional complexity, so the failure mode below
had no way to manifest.

Milestone 7-Final's two-radial-source case exposed the real defect
immediately: a position beside the connecting plank — nowhere near either
planet's actual domain — got an artificial pull toward the line connecting
the two planet centers, purely because it happened to sit within both
planets' (necessarily generous) falloff radii. `GravityResolver` was
answering "how do nearby sources sum" when the actual question was "which
source, if any, has any relationship to this position at all" — a
field-summation model standing in for an ownership model. This failed the
operator's own interactive validation ("the gravity switching
implementation is fundamentally flawed... the player appears to be able to
walk radially around the plank") despite passing every harness check M7-B's
own validation had run, because that validation only ever sampled along the
intended travel corridor, never the width around it. Neither the vector
math nor the harness sampling was wrong on its own terms; the MODEL the
math implemented was wrong, and the validation never looked at the
dimension that would have shown it.

### Design 2 (Milestone 7-Final, first attempt): `GravityContextMap` with explicit blended transitions

The replacement fixed the off-path contamination specifically:
`GravityRegion`s gave each planet an explicit, BOUNDED domain (a
`GravityVolume`, e.g. a sphere around its own center), and a
`GravityTransition` gave the plank's crossing an explicit, bounded extent
inside which exactly two named fields blended by a locally-defined
progress fraction — never sampling or blending anything outside that
bounded pair. A position beside the plank, outside the transition's own
extent, now correctly got exactly one planet's own unmodified field, or
zero in genuinely unclaimed space — verified directly with a full-volume
`SAMPLE_GRAVITY` sweep (not just the centerline) before this design was
shown to the operator at all.

It failed anyway, on retest: standing on the plank's OWN surface, near its
edges, still tilted noticeably (~18-20 degrees) toward the planets' shared
axis. The off-path bug was gone; the on-path one wasn't, because the
transition still computed the plank's gravity by blending the two
planets' raw radial math together — exactly what `GravityResolver` had
always done, just correctly SCOPED to a bounded region instead of an
unbounded falloff. Both designs shared the same deeper mistake, stated
precisely: neither ever represented "the plank's own intended local
physics" as a first-class thing. Both derived gravity on the plank from
the two planets' math, never by asking what a body resting on a flat
surface should coherently experience. A numerically smooth, provably
continuous, harness-clean interpolation is still the wrong physical claim
if the claim itself — "gravity here is some blend of two unrelated
sources" — doesn't correspond to what "being on a flat plank" should mean.

### Design 3 (the current one): pure ownership routing, no blending anywhere

The insight that broke the cycle: `PlayerController` already has machinery
(architectural law #14 — rate-capped `UpdateFrameOrientation`, continuous
airborne velocity integration) specifically built to turn a SUDDEN change
in sampled gravity into a smooth, gradual takeover. That machinery doesn't
care whether the raw `GravityField::Sample` sequence is continuous or has
a single instantaneous jump between two fixed steps — a rate cap bounds
how fast the player's own frame can rotate regardless of *why* the target
moved, and velocity integration is inherently continuous even when the
acceleration driving it isn't. Blending was never necessary to keep a
takeover smooth; it was solving a problem the consumer side already
solved, while failing to solve the real one.

So: `GravityContextMap` (`src/GravityContextMap.h/.cpp`, rewritten a
second time — the transition/blending machinery from Design 2 is deleted
outright, not kept alongside this) does pure containment-based ROUTING,
nothing else. It holds an ordered list of regions, each `{GravityField&,
GravityVolume&}`; `Sample(position)` returns the FIRST region's field's
own raw, completely unblended value whose volume contains that position,
or exactly the zero vector if none does:

```cpp
for (region : regions)
    if (region.volume->Contains(position))
        return region.field->Sample(position);   // raw, unblended
return glm::vec3(0.0f);                            // unclaimed space
```

No transitions, no progress fractions, no weighted blending of any kind —
the mechanism `GravityResolver` and Design 2's `GravityContextMap` both
had is gone entirely, not merely unused.

**Every named context is now coherent on its own terms:**

- Planet A and Planet B: `RadicalGravity` centered on themselves, each
  owning a `SphericalVolume` around its own center, generous enough to
  cover its own surface plus a jump's worth of margin, but well short of
  reaching the other planet's own region (regions of the SAME kind must
  never overlap each other — see below).
- The plank: the existing, UNMODIFIED `FaithfulGravity` — its hardcoded
  `(0,-9.81,0)` already matches the plank's own flat, axis-aligned surface
  exactly, with no new gravity implementation needed. Its own `BoxVolume`
  region is padded a little beyond the plank's actual collision box, so a
  jump arc a little above or beside the plank's own surface is still
  claimed by it rather than falling into unclaimed space.

**Overlap is resolved by registration order, deliberately, not
accidentally.** The plank's own region is registered BEFORE either
planet's, so it wins wherever its (padded) box geometrically overlaps a
planet's sphere — which it does, near the plank's own ends, since the
plank sits close enough to each planet's surface to be walkable without a
large jump. This is the ONE place overlap is allowed and intentional: a
more specific region (the plank) taking priority over a more general one
(an entire planet) it happens to reach into. Two regions of the SAME
specificity (planet vs. planet) must never overlap each other — there is
no principled tie-break for that case, so the composition root
(`Application.cpp`) is responsible for keeping their radii apart, the same
category of care falloff-tuning used to require, just a different shape
of it.

**Verified by sweeping the full volume — not just the corridor, and not
just the centerline within it, the mistake BOTH prior validation passes
made:** along the plank, across its full width (the specific dimension
that exposed both prior failures), above and below it, around both
planets, and at both region boundaries. The plank now reads EXACTLY
`(0,-9.81,0)` everywhere inside its region — not merely "close to
vertical," not "less tilted than before" — because it is no longer
computed from anything BUT `FaithfulGravity`'s own constant. Beside the
plank, outside its region: exactly the nearer planet's own field, or
exactly zero. See "Automated testing" for the dedicated tests that check
this as a standing regression, not just a one-time sweep.

**No per-consumer state, and why that's not assumed to be permanently
true:** position-only resolution is sufficient for M7-Final's own static
environments — there is no evidence here that a consumer's gravity context
needs memory of where it was a moment ago. This is NOT documented as a
permanent law: a future requirement (moving reference frames, for one)
may produce evidence that contextual membership can't always be derived
from world position alone, and per-consumer context state is not
inherently wrong when that evidence exists — it just isn't justified yet.

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
  within a walkable-slope threshold
  (`dot(normal, localUp) > 0.643`, ≈50°, matching Milestone 4's original
  choice, unchanged by the Milestone 7-Final physics migration).
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

**Neither the plank nor the second planet needed any changes here.**
Landing on either is exactly the same mechanism as landing on Planet A — a
real `PhysicsWorld::SweepPlayerShape` hit against a real static body,
gated by the same slope threshold — not a gravity-region event of any
kind. `PlayerController` never asks "which `GravityContextMap` region am I
in"; grounding and gravity context remain governed by entirely independent
code paths at every point during a crossing. There is no code anywhere
that snaps the player onto the plank or a planet once a gravity-context
boundary is crossed — landing happens if and only if the move-and-slide
loop's own collision query says it does.

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

### Bounding the reorientation rate (Milestone 7-B)

`UpdateFrameOrientation` used to apply the *entire* rotation from current
up to `localUp` in one fixed step, every step, unconditionally. Through
Milestone 7-A this was always imperceptibly smooth, because a single
`GravityField`'s direction only ever changes as a continuous function of
position — a normal jump's brief arc barely moves `localUp` at all. That
assumption silently breaks once a *resolved* gravity direction can change
quickly — originally reproduced near the edge of Milestone 7-B's
`GravityResolver` zone falloff, where the blended direction was most
sensitive to position; the current `GravityContextMap` (see "Gravity
context ownership") makes the point sharper still, since crossing a region
boundary is now a genuine, instantaneous jump in the raw sampled value
between two fixed steps, not even a fast continuous sweep. This law is
exactly what makes that acceptable rather than jarring. Reproduced directly
and severely before this fix existed (under the original `GravityResolver`
design):
a scripted transition run showed `m_frameOrientation`'s up jump by **85
degrees in a single `1/60s` fixed step** — a hard snap, not the "smooth
takeover" the brief requires, and the specific failure "do not hard-snap
from radial local up to `FaithfulGravity` local up" explicitly warns
against.

The fix bounds the rotation itself rather than trying to detect or
distrust the *sample* that produced it:

```cpp
delta = RotationBetweenUnitVectors(currentUp, localUp);
maxRadiansThisStep = radians(kMaxReorientationDegreesPerSecond) * fixedDeltaTime;
if (angle(delta) > maxRadiansThisStep)
    delta = angleAxis(maxRadiansThisStep, axis(delta));   // same axis, capped angle
m_frameOrientation = normalize(delta * m_frameOrientation);
```

`kMaxReorientationDegreesPerSecond` (`120`, `PlayerController.cpp`) is a
deliberate choice, not a magnitude threshold: it bounds *how fast the
player's own sense of up can physically change*, independent of why the
target moved, which needs no assumption about any concrete `GravityField`'s
typical magnitude — a threshold on the sampled gravity's own magnitude was
considered and rejected specifically because it would embed knowledge of a
concrete implementation's scale into code that must stay
implementation-agnostic (see "Gravity context ownership"). `120`/s is generous
against ordinary gameplay (walking the sphere at full speed turns `localUp`
at roughly `11`/s, so the cap is never felt there) while still meaningfully
bounding a transition-region swing. Verified directly: the same scripted
transition that showed an 85-degree single-step snap before this fix now
shows a maximum single-step change of `2.14` degrees — matching
`120°/s * 1/60s = 2°` almost exactly — with the reorientation spread
smoothly across roughly a second of real transition time instead.

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
speed is a constant **4 m/s**. As of Milestone 7-B, this instant,
input-driven velocity applies **only while grounded** — see "Velocity
continuity while airborne" below for why.

**Jumping** (`Space`) preserves the Milestone 4 rule exactly: direction is
`-normalize(gravity.Sample(position))` (never a hard-coded axis, never
derived from the sphere), magnitude a constant **5 m/s**, permitted only
when this step's own support query says grounded (see "Support"). No
double jump, coyote time, jump buffering beyond the input latch described
below, variable height, or wall jumping — a jump attempted while airborne
is discarded, not queued for the next landing.

### Velocity continuity while airborne (Milestone 7-B)

Before this milestone, `FixedUpdate` computed velocity the same way every
step, grounded or not: the tangential component came fresh from
`ComputeTangentVelocity` (i.e., from *current* WASD input, zero if nothing
is held), and only the component along the *current* `localUp` carried
over from the previous step. On the ground this is exactly the desired
instant, responsive control. In the air, it has a real consequence once
`localUp` itself can rotate substantially mid-flight (Milestone 7-B's
whole point): whatever part of the player's existing velocity has become
*tangential* to the newly-rotated `localUp` gets silently discarded and
replaced by (typically zero) fresh WASD input — not gravity decelerating
the player, but the engine actively erasing momentum every single step.
This is exactly the class of bug the brief's "velocity continuity"
requirement forbids ("do not zero velocity merely because gravity
ownership changes"), just arrived at implicitly rather than by an obvious
`velocity = newDirection * value` line.

Through Milestone 7-A this was invisible: a normal jump's brief arc barely
rotates `localUp` at all, so recomputing "fresh" tangential velocity every
step produced almost the same value as carrying it over would have.
Reproduced directly once it mattered: a scripted jump-and-coast through the
gravity transition (WASD released, so tangential velocity recomputed to
exactly zero every step) showed the player's outward speed collapse from
`5 m/s` toward zero over the course of the transition — far faster than
`RadicalGravity`'s own deceleration alone would produce — because the
"outward" component of velocity became "tangential" relative to the
rotating `localUp` and was thrown away each step, not merely decelerated.

Fixed by splitting the two cases instead of sharing one formula:

```cpp
if (isGrounded) {
    // unchanged: instant WASD-driven horizontal control, plus the small
    // per-step gravity nudge that keeps a grounded player glued to a
    // curved surface (see "Contact normal correctness"); a jump overrides
    // vertical speed outright.
    m_velocity = ComputeTangentVelocity(window, localUp) + localUp * verticalSpeed;
} else {
    // ordinary integration of the FULL velocity vector — not just its
    // localUp-aligned component — and WASD is not consulted at all.
    m_velocity += acceleration * fixedDeltaTime;
}
```

While airborne, WASD held or released now has no effect at all — the
player is not given new "air control" this milestone, just protected from
having existing momentum erased. Verified directly: the same jump-and-coast
scenario that used to lose its outward momentum now carries it through the
full transition and lands on the platform; a full regression pass (push,
object-to-object collision, a 60-second sustained walk, determinism,
15-second stability) confirmed no other behavior changed — see "Automated
testing."

**Collision-aware movement**: `PlayerController` never writes `m_position`
directly from a desired velocity. Each fixed step, the desired displacement
(`velocity * fixedDeltaTime`) is resolved by a small move-and-slide loop
(up to 4 iterations): sweep the capsule along the remaining displacement
via `PhysicsWorld::SweepPlayerShape`; if nothing is hit, apply the full
displacement and stop; if something is hit *and there's genuine clearance
left* (`hit.distance >= 0.02m`, the skin margin), advance up to just short
of it; if there's *not* — the capsule is already at or inside the intended
skin-margin clearance from whatever it hit — move straight back out along
the contact normal to restore exactly that margin instead. Either way,
remove the component of the *remaining* displacement that points into the
hit surface (an ordinary vector projection against the contact normal —
"slide"), and repeat with what's left. This is Judas's own resolution of
the conceptual pipeline the brief described (desired displacement → shape
query → allowable movement → contact information → Judas's interpretation
→ resulting position) — not a general physics solver, just enough
iterations to handle "hit one surface, then slide into a second" without
visibly sticking.

### Contact normal correctness and the "already touching" case (historical: Jolt-backed implementation, Milestones 5 through the first Milestone 7-Final attempt)

Fixed in Milestone 7-A, after human validation surfaced a visible ground
vibration while walking that traced back to here. `SweepPlayerShape`
(`PhysicsWorld.cpp`) previously derived the contact normal from Jolt's raw
`mPenetrationAxis` by flipping its sign based on
`dot(normal, displacement)` — reasoning that a contact normal should
oppose the direction of travel. That happens to agree with Jolt's own
convention for an ordinary in-flight hit (`mFraction > 0`, moving toward a
surface not yet touched), which is why it went unnoticed through
Milestones 5–7-A's earlier validation passes — but travel direction has no
necessary relationship to the true normal for a hit already at
`mFraction == 0` (shapes already touching at the start of the sweep),
which a grounded step's own small inward gravity nudge (see "Simulation
timing") produces routinely by design, eroding the skin margin toward zero
over many steps. There, the old heuristic could and did pick the wrong
sign — confirmed directly with a debug-instrumented harness run: the same
already-touching hit alternated between a correct outward normal and one
pointing straight into the sphere, iteration to iteration, within a single
fixed step. Jolt documents its own unconditionally-correct convention —
`-mPenetrationAxis.Normalized()` — which is what `SweepPlayerShape` uses
now, dropping the travel-direction heuristic entirely (kept only as a
last-resort fallback for the fully degenerate case where Jolt returns no
penetration axis at all).

That alone fixed the normal's reliability but not the underlying erosion:
with the skin margin already at exactly zero and no mechanism to restore
it (the move-and-slide loop only ever clamped closer-than-margin travel to
"do nothing," never corrected it), the capsule could and — reproduced
directly with a 60-second scripted walk — did reach a permanent, total
lockup: a correct-but-immovable state where every iteration's already-zero
distance produced a tangential "leftover" too close to perpendicular to
the (now-correct) normal to ever redirect movement again. The margin-
restoration change above (moving back out along the normal when
`hit.distance < kSkinMargin`, rather than merely refusing to move closer)
closes that gap. Verified with the same 60-second harness script: zero
frozen steps (previously ~85% of a 3600-step run), and the radial
distance-from-center — logged at full precision specifically to check for
this — settles into a stable, bounded ~`1.2mm` oscillation band rather
than eroding unboundedly toward (and getting stuck at) zero clearance.

### Contact normal correctness under Judas's own physics (Milestone 7-Final)

`SweepPlayerShape`'s current implementation (see "Physics ownership")
never derives a contact normal from travel direction at all.
`CapsuleDistanceToSphere`/`CapsuleDistanceToBox` (`src/Contacts.cpp`)
compute the normal directly from closest-point geometry — the vector from
the other shape's own surface point to the capsule's own closest
segment point — unconditionally, identically for an already-touching hit
and an in-flight one. There was never a travel-direction heuristic to get
right or wrong in the first place; the substep-sampled sweep this
implementation uses never needed one. The margin-restoration fix from
Milestone 7-A (moving back out along the normal when `hit.distance <
kSkinMargin`) carried over completely unchanged, since it lives in
`PlayerController`'s own move-and-slide loop, not in `SweepPlayerShape`.

Milestone 7-Final's own traversal testing under the new engine showed
markedly fewer grounded-flag transitions than the equivalent Jolt-backed
run (6 total across a full round-trip walk, versus over 150 previously,
all clustered in a brief window right at the flat-plank-to-curved-sphere
geometric seam) — recorded here as an observation, not a claim that the
~1.2mm residual oscillation documented above is now solved: no dedicated
investigation was done into why, and the current implementation's own
approximations (substep-sampled sweep resolution, single/multi-point
contact manifolds — see "Physics ownership") are new enough that this may
simply reflect different numerical behavior rather than a genuine
improvement. See "Remaining limitations."

**Reset** (`R`) restores the player entirely from data `PlayerController`
already holds (`Reset()`): spawn position, zero velocity, identity frame
orientation (which the very next fixed step immediately realigns to
whatever gravity says at the spawn point, so this is not a visible
discontinuity), spawn yaw, zero pitch, and clears any pending jump request.
There is no physics-side state to reset separately — unlike Milestone 4's
Jolt-backed player, all state lives in `PlayerController` itself. Demo
`Reset` logic is allowed to know the spawn point is "above the sphere";
`PlayerController`'s own code does not know a sphere exists.

### Reset and discontinuities

Added this milestone, alongside presentation interpolation, because the
two interact if not handled deliberately: `Reset()` also sets
`m_previousPosition = m_position` and `m_previousOrientation =
m_frameOrientation` — i.e. it synchronizes presentation history to the
same reset pose, not just the authoritative one. With both interpolation
endpoints identical, `GetPresentedPosition`/`GetPresentedOrientation`
return exactly the reset pose *regardless of `alpha`* — so the very next
render presents the reset position directly, never a blended "slide"
across the world from wherever the player was a moment before. Verified
directly with the harness: a `REALTIME` run holding forward movement and
triggering `R` mid-walk shows the presented position jump straight from
mid-walk coordinates to (within floating point) the exact spawn point on
the very next logged frame, with no intermediate values between the two —
a genuine teleport presented as a teleport, not smoothed into apparent
high-speed travel.

Tuning values (`src/PlayerController.cpp`, anonymous namespace): capsule
radius `0.3m`, capsule cylinder half-height `0.6m` (total capsule height
`1.8m`), eye height `0.7m` above the capsule center, move speed `4 m/s`,
jump speed `5 m/s`. All explicit, chosen for a readable demonstration, not
tuned for feel.

## Player-to-object interaction

Added in Milestone 7-A, unchanged in mechanism through the Milestone
7-Final physics migration. The player is still not a physics-engine body
(see "Player/controller ownership") — `SweepPlayerShape` is a read-only
query, never something the contact solver resolves — so contact with a
dynamic test object would otherwise never move it: the object would simply
act as one more static-feeling obstacle the player slides along. This is
the genuine limitation the brief anticipated ("if player-to-dynamic-body
interaction exposes a genuine limitation in the existing player
collision/query boundary, make the smallest correction necessary and
document it").

**The correction** lives entirely inside `PlayerController::FixedUpdate`'s
existing move-and-slide loop, at the point it already has a `ShapeSweepHit`
from this step's movement sweep:

```cpp
if (physics.IsDynamicBody(hit.hitBody)) {
    const glm::vec3 pushDirection = -hit.normal;
    const float playerSpeedIntoObject = glm::dot(m_velocity, pushDirection);
    if (playerSpeedIntoObject > objectSpeedIntoObject)  // never slows the object down
        physics.SetLinearVelocity(hit.hitBody, objectVelocity + pushDirection *
                                       (playerSpeedIntoObject - objectSpeedIntoObject));
}
```

Only the component of the player's velocity *along the contact normal* is
transferred, and only when it exceeds the object's own velocity along that
same axis — so the player can shove an object but never slow one down or
overwrite its motion along other axes (falling, rolling from an earlier
hit). This is deliberately not a general impulse/momentum system: it seeds
one velocity value and hands control straight back to the physics engine,
which owns everything that happens to the object from that point on
(further integration, friction, contact with the world or another
object). Static
world geometry (`physics.IsDynamicBody` is false for it) is unaffected —
`hit.hitBody` and `PhysicsWorld::IsDynamicBody`/`GetLinearVelocity`/
`SetLinearVelocity` were the only additions `PhysicsWorld` needed (see
`PhysicsWorld.h`).

**A consequence worth recording plainly**: because only the along-normal
component transfers, the player can only push an object in roughly the
direction the contact normal already allows — nudging an object sideways
by approaching from an angle and turning mid-push does not work well,
since a face's normal doesn't rotate just because the player's own
movement direction changes. This was discovered directly while devising a
harness scenario for object-to-object collision (see "Automated testing")
and is recorded here rather than "fixed," since a fuller push/carry system
is explicitly out of scope for this milestone (no grabbing, no carrying —
see "Deliberately Not Implemented").

Verified with the harness: holding forward from spawn, the player reaches
CubeA within ~40 fixed steps and its velocity jumps from a small falling
speed to several m/s in the push direction; player and cube then travel
together (roughly constant ~1m separation) for hundreds of further steps
as the player keeps walking, confirming sustained contact rather than a
single nudge — see "Automated testing."

## Physics test world

Milestone 7-Final replaces the single sphere with TWO independent static
spheres ("Planet A" and "Planet B," `kPlanetARadius`/`kPlanetBRadius = 20m`
each, centers `55m` apart — a genuine `15m` surface-to-surface gap, so the
two worlds read as visibly, physically distinct bodies, not one world
wearing two names) plus one flat static plank connecting them
(`kPlankCenter`/`kPlankHalfExtents`). All three are still small,
hand-authored test geometry, not planets as an engine subsystem (see
"Deliberately Not Implemented").

This geometry, like the single sphere's before it, is **demo/composition-
root knowledge only** — `Application.cpp`'s anonymous namespace is the only
place any of it is named. `PlayerController`, `DynamicBody`, `GravityField`,
and `PhysicsWorld` remain exactly as ignorant of it as they always were;
nothing about this milestone required loosening that boundary, even though
it's now backed by Judas's own physics engine rather than Jolt's.

**The plank's placement is a real physics-and-geometry problem, not an
aesthetic choice, and its exact numbers were derived, not guessed.**
Positioned near "pole height" (`y=18`, vs. each planet's own radius of
`20`) rather than directly between the two planet centers at their own
height — the latter was considered and rejected without needing to build
it: gravity toward Planet A and gravity toward Planet B would point in
roughly OPPOSITE horizontal directions on the direct line between the
centers (back toward whichever planet is nearer), which no placement could
turn into a direction pointing down through the plank rather than sideways
along it. Near pole height, both planets' pulls stay predominantly
vertical across the plank's own footprint. (As of Design 3 in "Gravity
context ownership," the plank's own gravity is `FaithfulGravity`'s
constant regardless of position, so this placement no longer controls the
plank's own tilt at all — it still matters for keeping the plank within
an easy walk/jump of each planet's actual surface.) The plank's ends sit
close enough to each planet's own surface (a real but modest ~1m gap/step,
well inside normal walk/jump range) that no deliberate gravity-weakening
trick (unlike Milestone 7-B's platform — see "Gravity context ownership")
is needed anywhere to make the crossing possible.

**Dynamic bodies, spread across all three regions** (`kDynamicObjectSpawns`,
`Application.cpp`): six bodies, two per region (Planet A, the plank, Planet
B) — the minimum arrangement that exercises all three gravity contexts
simultaneously and proves the architecture isn't secretly player-specific
or region-specific. Planet-relative spawns use the same demo-only helper
Milestone 7-A introduced, `PointAboveSphere(center, radius, direction,
heightAboveSurface)`; plank spawns are literal positions above its surface.
None of their resting positions are scripted or hand-aligned to gravity —
they fall and settle exactly like every other body here, verified directly
(see "Automated testing").

Exact coordinates are demo-authoring detail, not architecture — see
`Application.cpp` if the literal numbers matter. Nothing about this
arrangement is procedural, spawned at runtime, or editable; all six bodies
are created once in `Application::Run` (via `SpawnDynamicObjects`) and
destroyed once at shutdown, exactly like the two static spheres and the
plank already are.

## Dynamic bodies

**Representation** (`src/DynamicBody.h/.cpp`): a `DynamicBody` is the
minimum shared state Milestone 7-A's cubes and spheres both need — a
`BodyHandle`, a `Visual` (shape kind, half-extents or radius, and color,
for `Renderer` only), and the same previous/current pose pair
`PlayerController` uses for presentation (see below). It is deliberately
**not** an entity/component: it has no update-dispatch, no type registry,
no behavior of its own beyond snapshotting a pose and reading one back —
see "Deliberately Not Implemented" for why nothing more general was built
for four test objects.

**How gravity reaches a body**: see "Multiple gravity consumers" —
`PrepareDynamicBodiesForStep` samples the active `GravityField` at each
body's own position and calls the existing (Milestone 3)
`PhysicsWorld::ApplyLinearAcceleration`, exactly as `PlayerController`
does for itself.

**How the physics engine owns the rest**: once gravity is handed over,
`DynamicBody` (and everything above it) steps back completely.
`PhysicsWorld::Step` — Judas's own rigid-body integration, friction,
restitution, and contact resolution (see "Physics ownership") — decides the
body's resulting position **and orientation**. Nothing in this engine ever
writes a dynamic body's orientation directly or aligns it to local
gravity/up; a cube settles however contact with the world leaves it, and a
sphere is free to roll. This mirrors "Support" above (gravity direction and
contact response stay separately computed) applied to a body Judas doesn't
move itself at all.

**Transforms reaching rendering**: after `PhysicsWorld::Step`,
`SyncDynamicBodiesFromPhysics` (a second small free function alongside
`PrepareDynamicBodiesForStep`) reads each body's fresh
`PhysicsWorld::GetTransform` — the same query `PhysicsWorld` has exposed
since Milestone 3, previously unused by the active demo. `Application`'s
`drawScene` then draws each body via `Renderer::DrawBox`/`DrawSphere` at
its *presented* (interpolated) transform — see "Simulation/presentation
boundary."

## Simulation timing

Physics is stepped on a **fixed timestep of 1/60 second**
(`SimulationTiming::kFixedTimestep`, `src/SimulationTiming.h`, added this
milestone), unchanged in value since Milestone 3 — only pulled out of
`Application.cpp`'s own anonymous namespace into a tiny shared header so
the test harness's real-time diagnostic mode (see "Automated testing")
mirrors the interactive loop's timing behavior by construction rather than
by two files' literals happening to agree. `Application::Run` uses the
same accumulator as before:

```
accumulator += (clamped) render-frame delta time
while accumulator >= fixedTimestep and steps-this-frame < cap:
    physicsWorld.Step(fixedTimestep)          // advances any ordinary dynamic bodies
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

## Simulation/presentation boundary

Added this milestone, in direct response to "Diagnosis" above. The rule:

```
Authoritative state (m_position, m_frameOrientation, m_velocity,
m_lastGrounded, ...) is updated ONLY by PlayerController::FixedUpdate, at
the fixed 1/60s rate, exactly as before this milestone.

Presentation state is derived FROM authoritative state, purely for
rendering, and is never read back by FixedUpdate, gravity sampling,
collision queries, support/grounded logic, or jump logic.
```

**What may be interpolated:** the player's position and orientation, for
display only (`PlayerController::GetPresentedPosition`/
`GetPresentedOrientation`, and everything downstream of them —
`GetViewMatrix`, the box passed to `Renderer::DrawBox`).

**What must never be interpolated, and isn't:** authoritative position
velocity, grounded/support state, contact/collision results, gravity
sampling position, jump state, or any gameplay decision. `FixedUpdate`'s
body is unchanged except for one addition — snapshotting the pre-step pose
as the interpolation baseline (see below) — verified directly: the exact
same fixed-step-mode harness script that produced Milestone 5's evidence
produces **byte-for-byte identical CSV output** before and after this
milestone's changes.

### Mechanism

`PlayerController` keeps two poses:

```cpp
glm::vec3 m_position;           // authoritative: end of the most recent fixed step
glm::quat m_frameOrientation;   // authoritative: end of the most recent fixed step
glm::vec3 m_previousPosition;   // presentation only: end of the step BEFORE that
glm::quat m_previousOrientation;// presentation only: end of the step BEFORE that
```

`FixedUpdate` snapshots `m_previousPosition`/`m_previousOrientation` from
the current `m_position`/`m_frameOrientation` at the very top, before
anything else that step changes them — so after `FixedUpdate` returns, the
two pairs hold exactly the two endpoints a render between now and the next
fixed step needs to blend between.

`GetPresentedPosition(alpha)`/`GetPresentedOrientation(alpha)` blend
between them:

```cpp
position    = glm::mix  (m_previousPosition,    m_position,          clamp(alpha, 0, 1));
orientation = glm::slerp(m_previousOrientation,  m_frameOrientation,  clamp(alpha, 0, 1));
```

Position uses ordinary linear interpolation (`glm::mix`); orientation uses
spherical linear interpolation (`glm::slerp`) rather than a naive
per-component lerp, because quaternion components don't blend linearly
without renormalization artifacts — `slerp` is the mathematically correct
interpolation for unit quaternions and is what GLM provides for exactly
this. Verified directly: presented "up" vectors stayed within `6.4e-5` of
unit length across a 300-frame run spanning a large reorientation — `slerp`
is doing its job.

`alpha` is computed once per render frame in `Application::Run`, right
after the fixed-step `while` loop, as
`physicsAccumulator / SimulationTiming::kFixedTimestep` — literally "how
far real time has progressed into a fixed step that hasn't been simulated
yet," the standard interpolation factor for this technique. It's clamped
to `[0, 1]` defensively inside `GetPresentedPosition`/`GetPresentedOrientation`
themselves, so even a pathological accumulator value can't produce an
out-of-range blend or extrapolate past `m_position`.

### Catch-up frames

When a stall forces the `kMaxPhysicsStepsPerFrame` cap to fire,
`Application::Run` resets `physicsAccumulator` to `0.0` (unchanged
behavior from Milestone 3) — which makes `alpha = 0.0` for that one frame,
i.e. that frame presents exactly `m_previousPosition`/`m_previousOrientation`
(one fixed step "behind" `m_position`) rather than anything invalid. This
was exercised directly, not just reasoned about: a 12-second combined
walk/turn/jump harness run naturally hit the catch-up cap twice
(`stepsThisFrame == 8`) with `alpha` staying in `[0, 1]` throughout and no
NaN/Inf anywhere in the log. No scheduler redesign was needed or attempted.

### Extended to dynamic bodies (Milestone 7-A)

Milestone 6 predicted exactly this extension without building it ahead of
need (see the git history of this section). Milestone 7-A's dynamic cubes
and spheres are the second consumer, and needed exactly the shape already
anticipated: `DynamicBody` (`src/DynamicBody.h/.cpp`) keeps its own
`m_previousPosition`/`m_previousOrientation` alongside its current pose,
and `GetPresentedPosition(alpha)`/`GetPresentedOrientation(alpha)` blend
them with the identical `glm::mix`/`glm::slerp` pair `PlayerController`
uses, driven by the identical per-frame `alpha` (`Application::drawScene`
passes the same value to every body's presented-position call that it
passes to the player's).

**One real difference from the player, worth recording precisely**: for
the player, snapshot-and-integrate both happen inside one Judas-owned
method (`PlayerController::FixedUpdate`), so the snapshot naturally comes
first in that same function. A dynamic body's actual motion happens inside
`PhysicsWorld::Step` — the physics engine's own integration and contact
resolution, called from `Application`/`TestHarness`, not from inside some
Judas-owned per-body update — so `DynamicBody::SnapshotPrevious()` has to
be called from the *outside*,
immediately before `PhysicsWorld::Step`, rather than at the top of some
Judas-owned per-body update. `PrepareDynamicBodiesForStep` does exactly
that (snapshot, then sample gravity, then hand it to
`ApplyLinearAcceleration`) for every body, once, right before
`PhysicsWorld::Step` — see "Multiple gravity consumers." The authoritative
rule is otherwise identical and just as absolute: `SyncDynamicBodiesFromPhysics`
only ever *reads* `PhysicsWorld::GetTransform` after `Step`, never writes
back, and nothing gravity/collision-related ever reads a presented value —
verified the same way as the player: the fixed-step-mode CSV output (now
carrying per-body columns — see "Automated testing") is unaffected by
whether anything downstream chooses to interpolate for rendering, because
presentation is purely a read-side view over state that was already
final.

The static sphere still needs none of this — it never moves, so it still
has no "previous state" to blend from, and `Renderer::DrawSphere` still
just takes the fixed `kSphereCenter`/`kSphereRadius` it always has.

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

Each demo planet is drawn with the exact position and radius its static
sphere shape was created with (`Application.cpp`'s `kPlanetACenter`/
`kPlanetARadius`, `kPlanetBCenter`/`kPlanetBRadius`) — there is no
physics-driven transform to read back for a static body a game never
queries, so this is one place in the renderer that isn't reading a
`PhysicsWorld::GetTransform` result, simply because nothing in this
milestone ever needs a planet's transform to change. Same reasoning for
the plank.

### Coordinate conventions (current, local to this renderer/physics setup)

- World space is a conventional right-handed 3D space in engine-defined
  "world units," used directly as Judas's own physics engine's simulation
  space.
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
Its position and orientation come from
`PlayerController::GetPresentedPosition`/`GetPresentedOrientation` (see
"Simulation/presentation boundary") rather than the authoritative state
directly, as of this milestone — so the box visibly reorients smoothly as
the player walks around the sphere, matching the physical capsule's own
up-alignment (interpolated for display; the capsule shape itself is
radially symmetric and wouldn't visually change in physics regardless).

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

Unchanged in shape from Milestone 4/5: `PlayerController::GetViewMatrix`
computes a fixed **third-person** offset — an eye point `0.7m` above the
player's capsule center, then a camera position `4m` behind that eye point
along the current look direction plus `1m` extra height along `localUp`,
looking in that same direction. `glm::lookAt`'s up vector is the player's
current `localUp`, not a fixed world `+Y` — this is what keeps the camera
from rolling incorrectly or flipping as the player walks over the sphere's
horizon.

**New this milestone:** `GetViewMatrix` now takes a `presentationAlpha`
parameter and builds `eyePosition`/`localUp`/`front`/`cameraPosition` from
`GetPresentedPosition`/`GetPresentedOrientation` (see "Simulation/
presentation boundary") instead of the raw authoritative pose directly.
This was necessary, not optional: a camera rigidly attached to discrete
fixed-step player state judders even when the world itself is stationary
and the simulation is correct, exactly as the milestone brief predicted —
diagnosing the player's own visible motion (see "Diagnosis") and fixing
only the player box while leaving the camera reading raw authoritative
state would have "fixed" the box while leaving the exact same aliasing
visible in every frame *anyway*, since the whole screen is composed
through the camera. Camera and player box are always given the *same*
`presentationAlpha` value each frame (`Application::Run` computes it once
and passes it to both), so they move in visual lockstep — there is no
scenario where the box is smooth but the world behind it isn't, or vice
versa.

**What did *not* change, deliberately:** mouse look (`m_yaw`/`m_pitch`) is
still read and applied every render frame in `UpdateFrameInput`/
`GetViewMatrix`, completely unaffected by `presentationAlpha` — it was
already at full render-frequency responsiveness before this milestone, so
there was nothing to fix there, and interpolating it would only have added
latency to look input for no benefit. This is the "camera look can safely
remain partially render-frequency-driven while physical orientation
remains fixed-step authoritative" split the brief asked to preserve where
it made sense; here it did.

No spring-arm system, cinematic smoothing, camera lag, multiple camera
modes, or configurable camera effects were added — the fix is exactly
"read the same presentation state the player box reads," not a new camera
system. No free-fly debug mode exists (unchanged reasoning from Milestone
4: it would need a mode switch, which is more camera-system scope than any
milestone so far calls for).

## Main loop structure

`Application::Run()` (`src/Application.cpp`) owns the loop:

```
Init Window, load GL functions, Init Renderer
Init PhysicsWorld (Judas's own physics engine -- nothing to zero, no built-in gravity exists)
Construct RadicalGravity for each planet, FaithfulGravity for the plank, bound via GravityContextMap
Create the two static planet spheres and the static plank box
Create PlayerController, Spawn() it (creates its capsule shape via PhysicsWorld — no body)
dynamicBodies = SpawnDynamicObjects(physicsWorld)   // 6 bodies across Planet A/plank/Planet B

while (!window.ShouldClose()):
    window.PollEvents()              // close request, Escape toggle, R/Space one-shot flags
    frameDeltaTime = measured elapsed time since last frame, clamped

    player.UpdateFrameInput(window)  // mouse look + latch jump request; every frame

    if window.ConsumeResetRequest():
        player.Reset()
        for body in dynamicBodies: body.ResetToSpawn(physicsWorld)

    accumulator += frameDeltaTime
    while accumulator >= fixedTimestep and steps < cap:
        PrepareDynamicBodiesForStep(dynamicBodies, gravity, physicsWorld, fixedTimestep)
            // snapshot presentation history, sample gravity per body,
            // ApplyLinearAcceleration — see "Multiple gravity consumers"
        physicsWorld.Step(fixedTimestep)   // Judas's own engine: broadphase, narrowphase,
                                            // contact resolution, dynamic-body integration
        player.FixedUpdate(window, physicsWorld, gravity, fixedTimestep)  // see "Locomotion";
                                            // may also push a dynamic body it swept into — see
                                            // "Player-to-object interaction"
        SyncDynamicBodiesFromPhysics(dynamicBodies, physicsWorld)
        accumulator -= fixedTimestep

    alpha = accumulator / fixedTimestep   // presentation interpolation factor; see
                                           // "Simulation/presentation boundary"

    renderer.BeginFrame(...)
    renderer.SetCamera(player.GetViewMatrix(alpha), player.GetProjectionMatrix(aspectRatio))
    renderer.DrawSphere(planetACenter, planetARadius, planetAColor)
    renderer.DrawSphere(planetBCenter, planetBRadius, planetBColor)
    renderer.DrawBox(plankCenter, identity, plankHalfExtents, plankColor)
    renderer.DrawBox(player.GetPresentedPosition(alpha), player.GetPresentedOrientation(alpha),
                      player.GetRenderHalfExtents(), playerColor)
    for body in dynamicBodies:
        renderer.DrawBox/DrawSphere(body.GetPresentedPosition(alpha),
                                     body.GetPresentedOrientation(alpha), ..., body visual)
    renderer.EndFrame()
    window.SwapBuffers()

player.Destroy(physicsWorld)
for body in dynamicBodies: physicsWorld.DestroyBody(body.Handle())
physicsWorld.DestroyBody(planetABody); physicsWorld.DestroyBody(planetBBody)
physicsWorld.DestroyBody(plankBody); physicsWorld.Shutdown()
renderer.Shutdown()
// Window's destructor tears down the GL context, the window, and SDL itself.
```

The engine-level split, updated for this milestone:

- `Window` — window/input.
- `Renderer` — graphics: sphere and box geometry, drawn once per player/
  dynamic body per frame.
- `PhysicsWorld` — Judas's own physics engine (see "Physics ownership"):
  rigid-body state/integration, collision/contact resolution for the two
  static planets, the static plank, and the dynamic test objects, plus the
  player's collision-query surface (`CreatePlayerShape`/`SweepPlayerShape`/
  `DestroyPlayerShape`) and a few generic per-body queries
  (`IsDynamicBody`, `GetLinearVelocity`/`SetLinearVelocity` — see
  "Player-to-object interaction").
- `GravityField` / `FaithfulGravity` / `RadicalGravity` — Judas's gravity
  interface and its two canonical implementations, unchanged since
  Milestones 3/5 despite everything built on top of them since — see
  "Multiple gravity consumers."
- `GravityContextMap` — routes a position to exactly one of the above by
  ownership, never blending — see "Gravity context ownership."
- `PlayerController` — owns the player's entire state and behavior
  (position, velocity, orientation, locomotion, support, jumping, camera)
  — see "Player/controller ownership."
- `DynamicBody` — the minimum shared representation for a gravity-affected,
  presentation-interpolated test object; see "Dynamic bodies."
- `Application` — wires the above together, owns the loop, and is the only
  place that knows either planet's radius/center, the plank's geometry, or
  the test objects' spawn arrangement.

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

The low-level facility that replaced `CharacterVirtual`:
`PhysicsWorld::SweepPlayerShape` — a single function answering "how far can
this shape move, and what does it touch?" `PlayerController` calls it
twice per fixed step (once for the ground probe, up to four times for
move-and-slide) and makes every interpretive decision about the results
itself. Through Milestone 7-Final's first attempt, this wrapped Jolt's own
`JPH::NarrowPhaseQuery::CastShape`; as of the physics-ownership migration
(see "Physics ownership") it's Judas's own substep-sampled march instead —
the *answer* `SweepPlayerShape` gives `PlayerController` is unchanged in
shape (a hit flag, a distance, a normal, a body handle), so nothing above
this one function needed to know the underlying mechanism changed.

**Remaining `CharacterVirtual` (or any third-party character-controller)
dependency: none, and has been none since Milestone 5.** `grep -rn
CharacterVirtual src/` finds exactly one hit, a comment in
`PlayerController.h` contrasting this milestone's architecture with
Milestone 4's for a reader's benefit — no include, no type, no call. As of
Milestone 7-Final, this is stronger still: no third-party physics
middleware of any kind is linked into this repository at all.

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
  rotated in the physics engine's own frame — `PlayerController` supplies
  its *current* `m_frameOrientation` to every `SweepPlayerShape` call, so
  the query itself always uses the up-to-date orientation).
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
loop, driving `Window`'s input from the script
(`Window::SetTestActionState`/`QueueTestMouseDelta`/`RequestTestJump`/
`RequestTestReset`, all gated behind `SetTestInputMode(true)` so they touch
nothing in the normal SDL input path) instead of a real keyboard/mouse. Two
modes, chosen by whether the script contains a `REALTIME` directive:

- **Fixed-step mode** (default): runs a fixed number of physics steps as
  fast as possible — no real-time pacing, no vsync wait. One logged row is
  one fixed step. What Milestone 5 used to verify gameplay/physics logic
  in isolation.
- **Real-time mode** (`REALTIME <renderFrameCount>`, added in Milestone 6):
  mirrors `Application::Run`'s own interactive accumulator loop exactly
  (same `SimulationTiming` constants), so it reproduces the true
  render-frame-to-fixed-step relationship headlessly. One logged row is one
  *rendered frame*, including how many fixed steps ran that frame, the raw
  authoritative position/orientation, and the presented (interpolated)
  position/orientation — see "Diagnosis" for what this mode was built to
  find. In this mode `HOLD`/`LOOK`/`TAP` step numbers mean render-frame
  index rather than fixed-step index.

Both modes print one CSV line per logged row to stdout, and, at any
step/frame the script names, render the actual scene (through a
`drawScene` callback `Application` supplies, since the harness itself has
no idea what a "sphere" or a "player box" is) and write it to a PNG via
`Renderer::CaptureFrame` + the vendored `stb_image_write.h` (public
domain/MIT, `third_party/stb_image_write.h` — writing images is a solved
commodity problem, and this single dependency-free header was preferred
over inventing a PNG encoder or adding a fetched dependency for something
this small).

The script format (one directive per line, `#` for comments) is
deliberately minimal — `STEPS`, `LOG_EVERY`, `REALTIME`, `HOLD <key>
<from> <to>`, `LOOK <dx> <dy> <atStep>`, `TAP <SPACE|R> <atStep>`,
`SCREENSHOT <atStep> <file>` — exactly the primitives needed to script a
walk/jump/look sequence and measure its timing, not a general
input-recording or automation language.

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

**Milestone 6's real-time mode repeated the trick at the presentation
layer.** The fixed-step mode alone couldn't have diagnosed Milestone 6's
judder at all — it has no concept of "render frame," only fixed steps —
which is exactly why `REALTIME` was added rather than trying to squeeze
this diagnosis out of the existing mode. With it, one measurement
(per-render-frame position deltas, and the distribution of fixed steps per
frame) was enough to distinguish "the simulation itself is unstable" from
"stable simulation, presented badly" conclusively — see "Diagnosis" for
the full numbers. The lesson generalizes: when a new milestone's problem
needs a shape of evidence the harness can't currently produce, extend it
narrowly for that evidence rather than trying to force existing modes to
answer a question they weren't built for — but don't add measurement
capability nothing has asked for yet either.

**What it deliberately is not:** a general test framework, a CI system, a
replacement for the operator's own interactive pass, or a visual-diffing
tool. It has no assertions of its own — reading the CSV output and looking
at the PNGs is still a human (or Claude) judgment call, the same as
reading any other diagnostic log. Extending it (new script directives,
automatic pass/fail checks against expected values) should wait until a
specific future milestone actually needs that, not be built speculatively
now.

**Milestone 7-A: per-body columns, no new directives.** Both CSV modes now
append six columns per dynamic body, in spawn order (`obj0PosX..Z,
obj0VelX..Z`, `obj1...`, …), reading authoritative position and linear
velocity straight from the physics engine — no presented/interpolated
columns for bodies, unlike the
player's real-time row, since the automated checks below only ever need
ground truth. `RunTestHarness`, `RunFixedStepMode`, and `RunRealtimeMode`
all take the same `std::vector<DynamicBody>&` `Application` already built,
and drive it through the identical `PrepareDynamicBodiesForStep` →
`PhysicsWorld::Step` → `SyncDynamicBodiesFromPhysics` sequence the real
game loop uses, including on `TAP R` (resetting every body via
`DynamicBody::ResetToSpawn`, not just the player). No new script
directives were needed — `STEPS`/`LOG_EVERY`/`HOLD`/`TAP`/`REALTIME`
already covered everything this milestone needed to demonstrate.

What this evidence actually showed, run against the real build:

- **Per-body gravity direction.** A 360-step run with no player input
  showed all four objects converging toward the sphere's center along
  straight lines from their own distinct starting positions — each
  object's very first logged velocity direction matched
  `normalize(sphereCenter - itsOwnPosition)` for its own position, not a
  shared direction, confirming each body is sampling `GravityField` at its
  own position rather than sharing one player-derived value.
- **World collision, no tunneling.** The same run showed all four objects
  settle to within centimeters of `sphereRadius + (halfExtent or radius)`
  and stay there — never below it (tunneling) and never sinking further —
  across the whole run.
- **Determinism.** Running the same fixed-step script twice produced
  byte-for-byte identical CSV output both times.
- **Long-run stability.** A 900-step (15 simulated second) run with all
  four bodies active logged zero non-finite (NaN/Inf) values across every
  position/velocity column.
- **Player-to-object push.** A 400-step run holding forward from spawn
  showed Cube A's velocity jump from its small falling speed to several
  m/s in the push direction around step 40 (first contact), with player
  and cube then maintaining roughly constant ~1m separation for hundreds
  of further steps — sustained pushing, not one nudge.
- **Object-to-object collision.** With Cube A's spawn placed close enough
  to Sphere A (see "Physics test world"), the same no-player 360-step run
  showed Sphere A jump from exactly zero velocity to a sustained ~1 m/s
  roll the moment Cube A's fall reached it, then continue rolling (slowly
  decaying — ordinary Jolt angular damping on a smooth sphere, not a bug)
  for hundreds of further steps, still exactly on the sphere's surface
  throughout — a real cube↔sphere collision Jolt resolved, needing no
  player or engine involvement to trigger.
- **Reset.** A run that let everything fall/settle for 200 steps, then
  issued `TAP R 200`, showed every body's logged position and velocity at
  step 200 match its step-0 (spawn) values exactly, including a body
  (Sphere A) that had picked up real velocity from the object-to-object
  collision above moments earlier.
- **Presentation non-interference.** Structurally guaranteed the same way
  as the player (see "Simulation/presentation boundary") — confirmed
  running the `REALTIME` mode with dynamic bodies and player movement both
  active produced no crash, no non-finite values, and authoritative body
  columns unaffected by the presence of the player's own presented
  columns in the same row.

Attempting to steer Cube A sideways into Sphere A via the player (rather
than via spawn placement) is also what surfaced the push mechanism's
along-normal-only limitation recorded in "Player-to-object interaction" —
a case of the harness earning its keep as a design tool, not just a
verification one.

**Milestone 7-B: `SAMPLE_GRAVITY <x> <y> <z>`, and testing the boundary,
not just the endpoints.** A new directive, printed once before the main
step/frame loop (independent of player/step state, since it only needs
`gravity.Sample(point)`): `sample,posX,posY,posZ,gravX,gravY,gravZ,gravMag`
per requested point. Existing per-step CSV rows (both modes) also gained
`gravX,gravY,gravZ` columns — the effective gravity sampled at the
player's own position that row — so a transition can be inspected without
a separate pass. Added specifically because the brief required testing the
transition *region*, not merely confirming the two endpoints separately;
neither addition needed any change to `RunFixedStepMode`/`RunRealtimeMode`'s
core loop.

This was used far more as a *design* tool than a verification one — nearly
every wrong turn in this milestone was caught by a `SAMPLE_GRAVITY` sweep
or a scripted transition run before it reached the interactive build:

- **The magnitude hard-cliff.** A `SAMPLE_GRAVITY` line sweep straight from
  the sphere surface, through the transition region, to the platform first
  suggested the transition was working (direction rotated smoothly) — but
  a closer look showed *magnitude* pinned at exactly `9.81` for the entire
  sphere zone's radius, then an instant drop to exactly `0.0` just past
  it. That's what led to the weighted-average-times-combined-weight fix in
  `GravityResolver::Sample` — see "Gravity context ownership."
- **The uniform-fade regression.** A scripted plain standing jump at the
  player's spawn point (nowhere near the platform) — a "does existing
  behavior still work" sanity check, not a transition test — showed the
  player break through into near-zero gravity and never land again. This
  is what proved a `RadicalGravity` zone centered on the sphere's own true
  center weakens gravity identically everywhere on the sphere, not just
  near the departure point, and led to the offset-falloff-center design —
  see "Gravity context ownership" (this whole design was later replaced;
  see that section for why).
- **The orientation snap.** A full scripted transition run (walk to the
  departure point, stop, jump, coast) logged a `85`-degree change in the
  player's local-up between two *consecutive* fixed steps — found by
  computing the angle between consecutive logged `up` vectors across an
  entire run, not by eyeballing individual rows. Led directly to the
  rotation-rate cap in `UpdateFrameOrientation` — see "Orientation."
- **The momentum erasure.** Comparing a hand-derived expected trajectory
  (a simple point-mass simulation of the same `GravityResolver` algorithm,
  built specifically to cross-check the engine) against the actual logged
  velocity showed the player's outward speed collapsing far faster than
  gravity's own deceleration could explain — which is what surfaced the
  velocity-continuity bug in `PlayerController::FixedUpdate` — see
  "Velocity continuity while airborne."

**Full transition, both directions, logged and inspected end to end** (not
sampled at isolated points): a scripted run walks the player from spawn to
the departure point (`HOLD D`), stops, jumps (`TAP SPACE`), and lets
`REALTIME`-equivalent fixed-step logging run through the entire crossing.
Confirmed from the logs: authoritative position/velocity/orientation/grav
columns are finite throughout; per-step orientation change never exceeds
`~2.14°` (matching the rate cap); the player lands on the platform's real
collision geometry (`grounded` flips to `1` there, not from any special
casing); the reverse direction (walk off the platform's edge, fall, land
back on the sphere) was verified the same way, ending with the player at
exactly the sphere's normal standing distance and continuing ordinary
locomotion. See "Validation report" (delivered separately to the operator)
for the specific numbers.

**Milestone 7-Final: repeating the centerline-only mistake, then not
repeating it.** The first attempt at this milestone's own gravity design
was validated the same way M7-B's had been — a `SAMPLE_GRAVITY` sweep
along the intended travel path — and passed, then failed human validation
anyway, because the actual defect (sideways contamination off the plank's
own centerline) lived in a dimension that sweep never sampled. The second,
corrected validation practice swept the FULL spatial volume before ever
showing a build to the operator again: along the plank, across its full
width (not just the center), above and below its surface, around both
planets, and at both region boundaries. This is what caught the second
design's own on-surface tilt before it needed a third human-validation
cycle to find it, and it's what confirmed the final design reads exactly
`(0,-9.81,0)` everywhere on the plank, not merely "closer to vertical than
before."

**Two new standalone unit-test executables, `judas_physics_tests` and
`judas_collision_tests` (`tests/*.cpp`), added alongside the gameplay
harness — not a replacement for it.** These test the physics PRIMITIVES in
isolation (rigid-body integration, gravity application, narrowphase
contact generation, contact resolution, gravity-context routing) with no
window, no GL context, and no `Application`/`TestHarness` machinery at
all — a different layer than `JUDAS_TEST_SCRIPT` gameplay scripts, which
still exist and still matter (see "Automated testing" above), but can't
exercise "does rotating an entire scenario rigidly produce a rigidly
rotated result" as a single fast, deterministic, headless check. Both
build as their own CMake targets and run in well under a second:

- `judas_physics_tests` — uniform gravity in 7 directions (`±X, ±Y, ±Z`,
  one arbitrary), each checked against a common reference by rotating the
  whole scenario and rotating the result back; a full two-body scenario
  (different masses, non-origin starting positions) rotated the same way;
  static bodies verified immovable under force from any of 7 directions;
  radial gravity verified to produce identical fall distance from 6
  starting directions around a sphere.
- `judas_collision_tests` — a box settles to the same resting gap on an
  axis-aligned vs. an arbitrarily-rotated plane; friction decelerates
  identically under rotation; bounce restitution matches under rotation; a
  two-box stack under non-`-Y` gravity settles bounded and finite; three
  bodies simultaneously on Planet A / the plank / Planet B each fall
  toward only their own target (zero contamination); unclaimed space
  returns exactly zero gravity and a body there coasts unchanged (no
  invented axis); a full sweep from deep in Planet A's region through the
  plank's into Planet B's matches exactly one field's raw output at every
  point, with no magnitude cliff anywhere.

The rotate-the-scenario technique used throughout both suites (build a
reference result, then rebuild the identical scenario rotated by an
arbitrary quaternion, run identical logic, rotate the result back, compare)
is the single most direct test of "no global up or down" this project has
written: if rotating the entire universe changes the physics outcome, a
hidden axis assumption exists somewhere in the code under test. All of
these currently pass; a future regression in any of them is exactly the
signal that a change quietly reintroduced a world-axis assumption.

## Remaining limitations

Recorded honestly rather than left implicit — these are known, deliberately
deferred, not oversights:

- **Fixed (was: ground vibration while walking/pushing).** What first
  looked like straight-line-sweep-on-a-curve drift turned out, once
  instrumented directly, to be a real bug: `SweepPlayerShape`'s contact
  normal used a travel-direction-based sign heuristic that Jolt's own docs
  don't call for, and which is specifically wrong for an already-touching
  (`mFraction == 0`) hit — a state a grounded step's own small inward
  gravity nudge reaches routinely. Combined with the move-and-slide loop
  never correcting *back out* when already inside the skin margin (only
  ever refusing to move closer), this produced a visible oscillation in
  ordinary walking and, in one reproduced case, a complete permanent
  lockup after enough sustained straight-line walking. Both are fixed —
  see "Locomotion," "Contact normal correctness and the 'already touching'
  case" — verified via a 60-second scripted walk that previously froze
  solid partway through (zero frozen steps afterward) and via full-
  precision radial-distance logging (a stable, bounded ~`1.2mm`
  oscillation band once settled, rather than unbounded erosion toward,
  and lockup at, zero clearance). Not related to dynamic bodies, gravity,
  or anything else Milestone 7-A added directly — the bug predates this
  milestone (present since Milestone 5) but was invisible on the small 8m
  sphere; the 20m sphere's room for sustained fast straight-line walking
  is what first made it noticeable, and what let a 60-second scripted walk
  reproduce the full lockup for diagnosis.
- **Residual ~1.2mm steady-state radial oscillation while walking**, after
  the fix above. Not evaluated for further reduction — small enough that
  it wasn't the operator's concern, and chasing it further would require
  evidence it's still visually significant, which hasn't been shown.
- **Residual small motion jitter tracking genuine render-frame timing
  variance.** After the fix, presented per-frame motion still varies by
  about `±11%` of its mean in this environment's own test conditions (down
  from `±21%`, with the qualitative freeze/lurch artifacts eliminated
  entirely — see "Diagnosis"). This tracks real, unavoidable jitter in when
  frames are actually delivered (this environment's own frame interval
  varies roughly `16.3`–`18.5ms` frame to frame) rather than being
  decoupled from it the way the original bug was. No further smoothing was
  applied on top of correct interpolation to chase this down further — a
  render loop cannot present motion more smoothly than its own frame
  delivery is spaced, and papering over that with additional smoothing
  would just be adding latency for a difference this milestone's
  diagnostics couldn't demonstrate is visible.
- **Presentation always lags authoritative simulation by up to one fixed
  step (~16.7ms).** This is the standard, accepted cost of this
  interpolation technique (render `previous → current`, never extrapolate
  past `current`) — not a bug, and not something this milestone attempts
  to hide or avoid. At 1/60s this is far below the latency budget that
  would make input feel disconnected, which is why the brief's "preserve
  input responsiveness" requirement is satisfied by construction, not by
  measurement here.
- **The player-to-object push only transfers velocity along the contact
  normal.** (Milestone 7-A — see "Player-to-object interaction".) A
  player approaching from one angle and changing direction mid-push does
  not steer the object sideways the way a full impulse/momentum transfer
  would. Discovered while devising an object-to-object collision test
  scenario, recorded rather than fixed, since a fuller push/carry system
  is explicitly out of this milestone's scope.
- **Dynamic-body rolling decay is now untested territory.** (Was:
  Milestone 7-A, under Jolt's own default angular damping — a sphere set
  rolling kept rolling for several real seconds, decaying gradually.)
  Judas's own `RigidBody`/`IntegrateRigidBody` (Milestone 7-Final) applies
  no angular damping at all — nothing was added, because no evidence yet
  shows this demo's dynamic bodies need it. A sphere set rolling by this
  engine may keep rolling considerably longer than the old Jolt-backed
  build did, or indefinitely on a level enough surface. Not evaluated
  further without evidence it's visually objectionable; if it becomes one,
  a small velocity-proportional damping term in `IntegrateRigidBody` is
  the obvious, contained fix.
- **The rotation-rate cap (`kMaxReorientationDegreesPerSecond`, `120°/s`)
  is a fixed constant, not derived from anything about the active gravity
  configuration.** It was sized against Milestone 7-B's demonstration
  (fast enough to be imperceptible during ordinary sphere walking, slow
  enough to visibly smooth a ~90-degree reorientation over roughly a
  second) and never revisited for Milestone 7-Final's own crossings, which
  turned out to need the same order of magnitude of correction (max
  observed: `2.0°`/step, matching the cap almost exactly) without further
  tuning. A future scenario needing a much faster legitimate reorientation
  would need this revisited.
- **Box-vs-box contact uses a vertex-inside-the-other-box manifold, not
  full Sutherland-Hodgman face clipping.** (Milestone 7-Final — see
  "Physics ownership.") Correctly finds all four corners of a box resting
  flat on a larger surface (verified: settles without rocking, see
  `tests/CollisionTests.cpp`), but an edge-on-edge contact between two
  similarly-sized boxes falls back to a single approximated point rather
  than a true 1-2 point edge manifold. Not evaluated further without
  evidence this demo's boxes ever rest edge-to-edge in practice.
- **The player's sweep query is substep-sampled (24 steps + bisection
  refinement), not closed-form continuous collision detection.**
  (Milestone 7-Final.) Correct and precise at this engine's actual
  per-step displacement scale (centimeters, not meters), but a
  hypothetical much faster-moving shape could tunnel between substeps in
  principle. Not a concern for this demo's walking-speed player and
  slow-moving dynamic bodies; would need revisiting for anything moving
  meaningfully faster.
- **Broadphase is brute-force all-pairs.** (Milestone 7-Final.) Exactly
  right at this demo's body count (low teens) — a spatial structure would
  be unused machinery, not a correctness requirement — but doesn't scale
  past a few dozen bodies without becoming the actual bottleneck. Left
  alone until a future milestone's body count gives real evidence it's
  needed.

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
- **Large-world rebasing** — through Milestone 7-B this relied on Jolt's
  optional double-precision build mode. As of Milestone 7-Final's own
  physics engine (`src/RigidBody.h` and friends, all plain `glm::vec3`
  single-precision), that option no longer exists — this is now an open
  question, not a preserved one, and would need real design work (either a
  precision upgrade to Judas's own math or a floating-origin/rebasing
  scheme) if/when coordinates grow past what single-precision floats
  represent well. Recorded honestly as a genuine gap this migration
  introduced, not glossed over.
- **Terrain, many collision objects, raycasts/shape queries, constraints**
  — Milestone 5 was evidence this was practical under Jolt (the player's
  entire support/movement system built from exactly one query primitive,
  `CastShape`, used twice differently); Milestone 7-Final's own
  `PhysicsWorld::SweepPlayerShape` preserves the same one-primitive shape,
  now backed by Judas's own substep-sampled sweep instead. Terrain
  collision or more query types are a matter of adding narrowphase pair
  functions to `src/Contacts.*` (sphere/box exist; a heightfield or mesh
  type would be new work, not new architecture) through the same
  `PhysicsWorld` boundary.
- **Interpolated presentation for future dynamic/simulated objects** —
  **fulfilled in Milestone 7-A**, not merely still-preserved: the
  Milestone 6 presentation boundary (previous/current pose kept per
  object, blended by one shared per-frame `alpha`) now has a real second
  consumer (`DynamicBody`, four of them) reusing the identical shape
  without rework, exactly as predicted — see "Simulation/presentation
  boundary," "Extended to dynamic bodies." Still no generic multi-object
  *framework* beyond `DynamicBody` itself and the two small free
  functions that drive it; a future object type (a moving platform, a
  thrown item) is expected to reuse `DynamicBody` or the same pattern
  directly, not a new abstraction layer.
- **Many simultaneous dynamic objects, object-to-object interaction** —
  Milestone 7-A was evidence this was practical under Jolt; Milestone
  7-Final's own engine repeats the proof with six bodies across three
  regions, sharing one `GravityContextMap` and colliding with the world
  and each other, needing no new physics architecture. The limit now
  worth naming honestly: `PhysicsWorld::Step`'s broadphase is brute-force
  all-pairs (see "Physics ownership," "Remaining limitations") — correct
  and fast enough at this demo's body count, but the thing that would need
  raising (a spatial structure, not a constant) for meaningfully more
  bodies than this milestone actually uses.

## DELIBERATELY NOT IMPLEMENTED

Explicitly deferred, not forgotten:

- Gravity-source registration, sphere-of-influence systems, or any
  generalized multi-source gravity FRAMEWORK — Milestone 7-Final's two
  planets are two hand-wired `RadicalGravity` instances in
  `Application.cpp`, not a registration system. **Gravity blending
  specifically was tried, twice (`GravityResolver`, then a first
  `GravityContextMap` design) and REMOVED, not merely never attempted** —
  see "Gravity context ownership" for why continuous field-blending turned
  out to be the wrong model for "which gravity governs a consumer," not
  just an unbuilt nice-to-have.
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
- Any physics-debug-drawing (of any kind, from any engine)
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
- **Milestone 7-A additions:** an entity/component system, scene graph, or
  prefab architecture merely because several test objects now exist —
  `DynamicBody` is a plain struct-like class with two free functions
  driving it, not a framework (see "Dynamic bodies"); runtime object
  spawning, a spawn/placement UI, or any object-authoring tool — all four
  bodies are created once in `Application::Run` and destroyed once at
  shutdown; grabbing, carrying, throwing, or any interaction beyond
  walking into an object (see "Player-to-object interaction"); gravity
  volumes, multiple simultaneous gravity *sources*, or gravity priority
  systems (Milestone 7-A proves multiple gravity *consumers* of one
  source, a different axis from multiple sources — see "Multiple gravity
  consumers"); a moving-platform or attached-rider system; object health,
  destructibility, or pooling/lifecycle management beyond create-once/
  destroy-once
- A physics material/property system beyond per-body friction/restitution —
  each planet/plank/dynamic body gets explicit, sensible values; the
  player, having no physics-engine body, has none to configure
- **Milestone 7-B additions (historical — this design was later replaced;
  see "Gravity context ownership"):** a general arbitrary gravity-volume
  editor, gravity scripting, or a "priority"/layering system for gravity
  sources — `GravityResolver` was exactly two zones, each a point and two
  radii, sized by hand for that milestone's two environments; a third
  gravity implementation, or any mode-switching added to
  `FaithfulGravity`/`RadicalGravity` themselves — both remained exactly
  what they were, and neither ever heard of the other or of
  `GravityResolver`; a directional/cone-shaped or otherwise new zone
  *shape* — the offset-falloff-center technique reused the existing
  point-and-radius zone unchanged, just placed cleverly; a mutable global
  "current gravity" variable or mode switch of any kind — every
  `GravityResolver::Sample` call re-evaluated every zone independently for
  the position it was asked about; scene loading, world streaming, or
  swapping one environment out for another — both the sphere and the
  platform existed simultaneously for the entire run; moving Milestone
  7-A's dynamic bodies into the transition (not required, not attempted);
  a generalized "air control" system — the velocity-continuity fix
  specifically does *not* let WASD affect
  airborne velocity, only prevents existing momentum from being erased;
  any animation/rotation-smoothing system independent of authoritative
  orientation — the reorientation rate cap lives in `FixedUpdate`, is
  itself authoritative state, and the Milestone 6 presentation boundary
  remains strictly downstream of it, never a substitute for it
- **Milestone 7-Final additions:** a feature-complete general physics
  engine imitating Jolt — broadphase is brute-force all-pairs, box-vs-box
  uses a vertex-inside manifold rather than full clipping, the player's
  sweep is substep-sampled rather than closed-form continuous collision
  detection, and none of that was treated as a gap to fill beyond this
  demo's own needs (see "Physics ownership," "Remaining limitations");
  sleeping/waking (no evidence any body in this demo needs to stop being
  simulated when at rest — the body count is small enough that always
  stepping every body has no measured cost); a general constraint solver
  beyond contact resolution (no joints, hinges, or springs anywhere in
  this milestone); per-consumer gravity-context state or hysteresis — see
  "Gravity context ownership" for why position-only resolution is
  sufficient for THIS milestone's static environments, explicitly NOT
  documented as a permanent law; large-world/double-precision coordinates
  — Judas's own physics math is single-precision `glm`, a genuine
  capability this migration removed rather than preserved (see "FUTURE
  CONSTRAINTS PRESERVED"); any third gravity-field implementation or
  change to `FaithfulGravity`/`RadicalGravity` themselves — the plank's
  entire solution was reusing `FaithfulGravity` completely unmodified
