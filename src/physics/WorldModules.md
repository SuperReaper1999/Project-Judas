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
| `WorldCCDSearch.cpp` | Conservative impact search, finite drift bounds and continuation certification |
| `WorldCCDQuery.h` / `WorldCCDDispatch.cpp` | Private read-only CCD views, bounded lane scratch and joined independent query batches |
| `WorldImpactEvents.cpp` | Initial response, event scheduling, fallback islands and authoritative observation/motion publication |
| `WorldIncrementalImpact.cpp/.h` | Private growing contact transaction for free primitives; actual impulse dependencies, deferred-gravity preparation and commit/fallback boundary |
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

Growing impact discovery also checks the existing remaining-trajectory enclosure
of a candidate against the expanding body's actual event-time bound. Only a
finite, disjoint enclosure avoids preparing detailed geometry. Touching,
uncertain and unsupported cases retain native contact queries; no negative
manifold, impulse or contact truth is cached by this check.

`ImpactCandidateCache.h` retains sorted broadphase candidate lists only during
one impact-event pass. Any fat-proxy reinsertion invalidates those lists,
including movement of the other endpoint. A contained query filters the retained
superset against current proxy bounds; queries outside that enclosure or the
cache budget use the ordinary tree query. Collision filters, actual geometry,
contact response and impact times are still evaluated normally.

`ImpactEventSchedule.h` owns the step-local pending-event heap and endpoint
adjacency index. Chronology remains exact time followed by lexicographic pair
identity. No-impact identities occupy neither the heap nor its dependency lists;
a changed endpoint discovers them again through broadphase candidates. The
existing `RemainingImpactBound` supplies a whole-interval separation certificate
before pair expansion and during island discovery. It is invalidated on every
actual motion change and can retain a conservative superset on an unchanged
trajectory. Uncertain/unsupported shapes use the original query. These are
geometric proofs, not cached sampled misses or velocity tolerances. See
[the event dependency investigation](../../docs/PREALPHA_EVENT_DEPENDENCIES.md).

At the initial physical instant, only actually touching rows connect impact
components. Positive-gap speculative rows remain available for the later
ordinary gap/time solve; they cannot merge disconnected shock/restitution
components or invalidate the other component's warm guesses.

The [persistent-contact redesign](../../docs/PREALPHA_INCREMENTAL_IMPACTS.md)
separates ordinary support-force warm starts from new impact increments. A new
impact grows its private contact domain only through actual per-row represented
velocity changes, without an island-size or impulse cutoff. Appended constraints
retain the event's earlier impulses and immutable incoming velocity snapshots.
The existing support sets own continuation classification; no second history
registry, save schema or component lifecycle is introduced. Hard-jointed dynamic
bodies, non-primitive geometry and external velocity coupling retain the complete
legacy solve. A rejected trial publishes no body, sleep or observation changes.
Growing-event solve timing excludes its nested discovery timing; other profiler
categories retain their documented inclusive meaning.

The world lazily owns the growing transaction's scratch storage. It reuses
capacity across fixed steps, but resets every row, frame, membership set and
result for each attempted event. Solver views and saved body pointers retire
before private body arrays can relocate. Current event-time poses initialize
every participating body; scratch is neither continuation history nor saved
physical state. Separate worlds own separate workspaces, and shutdown releases
their retained high-water capacity. A world without an impact allocates none of
this optional workspace.

Ordinary free-body support and transient growing impacts share the prepared
normal-patch response. A point-only transient prototype reduced disturbed solve
cost but regressed ordinary settling and sleeping; it is not retained. Every
manifold point remains represented. The existing energy and feature-displacement
guards still decide private-trial publication; they are not proofs of exact
complementarity or universal CCD completeness.

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


`ContactGroupUpdates.h` stages only touched primitive groups in the sorted
completed-observation and warm-start stores. Reads resolve the latest patch or
binary-search the original store. Generation-sensitive replacement remains with
the event owner. One ordered splice publishes at the boundary, including failure
unwind; coupled fallback publishes before globally removing its affected rows.
No solver reads the staged history and no new persistent state is introduced.
This removes repeated whole-world erases/scans without changing warm-start values
or stable row order. The independent reference test compares the former
mutation/sort algorithm, including slot reuse and failure publication.

## Ordering and interfaces

The public coordinator retains the existing `Step`, `StepCoupled` and origin
translation order. Moving a method does not authorize changing when it runs.
Methods in the modules operate on that coordinator's private state; scratch data
must not become another source of physical truth.

`WorldMk2Solver.cpp` (MK2, the default) solves every step it is selected for,
`StepCoupled` intervals and `VelocityCoupling` participants included; MK1's TOI
pipeline runs only under `JUDAS_PHYSICS_MK1`. A coupling keeps its per-step
contract: `begin(dt)` once after row preparation, exactly `iterations`
`solve(dt, round)` calls spread across the substeps' velocity rounds, and
`finish(dt)` once after the last pose advance. Every awake dynamic body is then
integrated per substep. A body a participant wakes mid-solve joins at that round
(deferred velocity consumed, its rows re-prepared, its joints appended without
re-warming solved rows). Joint warm starts are per-substep impulses and are
rescaled when a step's substep length differs from the one that wrote them.

Initial contact geometry and eligible independent CCD searches may borrow the
existing job service. Workers read one frozen event boundary and write private
indexed results; the owner joins them before canonical event publication. CCD
queries own derived pose/bounds samples, never body motion or contact history.
Small, unsupported, over-budget and unavailable-pool batches retain serial search.
The optional scratch payload is bounded at 32 MiB; metadata is counted separately.
The profiler distinguishes summed query CPU time from elapsed batch wall time.
Body mutation, event ordering, solving and callback publication remain on the
world owner. No public method becomes generally thread-safe.

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
