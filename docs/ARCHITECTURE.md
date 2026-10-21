# Judas architecture

Current implementation map through Experimental checkpoint
`be1b7d634c674dca34f3a36299da510492ca2aea` and this documentation cleanup.
This describes ownership and integration boundaries, not Alpha certification.
See [current status](PREALPHA.md), [documentation index](README.md), and
[JudasJS](JUDASJS.md). The former 10,248-line milestone chronology is preserved
verbatim after the archive banner in [Architecture history](ARCHITECTURE_HISTORY.md).
Old limits and pending-review labels there do not override this reference.
Source links require the matching checkout/source archive; SDK/runtime packages
do not contain every project, native source or historical evidence artifact.

## Engine and game boundary

Judas owns simulation, rendering, resources, authoring and platform mechanisms.
Projects own input mappings, camera policy and game rules through trusted
JavaScript. A query is not a weapon; a hinge is not a target. CharacterMotor,
camera and skeleton remain separate.

Native compatibility controllers remain for older projects. New scripted projects
use `legacyGameplay: false`. Historical `PlayerController` and demo controls are
not the modern gameplay API.

| Responsibility | Owner and source |
|---|---|
| Authored definitions | [Scene](../src/Scene.h), named serializers and project assets |
| Live component state | [RuntimeWorld](../src/RuntimeWorld.h) and subsystem implementations |
| Session/scene replacement | [GameSession](../src/GameSession.h), [SceneSession](../src/SceneSession.h) |
| Frame/fixed ordering | [InteractivePlay](../src/InteractivePlay.cpp), [Simulation](../src/Simulation.cpp) |
| VM, instances and callbacks | [ScriptSystem](../src/ScriptSystem.h), [world script bridge](../src/WorldScripts.cpp) |
| Rigid simulation/queries | [PhysicsWorld](../src/PhysicsWorld.h), [private module map](../src/physics/WorldModules.md) |
| Graphics/GPU objects | [Renderer](../src/Renderer.h), [presentation](../src/WorldPresentation.cpp) |
| Resource preparation/residency | [ResourceManager](../src/ResourceManager.h), [asset pipeline](M93_ASSET_PIPELINE.md) |
| Durable restoration | [WorldPersistence](../src/WorldPersistence.h), [save evolution](M98_SAVE_EVOLUTION.md) |
| Regional state | [WorldStreaming](../src/WorldStreaming.h), [living worlds](M97_LIVING_WORLD.md) |
| Network authority | [NetworkSession](../src/NetworkSession.h), [replication scope](M99_NETWORK.md) |

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
support. The [integration matrix](PREALPHA.md#ownership-and-integration-contracts)
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

Initial geometry and eligible independent CCD queries can borrow the existing
job pool over frozen inputs, using private derived scratch and bounded batches.
The owner joins results in deterministic pair order. Solving, chronology, body mutation and
callbacks remain owner-thread work; public physics methods are not generally
thread-safe.

Contacts, continuation identities, warm starts and completed observations are
separate state. A cache hit does not license skipping geometric invalidation.
Modern saves retain supported continuation data; an older snapshot cannot supply
historical data it never stored.

The original four curved-mesh continuation regressions are corrected and asserting
in the focused gate. Bounded CCD sampling, sidedness and deep initial overlap
remain limitations. Dense-contact cost is workload dependent. The latest
[ordinary support-history correction](PREALPHA_SUPPORT_CONTINUITY.txt) removes
repeated cold starts after eligible primitive impacts; it preserves separate
ownership of ordinary iterative guesses and actual completed event reactions.
It improves the measured 1,024-piece room without changing CCD or solver quality,
but does not establish universal 60 Hz performance. The subsequent
[CCD query and contact-history follow-up](PREALPHA_CCD_QUERY_FOLLOWUP.txt) removes
whole-world history scans per event and certifies cheap margin decisions before
full interval arithmetic, preserving exact fallback and physical trajectories. See
[current measured limits](PREALPHA.md#remaining-readiness-concerns) and the
[dense-support follow-up](PREALPHA_DENSE_SUPPORT_FOLLOWUP.md).

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
[CharacterMotor](judasjs/character.md), [planetary terrain](M85_PLANETARY_TERRAIN.md)
and [assembly travel](M90_ASSEMBLY_TRAVEL.md).

## Physical assemblies and liquids

Bodies, joints, wheels, powertrains, structures, material geometry, animation and
ragdolls compose through existing owners and fixed phases. They are optional.
JS owns driver policy, damage meaning and gameplay. Query-contact wheels require
no fixed chassis/drivetrain arrangement. Coarse physical crash sections and
material deformation have different practical cost envelopes.

Navigation supplies guidance; it does not own motor movement. Resolved skeleton
poses, physical contributions, IK and sockets share their pose authority.
References are grouped under [physics and worlds](README.md#physics-and-worlds).

M54 reservoirs/containers own quantity; optional M55 surfaces evolve distribution.
Owner-local storage survives supported shifts/restoration. Legacy PBF and
protected independent research are separate representations. A passing ledger
is not general fluid-fidelity or exterior-momentum certification. FLU-001 and
the half-turn grid-query identity limitation remain. See [reservoirs](LIQUID_RESERVOIRS.md),
[surfaces](LIQUID_SURFACES.md) and [current limits](PREALPHA.md).

## Rendering, cameras and resources

Renderer owns graphics/context use. Resource workers decode CPU content; private
GPU upload/replacement publishes on the graphics owner. Cancellation and eviction
must not expose incomplete resources. Async pumps have soft time budgets:
indivisible driver calls and decode expansion are not hard latency/memory guarantees.

Compatible opaque geometry uses pass-scoped instancing. Each view selects its own
lights/shadows; planetary LOD considers active interests within shared budgets.
Secondary/local views do not step another world. Cooked mip chains and whole-resource
residency exist; independent mip residency and universal material/pose instancing
do not. See [rendering](judasjs/rendering.md), [assets](M93_ASSET_PIPELINE.md) and
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
regions with reasons. See [saves](M98_SAVE_EVOLUTION.md) and [living worlds](M97_LIVING_WORLD.md).

Online sessions are optional and host-authoritative. Their entity subset is narrower
than offline simulation: vehicles, articulation, liquids, terrain and arbitrary
spawned identities are not universally replicated. Local player identity is
independent of devices and controlled entities. See [network/security boundaries](M99_NETWORK.md)
and [local multiplayer](M89_LOCAL_MULTIPLAYER.md).

## Native interfaces, builds and validation

Native headers document actual interfaces, not a stable binary SDK ABI promise.
JudasJS is the supported project scripting surface: trusted QuickJS modules,
without Node/npm or unrestricted host modules. Optional TypeScript tooling gives
completion for JavaScript; it is not a runtime dependency.

CMake separates subsystem targets under `cmake/targets/`, shares test helpers and
finalizes SDK/validation after declarations. Build identity records version,
channel and exact source inputs; archives can carry provenance without Git.
See [build identity](BUILD_RELEASES.md), [distribution](DISTRIBUTION.md) and
[focused checks](PREALPHA.md#focused-validation-from-a-clean-checkout).

Linux is the acceptance reference. Windows VM/Android receipts have individual
version/hardware scope. Main is human-tested release lineage; Experimental is
development. No M100 or Alpha release is declared.
