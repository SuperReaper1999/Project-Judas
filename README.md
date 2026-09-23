# Project Judas

A purpose-built game engine for a future game involving large spherical
planets, spacecraft, arbitrary gravity, procedural terrain, and seamless
transitions between planets, ships, and open space.

This is **not** a general-purpose engine and is not trying to compete with
Unity, Unreal, or Godot. It exists to serve one specific class of game, and
its architecture is deliberately narrow.

## Status: Milestone 13

A controllable player walks, jumps, and falls under real physics — Judas's
own physics engine, not a third-party library — across two independent
spherical worlds ("planets," radius `20m` each, `55m` apart) connected by a
flat plank. Each planet has its own **radial** gravity pulling toward its
own center; the plank has its own **uniform** gravity matching its own flat
surface. There is no universal "up": the player's own sense of up
continuously reorients to match whichever gravity context currently
governs it, and which context governs a given position is decided by
simple ownership — a position belongs to exactly one world, never a blend
of two. See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the two
earlier gravity-context designs that each passed automated checks and
still failed interactive validation before this one, why, and why Jolt
Physics was removed entirely along the way.

Both planets and the plank host ordinary dynamic bodies (cubes and
spheres) that sample the same Judas-owned gravity the player does, purely
from their own position — proof that gravity was never player-specific or
region-specific. They fall, land, roll, and collide with the world and
each other under real physics; walk into one and you can push it.

The player can walk from Planet A, onto the plank, across it, onto
Planet B, and back. Gravity hands off coherently at every boundary,
support is always collision-derived (never a gravity-region event), and
the plank reads as ordinary flat ground everywhere on its surface,
including its edges — no sideways pull toward either planet, anywhere on
it. See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the full,
honest retrospective on how two earlier attempts got that wrong.

**New this milestone:** a controllable flying primitive rests on the
plank. Walk (or hop) onto it and press `F` to take control — WASD/Q/E fly
it around in full 3D, A/D turn it — then press `F` again to hand control
back to the player. The player stays a real, physically simulated
participant throughout: standing on the primitive while it translates,
climbs, or turns carries the player along coherently, and jumping off (or
letting go of control while it's moving) preserves whatever motion it was
imparting, rather than snapping the player back to a fixed offset. See
`docs/ARCHITECTURE.md`, "Milestone 8," for how input authority moves
between the player and the primitive and the moving-support physics fix
that made carrying the player during flight work correctly.

**New this milestone:** the renderer can load a real static model from
disk, texture it, and light it. A small hand-authored beacon (a low-poly
pyramid, `assets/models/beacon.obj`) stands near the player's spawn point
on Planet A, wearing a real texture (`assets/textures/beacon.png`) and
shaded by one small ambient term plus one directional light — every
existing box and sphere in the scene (planets, plank, player, dynamic
bodies, the flying primitive) now carries real surface normals and is lit
the same way, through the same shader. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 9," for the
model/texture format choices, the mesh/texture ownership boundary, and the
exact lighting model.

**New this milestone:** ordinary walking is deliberately pleasant to play,
not just functionally correct. Starting, stopping, and reversing direction
now accelerate and decelerate smoothly instead of snapping instantly to a
new velocity; the player can nudge their movement modestly while airborne
without losing existing momentum; and a short staircase plus one ramp
(a walk from spawn on Planet A) can be climbed and descended just by
walking into them — no jump required. The same mechanism makes the flying
primitive's own low edge naturally boardable now too. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 10," for the
exact acceleration model and the step-up/step-down design (including a
real edge-case bug found and fixed along the way).

**New this milestone:** the flying primitive from Milestone 8 is now a
proper spacecraft. Board it and press `F` as before, but control is now
full 6-degree-of-freedom: translate along all three of the spacecraft's
own local axes and pitch/yaw/roll it independently, entirely relative to
its own current orientation, never a fixed world direction. The player is
now **physically secured** to it while piloting — not merely carried like
an ordinary moving platform (Milestone 8's mechanism, still exactly what
runs whenever nobody is piloting it) — so rolling the spacecraft upside
down under gravity does not make the pilot fall off; it stays exactly
where it sat down through any combination of translation and rotation.
Releasing control (`F` again) preserves the player's exact position and
orientation and hands it the spacecraft's own real velocity at that
instant, including the extra motion a rotating spacecraft imparts at an
off-center point — no reset, no snap to a fixed seat, no freeze. It now
renders as a real imported model (`assets/models/plane.obj`) instead of a
plain box, through the same Milestone 9 model/texture/lighting path. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 11," for the
secured-pilot attachment mechanism and what this deliberately isn't (no
vehicle framework, no artificial gravity, no orbital mechanics, no
multiple spacecraft).

**New this milestone:** the spacecraft's controls are genuine force/
torque-driven inertia, not a directly commanded speed. Holding a
translation key applies real force (accelerating it gradually, per
`F = ma`, using its actual mass); holding a rotation key applies real
torque (spinning it up gradually, per its actual inverse inertia tensor —
pitch, yaw, and roll genuinely accelerate at different rates, since the
spacecraft isn't shaped the same along all three axes). **Releasing every
key does not stop it** — with no force acting, it keeps moving at exactly
the velocity it had; with no torque acting, it keeps rotating at exactly
the angular velocity it had. Turning the nose does not turn existing
momentum: build up speed, let go, spin the ship around, and it keeps
travelling the original way — now effectively flying backwards — until
thrust is applied against that motion to actually slow it down, stop it,
and reverse it. See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md),
"Milestone 12," for the exact force/torque magnitudes, the real numeric
evidence for all of this, and a couple of honestly-documented rough edges
(rotating while still resting on the ground is realistically stiff;
a very hard, fast collision can shed more speed than gentle friction alone
would suggest). That's it — no mantling, climbing, terrain, real planets,
more than one controllable vehicle, shadows, or further gameplay yet.

**New this milestone:** Judas has its own UI system — a persistent HUD and
a working pause menu, both rendered through a new screen-space overlay
path behind the same raw-GL boundary every other draw call already
respects. A small panel in the top-left corner always shows five genuinely
live values: grounded/airborne, current local gravity magnitude, whether
you're controlling the player or the spacecraft, pilot-attachment state,
and the spacecraft's current speed. Press `Escape` to pause: the world
freezes completely (see below), the screen dims, and a menu appears with
`Resume`, `Options`, and `Quit` — `Options` is one nested screen with a
real "Show HUD" toggle. Navigate with the arrow keys and `Enter`, or point
and click with the mouse (the cursor is released automatically while a
menu is open, and recaptured the instant it closes). `Escape` itself is
context-sensitive: it opens the menu from gameplay, backs out of the
nested screen to the root, and closes the menu entirely (resuming) from
the root — matching the required flow of gameplay -> pause -> nested ->
back -> resume -> gameplay. **Pausing freezes the simulation completely**,
not just input: no physics stepping, no gravity, no player movement,
while paused — chosen deliberately over "keep simulating behind the menu"
so a coasting Milestone 12 spacecraft doesn't keep drifting while its
pilot is stuck in a menu unable to react. Text is drawn with a newly
vendored font loader (`stb_truetype`, baking DejaVu Sans into one GPU
atlas — see `assets/fonts/DejaVuSans-LICENSE.txt` for its license). See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 13," for the
full design, the single input-ownership boundary that keeps gameplay code
from needing to know a menu exists, and the pause/simulation policy.

This is intentional — see `docs/ARCHITECTURE.md` for why, what's
deliberately not built yet, and how this small foundation avoids blocking
the much larger long-term design. Earlier milestones are preserved as git
tags (`milestone-1` through `milestone-13`, once this one is tagged) rather
than kept running alongside the current demo.

## Building

### Requirements

- Linux (developed and tested on Ubuntu)
- CMake 3.20+
- A C++17 compiler (GCC or Clang)
- SDL2 development headers
- GLM development headers

No physics-engine dependency to fetch — Judas owns its own physics (see
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Physics ownership"), so
configuring needs no network access at all. Model/texture loading
(Milestone 9) needs no extra system packages either — `tinyobjloader` and
`stb_image` are single-header, vendored dependencies (`third_party/`,
committed to the repository, same as the pre-existing `stb_image_write.h`)
rather than fetched or installed separately.

On Ubuntu/Debian:

```bash
sudo apt install cmake libsdl2-dev libglm-dev build-essential
```

### Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
```

### Run

```bash
./build/judas
```

Run from the repository root (as shown above) — Milestone 9's demo model/
texture (`assets/models/beacon.obj`, `assets/textures/beacon.png`) and
Milestone 11's spacecraft model (`assets/models/plane.obj`) are loaded via
paths relative to the current working directory, and `judas` reports an
error and exits if it's run from somewhere else and can't find them.

## Controls

The mouse is captured on launch and controls where the player looks. WASD
walks the player relative to that look direction and the current surface
(not a free-flying camera).

| Action                  | Keys                  |
|-------------------------|-----------------------|
| Walk forward            | `W` or `Up Arrow`     |
| Walk backward           | `S` or `Down Arrow`   |
| Strafe left             | `A` or `Left Arrow`   |
| Strafe right            | `D` or `Right Arrow`  |
| Jump (only while grounded) | `Space`             |
| Look around             | Mouse movement        |
| Pause / back / resume   | `Escape`               |
| Take/release piloting control of the spacecraft (only while standing on it) | `F` |
| Reset the player and world | `R`               |

While piloting the spacecraft (after pressing `F` while standing on it),
WASD/Q/E and a separate IJKL+U/O cluster mean something different — see
"The spacecraft" below.

Close the window normally (window controls / `Alt+F4` / etc.) to exit.

## HUD and pause menu

A small panel in the top-left corner always shows five live values:
grounded/airborne, the current local gravity magnitude, whether you're
controlling the player or the spacecraft, pilot-attachment state, and the
spacecraft's current speed.

Press `Escape` to pause. The mouse is released automatically (no need to
press anything else to get a usable cursor) and the world freezes
completely — nothing moves, including the spacecraft, until you resume.

| Menu action              | Keys                          |
|---------------------------|-------------------------------|
| Navigate up / down        | `Up Arrow` / `Down Arrow`     |
| Activate the focused button | `Enter`                     |
| Hover / click a button    | Mouse movement / left click   |
| Back one level / resume   | `Escape`                      |

From the pause screen, `Options` opens one nested screen with a single
"Show HUD" toggle — `Escape` (or the `Back` button) returns to the pause
screen; `Escape` again (or `Resume`) closes the menu and hands input back
to gameplay, recapturing the mouse automatically. `Quit` closes the
application from the menu, same as closing the window normally. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 13," for the
full design and why pausing freezes the simulation rather than merely
suppressing input.

### Steps and slopes

A short staircase and one ramp stand a walk from spawn on Planet A —
just walk into them. Ordinary steps and low ledges (including the
spacecraft's own edge — see below) are climbed and descended automatically
while walking; nothing extra to press. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 10," for how
this works and its limits (how tall a step counts, how steep a slope stays
walkable).

### The spacecraft

A small aircraft-shaped spacecraft (`assets/models/plane.obj`) rests on
the plank — the Milestone 8 flying primitive, repurposed, not replaced.
Walk straight into it — its edge is a normal step, not a wall, so it's
boarded by ordinary walking, no jump needed — and press `F` to take
control. The player is physically **secured** to it the instant control
is taken (see `docs/ARCHITECTURE.md`, "Milestone 11") — it can be flown
upside down, sideways, or through any combination of rotations without
the pilot falling off:

| Action                             | Keys        |
|-------------------------------------|-------------|
| Move forward / backward             | `W` / `S`   |
| Strafe left / right                 | `A` / `D`   |
| Ascend / descend (local up/down)    | `E` / `Q`   |
| Pitch up / down                     | `I` / `K`   |
| Yaw left / right                    | `J` / `L`   |
| Roll left / right                   | `U` / `O`   |
| Release control                     | `F`         |

Every one of these is relative to the spacecraft's OWN current
orientation — never gravity, never a fixed world axis (consistent with
how nothing else in this engine assumes a universal up either). After a
180-degree roll, "ascend" still means "toward the spacecraft's own roof,"
whatever direction that now points in world space. Mouse look still moves
the camera freely (a free look independent of the spacecraft's own
attitude, driven purely by mouse motion) and is never also applied to the
spacecraft's own orientation — attitude control is keyboard-only,
specifically so the two never double up on the same input.

**As of Milestone 12, every key above applies FORCE or TORQUE, not a
speed.** Holding forward accelerates gradually rather than snapping to a
fixed speed; letting go doesn't stop the spacecraft — it keeps coasting
at whatever velocity it had, indefinitely, until something (more thrust,
gravity, or a collision) changes it. The same is true of rotation: torque
builds angular velocity, and releasing the key leaves it spinning. To
actually slow down or stop turning, apply force or torque in the opposite
sense — there is no braking, damping, or auto-level anywhere in this
engine. See `docs/ARCHITECTURE.md`, "Milestone 12," for the exact force/
torque magnitudes and the numeric evidence behind all of this. One
practical note: while still resting on the plank, ordinary ground friction
can make rotation feel stiff or entirely unresponsive (a real, physically
correct effect of this demo's friction coefficient, not a bug) — ascend a
little first if turning in place doesn't seem to do anything.

Pressing `F` again hands input authority straight back to the player,
releases the attachment, preserves exactly where the player was, and
gives it the spacecraft's own real velocity at that instant (including a
rotating spacecraft's own angular contribution) — see
`docs/ARCHITECTURE.md`, "Milestone 11," for the exact formula and what
happens if nothing is supporting the player at that point (it falls, same
as anyone else).

## Automated testing (developer tooling)

Judas can also run headlessly, driven by a scripted input sequence instead
of a real keyboard/mouse, logging player state and optionally dumping
screenshots — including a real-time mode that reproduces actual
render-frame timing for diagnosing presentation/smoothness issues. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md#automated-testing) for the
full script format:

```bash
JUDAS_TEST_SCRIPT=path/to/script.txt ./build/judas
```

Six standalone, headless test executables also exist (no window, no GL
context): `judas_physics_tests` and `judas_collision_tests` (rigid-body/
collision/gravity-context primitives); `judas_asset_tests` (Milestone 9 —
model/texture loading, parses the real committed demo assets and checks
vertex/index/UV/normal data and texture dimensions; run from the
repository root, same reason as `judas` itself); `judas_step_climb_tests`
(Milestone 10 — the step-up/step-down primitives behind automatic stair
climbing, including the same rotate-the-whole-scenario "no global up"
check the physics suites use); `judas_pilot_attachment_tests` (Milestone
11 — the secured-pilot attachment math: acquisition/release round trips,
translation and rotation including a 180-degree roll, zero-drift under
many repeated rotations, and the release-velocity formula's angular
contribution, plus the same rotate-the-whole-scenario check); and
`judas_spacecraft_control_tests` (Milestone 11's control-mapping suite,
rewritten for Milestone 12's force/torque-driven inertia — see
`docs/ARCHITECTURE.md`, "Milestone 12" — rather than extended: it now
calls `PhysicsWorld::Step` after every control call and measures the real
integrated result, covering sustained-thrust acceleration, coasting after
release, orientation/velocity independence, counter-thrust, perpendicular
thrust, F/m mass response, angular coasting, counter-torque, the real
inverse-inertia-tensor's per-axis response, gravity+thrust composition, a
no-op while uncontrolled, and the same rotate-the-whole-scenario check);
and `judas_ui_tests` (Milestone 13 — the UI navigation/input-ownership
logic: the screen stack, focus navigation and hit-testing, the pause
menu's exact required open/nested/back/resume flow, the HUD-visibility
toggle's persistent state, and the same boolean the real game loop gates
gameplay input on — pure CPU logic, no window/GL/font; UI appearance and
real interaction are human-validated instead, see `docs/ARCHITECTURE.md`,
"Milestone 13, Automated evidence"):

```bash
cmake --build build --target judas_physics_tests judas_collision_tests judas_asset_tests judas_step_climb_tests judas_pilot_attachment_tests judas_spacecraft_control_tests judas_ui_tests
./build/judas_physics_tests && ./build/judas_collision_tests && ./build/judas_asset_tests && ./build/judas_step_climb_tests && ./build/judas_pilot_attachment_tests && ./build/judas_spacecraft_control_tests && ./build/judas_ui_tests
```

## Assets

`assets/models/beacon.obj` and `assets/textures/beacon.png` (Milestone 9),
and `assets/models/plane.obj` (Milestone 11), are original content
authored for this project (not derived from any external asset) — public
domain / CC0-equivalent, redistributable without restriction. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 9, Assets," and
"Milestone 11," for the full provenance notes.

`assets/fonts/DejaVuSans.ttf` (Milestone 13) is the DejaVu Sans font,
under the Bitstream Vera License (a permissive, redistribution-friendly
license) — see [`assets/fonts/DejaVuSans-LICENSE.txt`](assets/fonts/DejaVuSans-LICENSE.txt)
for the full text. Rasterized at runtime via `stb_truetype`
(`third_party/stb_truetype.h`, public domain).

## License

MIT — see [`LICENSE`](LICENSE). Do what you like with it.
