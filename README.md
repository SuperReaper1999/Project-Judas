# Project Judas

A purpose-built game engine for a future game involving large spherical
planets, spacecraft, arbitrary gravity, procedural terrain, and seamless
transitions between planets, ships, and open space.

This is **not** a general-purpose engine and is not trying to compete with
Unity, Unreal, or Godot. It exists to serve one specific class of game, and
its architecture is deliberately narrow.

## Status: Milestone 1

The engine currently does exactly one thing: it opens a window, renders a
2D box, and lets you move that box around with the keyboard. That's it.

This is intentional — see [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for
why, what's deliberately not built yet, and how this small foundation avoids
blocking the much larger long-term design.

## Building

### Requirements

- Linux (developed and tested on Ubuntu)
- CMake 3.16+
- A C++17 compiler (GCC or Clang)
- SDL2 development headers

On Ubuntu/Debian:

```bash
sudo apt install cmake libsdl2-dev build-essential
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

| Action    | Keys                 |
|-----------|-----------------------|
| Move up   | `W` or `Up Arrow`    |
| Move down | `S` or `Down Arrow`  |
| Move left | `A` or `Left Arrow`  |
| Move right| `D` or `Right Arrow` |

Close the window normally (window controls / `Alt+F4` / etc.) to exit.

## License

MIT — see [`LICENSE`](LICENSE). Do what you like with it.
