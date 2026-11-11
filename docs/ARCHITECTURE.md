# Judas architecture

Implementation map for the current Alpha source.
[Engine version](../engine-version.json) and [release manifests](https://github.com/SuperReaper1999/Project-Judas/releases)
identify source and published packages separately.
This describes ownership and integration boundaries, not universal certification.
See [current status](CURRENT_STATUS.md), [documentation index](README.md), and
[JudasJS](JUDASJS.md). Historical architecture and milestone reports remain in Git history.
Source links require the matching checkout/source archive; SDK/runtime packages
do not contain every project, native source or historical evidence artifact.

## Optional utility decisions (Experimental)

`UtilityAI.{h,cpp}` is a world-independent, bounded decision/timer core. It owns
neither targets nor movements. `ScriptUtilityAI` lazily owns native agents per
script entity/slot; the private `scripting/library/AI.h` wrapper owns synchronous
action callbacks. ScriptSystem retires them through its existing destruction,
fault and reload boundaries. No-agent projects have no agent registry or
background evaluation loop. Optional `AISensing` uses existing MK2 spatial queries
with explicit result/cast budgets and timed stale observations; it does not replace
the broadphase. `AIReservationBoard` provides exclusive simulation-time leases.
Their script bridges are lazy and slot-owned. Claims are released before teardown
callbacks; no new claims are admitted during retirement. Caches invalidate on
origin transitions and are discarded with their script session. Neither helper
serializes runtime tokens; actions reacquire resources after restoration.

Definitions are constrained JSON default exports in ordinary tracked `.ai.js`
script modules, reusing asset ownership, imports, fingerprints and export.
`EditorAIWorkflow` creates paired source/definition assets and a copyable actor
starter through the normal scene edit/undo paths. `UtilityAIEditor` shares native validation and exposes opt-in authoring/live
inspection through existing panels/preferences. Gameplay explicitly stores native
snapshots inside script state; modern persistence and regional scripting remain
the owning lifecycle. No new physics, navigation, save or replication system is
introduced. [Guide](GAME_AI.md) · [qualification](GAME_AI_QUALIFICATION.md).

## Engine and game boundary

Judas owns simulation, rendering, resources, authoring and platform mechanisms.
Projects own input mappings, camera policy and game rules through trusted
JavaScript. A query is not a weapon; a hinge is not a target. CharacterMotor,
camera and skeleton remain separate.

There is no engine-owned demonstration gameplay. Project scripts own controls, cameras, menus and interaction. CharacterMotor and reusable physical queries remain native primitives.

| Responsibility | Owner and source |
|---|---|
| Authored definitions | [Scene](../src/Scene.h), named serializers and project assets |
| Live component state | [RuntimeWorld](../src/RuntimeWorld.h) and subsystem implementations |
| Session/scene replacement | [SceneSession](../src/SceneSession.h) |
| Frame/fixed ordering | [InteractivePlay](../src/InteractivePlay.cpp), [Simulation](../src/Simulation.cpp) |
| VM, instances and callbacks | [ScriptSystem](../src/ScriptSystem.h), [world script bridge](../src/WorldScripts.cpp) |
| Rigid simulation/queries | [PhysicsWorld](../src/PhysicsWorld.h), [private module map](../src/physics/WorldModules.md) |
| Graphics/GPU objects | [Renderer](../src/Renderer.h), [presentation](../src/WorldPresentation.cpp) |
| Resource preparation/residency | [ResourceManager](../src/ResourceManager.h), [asset pipeline](ASSET_PIPELINE.md) |
| Durable restoration | [WorldPersistence](../src/WorldPersistence.h), [save evolution](SAVE_EVOLUTION.md) |
| Regional state | [WorldStreaming](../src/WorldStreaming.h), [living worlds (historical)](https://github.com/SuperReaper1999/Project-Judas/blob/2ee4203a0053cc545eab4a184c627f4f5507c4bd/docs/M97_LIVING_WORLD.md) |
| Network authority | [NetworkSession](../src/NetworkSession.h), [replication scope](NETWORKING.md) |

## Identity, handles and publication

Authored IDs, runtime generation-safe handles, network identities and graphics
tokens serve different purposes. Do not serialize a transient handle as durable
project identity. JudasJS handles validate their world/generation and reacquire
native state. Stale behavior is subsystem-specific; see
[safe handles and lifecycle](judasjs/lifecycle.md).

Definitions are distinct from live state. Play does not silently write changes
back to authored content. Prefabs remap references before publication. Workers
prepare private products; the owning world/resource/render thread validates and
publishes them. Failed replacement retains the last good product.

Rendering support does not establish regional suspension, cold-save or replication
support. The [integration matrix](CURRENT_STATUS.md)
states these independently guarded contracts.

## Simulation ordering

`Simulation.cpp` is authoritative. This phase map does not promise callback
ordering between unrelated script instances.

1. Commit an origin request at a fixed boundary; update navigation, begin the
   liquid step, process incoming network work and invoke fixed scripts.
2. Prepare weather, opted-in character/reference/physical animation and conserved
   liquids; apply the applicable gravity and other force consumers.
3. Advance mechanical sources, wheel contacts/load feedback and rigid physics.
   Coupled material/mechanical configurations use their bounded authored substeps.
4. Advance legacy fluid coupling where present, combustion/coarse state, remaining
   character work and final physical/animation/socket/deformable observations.
5. Publish wheels, powertrains, particles and simulation time; deliver physics and
   fracture events, drain signals and publish network snapshots.

Replica clients process snapshots and fixed scripts without a second authoritative
solver. Joining inside a callback is checked again before local physics runs.
Frame/UI work, input edges and pause follow `InteractivePlay.cpp` and the
[lifecycle reference](judasjs/lifecycle.md).

Presentation callbacks consume interpolated poses after fixed catch-up. Use
`Entity.presentedTransform` for view-following and `Entity.transform` for
physical state. Presentation must not own simulation. Shooting queries and
impulses keep their ordinary authoritative semantics.

## Physics ownership and modules

Judas owns its rigid solver. `PhysicsWorld.cpp` coordinates one world; private
modules separate body lifecycle, bounds, queries, CCD search/events, contacts,
joints, sleeping, particles and persistence. They share one private state owner.
[PhysicsWorldSources.cmake](../cmake/PhysicsWorldSources.cmake) supplies the same
module list to engine and direct fixtures; configuration rejects a partial world.
See the [module map](../src/physics/WorldModules.md) for ordering and scratch lifetime.

The contact and joint solve is **Judas Physics MK2**
(`src/physics/WorldMk2Solver.cpp`): a substepped simultaneous block solve with
arrival-instant impacts, a bounded fast-body CCD pass and split-impulse joint
position projection. It replaced the MK1 time-of-impact event pipeline, which was
retired with its white-box pipeline tests. The
[MK2 architecture](MK2_PHYSICS_ARCHITECTURE.md) records design, measurements and
open gaps. Physical balance uses the capture-point stepping controller by default
(`BalanceSettings::captureStepping`; `false` or `JUDAS_BALANCE_CAPTURE_STEPPING=0`
selects the established controller).

Initial contact geometry can borrow the existing job pool over frozen inputs,
using private derived scratch. The owner joins results in deterministic pair
order. Solving, body mutation and callbacks remain owner-thread work; public
physics methods are not generally thread-safe.

Contacts, continuation identities, warm starts and completed observations are
separate state. A cache hit does not license skipping geometric invalidation.
Modern saves retain supported continuation data; an older snapshot cannot supply
historical data it never stored. Joint continuation records written by MK1 load
as per-step warm impulses.

Bounded CCD sampling, sidedness and deep initial overlap remain limitations.
Dense-contact cost is workload dependent and universal 60 Hz performance is not
established; see [current measured limits](CURRENT_STATUS.md)
and the MK2 architecture's timing tables. The pre-alpha support-continuity and
CCD-query follow-up reports describe the retired MK1 pipeline.

## Coordinates, gravity and large worlds

Origins and planetary positions use binary64. The active physical chart and most
public vectors use local binary32. There is one bounded chart, not independently
simulated physics everywhere on a planet. Origin changes are checked transactions;
active spatial participants must remain inside the 8 km chart.

[WorldOrigin](../src/WorldOrigin.cpp) admits participants and translates world-space
history before publishing the new frame. Local geometry, stable handles and
scalar mechanical state retain their owners. Retained regions decode against
their capture origin. Legacy PBF/emitted liquid and pending/unsupported
transactions retain explicit rejection paths.

Gravity is sampled at the consumer's current position. Faithful/uniform and
`RadicalGravity`/radial are interchangeable fields; Radical is the intentional
name. Optional scripted entity selection uses the resolver and defaults to
normal authored behavior. Support normal is separate from gravity; world-Y is
not universal up. See [gravity selection](GRAVITY_SELECTION.md),
[CharacterMotor](judasjs/character.md), [planetary terrain](PLANETARY_TERRAIN.md)
and [assembly travel](ASSEMBLY_TRAVEL.md).

## Physical assemblies and liquids

Bodies, joints, wheels, powertrains, structures, material geometry, animation and
ragdolls compose through existing owners and fixed phases. They are optional.
JS owns driver policy, damage meaning and gameplay. Query-contact wheels require
no fixed chassis/drivetrain arrangement. Coarse physical crash sections and
material deformation have different practical cost envelopes.

Navigation supplies guidance; it does not own motor movement. Resolved skeleton
poses, physical contributions, IK and sockets share their pose authority.
References are grouped under [physics and worlds](README.md#gameplay-and-content).

M54 reservoirs/containers own quantity; optional M55 surfaces evolve distribution.
Owner-local storage survives supported shifts/restoration. Legacy PBF and
protected independent research are separate representations. A passing ledger
is not general fluid-fidelity or exterior-momentum certification. FLU-001 and
the half-turn grid-query identity limitation remain. See [reservoirs (historical)](https://github.com/SuperReaper1999/Project-Judas/blob/2ee4203a0053cc545eab4a184c627f4f5507c4bd/docs/LIQUID_RESERVOIRS.md),
[surfaces (historical)](https://github.com/SuperReaper1999/Project-Judas/blob/2ee4203a0053cc545eab4a184c627f4f5507c4bd/docs/LIQUID_SURFACES.md) and [current limits](CURRENT_STATUS.md).

## Rendering, cameras and resources

Experimental Renderer V2 keeps one renderer and shared portable scene path.
`Renderer` coordinates public operations, while private `render/` owners handle
capabilities, shader programs, typed submission, streaming storage, view uniforms,
particle presentation, scoped targets and bounded compute. Their coordinator state
is hidden behind `RendererInfrastructure`, so an implementation-owner change does
not force unrelated users of `Renderer.h` to include its private modules.
[Ownership, controls and qualification](RENDERER_V2.md).

Renderer owns graphics/context use. Resource workers decode CPU content; private
GPU upload/replacement publishes on the graphics owner. Cancellation and eviction
must not expose incomplete resources. Async pumps have soft time budgets:
indivisible driver calls and decode expansion are not hard latency/memory guarantees.

Compatible opaque geometry uses pass-scoped instancing. Each view selects its own
lights/shadows; planetary LOD considers active interests within shared budgets.
Secondary/local views do not step another world. Cooked mip chains and whole-resource
residency exist; independent mip residency and universal material/pose instancing
do not. See [rendering](judasjs/rendering.md), [assets](ASSET_PIPELINE.md) and
[scalability](SCALABILITY.md).

Weather is a prescribed optional field, not atmospheric CFD. Graphics quality
does not change wind forces. Retained UI is player-addressed and project-authored;
menu/inventory meaning remains JS. Audio retains its shared-listener policy.
Secondary-view and platform limitations stay explicit in subsystem references.

## Persistence, streaming and networking

Cold restore prepares a private candidate. Identities, ordered migrations,
backups and rejection paths preserve originals. The engine does not automatically
merge newly authored content or rewrite unknown native topology. Regional
suspension retains supported state/edited terrain; unsupported dependencies pin
regions with reasons. See [saves](SAVE_EVOLUTION.md) and [living worlds (historical)](https://github.com/SuperReaper1999/Project-Judas/blob/2ee4203a0053cc545eab4a184c627f4f5507c4bd/docs/M97_LIVING_WORLD.md).

Online sessions are optional and host-authoritative. Their entity subset is narrower
than offline simulation: vehicles, articulation, liquids, terrain and arbitrary
spawned identities are not universally replicated. Local player identity is
independent of devices and controlled entities. See [network/security boundaries](NETWORKING.md)
and [local multiplayer](LOCAL_MULTIPLAYER.md).

## Editor V2 ownership (Experimental)

The desktop editor remains an engine consumer, with one project/document/history,
one authoritative played world and the shared JudasJS runtime. `EditorApplication`
coordinates the frame; it does not implement hierarchy, inspector, import or
export bodies. [Editor V2](EDITOR_V2.md) maps the current source owners and
qualification boundaries.

`editor/EditorShell` owns dock/UI composition and platform-window presentation.
`EditorPanelRegistry` performs deterministic setup-time registration and phase/
visibility dispatch over a frame-scoped borrowed `EditorContext`. The existing
`EditorDocument` owns authored mutations and undo. `EditorProjectWorkflow`,
`EditorRequestWorkflow` and `EditorExportWorkflow` own project/document commands;
`EditorPlaySession` owns Start/Stop and the single runtime; `EditorSceneViewport`
and `EditorViewportPresenter` own camera/targets/content bounds and input mapping.

Actual panel bodies live in `HierarchyPanel`, `InspectorPanel`, the separate
asset list/details/import modules, scene settings, profiler, streaming and UI
asset modules. `AssetPublicationWorkflow` publishes completed import/cook results
at a safe authoring boundary even while the browser is hidden. Panel-local drafts
remain local; shared services/selection and persistent visibility are explicit.
The application frame does not gain a panel-specific branch when a descriptor is
registered. The Console extension is the focused proof path, qualified separately.

Breakpoint suspension draws the safe shell and debugger only. It must not pump
simulation, save/load, streaming publication or recursive script execution while
QuickJS's C/JS stacks are suspended. Platform windows share the one editor ImGui
context; they do not create a scripting VM or physical world.

Docking uses the pinned Dear ImGui docking vendor with its matching SDL2/OpenGL
backends. Native OS detachment is admitted only for x11/windows; other backends
retain docking/in-window floating. Linux V2 automated qualification covers native
detached Scene/Game views; Windows V2 has not been rerun. Scene/Game targets use
1× logical resolution, not native HiDPI backing-pixel resolution. The
[qualification receipt](EDITOR_V2_QUALIFICATION.md) records the scoped checks;
combined human acceptance remains pending. This is not an Alpha v2.0 release.

## Native interfaces, builds and validation

Native headers document actual interfaces, not a stable binary SDK ABI promise.
JudasJS is the supported project scripting surface: trusted QuickJS modules,
without Node/npm or unrestricted host modules. Optional TypeScript tooling gives
completion for JavaScript; it is not a runtime dependency.
Subsystem bindings and facade fragments are registered through one compiled
[scripting catalogue](../src/scripting/Modules.json); the [Scripting V2 guide](judasjs/scripting-v2.md)
explains runtime ownership and local extension. Metadata/property inspection
lives in `scripting/ScriptMetadata.cpp`, sharing the private VM implementation.

CMake separates subsystem targets under `cmake/targets/`, shares test helpers and
finalizes SDK/validation after declarations. Build identity records version,
channel and exact source inputs; archives can carry provenance without Git.
See [build identity](BUILD_RELEASES.md), [distribution](DISTRIBUTION.md) and
[focused checks](CURRENT_STATUS.md).

Linux is the acceptance reference. Windows VM/Android receipts have individual
version/hardware scope. Main is human-tested release lineage; Experimental is
development. Published Alpha identities remain in the release record; this
Experimental editor work does not publish a version or change release channels.

## Project policy and basic water

Project-wide water policy is serialized by both Project and NamedAuthoring codecs
and captured by RuntimeWorld::ConfigureProject before construction. Runtime,
editor Play, scene replacement, dedicated hosting and cold-save staging apply
the same policy. UI lives in `src/editor/ProjectSettingsPanel.cpp`; it reuses the
existing project input, classification, navigation, localization, audio, residency
and export settings rather than introducing a parallel configuration service.

`BasicWater` is a prescribed oriented volume with GPU visual waves and cheap
swimming/optical queries. It does not own conserved quantity or rigid-body forces.
RuntimeWorld indexes authored/streamed owners; `WorldWater.cpp` samples their
existing entity poses. Alpha distributions reject advanced-fluid project activation before world construction
and export due to contact-stability issues; Experimental/local builds retain the
opt-in implementation for repair. See [restriction (historical)](https://github.com/SuperReaper1999/Project-Judas/blob/2ee4203a0053cc545eab4a184c627f4f5507c4bd/docs/M100_FLUID_CONTACT_STABILITY.md).
Advanced Game Water V2 updates and allocations
are gated by explicit project opt-in. No fluid numerical solver, MK2 contact
solver or physical balance policy is redesigned by this separation.

See [Project Settings and water](PROJECT_SETTINGS_AND_WATER.md) for exact defaults,
intentional Pre-Alpha compatibility changes, supported ownership and limitations.


### Developer observation boundaries

`src/editor/RuntimeInspector.cpp` is optional editor-only UI over current ownership,
region retention, native body/motor publication, `ScriptSystem::InspectPhase` and
already-initialized save-slot metadata. It does not serialize VM state, start a
save service, request regions or invoke callbacks. Its native lifecycle observation
is deliberately not a new JudasJS readiness promise. `SaveEvolution` supplies the
same identity mismatch description to rejected loads and save listings; migration
policy and native participant validation remain unchanged. Current operating
instructions and the shipped small SDK example are in `docs/DEVELOPER_WORKFLOWS.md`.

### Native 2D

Sprites and orthographic cameras share the existing renderer/scene path. Native
planar bodies share MK2's world, clock, handles, CCD, contact and joint ownership;
`PlanarGeometry` owns lamina shapes, contact witnesses and snapshot casts. Reduced
linear/angular mobility restricts physical response to the fixed authored plane
without post-step clamps or three extra bilateral constraints per body.
`PlanarAuthoring` centralizes runtime/preflight boundaries. Planar pin/weld rows
use the existing joint solver. Scene descriptors and modern physical save
participants retain geometry, mass, inertia and fixed plane; legacy world deltas
reject them. Player movement/cameras remain ordinary editable project JavaScript.
See [Stage 2 (historical)](https://github.com/SuperReaper1999/Project-Judas/blob/2ee4203a0053cc545eab4a184c627f4f5507c4bd/docs/2D_STAGE2.md) and [2D API/boundaries](judasjs/physics-2d.md).

Sprite sheets and frame clips are optional typed CPU resources. `SpriteAnimation`
owns content validation/sampling; `WorldSpriteAnimation` owns fixed-boundary
instance clocks and observations. Skeletal and sprite markers share interval
traversal, while preview/render passes never advance gameplay. Existing scene
definitions, prefab/stream snapshots, resource generations and modern saves own
their normal lifetime; conditional content hashes prevent silent clip reinterpretation.
See [Stage 3 (historical)](https://github.com/SuperReaper1999/Project-Judas/blob/2ee4203a0053cc545eab4a184c627f4f5507c4bd/docs/2D_STAGE3.md) and [the sprite animation API](judasjs/sprite-animation.md).

`authoring/Tilemap` compiles bounded editor-owned sparse grid recipes to ordinary
sprites and chunk-merged static planar boxes. `editor/TilemapEditor` owns palette
selection, ray/grid painting and draft strokes; `EditorDocument` owns publication
and Undo. Existing scene recipes retain source metadata; runtime physics, saves,
streaming and exports use the compiled ordinary entities. There is no additional
tile simulation or runtime clock. See [Stage 4 (historical)](https://github.com/SuperReaper1999/Project-Judas/blob/2ee4203a0053cc545eab4a184c627f4f5507c4bd/docs/2D_STAGE4.md) and the
[tile authoring guide](judasjs/tilemaps-2d.md) for limits and ownership.

## Initial game readiness

`StartupScreen` owns the game loading display; `StartupMovie` decodes its cooked
animation through the existing image loader. `WorldStartup` discovers readiness
through normal resource and presentation paths without advancing simulation.
The application coordinates this boundary before beginning its gameplay clock.
See [startup loading](STARTUP_LOADING.md) for lifecycle, packaging and limits.
