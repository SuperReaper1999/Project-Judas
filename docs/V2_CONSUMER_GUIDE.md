# Local Alpha V2 SDK: consumer guide

The local **Judas Engine Alpha v2.0.0** SDK uses SemVer
`2.0.0-alpha.1` and the **Experimental** build channel. It combines Scripting,
Editor, Renderer and Particles V2 for project testing. It is not a published
Alpha release or a claim of combined human acceptance. The
[publication receipt](RELEASE_STATUS.json) describes the separately published
version; do not change it to describe this local SDK.

## Start with the installed SDK

Use documentation and declarations from the SDK actually selected by the
project, rather than mixing an older SDK's examples with current source APIs.

```sh
SDK=/absolute/path/to/the/local-v2-sdk
PROJECT=/absolute/path/to/Game/YourGame.judasproj
"$SDK/judas_editor" --build-info-json
python3 "$SDK/tools/package_sdk.py" verify "$SDK"
"$SDK/judas_script_check" "$PROJECT"
"$SDK/judas_scene_author" --validate project "$PROJECT"
```

`BUILD_INFO.json` and each tool's `--build-info-json` identify the version,
channel, source revision/fingerprint, configuration and compiler. The SDK
manifest/checksums verify its installed files. Keep the installed SDK intact;
put game scripts, imported assets and generated content in the game project.
Point `jsconfig.json`'s `judas` mapping and declaration include at that same
SDK's `docs/judas.d.ts`. Retain the old SDK until the game has been checked with
the replacement; changing a launcher does not migrate or delete game content.

Open the project with `"$SDK/judas_editor" "$PROJECT"`. See
[First project](FIRST_PROJECT.md) and [Editor guide](EDITOR_GUIDE.md) for ordinary
creation, importing, hierarchy, components, scripts, Play/Stop and export.
[Editor V2](EDITOR_V2.md) covers workspaces, docked/floating panels and the
updated interaction model. Advanced tools remain available through the editor;
the V2 interface does not replace the scene system or require a new game format.

## Scripting: the same language and lifecycle

Games still import one `judas` module. Existing script slots, callbacks, state,
debugging, deliberate reload and Play/Stop remain the normal workflow.
[Scripting V2](judasjs/scripting-v2.md) describes the modular native bindings;
games do not import those native implementation modules separately.

Use the [JudasJS index](JUDASJS.md), [getting started](judasjs/getting-started.md),
[cookbook](judasjs/cookbook.md), [API inventory](judasjs/API_INVENTORY.md) and
[declaration file](judas.d.ts) to find an operation. Read the subsystem reference
for phase, ownership, readiness, failure and persistence rules. A declared
operation is not a promise that it works in every phase or subsystem combination.

Intentional differences relevant to older projects:

- `entity.setAudio(patch)` rejects **unknown enumerable option names** before
  native mutation. Use the exact `AudioSettingsPatch` keys; `volume` belongs to
  an emitter, while `audio.setGroup(name, {gain: ...})` controls a group. See
  [audio](judasjs/audio.md).
- `entity.burst(count)` requires an actual finite integer number from 0 to
  100000. Numeric strings, fractions, negatives and nonfinite values throw;
  there is no previous uint32 coercion. Other game-owned functions also named
  `burst` do not automatically acquire this native contract.
- Entity prototype enumeration is grouped by subsystem. Do not use native
  member ordering as gameplay logic. Member names and descriptors are retained.
- Native C++ binding failures become contextual JavaScript errors; existing
  JavaScript error types pass through. Check actual runtime diagnostics instead
  of interpreting a failed call as a successful default result.

No script conversion tool or wholesale API renaming is required for this pass.
These changes do not certify compatibility of every old game or save: check
the project and its real gameplay before removing its previous SDK.

## Terrain, navigation and NPC authoring

[SDK authoring](SDK_AUTHORING.md) is the property-level terrain → navmesh → NPC
movement reference, with exact scene keys and the packaged Navigation Workshop.
Use its matching profile IDs, collision source masks and bake workflow. A visible
terrain mesh alone is not a baked navigation surface. Rebuild navigation after
changing relevant collision or terrain geometry.

For [native Terrain V2 landscapes](TERRAIN_V2.md), use source-owned navigation:
the terrain worker prepares changed collision and affected navigation tiles
together, then publishes the complete revision. A separate static bake that
includes the editable landscape is rejected rather than left stale. See the
[current qualification and limits](TERRAIN_V2_QUALIFICATION.md) and
[independent residency budgets](TERRAIN_V2_RESIDENCY.md). Runtime collision,
particle coverage and fluid solid queries remain independent of visual LOD.

[Navigation](judasjs/navigation.md), [CharacterMotor](judasjs/character.md) and
[Game AI](GAME_AI.md) cover movement and decisions. Utility AI chooses project
actions; it does not automatically create movement, combat, jobs or inventory.
Keep those rules in project JavaScript and handle missing/partial/invalid routes.
Do not probe random scene property names when the SDK authoring reference already
defines their schema.

## Rendering and particles

[Renderer V2](RENDERER_V2.md) describes capability-based desktop OpenGL and the
portable paths. Selecting a modern rendering option does not guarantee that the
current context supports it. Keep secondary-camera, memory and quality budgets
appropriate to the actual target device.

[Particles V2](PARTICLES_V2.md) documents authoring, collision coverage, budgets
and [particle scripting](judasjs/effects-camera.md#particles). Start with the
complete [copyable particle script](judasjs/examples/particles.js): attach it to
an object that already has a Particle Emitter and provide real receiving
collision geometry. It uses existing public operations and the shared scripting
lifecycle, not an extra particle VM or one rigid body per particle.

- CPU is the default. Moving-body collision, local-space particles, sticking
  and **reliable gameplay impacts** require CPU. Unsupported required-GPU work
  is diagnosed; `auto` may use CPU. Read `entity.particleStats` rather than
  assuming GPU ran. Its `stateTransferBytes` counts exceptional backend copies.
- GPU cosmetic observations are bounded, delayed and may drop with counters.
  Reliable CPU observations retain captured work and backpressure when full.
  Drain them promptly with `takeParticleImpacts()`; do not hide them by clearing
  or destroying their owners. A blocked captured step must finish before a save
  can be prepared. Pausing it first can prevent that completion.
- Live visual particles are transient. Persist the **game-owned receiving
  state**, not every droplet. Surface Workshop demonstrates receiver-local
  coating, changed real friction and cold-save restoration. It is not a decal
  pretending to be durable material, nor a snow or fluid solver.

The packaged `projects/particle_v2_lab/README.md` supplies the workshop controls,
qualification boundaries and human checklist. Its 100k stress scene is selected
explicitly; that fixed-geometry performance result does not certify 100k reliable
moving-world particles or a complete game frame on every platform.

## Save, export and support boundaries

Use the normal [save/migration](M98_SAVE_EVOLUTION.md),
[streaming](judasjs/streaming.md) and [scene transition](SCENE_TRANSITIONS.md)
contracts. Keep a backup of the project and saves before a major SDK upgrade.
Run an actual game, save it, close the process, reload and verify the physical
and game-owned result; a script syntax pass alone does not test persistence.

Export with the selected SDK's matching exporter and runtime:

```sh
"$SDK/judas_export" "$PROJECT" /absolute/path/to/a/new-standalone "$SDK/judas"
```

Run the exported game after relocation, outside the engine checkout. Preserve
its complete package and provenance files. A local host-built SDK is not by
itself a clean-machine or older-distribution certification.

**The published Alpha fluid lock is unchanged.** Existing Experimental/Local
builds retain advanced-fluid opt-in for repair and regression work; that
availability is not qualification for a consumer game. Keep Advanced Fluid
Dynamics disabled in this RPG upgrade. Basic Water and swimming remain
independent, and Particles V2 is not the fluid solver. Browser support still
needs its explicit opt-in and separate toolchain. Desktop Linux results do not
establish Windows, Android-device, browser or controller acceptance for every V2
change. See the
[support matrix](ALPHA_SUPPORT_MATRIX.md) and the individual V2 qualification
records for the actual tested scope.
