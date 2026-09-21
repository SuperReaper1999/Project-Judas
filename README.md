# Project Judas

A purpose-built game engine for a future game involving large spherical
planets, spacecraft, arbitrary gravity, procedural terrain, and seamless
transitions between planets, ships, and open space.

This is **not** a general-purpose engine and is not trying to compete with
Unity, Unreal, or Godot. It exists to serve one specific class of game, and
its architecture is deliberately narrow.

## Status: Milestone 2

The engine renders three cubes in 3D space with correct perspective and
depth testing, and lets you fly a free camera through the scene with the
keyboard and mouse. That's it — no lighting, physics, terrain, or gameplay
yet.

This is intentional — see [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for
why, what's deliberately not built yet, and how this small foundation avoids
blocking the much larger long-term design. Milestone 1 (a 2D box moved by
the keyboard) is preserved as the `milestone-1` git tag rather than kept
running alongside this milestone's demo.

## Building

### Requirements

- Linux (developed and tested on Ubuntu)
- CMake 3.16+
- A C++17 compiler (GCC or Clang)
- SDL2 development headers
- GLM development headers

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

The mouse is captured on launch and controls camera look directly.

| Action           | Keys                  |
|------------------|-----------------------|
| Move forward     | `W` or `Up Arrow`     |
| Move backward    | `S` or `Down Arrow`   |
| Strafe left      | `A` or `Left Arrow`   |
| Strafe right     | `D` or `Right Arrow`  |
| Move up          | `Space`               |
| Move down        | `Left Ctrl`           |
| Look around      | Mouse movement        |
| Release/recapture mouse | `Escape`       |

Close the window normally (window controls / `Alt+F4` / etc.) to exit.

## License

MIT — see [`LICENSE`](LICENSE). Do what you like with it.
