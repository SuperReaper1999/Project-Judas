# V2 architecture and ownership

Engine, editor and SDK consume the same runtime, serializers and asset services.
A project supplies gameplay policy in JudasJS; the engine supplies mechanics,
resources, authoring and application services. [Readiness](DEVELOPMENT_READINESS.md)
records the qualified configuration. Source existence is not device qualification.

## Application and world lifetime

| Owner | Responsibility | Boundary |
| --- | --- | --- |
| `EngineHost` | Window/context, input, renderer, audio, jobs and resources | Shared by standalone applications, editor and native tests |
| `Application` | Ordinary application loop, service pumping and harness entry | Harness drives this application; it is not a substitute engine |
| `InteractivePlay` | Play lifecycle, accumulator and pause/presentation | One played world, including editor Scene/Game views |
| `RuntimeWorld` | Live entities/components, handles, native systems and script VM | Authored Scene is input, not a mutable alias of runtime state |
| `SceneSession` | Project/scene transitions and session policy | Validates/stages replacement before retiring the active world |
| `ResourceManager` / `AssetDatabase` | Registered identities, worker preparation and context-thread publication | Resource readiness/failure is explicit; file existence alone is insufficient |
| `SaveService` / `WorldPersistence` | Coherent supported-state capture/restore | Ordinary slots, not old pose overlays or serialized VM heaps |
| `WorldStreaming` | Region preparation, pins, retention and suspension | Explicit supported participant matrix; not arbitrary world serialization |

Stop retires runtime work/resources through their owners and restores authoring;
Play builds a fresh world. A failed startup/asset/transition must not publish a
half-valid world. Handles carry owner/generation identity and become invalid at
retirement. Explicit valid/dispose/readiness APIs have subsystem-specific phases.
See [developer workflows](DEVELOPER_WORKFLOWS.md) and [persistence](PERSISTENCE.md).

## One authoritative simulation schedule

`Simulation.cpp::StepWorld` implements both `StepPlayedWorld` and
`StepAuthoritativeWorld`. The normal fixed boundary:

1. Commit a checked origin shift, update navigation/network preparation and run
   fixed scripts. A network replica advances its local script clock and applies
   resolved network state without running a second authoritative physics solve.
2. Prepare weather, opted-in motor/reference animation, physical animation/balance,
   material mechanics, dynamic forces, celestial/coarse coupling and opt-in grid water.
3. Advance MK2 chronological rigid intervals. Vehicle mechanics and deformable
   substeps couple at these boundaries; their quality budgets do not step the
   whole world repeatedly. Owner-only contacts/reactions preserve ordering.
4. Publish remaining motors/body transforms, wheel/powertrain state, animation,
   ragdolls, sockets, materials and completed mechanical continuity.
5. Update particles and simulation time, dispatch physics/material events, drain
   signals, publish network state and evaluate the optional fidelity policy.

Frame scripts, fixed scripts, UI updates and presentation callbacks are distinct.
Presentation interpolates settled state and cannot author physical motion. The
accumulator exposes clipping/discard/catch-up limits; [performance qualification](WHOLE_ENGINE_PERFORMANCE.md)
must report actual lost simulation time, not just frame throughput.

## System ownership map

| Capability | Current owner and authoritative guide |
| --- | --- |
| Rigid motion/contact/CCD | `PhysicsWorld`, `src/physics/WorldMk2*`; [MK2](MK2_PHYSICS_ARCHITECTURE.md), [physics overview](PHYSICS_V2.md), [module map](../src/physics/WorldModules.md) |
| Planar bodies/queries | Native planar geometry in the same physical world; [2D physics](judasjs/physics-2d.md) |
| Motors and prescribed bodies | CharacterMotor and kinematic commands; [motor](CHARACTER_MOTOR.md), [kinematics](KINEMATIC_BODIES.md) |
| Gravity and coarse evolution | Native gravity resolver, optional per-entity selection, celestial/coarse records; [gravity](GRAVITY_SELECTION.md), [scaling](SCALABILITY.md) |
| Vehicles and damage | Wheels, powertrains, structural finite connections, materials and fracture; [vehicles](VEHICLES_V2.md), [deformables](DEFORMABLES.md), [structures](STRUCTURES.md) |
| Skeletons and physical poses | Immutable skeletal assets, world-owned playback/IK, mapped articulations; [animation](ANIMATION.md), [IK](MULTI_TARGET_IK.md), [balance](BALANCE.md) |
| Terrain and navigation | Source-owned landscape with independent source/collision/GPU products; [terrain](TERRAIN_V2.md), [navigation](NAVIGATION.md) |
| Rendering and lighting | Renderer/context resources, ShadowSystem and light lists; [renderer](RENDERER_V2.md), [lighting](LUCIFER_V2.md) |
| Particles | World-owned effects, explicit CPU/GPU capability policy; [particles](PARTICLES_V2.md) |
| Basic and advanced water | Independent prescribed surface/swimming versus one opt-in conservative grid; [water policy](PROJECT_SETTINGS_AND_WATER.md), [advanced water](ADVANCED_FLUIDS_V2.md) |
| Weather, atmosphere and fire | Native weather/atmosphere/combustion owners; [weather](WEATHER.md); unsupported persistence combinations reject |
| Utility decisions | World-independent decision core plus per-script agent/sensing/lease ownership; [AI](GAME_AI.md) |
| UI, text and audio | Shared document/layout renderer, Unicode shaping and audio backend/voices; [UI](RUNTIME_UI.md), [localization](LOCALIZATION.md), [audio](AUDIO_STREAMING.md) |
| Input/local players | Logical actions, physical device identities and player/view routing; [input](INPUT.md), [local multiplayer](LOCAL_MULTIPLAYER.md) |
| Online sessions | Explicit authority, transport and supported replication records; [networking](NETWORKING.md) |

Similar representations are not necessarily duplicates: visual meshes and physical
geometry have different owners; CPU particle fallbacks are legitimate capability
policies. Independent physics-core/contact-material experiment branches are not
selected by the normal CMake targets. Their preserved source provenance is in
[the source-boundary receipt](qualification/source-boundary.json), not in temporary
build outputs or an accepted alternative runtime.

## Scripting boundary

One QuickJS runtime exports the `judas` module. `ScriptSystem` coordinates slots,
budgets, lifecycle and exceptions; `src/scripting/Modules.json` declares subsystem
registration and embedded library fragments. Handlers live under
`src/scripting/modules`; public wrappers under `src/scripting/library` share that
VM and lifecycle. A subsystem's C++ interface does not imply a JudasJS binding.

Use [the typed public API](judas.d.ts) and [reference index](JUDASJS.md).
[The extension guide](judasjs/scripting-v2.md) explains ownership-safe native
registration. Scripts own gameplay decisions, movement intent, camera policy and
bounded JSON state, not hidden native demo controllers. Re-entry/retirement rules,
start versus restore, presentation observations and deliberate reload are explicit.

## Editor and authoring boundary

The editor owns editable documents, undo/drafts, tools and panels. It uses the same
Project/Scene/Prefab/UI serializers, import/cook services and runtime as the SDK.
Worker jobs publish validated products in Edit; they do not run game JavaScript,
physics or raw OpenGL. Drafts, scene edits and filesystem publication have distinct
undo/conflict rules. [Named authoring](NAMED_AUTHORING.md) preserves these contracts.

Scene/Game tabs, detached views and auxiliary cameras observe one played world.
`EditorViewportPresenter` chooses primary presentation cadence; a hidden Game tab
is not rendered because its target exists. Terrain previews reuse runtime height
LOD and immutable source publications, retaining exact picking and brush pinning.
See [the editor guide](EDITOR_GUIDE.md) for authoring and panel extension.

## Coordinates, persistence and delivery

Local transforms/physics use local simulation metres. Geographic source/anchors
and world origin use binary64; subtract the origin before narrowing to local
float geometry. One checked active chart does not become simultaneous distant
physics islands. [Origin transactions](ASSEMBLY_TRAVEL.md) translate actual owners,
preserve momenta/handles and explicitly notify script-owned local-point caches.

Modern saves capture supported native participants and bounded script state at a
coherent boundary. Load validates content/participants, prepares a replacement,
restores native state, republishes derived observations, then runs script restore.
[Save evolution](SAVE_EVOLUTION.md) is explicit developer policy, not automatic
recovery of removed formats or unrecorded physical state.

SDKs contain matching tools/runtime, registered dependencies, current guides,
declarations and curated examples. Source-only examples are identified by
`packaging/sdk-inventory.json`; projects and exported games are separate from SDK
installation. [Distribution](DISTRIBUTION.md) and [exports](EXPORTS.md) specify
checksum/identity/relocation rules. Browser/Android game runtimes reuse this engine
with explicit backend limits; there is no browser or mobile desktop-editor promise.

Native build targets and meaningful registered tests are declared in CMake and
`tools/validation/suites.json`. [CI](CI.md), [the harness](TEST_HARNESS.md),
[diagnostics](PROFILER.md) and [contribution/storage policy](DOCUMENTATION_MAINTENANCE.md)
explain reproducible work. [Intentional removals](V2_BREAKING_CHANGES.md) are a short
compatibility contract, not an alternate old-engine manual.
