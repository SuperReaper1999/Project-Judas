# Project Judas

A purpose-built game engine for a future game involving large spherical
planets, spacecraft, arbitrary gravity, procedural terrain, and seamless
transitions between planets, ships, and open space.

This is **not** a general-purpose engine and is not trying to compete with
Unity, Unreal, or Godot. It exists to serve one specific class of game, and
its architecture is deliberately narrow.

## Status: Milestone 7-B

A controllable player walks, jumps, and falls under real physics
([Jolt Physics](https://github.com/jrouwe/JoltPhysics)) on the surface of a
large sphere (radius `20m`), with **radial** gravity pulling toward its
center. There is no universal "up": the player's own sense of up
continuously reorients to match wherever gravity currently points, so you
can walk all the way around the sphere onto what was originally "the other
side." Judas owns the player's movement, orientation, and support logic
directly, using Jolt only for low-level collision queries. Ordinary
locomotion is visually smooth: the underlying fixed-timestep simulation is
unchanged (still authoritative, still deterministic), but what actually
gets rendered each frame is a presentation-only interpolation between two
simulation states rather than the latest one shown directly — see
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the measurements that
showed this was needed and why.

The sphere also hosts four ordinary Jolt dynamic bodies (two cubes, two
spheres) scattered around it. Each one samples the same Judas-owned gravity
the player does, purely from its own position — proof that gravity was
never player-specific. They fall, land, roll, and collide with the sphere
and each other under real physics; walk into one and you can push it.

A second, completely different physical environment now coexists with the
sphere: a flat platform under **uniform** gravity, positioned near the
sphere's equator. Strafe toward it and jump off the sphere's surface, and
gravity smoothly hands off from radial to uniform as you cross — no code
anywhere decides which implementation is "active" globally, and the
player has no idea which one is currently governing it. You physically
land on the platform under ordinary collision, walk and jump normally
under its uniform gravity, and can walk back off the edge to fall back
onto the sphere the same way. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for how gravity is resolved
spatially, why a naive approach broke ordinary jumping everywhere on the
sphere before the real design was found, and how the transition stays
smooth. That's it — no lighting, terrain, real planets, or gameplay yet.

This is intentional — see `docs/ARCHITECTURE.md` for why, what's
deliberately not built yet, and how this small foundation avoids blocking
the much larger long-term design. Earlier milestones are preserved as git
tags (`milestone-1` through `milestone-7a`) rather than kept running
alongside the current demo.

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
walks the player relative to that look direction and the sphere's surface
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
