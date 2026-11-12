# Judas Engine Alpha v2.0.0 — local Experimental preview

Source identity: `2.0.0-alpha.1`. This is an **unpublished Experimental build**
for local project testing. It is not a published release, an accepted release
candidate or a replacement for the published Alpha v1.2.5 packages. No release
tag, update-channel manifest or main-branch promotion is part of this work.

## Included development work

- **Scripting V2:** one shared JudasJS runtime and lifecycle, subsystem-owned
  binding modules, shared validation/error infrastructure and extension tests.
  See [the architecture guide](judasjs/scripting-v2.md) and
  [current declarations](judas.d.ts).
- **Editor V2:** dockable/detachable workspaces, reusable visual controls,
  reorganized Hierarchy/Inspector/assets and independently owned editor panels.
  Existing project authoring, Play/Stop and deliberate script debugging/reload
  remain available. See [the editor guide](EDITOR_V2.md).
- **Renderer V2:** capability-selected modern desktop OpenGL paths with portable
  fallbacks, modular submission/shader/resource owners and compute infrastructure.
  See [measured scope and platform boundaries](RENDERER_V2.md).
- **Particles V2:** optional native collision snapshots, CPU reliable impacts,
  resident desktop GPU effects and persistent game-owned deposition examples.
  See [backends, limits and authoring](PARTICLES_V2.md).
- **Terrain V2:** source-owned heightfields, local sparse excavation, planetary
  precision and independently budgeted source/collision/render residency. See
  [terrain workflows](TERRAIN_V2.md).
- **Advanced Fluids V2:** game-resolution conservative grid water with independent
  presentation; operator gameplay/visual review passed on Experimental. The Alpha
  lock remains, and no supported release is unlocked by these notes.
- **Architectural purge:** retired native demo gameplay, PBF and reservoir liquids,
  bounded V1 terrain, historical apps and compatibility paths. See
  [intentional removals](V2_BREAKING_CHANGES.md).
- Asset import diagnostics flag plausible unit/scale mismatches with dimensions
  and corrective guidance. They are advisory; importing a legitimately large
  environment remains possible and no automatic rescaling is implied.

The earlier [v1.2.5 API additions](ALPHA_V1_2_5_NOTES.md) are also included when
upgrading a project from an older SDK.

## Compatibility and use

Read the [V2 consumer guide](V2_CONSUMER_GUIDE.md) before updating a project.
Use this SDK's declarations, examples and subsystem references rather than an
older installed SDK. Back up project metadata and retain the previous SDK for
rollback during the combined game/editor trial.

Intentional corrections include rejecting unknown enumerable audio options and
requiring particle `burst(count)` to receive a finite integer number from zero
through 100,000. Native binding modularization does not introduce a second VM,
an asynchronous gameplay scheduler or a required script-conversion step.

Basic Water and swimming remain independent of Advanced Fluids. **V2 supports explicit project opt-in in every build channel.** Published V1 packages remain unchanged; V2 remains only on Experimental. Its accepted gameplay/visual review is separate from release qualification. Keep Advanced Fluids
disabled in the RPG project. New V2 platform results retain their recorded scope;
existing Windows/Android acceptance is not blanket certification of this new
local build. A host-built Linux SDK is not automatically an Ubuntu 24.04 ABI
distribution. The source and binary build receipts identify what was tested.

## Publication boundary

The publication record in [RELEASE_STATUS.json](RELEASE_STATUS.json) remains
unchanged. Final human acceptance and separate release qualification are still
required before publishing Alpha v2.0.0 packages or its tag.
