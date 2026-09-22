# Project Judas

A purpose-built game engine for a future game involving large spherical
planets, spacecraft, arbitrary gravity, procedural terrain, and seamless
transitions between planets, ships, and open space.

This is **not** a general-purpose engine and is not trying to compete with
Unity, Unreal, or Godot. It exists to serve one specific class of game, and
its architecture is deliberately narrow.

## Status: Milestone 10

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
real edge-case bug found and fixed along the way). That's it — no
mantling, climbing, terrain, real planets, vehicles, shadows, or further
gameplay yet.

This is intentional — see `docs/ARCHITECTURE.md` for why, what's
deliberately not built yet, and how this small foundation avoids blocking
the much larger long-term design. Earlier milestones are preserved as git
tags (`milestone-1` through `milestone-10`, once this one is tagged) rather
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
texture (`assets/models/beacon.obj`, `assets/textures/beacon.png`) are
loaded via paths relative to the current working directory, and `judas`
reports an error and exits if it's run from somewhere else and can't find
them.

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
| Release/recapture mouse | `Escape`               |
| Take/release control of the flying primitive (only while standing on it) | `F` |
| Reset the player and world | `R`               |

While controlling the flying primitive (after pressing `F` while standing
on it), WASD/turn keys mean something different — see "The flying
primitive" below.

Close the window normally (window controls / `Alt+F4` / etc.) to exit.

### Steps and slopes

A short staircase and one ramp stand a walk from spawn on Planet A —
just walk into them. Ordinary steps and low ledges (including the flying
primitive's own edge — see below) are climbed and descended automatically
while walking; nothing extra to press. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 10," for how
this works and its limits (how tall a step counts, how steep a slope stays
walkable).

### The flying primitive

A flat, boardable box rests on the plank. Walk straight into it — as of
Milestone 10 its edge is a normal step, not a wall, so it's boarded by
ordinary walking, no jump needed (see "Milestone 10" below) — and press
`F` to take control:

| Action                    | Keys        |
|----------------------------|-------------|
| Move forward / backward    | `W` / `S`   |
| Turn (yaw) left / right    | `A` / `D`   |
| Ascend / descend           | `E` / `Q`   |
| Release control            | `F`         |

"Ascend/descend" is relative to whichever gravity currently governs the
primitive's own position (see `docs/ARCHITECTURE.md`), not a fixed world
axis — consistent with how nothing else in this engine assumes a
universal up either. Mouse look still moves the camera freely while
flying. Pressing `F` again hands input authority straight back to the
player, who can walk and jump normally immediately afterward.

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

Four standalone, headless test executables also exist (no window, no GL
context): `judas_physics_tests` and `judas_collision_tests` (rigid-body/
collision/gravity-context primitives); `judas_asset_tests` (Milestone 9 —
model/texture loading, parses the real committed demo assets and checks
vertex/index/UV/normal data and texture dimensions; run from the
repository root, same reason as `judas` itself); and `judas_step_climb_tests`
(Milestone 10 — the step-up/step-down primitives behind automatic stair
climbing, including the same rotate-the-whole-scenario "no global up"
check the physics suites use):

```bash
cmake --build build --target judas_physics_tests judas_collision_tests judas_asset_tests judas_step_climb_tests
./build/judas_physics_tests && ./build/judas_collision_tests && ./build/judas_asset_tests && ./build/judas_step_climb_tests
```

## Assets

`assets/models/beacon.obj` and `assets/textures/beacon.png` are original
content authored for this project (not derived from any external asset) —
public domain / CC0-equivalent, redistributable without restriction. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 9, Assets," for
the full provenance note.

## License

MIT — see [`LICENSE`](LICENSE). Do what you like with it.
