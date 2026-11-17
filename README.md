# Judas Engine V2

A C++ engine, desktop editor and matching JudasJS SDK for 2D/3D games. Game rules
belong to project scripts; shared native systems own simulation, assets and delivery.

<!-- judas-release-status:start -->
Source identity is **Judas Engine Alpha v2.0.0** (`2.0.0-alpha.1`).
[Source notes](docs/ALPHA_V2_0_0_NOTES.md) · [Published packages](https://github.com/SuperReaper1999/Project-Judas/releases) ·
[Support boundaries](docs/ALPHA_SUPPORT_MATRIX.md).

Source identity and publication are separate. The
[publication record](docs/RELEASE_STATUS.json) is a dated receipt.
Check the release page for subsequently published packages.
<!-- judas-release-status:end -->

## Start here

- [Create a project](docs/FIRST_PROJECT.md) and use the [editor](docs/EDITOR_GUIDE.md).
- [Make a 2D game](docs/2D_QUICKSTART.md), or follow [SDK terrain/navigation authoring](docs/SDK_AUTHORING.md).
- Find exact [JudasJS APIs](docs/JUDASJS.md), [types](docs/judas.d.ts) and [examples](docs/judasjs/cookbook.md).
- Read [V2 architecture](docs/ARCHITECTURE.md), [supported boundaries](docs/ALPHA_SUPPORT_MATRIX.md) and the [qualified development baseline](docs/DEVELOPMENT_READINESS.md).
- Browse the [documentation index](docs/README.md) for subsystem and developer workflows.

## Build and test

Install the [native build prerequisites](docs/DISTRIBUTION.md). From the checkout:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUDAS_BUILD_CHANNEL=Experimental -DJUDAS_ENABLE_VALIDATION=ON
python3 tools/workspace_storage.py run -- cmake --build build --target judas_prealpha_tests judas_linux_sdk -j2
python3 tools/workspace_storage.py run -- ctest --test-dir build -L prealpha -j1 --output-on-failure
```

Reuse the active build and keep outputs under the [bounded storage policy](docs/WORKSPACE_STORAGE.md).
Graphics tests need a real desktop context or the declared CI Xvfb/Mesa route;
software graphics and synthetic input do not imply hardware or human acceptance.
[Ubuntu baseline](docs/LINUX_BASELINE_BUILD.md), [Windows](docs/WINDOWS.md),
[Android](docs/ANDROID.md) and [Experimental browser export](docs/BROWSER_SUPPORT.md)
have different prerequisites and qualification scopes.

## Develop with the current platform

Basic Water/swimming are independent default-enabled effects. Advanced Fluids V2
is explicit project opt-in in every V2 channel. Terrain, rendering/lighting,
particles, production MK2, animation/IK, UI, audio, networking and modern saves
have their own documented representations and limits. [Compatibility removals](docs/V2_BREAKING_CHANGES.md)
are deliberate; current instructions do not depend on retired demos or old SDKs.

Try [Willow Town](projects/willow_town/README.md), [Terrain Workshop](projects/terrain_v2_workshop/README.md),
[Planetary Workshop](projects/terrain_v2_planet_workshop/README.md),
[Graffiti Workshop](projects/graffiti_workshop/README.md) or [Stillwater](projects/basic_water_lab/README.md).
[The SDK inventory](packaging/sdk-inventory.json) identifies bundled and source-only projects.

Work on Experimental; [main promotion](AGENTS.md) and publication require explicit
operator acceptance. [Documentation maintenance](docs/DOCUMENTATION_MAINTENANCE.md)
explains source verification, reference checks and keeping historical clutter out.

Copyright © 2026 Conner / SuperReaper1999. [MIT licence](LICENSE).
