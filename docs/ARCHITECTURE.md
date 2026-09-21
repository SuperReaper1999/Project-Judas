# Architecture

This document explains the technical decisions behind Project Judas and how
they relate to the engine's long-term purpose. It is meant to be readable by
someone with no prior context on this project. It is updated in place as
milestones land, rather than kept as a per-milestone snapshot — see
"Milestone history" below for how to recover an earlier milestone exactly.

## What exists right now (Milestone 7-A)

Open a window. A large static sphere (radius `20m`, up from Milestone 5/6's
`8m` — see "Physics test world") exists in 3D space with **radial** gravity
pulling toward its center. A Judas-owned player (not a Jolt character
controller of any kind — see "Player/controller ownership") falls onto the
sphere, stands on its curved surface, and can walk all the way around it —
including onto what would have been "the side" or "the underside" from the
spawn point's perspective — because the player's own sense of "up"
continuously reorients to match whichever way gravity is currently
pulling. `Space` jumps away from the local surface; gravity brings the
player back and Jolt's own collision queries detect the landing. `R`
resets the player. Ordinary locomotion is visually smooth: the fixed-step
simulation is unchanged, but what gets *rendered* each frame is a
presentation-only interpolation between two authoritative simulation
states rather than the latest one presented directly — see "Diagnosis" and
"Simulation/presentation boundary" below.

As of this milestone, the sphere also carries four ordinary Jolt dynamic
bodies (two cubes, two spheres) scattered around it — see "Physics test
world" and "Dynamic bodies." Each one samples the same `GravityField` the
player does, purely from its own current position, with no knowledge of
the sphere, the player, or which `GravityField` implementation is active.
They fall, land, roll, and collide with the sphere and with each other
under ordinary Jolt rigid-body dynamics — Judas supplies acceleration only,
never orientation or resting position. The player can walk into one and
push it — see "Player-to-object interaction." Nothing else. See the root
`README.md` for build/run instructions and controls.

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

### Contact normal correctness and the "already touching" case

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

Added in Milestone 7-A. The player is still not a Jolt body (see
"Player/controller ownership") — `SweepPlayerShape` is a read-only query,
never something Jolt's own contact solver resolves — so contact with a
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
one velocity value and hands control straight back to Jolt, which owns
everything that happens to the object from that point on (further
integration, friction, contact with the sphere or another object). Static
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

The demo sphere's radius grew from Milestone 5/6's `8m` to `20m`
(`kSphereRadius`, `src/Application.cpp`) — still a small, hand-authored
test environment, not a planet (see "Deliberately Not Implemented"), but
large enough to hold the player and four separated dynamic objects without
crowding the same few square meters, and to give a `4 m/s` walking player
room to actually traverse distance between them. `20m` was chosen, not
derived: large enough for the arrangement below to read as spatially
separate locations with visibly different local gravity directions, small
enough that a full "lap" (~125m circumference) stays a short, purposeful
walk rather than a trek.

This radius, like the sphere's existence at all, is **demo/composition-root
knowledge only** — `Application.cpp`'s anonymous namespace is the only
place it's named. `PlayerController`, `DynamicBody`, `GravityField`, and
`PhysicsWorld` remain exactly as ignorant of it as they were in Milestone
5/6; nothing about this milestone required loosening that boundary.

**Object arrangement** (`kDynamicObjectSpawns`, `Application.cpp`): four
bodies, placed via a small demo-only helper, `PointAboveSphere(center,
radius, direction, heightAboveSurface)`, that normalizes `direction` and
places a point that far beyond the sphere's own radius — a placement
convenience, not something any engine type provides or needs.

- **Cube A** and **Sphere A** spawn close together (~1.3m apart at rest),
  near the player's own spawn point — Cube A begins `2m` above the surface
  and visibly falls onto it (falling onto the surface); Sphere A begins
  already resting on it (`0.05m` clearance, effectively touching at
  spawn — resting on the surface). Both are within an easy walk of the
  player's spawn, for the push test above, and close enough that Cube A's
  own fall visibly disturbs Sphere A on landing (see "Automated testing"
  for the logged evidence) — an object-to-object (cube↔sphere) collision
  that needs no player involvement to demonstrate.
- **Cube B** and **Sphere B** spawn at deliberately different locations
  around the sphere (roughly the "equator" and the far "pole" relative to
  the player's spawn point), each falling or resting independently, so
  their own local gravity direction is visibly different from the
  player's and from Cube A/Sphere A's — see "Multiple gravity consumers"
  for the numerical confirmation.

Exact coordinates are demo-authoring detail, not architecture — see
`Application.cpp` if the literal numbers matter. Nothing about this
arrangement is procedural, spawned at runtime, or editable; all four
bodies are created once in `Application::Run` (via `SpawnDynamicObjects`)
and destroyed once at shutdown, exactly like the static sphere already
was.

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

**How Jolt owns the rest**: once gravity is handed over, `DynamicBody` (and
everything above it) steps back completely. `PhysicsWorld::Step` — ordinary
Jolt rigid-body integration, friction, restitution, and contact resolution
— decides the body's resulting position **and orientation**. Nothing in
this engine ever writes a dynamic body's orientation directly or aligns it
to local gravity/up; a cube settles however contact with the sphere leaves
it, and a sphere is free to roll. This mirrors "Support" above (gravity
direction and contact response stay separately computed) applied to a body
Judas doesn't move itself at all.

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
`PhysicsWorld::Step` — Jolt's own integration, not Judas's — so
`DynamicBody::SnapshotPrevious()` has to be called from the *outside*,
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
Init PhysicsWorld (registers Jolt types, zeroes Jolt's own gravity)
Construct a RadicalGravity centered on the demo sphere, bound to a GravityField&
Create the static sphere body
Create PlayerController, Spawn() it (creates its capsule Shape via PhysicsWorld — no body)
dynamicBodies = SpawnDynamicObjects(physicsWorld)   // Milestone 7-A: 4 ordinary Jolt bodies

while (!window.ShouldClose()):
    window.PollEvents()              // close request, Escape toggle, R/Space one-shot flags
    frameDeltaTime = measured elapsed time since last frame, clamped

    player.UpdateFrameInput(window)  // mouse look + latch jump request; every frame

    if window.ConsumeResetRequest():
        player.Reset()
        for body in dynamicBodies: body.ResetToSpawn(physicsWorld)   // Milestone 7-A

    accumulator += frameDeltaTime
    while accumulator >= fixedTimestep and steps < cap:
        PrepareDynamicBodiesForStep(dynamicBodies, gravity, physicsWorld, fixedTimestep)
            // Milestone 7-A: snapshot presentation history, sample gravity per body,
            // ApplyLinearAcceleration — see "Multiple gravity consumers"
        physicsWorld.Step(fixedTimestep)   // advances every Jolt body: sphere contact,
                                            // dynamic-body integration, object<->object contact
        player.FixedUpdate(window, physicsWorld, gravity, fixedTimestep)  // see "Locomotion";
                                            // may also push a dynamic body it swept into — see
                                            // "Player-to-object interaction"
        SyncDynamicBodiesFromPhysics(dynamicBodies, physicsWorld)   // Milestone 7-A
        accumulator -= fixedTimestep

    alpha = accumulator / fixedTimestep   // presentation interpolation factor; see
                                           // "Simulation/presentation boundary"

    renderer.BeginFrame(...)
    renderer.SetCamera(player.GetViewMatrix(alpha), player.GetProjectionMatrix(aspectRatio))
    renderer.DrawSphere(sphereCenter, sphereRadius, sphereColor)
    renderer.DrawBox(player.GetPresentedPosition(alpha), player.GetPresentedOrientation(alpha),
                      player.GetRenderHalfExtents(), playerColor)
    for body in dynamicBodies:            // Milestone 7-A
        renderer.DrawBox/DrawSphere(body.GetPresentedPosition(alpha),
                                     body.GetPresentedOrientation(alpha), ..., body visual)
    renderer.EndFrame()
    window.SwapBuffers()

player.Destroy(physicsWorld)
for body in dynamicBodies: physicsWorld.DestroyBody(body.Handle())   // Milestone 7-A
physicsWorld.DestroyBody(sphere); physicsWorld.Shutdown()
renderer.Shutdown()
// Window's destructor tears down the GL context, the window, and SDL itself.
```

The Milestone 3/4 flat floor is not part of this milestone's active demo
(recoverable via the `milestone-4` tag) — a flat floor under
`FaithfulGravity` and a sphere under `RadicalGravity` active in the same
scene would need two different simultaneous gravity fields, which
contradicts "the composition root selects one implementation," so keeping
it would have meant either an incoherent scene or building multi-source/
composite gravity that this milestone explicitly excludes. Milestone 3's
original dynamic cube, specifically, is effectively superseded by
Milestone 7-A's own dynamic test objects (see "Physics test world") —
`PhysicsWorld::CreateDynamicBox`/`CreateDynamicSphere` are now genuinely
exercised by the active demo rather than sitting unused.

The engine-level split, updated for this milestone:

- `Window` — window/input.
- `Renderer` — graphics: sphere and box geometry, drawn once per player/
  dynamic body per frame.
- `PhysicsWorld` — physics: ordinary Jolt bodies (the static sphere and
  Milestone 7-A's dynamic test objects) plus the player's collision-query
  surface (`CreatePlayerShape`/`SweepPlayerShape`/`DestroyPlayerShape` —
  replacing Milestone 4's `CharacterVirtual`-backed methods) plus a few
  generic per-body queries added this milestone (`IsDynamicBody`,
  `GetLinearVelocity`/`SetLinearVelocity` — see "Player-to-object
  interaction").
- `GravityField` / `FaithfulGravity` / `RadicalGravity` — Judas's gravity
  interface and its two implementations, now consumed by more than one
  kind of caller — see "Multiple gravity consumers."
- `PlayerController` — owns the player's entire state and behavior
  (position, velocity, orientation, locomotion, support, jumping, camera)
  — see "Player/controller ownership."
- `DynamicBody` (Milestone 7-A) — the minimum shared representation for a
  gravity-affected, presentation-interpolated test object; see "Dynamic
  bodies."
- `Application` — wires the above together, owns the loop, and is the only
  place that knows the sphere's radius/center or the test objects' spawn
  arrangement.

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
obj0VelX..Z`, `obj1...`, …), reading authoritative position and Jolt
linear velocity — no presented/interpolated columns for bodies, unlike the
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
- **Dynamic-body rolling decays slowly.** (Milestone 7-A.) A sphere set
  rolling by a collision (see "Automated testing") keeps rolling for
  several real seconds, decaying only via Jolt's own default angular
  damping rather than any rolling-resistance model — physically
  unsurprising for a smooth sphere on a smooth-ish surface, not a
  stability bug (verified finite/stable over a much longer window than it
  takes to visibly slow down), and not something this milestone tunes
  further.

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
  Milestone 7-A is itself the evidence this is practical: four ordinary
  Jolt dynamic bodies, sharing one `GravityField` and colliding with the
  world and each other, needed no new physics architecture — `kMaxBodies`/
  `kMaxBodyPairs`/`kMaxContactConstraints` (`PhysicsWorld.cpp`, currently
  `128`, sized for this milestone's handful of bodies) are the only limits
  that would need raising for more.

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
  the sphere gets explicit, sensible values; the player, having no Jolt
  body, has none to configure
