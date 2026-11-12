# Judas Engine

A C++ game engine with project-owned JudasJS gameplay, 2D and 3D authoring,
MK2 physics, large worlds and optional simulation systems.

<!-- judas-release-status:start -->
Source identity is **Judas Engine Alpha v2.0.0** (`2.0.0-alpha.1`).
[Source notes](docs/ALPHA_V2_0_0_NOTES.md) · [Published packages](https://github.com/SuperReaper1999/Project-Judas/releases) ·
[Support boundaries](docs/ALPHA_SUPPORT_MATRIX.md).

Source identity and publication are separate. The
[publication record](docs/RELEASE_STATUS.json) is a dated receipt.
Check the release page for subsequently published packages.
<!-- judas-release-status:end -->

## Start here

- [First project](docs/FIRST_PROJECT.md) and [Editor V2 guide](docs/EDITOR_V2.md).
- [SDK authoring: terrain → navigation → NPC movement](docs/SDK_AUTHORING.md).
- [JudasJS reference](docs/JUDASJS.md), [typed API](docs/judas.d.ts) and [cookbook](docs/judasjs/cookbook.md).
- [2D game workflow](docs/2D_GAME_WORKFLOW.md): sprites, tiles, animation, planar collision and traversal.
- [Documentation index](docs/README.md), [support matrix](docs/ALPHA_SUPPORT_MATRIX.md) and [current limitations](docs/CURRENT_FINDINGS.md).

## Current V2 source

Scripting, Editor, Renderer, Particles and Terrain V2 are integrated on Experimental.
Advanced Fluids V2 has a playable conservative water grid and independent visual
surface; **V2 supports project opt-in in every build channel**. Basic Water and
swimming work independently and are enabled by default.

The [architectural purge](docs/ARCHITECTURAL_PURGE.md) removes native demo gameplay,
pose-delta saves, PBF fluids, reservoir/container liquids and separate V1 terrain.
No historical API or project compatibility is promised. Historical demos and their
matching implementations remain in Git history. Six consumer games have separate
editable preservation copies; they are not engine dependencies.

Try [Terrain Workshop](projects/terrain_v2_workshop/README.md),
[Planetary Workshop](projects/terrain_v2_planet_workshop/README.md),
[Graffiti Workshop](projects/graffiti_workshop/README.md),
[Stillwater](projects/basic_water_lab/README.md) or
[Willow Town](projects/willow_town/README.md). The complete curated list is in the
[purge report](docs/ARCHITECTURAL_PURGE.md).

## Build and qualify

Follow [installation/distribution](docs/DISTRIBUTION.md) for dependencies,
[build identity](docs/BUILD_RELEASES.md) for provenance and
[Linux baseline builds](docs/LINUX_BASELINE_BUILD.md) for Ubuntu-compatible SDKs.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUDAS_BUILD_CHANNEL=Experimental -DJUDAS_ENABLE_VALIDATION=ON
cmake --build build --target judas_prealpha_tests judas_linux_sdk -j2
ctest --test-dir build -L prealpha --output-on-failure
```

Graphics checks require a working desktop GL context or Xvfb/Mesa. Headless native
checks do not prove hardware/controller/audio acceptance. See [Windows](docs/WINDOWS.md),
[Android](docs/ANDROID.md) and [experimental browser export](docs/BROWSER_SUPPORT.md).
Source identity is not a published release or human acceptance of this checkpoint.

Copyright © 2026 Conner / SuperReaper1999. [MIT licence](LICENSE).
