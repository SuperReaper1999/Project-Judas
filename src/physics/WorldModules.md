# Physics world implementation

`PhysicsWorld.h` remains the public API. `PhysicsWorld.cpp` coordinates one fixed
step; the files here implement distinct parts of that same world. They do not
create additional physics worlds or change which subsystem owns motion.

## Ownership

`WorldInternal.h` defines the private `PhysicsWorld::Impl`. One `PhysicsWorld`
owns it from `Init` to `Shutdown`. Bodies, generations, the broadphase tree,
constraints, continuation history and scratch storage have the same lifetime.
The declaration order and initializers are retained from the original world.
These headers are implementation details, not an SDK interface.

| Module | Responsibility |
|---|---|
| `WorldBodies.cpp` | Body creation/destruction, motion commands, impulses, physical properties and public body observations |
| `WorldBounds.cpp` | Prepared bounds, broadphase proxies, ordered candidate discovery and cache accounting |
| `WorldMk2Solver.cpp` | The substepped simultaneous solve: contact blocks, joints, arrival-instant impacts, fast-body continuous pass and contact history publication |
| `WorldCCDSearch.cpp` / `WorldCCDQuery.h` | Conservative impact search and finite drift bounds, behind a private read-only CCD view (the fast-body pass's exact query) |
| `WorldContacts.cpp` | Contact observations, touch phases and optional joined initial-geometry preparation |
| `WorldSleep.cpp` | Wake propagation, sleep islands and explicit sleep-state persistence |
| `WorldJoints.cpp` | Joint construction/configuration and preparing existing solver inputs |
| `WorldQueries.cpp` | Native casts, closest-point/contact samples and character sweeps |
| `WorldParticles.cpp` | Existing finite-mass particle/body velocity coupling and its numerical preconditioner |
| `WorldPersistence.cpp` | Existing native contact/joint/kinematic continuation records and context validation |
| `WorldProfiling.h` | Inclusive, nested per-category work timers, avoiding one profiler event per CCD predicate |

`WorldHotAccessors.h` keeps small generation, classification and exact-cache
checks available to the compiler. `WorldImpactQueries.h` does the same for the
shared represented-pose/contact query path. Neither header owns independent
state or supplies an alternative collision policy.

`WorldBounds.cpp` discovers candidate pairs with an active-leaf tree traversal.
Caller-owned ancestor marks and traversal scratch are derived anew each query;
no old candidate membership is retained across steps, mutations or origin changes.
Each overlapping unordered leaf pair is visited once, then current collision
policy is applied and the result is canonically sorted. Other tree queries keep
their existing interface. The profiler's root-query count describes a batched
traversal now; its overlapping-leaf count counts emitted pair endpoints, not the
old repeated single-body-query visits.

`ContactConstraint` groups velocity-phase data together before cold geometry
and positional anchors. Its named data and save contracts are unchanged, but
native consumers require matching rebuilt headers and engine binaries.
`ContactVelocity.h` evaluates friction's angular mass through the explicit
`r x direction` Jacobian. This preserves exact represented rotational nulls and
avoids two redundant outer cross products; normal mass, Coulomb limits and
iteration policy remain unchanged. Floating association can change trajectories.

`RigidBody::InverseInertiaWorld` publishes symmetric authored tensors with exactly
matching mirrored entries. Independent float rounding previously made the world
tensor slightly asymmetric; completing a four-point normal matrix from its upper
triangle could then create a false negative eigenvalue. Contacts, joints, direct
impulses and impact energy observations now share the corrected tensor. The native
quaternion-to-rotation mapping and normal solver tolerances remain unchanged.
Malformed or already asymmetric authored local tensors retain the original path;
the correction does not invent positive inertia for locked or invalid axes.

## Ordering and interfaces

The public coordinator retains the existing `Step`, `StepCoupled` and origin
translation order. Moving a method does not authorize changing when it runs.
Methods in the modules operate on that coordinator's private state; scratch data
must not become another source of physical truth.

`WorldMk2Solver.cpp` solves every step, `StepCoupled` intervals and
`VelocityCoupling` participants included. (MK1's time-of-impact event pipeline
was retired; docs/MK2_PHYSICS_ARCHITECTURE.md records the replacement.) A
coupling keeps its per-step
contract: `begin(dt)` once after row preparation, exactly `iterations`
`solve(dt, round)` calls spread across the substeps' velocity rounds, and
`finish(dt)` once after the last pose advance. Every awake dynamic body is then
integrated per substep. A body a participant wakes mid-solve joins at that round
(deferred velocity consumed, its rows re-prepared, its joints appended without
re-warming solved rows). Joint warm starts are per-substep impulses and are
rescaled when a step's substep length differs from the one that wrote them.

Initial contact geometry may borrow the existing job service. Workers read one
frozen step boundary and write private indexed results; the owner joins them
before contact generation. CCD queries own derived pose/bounds samples, never
body motion or contact history. Body mutation, solving and callback publication
remain on the world owner. No public method becomes generally thread-safe.

Generation checks, restored-context invalidation, completed observations and warm
starts remain distinct mechanisms. A cached contact is not a general license to
skip geometric invalidation, save unknown historical state or infer mesh-sided
contact ownership.

## Building and checking changes

`cmake/PhysicsWorldSources.cmake` supplies one source list for the engine and all
headless fixtures that compile the world directly. Configuration rejects a
fixture that selects only part of that list. Add a cohesive module there rather
than copying its path across individual targets.

The extraction is checked independently of functional corrections using exact
method-body/state tokens, unchanged public headers, identical test objects linked
against old/new engine archives, physical trajectory digests and matched timing
workloads. See `docs/evidence/prealpha-pass4/refactor/` for the actual receipts.
Those comparisons establish preservation for the measured contracts; they do
not certify previously failing CCD cases or every possible physical trajectory.

The scoped `.clang-format` expands dense code for readability. Do not shorten
formatting to meet a line count or move unrelated code into a replacement giant
file. Keep functional changes separable from extraction and formatting evidence.
