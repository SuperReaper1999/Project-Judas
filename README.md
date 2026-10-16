Copyright (c) 2026 Conner / SuperReaper1999 · [MIT License](LICENSE).

# Project Judas

Judas is a general-purpose, C++ game engine for building physical, scriptable
worlds. Its capabilities are optional: use the parts you need, from a small
flat-terrain game to large planetary scenes with vehicles, spacecraft,
arbitrary gravity, terrain, liquids, weather, deformables and seamless world
transitions. Ordinary game policy lives in project JavaScript; the engine
provides the reusable systems and runtime.

Judas is an **Alpha / pre-Alpha project**. The engine is actively developed,
platform coverage is uneven, and some advanced systems remain experimental.
Read the documented scope and validation evidence before treating any feature
or performance result as a guarantee.

## Current status

The latest human-accepted release on `main` is **M89**, including optional local
multiplayer alongside native Linux, Windows and Android runtimes. Android targets
Android 16 with OpenGL ES 3, ARM64 phones/tablets and x86_64 emulators. Broader
hardware and physical-controller coverage remains outstanding; see the platform
and milestone evidence for the exact validation scope.

**M90 large-world assembly travel** is the current Experimental candidate.
Vehicles, wheels, powertrains, characters, navigation, articulated poses,
attachments, deformables, audio and particles can now participate in the checked
local-origin transition. This remains a bounded local physics chart; active
liquids/PBF and atmosphere/combustion still reject transitions. Start with the
[travel lab](projects/travelling_lab/README.md), [M90 contracts](docs/M90_ASSEMBLY_TRAVEL.md)
and [validation evidence](docs/evidence/m90/REPORT.md). M90 human acceptance is pending.

See also [local multiplayer](docs/M89_LOCAL_MULTIPLAYER.md) and its
[validation report](docs/evidence/m89/REPORT.md). Automated checks do not replace
physical or human acceptance.

## Highlights

- C++ engine with SDL2, OpenGL 3.3 Core on desktop, and an optional OpenGL ES
  3 Android runtime.
- Rigid-body physics, joints, CCD, character motors, wheels, powertrains,
  deformables, fracture and physical structural connections.
- Local and planetary terrain, volumetric terrain authoring, navigation,
  world streaming, saves and scene transitions.
- Optional liquids, weather, atmosphere, particles, audio, animation,
  ragdolls, cameras, UI and profiling.
- Project JavaScript through JudasJS for gameplay, input, cameras, UI,
  persistence and other game-owned policy.
- An editor, project/scene authoring format, standalone Linux export and
  native Android project export.

The demonstrations in `projects/` are editable capability examples, not a
single bundled game. Useful starting points include [Breakwater](projects/breakwater/README.md),
[Core Scramble](projects/core_scramble/README.md), [Mechanical Arcade](projects/physics_stress_lab/README.md),
the [Weather Lab](projects/weather_lab/README.md), and the [Planetary Terrain
Lab](projects/planetary_terrain_lab/planetary_terrain_lab.judasproj).

## Build on Linux

Linux is the primary development and validation platform. Install CMake 3.20+,
a C++17 compiler, SDL2 and GLM development packages, Python 3, and a working
OpenGL 3.3 environment. On Ubuntu/Debian:

```sh
sudo apt install cmake libsdl2-dev libglm-dev build-essential
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Run a project or scene with the runtime, or open one in the editor:

```sh
./build/judas judas_tech_demo.judasproj
./build/judas projects/tiny_game/tiny_game.judasproj
./build/judas_editor projects/tiny_game/tiny_game.judasproj
```

The first build compiles local third-party sources and can take a while.
See [the editor guide](docs/EDITOR_GUIDE.md) and each example's README for
controls and project-specific setup.

## Android

Android export is optional and requires the Android SDK command-line tools,
JDK 17+, NDK 28.2.13676358, Android 16 / API 36, CMake and Python 3.11+.
The supported export host is Linux:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target judas judas_export -j4
python3 tools/build_android.py projects/breakwater/breakwater.judasproj \
  --sdk "$ANDROID_SDK_ROOT" --abis arm64-v8a
```

See [Native Android games](docs/ANDROID.md) for the complete setup, export
options, lifecycle behavior and current limitations.

## Documentation

- [Architecture](docs/ARCHITECTURE.md) — engine boundaries and major systems
- [JudasJS](docs/JUDASJS.md) and [getting started](docs/judasjs/getting-started.md)
- [Editor guide](docs/EDITOR_GUIDE.md) and [content authoring](docs/CONTENT_AUTHORING.md)
- [Android](docs/ANDROID.md) and [Windows notes](docs/WINDOWS.md)
- [Feature documentation](docs/) and [validation evidence](docs/evidence/)
- [Archived historical README](docs/DontReadMe_Final.md) — the previous root
  README, preserved verbatim for reference

## Contributing

Build a focused example, read the relevant feature documentation, and keep
claims tied to reproducible repository evidence. Contributions that improve
the engine, examples, documentation or validation are welcome; experimental
features should remain clearly scoped.

## License

Project Judas is licensed under the [MIT License](LICENSE). Vendored
dependencies retain their own notices and licence terms.
