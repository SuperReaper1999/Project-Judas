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
| `WorldImpactEvents.cpp` | Initial impact response, event scheduling, affected-island discovery and velocity publication |
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

## Ordering and interfaces

The public coordinator retains the existing `Step`, `StepCoupled` and origin
translation order. Moving a method does not authorize changing when it runs.
Methods in the modules operate on that coordinator's private state; scratch data
must not become another source of physical truth.

Only initial contact geometry may borrow the existing job service. Workers write
private indexed results; the owner joins them before consuming the results.
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
