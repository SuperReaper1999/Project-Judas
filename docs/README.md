# Judas documentation

Start here for the current Pre-Alpha engine. [Repository overview](../README.md),
[current status and limits](PREALPHA.md), [architecture](ARCHITECTURE.md).
This is development documentation, not an Alpha release or a compatibility promise.

## Start a project

- [First visible project](FIRST_PROJECT.md): build, camera, script, Play and export.
- [Editor guide](EDITOR_GUIDE.md): authoring, selection, tools and iteration.
- [JudasJS reference](JUDASJS.md): public module, handles, lifecycle and subsystem API.
- [Editor completion](judasjs/getting-started.md) with [judas.d.ts](judas.d.ts).
- [Copyable cookbook](judasjs/cookbook.md) and [runtime/type/reference inventory](judasjs/API_INVENTORY.md).
- [Scripting](SCRIPTING.md): execution boundary and existing project integration.

## Build, distribute and verify

| Task | Reference |
|---|---|
| Versions, channels and source archives | [Build identity](BUILD_RELEASES.md) |
| Packages, SDKs and checksums | [Distribution](DISTRIBUTION.md) |
| Windows prerequisites and qualified scope | [Windows](WINDOWS.md) |
| Native APK export and mobile limits | [Android](ANDROID.md) |
| Focused regression groups and preserved failures | [Pre-Alpha validation](PREALPHA.md#focused-validation-from-a-clean-checkout) |
| Application harness and automation | [Test harness](TEST_HARNESS.md) |
| Profiling costs | [Profiler](PROFILER.md) |
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
| Editable volumes and planets | [Volumetric terrain](M84_VOLUMETRIC_TERRAIN.md), [planetary terrain](M85_PLANETARY_TERRAIN.md) |
| Continuous travel and persistent regions | [Assembly travel](M90_ASSEMBLY_TRAVEL.md), [living worlds](M97_LIVING_WORLD.md) |
| Collision policy and practical dense limits | [Collision continuity](M91_COLLISION_CONTINUITY.md), [latest measured follow-up](PREALPHA_EVENT_DEPENDENCIES.md) |

## Content, presentation and game services

- [Model import](MODEL_IMPORT.md), [named authoring](NAMED_AUTHORING.md),
  [content authoring](CONTENT_AUTHORING.md), [materials](MATERIALS.md),
  [cooked assets/LOD/residency](M93_ASSET_PIPELINE.md), [scalability contracts](SCALABILITY.md).
- [Rendering/lights](judasjs/rendering.md), [weather](M86_WEATHER.md),
  [audio](M60_AUDIO.md), [particles/cameras](judasjs/effects-camera.md).
- [Runtime UI](RUNTIME_UI.md), [UI toolkit](M96_UI_TOOLKIT.md),
  [localization](judasjs/localization.md), [developer tools](M94_DEVELOPER_TOOLS.md).
- [Scene transitions](SCENE_TRANSITIONS.md), [saves/migrations/recovery](M98_SAVE_EVOLUTION.md),
  [mods](MODDING.md), [local multiplayer](M89_LOCAL_MULTIPLAYER.md),
  [online multiplayer/security boundaries](M99_NETWORK.md).

## Learn from a consumer game

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
