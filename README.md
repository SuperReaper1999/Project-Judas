# Project Judas

A purpose-built game engine for a future game involving large spherical
planets, spacecraft, arbitrary gravity, procedural terrain, and seamless
transitions between planets, ships, and open space.

This is **not** a general-purpose engine and is not trying to compete with
Unity, Unreal, or Godot. It exists to serve one specific class of game, and
its architecture is deliberately narrow.

## Status: Milestone 7-Final

A controllable player walks, jumps, and falls under real physics — **as of
this milestone, Judas's own physics engine, not a third-party library** —
across two independent spherical worlds ("planets," radius `20m` each,
`55m` apart) connected by a flat plank. Each planet has its own **radial**
gravity pulling toward its own center; the plank has its own **uniform**
gravity matching its own flat surface. There is no universal "up": the
player's own sense of up continuously reorients to match whichever gravity
context currently governs it, and which context governs a given position
is decided by simple ownership — a position belongs to exactly one world,
never a blend of two. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the two earlier
gravity-context designs that each passed automated checks and still failed
interactive validation before this one, why, and why Jolt Physics was
removed entirely along the way.

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
honest retrospective on how two earlier attempts got that wrong. That's
it — no lighting, terrain, real planets, or gameplay yet.

This is intentional — see `docs/ARCHITECTURE.md` for why, what's
deliberately not built yet, and how this small foundation avoids blocking
the much larger long-term design. Earlier milestones are preserved as git
tags (`milestone-1` through `milestone-7final`) rather than kept running
alongside the current demo.

## Building

### Requirements

- Linux (developed and tested on Ubuntu)
- CMake 3.20+
- A C++17 compiler (GCC or Clang)
- SDL2 development headers
- GLM development headers

No physics-engine dependency to fetch — Judas owns its own physics (see
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Physics ownership"), so
configuring needs no network access at all.

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
| Reset the player        | `R`                    |

Close the window normally (window controls / `Alt+F4` / etc.) to exit.

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

## License

MIT — see [`LICENSE`](LICENSE). Do what you like with it.
