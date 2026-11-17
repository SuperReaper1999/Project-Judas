# Judas V2 developer documentation

<!-- judas-release-status:start -->
Source identity is **Judas Engine Alpha v2.0.0** (`2.0.0-alpha.1`).
[Source notes](ALPHA_V2_0_0_NOTES.md) · [Published packages](https://github.com/SuperReaper1999/Project-Judas/releases) ·
[Support boundaries](ALPHA_SUPPORT_MATRIX.md).

Source identity and publication are separate. The
[publication record](RELEASE_STATUS.json) is a dated receipt.
Check the release page for subsequently published packages.
<!-- judas-release-status:end -->

Choose a task below. Each feature has one practical/native contract and, where
scriptable, an exact JudasJS reference. The [qualification matrix](DEVELOPMENT_READINESS.md)
owns tested configurations; guide contents alone do not certify every platform.

## Create, author and deliver

- [First project](FIRST_PROJECT.md), [editor](EDITOR_GUIDE.md), [2D quick start](2D_QUICKSTART.md), [traversal](2D_TRAVERSAL.md).
- [SDK consumer workflow](V2_CONSUMER_GUIDE.md), [terrain → navigation → NPC](SDK_AUTHORING.md), [distribution](DISTRIBUTION.md), [exports](EXPORTS.md).
- [Named content/CLI](NAMED_AUTHORING.md), [assets and cooking](ASSET_PIPELINE.md), [model import](MODEL_IMPORT.md), [prefabs](PREFABS.md).
- [Ownership/readiness walkthrough](DEVELOPER_WORKFLOWS.md), [startup presentation](STARTUP_LOADING.md), [project and water settings](PROJECT_SETTINGS_AND_WATER.md).

## Scripting and gameplay

- [JudasJS index](JUDASJS.md), [typed declarations](judas.d.ts), [getting started](judasjs/getting-started.md), [cookbook](judasjs/cookbook.md), [binding architecture](judasjs/scripting-v2.md), [debugging/reload](SCRIPT_DEBUGGING.md).
- [Input](INPUT.md), [local players](LOCAL_MULTIPLAYER.md), [classification](CLASSIFICATION.md), [signals](SIGNALS.md), [AI](GAME_AI.md), [navigation](NAVIGATION.md).
- [Runtime UI](RUNTIME_UI.md), [drag/drop and model previews](UI_TRANSFERS_AND_PREVIEWS.md), [localization](LOCALIZATION.md).
- [Audio assets](AUDIO_AUTHORING.md), [streaming/acoustics](AUDIO_STREAMING.md), [weather](WEATHER.md), [networking](NETWORKING.md).

## Worlds, state and physical systems

- [Architecture/ownership](ARCHITECTURE.md), [physics overview](PHYSICS_V2.md), [MK2 contract](MK2_PHYSICS_ARCHITECTURE.md), [physics modules](../src/physics/WorldModules.md).
- [Collision authoring](COLLISION_AUTHORING.md), [continuity](COLLISION_CONTINUITY.md), [events](COLLISION_EVENTS.md), [queries](PHYSICS_QUERIES.md), [batches](BATCHED_QUERIES.md), [sleep/scaling](PHYSICS_SCALING.md).
- [Character motor](CHARACTER_MOTOR.md), [kinematic bodies](KINEMATIC_BODIES.md), [joints](JOINTS.md), [gravity](GRAVITY_SELECTION.md).
- [Animation/pose composition](ANIMATION.md), [timeline/root motion/events](ANIMATION_WORKFLOW.md), [retargeting](ANIMATION_RETARGETING.md), [IK](MULTI_TARGET_IK.md), [ragdolls/physical modes](RAGDOLLS.md), [balance limits](BALANCE.md).
- [Vehicles/wheels](VEHICLES_V2.md), [powertrains](POWERTRAINS.md), [structures](STRUCTURES.md), [mechanical damage](MECHANICAL_DAMAGE.md), [finite crash sections](COARSE_CRASH.md).
- [Deformable authoring](DEFORMABLES_V2_AUTHORING.md), [numerical ownership](DEFORMABLES.md), [bending](BENDING.md), [fracture](FRACTURE.md), [optional native constraint participants](PHYSICAL_CONSTRAINT_BATCH.md).
- [Terrain](TERRAIN_V2.md), [residency](TERRAIN_V2_RESIDENCY.md), [planetary precision](PLANETARY_TERRAIN.md), [origin/assembly travel](ASSEMBLY_TRAVEL.md).
- [Streaming/suspension](WORLD_STREAMING.md), [scene transitions](SCENE_TRANSITIONS.md), [save slots](PERSISTENCE.md), [migration/recovery](SAVE_EVOLUTION.md), [mods](MODDING.md), [scalability](SCALABILITY.md).

## Rendering and water

- [Renderer ownership/policy](RENDERER_V2.md), [lighting/shadows/exposure](LUCIFER_V2.md), [runtime appearance/visibility](RUNTIME_RENDERING.md), [materials](MATERIALS.md), [camera targets](CAMERA_TARGETS.md), [particles](PARTICLES_V2.md).
- [Basic versus advanced water](PROJECT_SETTINGS_AND_WATER.md), [conservative grid water](ADVANCED_FLUIDS_V2.md), [playable water workflow](GAME_WATER_PREVIEW.md), [water presentation](WATER_V2_PRESENTATION.md).

## Build, qualify and contribute

- [Build identity](BUILD_RELEASES.md), [CI](CI.md), [test harness](TEST_HARNESS.md), [profiler/GPU diagnostics](PROFILER.md), [physical event debugger](PHYSICS_EVENT_DEBUGGER.md), [whole-application performance](WHOLE_ENGINE_PERFORMANCE.md).
- [Linux reference userland](LINUX_BASELINE_BUILD.md), [Windows](WINDOWS.md), [MinGW cross-build](WINDOWS_CROSS_BUILD.md), [Android](ANDROID.md), [browser](BROWSER_SUPPORT.md).
- [Supported scope](ALPHA_SUPPORT_MATRIX.md), [qualified baseline](DEVELOPMENT_READINESS.md), [known limitations](CURRENT_FINDINGS.md), [intentional compatibility breaks](V2_BREAKING_CHANGES.md).
- [Versioning](VERSIONING.md), [release procedure](RELEASING.md), [current source notes](ALPHA_V2_0_0_NOTES.md), [storage](WORKSPACE_STORAGE.md), [documentation maintenance](DOCUMENTATION_MAINTENANCE.md).

Source links/native fixtures require the matching source checkout; not every
source example is bundled in an SDK. Keep legal notices and asset provenance with
their assets. Superseded reports, screenshots, benchmark histories and roadmaps
belong to Git history. Only concise current qualification/source-boundary receipts
remain in `qualification/`; this directory is not a historical archive.
