# Project Judas

A purpose-built game engine for a future game involving large spherical
planets, spacecraft, arbitrary gravity, procedural terrain, and seamless
transitions between planets, ships, and open space.

This is **not** a general-purpose engine and is not trying to compete with
Unity, Unreal, or Godot. It exists to serve one specific class of game, and
its architecture is deliberately narrow.

## Status: Milestone 4

A controllable player — a capsule-shaped physics character — stands on a
floor, walks, jumps, and falls under real rigid-body physics
([Jolt Physics](https://github.com/jrouwe/JoltPhysics)). Gravity is
supplied by the engine itself (not the physics library), jump direction is
derived from that same gravity rather than a hard-coded axis, and
ground/support state comes from the physics controller's own contact
detection rather than a height check. The Milestone 3 falling cube is
still there too. That's it — no lighting, terrain, planets, or gameplay
yet.

This is intentional — see [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for
why, what's deliberately not built yet, and how this small foundation avoids
blocking the much larger long-term design. Earlier milestones are preserved
as git tags (`milestone-1`, `milestone-2`, `milestone-3`) rather than kept
running alongside the current demo.

## Building

### Requirements

- Linux (developed and tested on Ubuntu)
- CMake 3.20+ (Jolt Physics' own build requires 3.20)
- A C++17 compiler (GCC or Clang)
- SDL2 development headers
- GLM development headers
- Network access on first configure — [Jolt Physics](https://github.com/jrouwe/JoltPhysics)
  is fetched automatically by CMake (`FetchContent`, pinned to `v5.6.0`);
  it is not vendored in this repository or installed via a package
  manager. Later configures use CMake's local cache and don't need network
  access again.

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
walks the player relative to that look direction (not a free-flying
camera).

| Action                  | Keys                  |
|-------------------------|-----------------------|
| Walk forward            | `W` or `Up Arrow`     |
| Walk backward           | `S` or `Down Arrow`   |
| Strafe left             | `A` or `Left Arrow`   |
| Strafe right            | `D` or `Right Arrow`  |
| Jump (only while grounded) | `Space`             |
| Look around             | Mouse movement        |
| Release/recapture mouse | `Escape`               |
| Reset the player and the cube | `R`              |

Close the window normally (window controls / `Alt+F4` / etc.) to exit.

## License

MIT — see [`LICENSE`](LICENSE). Do what you like with it.
