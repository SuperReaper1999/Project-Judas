# Judas documentation

<!-- judas-release-status:start -->
Source identity is **Judas Engine Alpha v2.0.0** (`2.0.0-alpha.1`).
[Source notes](ALPHA_V2_0_0_NOTES.md) · [Published packages](https://github.com/SuperReaper1999/Project-Judas/releases) ·
[Support boundaries](ALPHA_SUPPORT_MATRIX.md).

Source identity and publication are separate. The
[publication record](RELEASE_STATUS.json) is a dated receipt.
Check the release page for subsequently published packages.
<!-- judas-release-status:end -->

Start with the [editor guide](EDITOR_GUIDE.md), [JudasJS reference](JUDASJS.md),
[SDK authoring](SDK_AUTHORING.md) and [current outstanding work](OUTSTANDING_WORK.md).
[Versioning](VERSIONING.md) and [publication](RELEASING.md) describe distribution.

## Start a project

- [Local Alpha V2 consumer guide](V2_CONSUMER_GUIDE.md): select the matching SDK, learn the V2 workflows, check intentional API differences and retain unpublished-build boundaries.
- [First visible project](FIRST_PROJECT.md): build, camera, script, Play and export.
- [Editor guide](EDITOR_GUIDE.md): authoring, selection, tools and iteration.
- [Renderer V2](RENDERER_V2.md): capability-based desktop OpenGL, modular GPU owners, measured submission/shader improvements and portable boundaries.
- [Fluids with Renderer V2](FLUID_RENDER_V2_COMPARISON.md): matched GTX 970 measurements, unchanged physical trajectories, and the open persistent-upload tail finding.
- [Editor V2 preview](EDITOR_V2.md): docked workspaces, module ownership and pending combined review.
- [JudasJS reference](JUDASJS.md): public module, handles, lifecycle and subsystem API.
- [Editor completion](judasjs/getting-started.md) with [judas.d.ts](judas.d.ts).
- [Copyable cookbook](judasjs/cookbook.md) and [runtime/type/reference inventory](judasjs/API_INVENTORY.md).
- [Scripting](SCRIPTING.md): execution boundary and existing project integration.
- [2D quick start](2D_QUICKSTART.md): presets, collision, scripts, tiles, animation and export.
- [Game AI](GAME_AI.md) and [AI scripting](judasjs/ai.md): author actions, inspect decisions, sense physical targets and reserve resources.
- [Experimental browser export](BROWSER_SUPPORT.md): explicit launch opt-in and a separate Linux/Emscripten toolchain; supported boundaries and qualification records.
- [Shadow-filter correction evidence](SHADOW_FILTER_CONTINUITY.txt): continuous shadow edges, native controls and remaining graphics limitations; published with its recorded release qualification.
- [Locked-fluid motion-clock repair](FLUID_MOTION_CLOCK_REPAIR.txt): impact-ejection correction, neutral-body controls and remaining fluid limits.
- [Fluid contact transient and representation review](FLUID_TRANSIENT_AND_REPRESENTATION_REVIEW.txt): captured premature contact impulses, causal controls and non-particle alternatives; investigation, not a qualified replacement solver.
- [Fluid clearance repair and grid prototype](FLUID_CLEARANCE_REPAIR_AND_GRID_PROTOTYPE.txt): coherent hard/soft contact handling, unchanged pool replay, final regression evidence and measured limits of an isolated non-particle design; Alpha lock retained.
- [Advanced Fluids settling trials](FLUID_SETTLING_TRIALS_20261103.md) records the remaining settling failures, pressure and support experiments, and the player regression that ruled out the most promising candidate.
- [Pressure support and occupied volume](FLUID_PRESSURE_OCCUPANCY_20261103.md) traces the wall response, reproduces clustered pressure equilibria and compares a conservative-volume control on the same deep-pool dimensions.
- [Moving occupied-volume prototype](../tools/prototypes/fluid_volume_grid/MOVING_GRID.md) covers translating cut geometry, pressure reactions, evolving phase transport and a native player test.

## Build, distribute and verify

| Task | Reference |
|---|---|
| Versions, channels and source archives | [Build identity](BUILD_RELEASES.md) |
| Intended Alpha support boundaries | [Support matrix](ALPHA_SUPPORT_MATRIX.md) |
| Final bounded Linux qualification | [Current receipts and limits](PREALPHA_FINAL_QUALIFICATION.md) |
| Packages, SDKs and checksums | [Distribution](DISTRIBUTION.md) |
| Windows prerequisites and qualified scope | [Windows](WINDOWS.md) |
| Native APK export and mobile limits | [Android](ANDROID.md) |
| Focused regression groups and preserved failures | [Pre-Alpha validation](PREALPHA.md#focused-validation-from-a-clean-checkout) |
| Application harness and automation | [Test harness](TEST_HARNESS.md) |
| Profiling costs | [Profiler](PROFILER.md) |
| Lightweight native graphics error capture | [Experimental GPU diagnostics](GPU_DIAGNOSTICS.txt) |
| Keeping references aligned | [Documentation maintenance](DOCUMENTATION_MAINTENANCE.md) |

## Physics and worlds

| Area | Current reference |
|---|---|
| Gravity and independent script selection | [Gravity selection](GRAVITY_SELECTION.md) |
| Character motion and fixed-step intent | [CharacterMotor](judasjs/character.md) |
| Queries, filtering and hit data | [Physics queries](PHYSICS_QUERIES.md), [collision events](COLLISION_EVENTS.md) |
| Constraints and limits | [Joints](JOINTS.md) |
| Navigation guidance | [Navigation](NAVIGATION.md) |
| Skeletons, animation and physical poses | [Animation](ANIMATION.md), [workflow/timeline](M95_ANIMATION_WORKFLOW.md), [pose composition](POSE_COMPOSITION.md), [ragdolls](RAGDOLLS.md) |
| Structures and deformation | [Structural failure](M79_STRUCTURES.md), [bending](M80_BENDING.md), [coarse crash assemblies](M83_COARSE_CRASH.md) |
| Wheels and mechanics | [Wheels](M81_WHEELS.md), [powertrains](M82_POWERTRAINS.md), [mechanical damage](M83_MECHANICAL_DAMAGE.md) |
| Conserved liquid quantities/surfaces | [Reservoirs](LIQUID_RESERVOIRS.md), [dynamic surfaces](LIQUID_SURFACES.md) |
| Advanced-fluid qualification (Alpha locked) | [Local wet reconstruction](FLUID_LOCAL_WET_RECONSTRUCTION.txt), [contact and query-cost corrections](FLUID_CONTACT_AND_QUERY_REPAIR.txt), [finite-support corrections](FLUID_FINITE_SUPPORT_REPAIR.md), [measurement audit](FLUID_MEASUREMENT_AUDIT.md) — retained failures, force accounting and geometric uncertainty |
| Editable volumes and planets | [Volumetric terrain](M84_VOLUMETRIC_TERRAIN.md), [planetary terrain](M85_PLANETARY_TERRAIN.md) |
| Terrain V2 development | [Current implementation and remaining work](TERRAIN_V2.md) |
| Continuous travel and persistent regions | [Assembly travel](M90_ASSEMBLY_TRAVEL.md), [living worlds](M97_LIVING_WORLD.md) |
| Collision policy and practical dense limits | [Collision continuity](M91_COLLISION_CONTINUITY.md), [support ownership and measured costs](PREALPHA_SUPPORT_CONTINUITY.txt) |

## Content, presentation and game services

- [Model import](MODEL_IMPORT.md), [named authoring](NAMED_AUTHORING.md),
  [content authoring](CONTENT_AUTHORING.md), [materials](MATERIALS.md),
  [cooked assets/LOD/residency](M93_ASSET_PIPELINE.md), [scalability contracts](SCALABILITY.md).
- [Rendering/lights](judasjs/rendering.md), [weather](M86_WEATHER.md),
  [audio](M60_AUDIO.md), [particle emitters/backends](PARTICLES_V2.md),
  [particle/camera scripting](judasjs/effects-camera.md).
- [Runtime UI](RUNTIME_UI.md), [UI toolkit](M96_UI_TOOLKIT.md),
  [localization](judasjs/localization.md), [developer tools](M94_DEVELOPER_TOOLS.md).
- [Scene transitions](SCENE_TRANSITIONS.md), [saves/migrations/recovery](M98_SAVE_EVOLUTION.md),
  [mods](MODDING.md), [local multiplayer](M89_LOCAL_MULTIPLAYER.md),
  [online multiplayer/security boundaries](M99_NETWORK.md).

## Learn from a consumer game

[Surface Workshop](../projects/particle_v2_lab/README.md) is the editable particle
project: reliable game-owned coating, native moving receivers, rotated gravity,
saves and a separately selected GPU stress scene. It does not enable Advanced
Fluids. See its controls and qualification boundaries before entering stress.

[Little Haven](../projects/ai_workshop/README.md) demonstrates native Utility AI
with project-owned needs and patrol/hide actions.
[Willow Town](../projects/willow_town/README.md) expands that workflow into an
editable top-down life game with employment, resource use, aging, death and
cold-save continuation. See its [life qualification](../projects/willow_town/Tests/LIFE_QUALIFICATION.md)
for the actual 20-minute observation and remaining limits. Both are included as
editable SDK/source learning projects; package manifests identify their exact files.


[Hearthworld survival sandbox](HEARTHWORLD_SURVIVAL_SANDBOX.md) combines a small
planet, tool collection, physical tree felling, a force-based gravity gun,
construction, bounded excavation and cold saves in ordinary project JavaScript.

## Current guidance and historical evidence

A milestone filename can still contain a useful current subsystem contract.
Its original acceptance/timing statements remain scoped to that checkpoint.
Use [Pre-Alpha status](PREALPHA.md) for subsequent corrections and limitations.
Do not turn a synthetic check or old platform receipt into human acceptance.

[Architecture history](ARCHITECTURE_HISTORY.md), [the archived README](DontReadMe_Final.md),
[FTFT stabilization status](STABILIZATION_STATUS.md) and `docs/evidence/` preserve
historical reasoning, genuine failures and measured results. The superseded
[M83 dense-node crash account](M83_STRUCTURAL_CRASH.md) also remains historical;
some of its original links require local, untracked experiment artifacts. Use
[coarse physical crash sections](M83_COARSE_CRASH.md) for current production.
These historical documents are not the
current feature/support index. Protected evidence is not rewritten when a later
correction passes.

The explicit [current-reference list](../tools/verification/current-docs.json)
is checked by `python3 scripts/check_current_docs.py`. It verifies local
destinations and Markdown anchors. CI additionally uses `--require-tracked` to
reject links to untracked files. The small parser covers inline links and reference
definitions; undefined reference usages and arbitrary Markdown extensions are
outside its scope. It does not prove prose, temporal ordering, platform acceptance
or every project asset; those still need review.

- [UI transfers and live 3D previews](UI_TRANSFERS_AND_PREVIEWS.md) — Alpha 1.2.2 APIs and editable Satchel Menu Workshop.
