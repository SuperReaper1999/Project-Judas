# Physics in V2

V2 uses one production MK2 rigid-body world. Character motors, wheels,
powertrains, deformable materials, fracture, terrain and optional water participate
through explicit ownership boundaries; none is a second authoritative rigid solver.
The separate physics-core and contact-material experiments are not selected by the
ordinary build. See [the production solver contract](MK2_PHYSICS_ARCHITECTURE.md).

## Choose a representation

| Requirement | Authoring / runtime interface | Boundary |
| --- | --- | --- |
| Rigid collision and motion | Body, cooked shapes, `entity.body` | Explicit physical geometry; visual scale is not automatic collider scaling |
| Script-owned character | CharacterMotor, [character reference](judasjs/character.md) | Script owns input/camera/game rules; motor owns supported sweep/support movement |
| Scripted continuous obstacle | Kinematic body and target | Use a target for continuous motion; transform writes are teleports |
| Native planar game | [2D bodies](judasjs/physics-2d.md) | Planar and 3D physical bodies cannot coexist in one physical world |
| Constraint assembly | [Joints](JOINTS.md), [structures](STRUCTURES.md) | Authored frames and finite supported joint laws |
| Vehicle | [Vehicles V2](VEHICLES_V2.md), [powertrains](POWERTRAINS.md) | Native suspension/tyres/rotors; project-owned driving policy |
| Cloth, solid or physical panel | [Deformables](DEFORMABLES.md), [authoring](DEFORMABLES_V2_AUTHORING.md) | Finite discretization and sampled soft contacts, not arbitrary mesh CCD |
| Damage and debris | [Fracture](FRACTURE.md), [mechanical damage](MECHANICAL_DAMAGE.md) | Actual finite connections/parts, not a cosmetic damage overlay |
| Terrain | [Terrain V2](TERRAIN_V2.md) | Source-owned static surface/volume products and explicit publication |
| Water interaction | [Project water policy](PROJECT_SETTINGS_AND_WATER.md) | Basic Water is prescribed; Advanced Fluids is opt-in conserved grid water |

## Fixed-step ownership

`Simulation.cpp::StepAuthoritativeWorld` owns normal integration order. Scripts
submit intent at the fixed boundary; navigation and network roles have their own
preparation. A network replica does not run duplicate local authoritative physics.
Animation/physical targets prepare, external forces are applied, and MK2 advances
chronological rigid intervals. Vehicle and deformable callbacks couple through
those intervals. Final component observations, poses, sockets, events, signals and
network snapshots publish through the same world.

Material quality subdivisions are local accuracy budgets, not permission to
multiply whole-world integration time. Full internal/self-contact/finish kernels
can use independent worker batches with exclusive numerical data; rigid contacts,
attachment reactions and publication stay ordered on the owner. Platforms whose
instance move cannot guarantee no-throw transfer use the same kernels on the owner.
No friction, contact coverage, iterations or physical time is removed by that policy.

The simulation accumulator has explicit catch-up/clipping limits. A fixed-step
count or a passing functional test does not prove zero time loss. The benchmark
reports incoming clipping, discarded backlog, caps and actual completed steps even
with detailed profiling disabled. Never hide an overloaded workload by changing
those policies or timing only its cheapest frames.

## Contacts, queries and events

Author geometry using [collision cooking](COLLISION_AUTHORING.md). Visual meshes,
physical compounds, sensors and query filters are separate choices. Broadphase
candidates are not exact intersections. Use [shape queries](PHYSICS_QUERIES.md)
and [batched queries](BATCHED_QUERIES.md) for the documented geometry/result semantics.
[Collision events](COLLISION_EVENTS.md) deliver generation-safe observations at
safe callback boundaries; removal and stale handles have explicit lifetimes.

MK2 owns warm starts, contact history, sleep and chronological collision response.
Preparation caches retain conservative geometry/revision checks and rebuild when
invalidated. They do not replace the physical law or authorize skipping relevant
bodies. [The event debugger](PHYSICS_EVENT_DEBUGGER.md) supports safe pauses,
checkpoint/replay and numerical inspection. The optional native
[constraint batch interface](PHYSICAL_CONSTRAINT_BATCH.md) has a narrower,
experimental contract and no automatic JudasJS or fluid-backend selection.

## Persistence, performance and support

Use normal [save slots](PERSISTENCE.md), not pose-only overlays. Persistent physical
state and supported component participants restore before the resumed fixed step;
unsupported or incomplete historical state rejects. [Origin shifts](ASSEMBLY_TRAVEL.md)
translate owned local points while retaining identities, velocities and local frames.
Streaming support remains participant-specific, not arbitrary assembly suspension.

Dense contact, severe impacts, folding panels and large pile-ups remain expensive.
Bounded CCD can hold/truncate travel, elastic runs can dissipate energy, and extreme
spin has a limited envelope. These limits are not removed by the native Linux
[development-ready verdict](DEVELOPMENT_READINESS.md). Whole-frame measurement and
hitch/time-loss interpretation belong in [performance engineering](WHOLE_ENGINE_PERFORMANCE.md).

The native validation manifest covers rigid contacts/CCD/rotation, original mesh
arrival and continuation, vehicles, material coupling, saved endpoints and
lifecycle. Reuse the current source-aware runner rather than an obsolete demo
project or private crash-yard launcher. Private vehicle assets are not distributed
with the engine or SDK. A synthetic native pass is not human physical acceptance.
