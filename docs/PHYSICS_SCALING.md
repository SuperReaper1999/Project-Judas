# Physics scaling and sleep islands

## Implemented changes and ownership

| Mechanism | Change | Preserved contract |
|---|---|---|
| `PhysicsWorld::Impl::Settle`, `src/PhysicsWorld.cpp` | Retained slot-indexed sleep graph/island scratch; aggregate current support, joint and motion conditions without rescanning every joint/contact for every island | Same generation validation, union order, ascending island/member order, physical quiet/anchor tests, timers and wake behavior |
| `JointSolver`, `src/JointSolver.h/.cpp` | Retained open-addressed pointer membership for prepared joint states and participating bodies | Input/row order and the first-encounter `Bodies()` vector remain authoritative; lookup tables are never used to order a solve |
| Box preparation, `src/CollisionGeometry.cpp` | Reserve eight transformed vertices and six normals; replace repeated set construction with the same twelve canonical edges | Same faces, transformed arithmetic, normal calculations, sorted edge/witness order and SAT edge identities; cooked hull path unchanged |
| Existing profiler profiling in `src/PhysicsWorld.cpp` | Additional phase scopes and bounded aggregate work counters | Diagnostics observe work; they do not select physical quality, contact policy or simulation behavior |
| Initial primitive geometry, `PhysicsWorld::PrepareInitialContactGeometry` | Joined, bounded geometry batches borrow the existing `JobSystem`; owner consumes indexed results in candidate order | Bodies, response rows, impact scheduling, callbacks and authoritative state remain owner-thread operations |
| Motion/CCD geometry, `src/RigidMotion.h`, `src/RigidMotionBounds.h` | Geometry-only ledger samples, exact-key derived pose/orientation caches and conservative primitive sweep bounds | Same anchored represented pose; full physical angular-velocity transport still occurs for physical consumers; uncertain/unsupported bounds retain the ordinary query path |
| Primitive contact existence, `src/Contacts.cpp` | Zero-margin box/sphere existence shares validated contact predicates without unnecessary witness construction; SAT orientation rows are prepared on demand | Same touching/uncertainty boundary and fallback predicates; no margin inflation, axis tolerance or reduced response manifold |
| Impact bookkeeping/publication, `src/PhysicsWorld.cpp` | Constant-time membership, first-discovered canonical manifold reuse and one stable completed-observation sort per impact pass | Existing first-hit discovery, contact-row order, actual geometry, primitive identity and final observation order remain authoritative |

These are storage, traversal and diagnostic changes. There is no new solver,
integration method, tolerance, iteration budget, contact reduction policy, CCD
exemption or sleep threshold. Before/after trajectory digests and unchanged
physical regressions must support that claim for the exercised workloads.

### Sleep islands

Every fixed step rebuilds logical membership from current, generation-validated
contacts, touches and active joints. Retained arrays contain derived scratch, not
another source of body authority or serialized continuation. Destroyed or reused
slots cannot retain an old island's logical membership.

Static support anchors islands without joining unrelated assemblies through a
shared floor. Existing support, angular/linear motion, joint errors, motor state,
limits and quiet-time requirements still determine sleep. Deep contact proximity
retains its existing geometric test. Inactive objects retain collision/query
geometry, contact/lifecycle state and explicit-force/impact wake behavior.

### Joint membership

The pointer tables answer only “already prepared?” and “already encountered?”.
`Prepare(..., append=false)` clears their logical contents; storage capacity is
retained. Appending preserves previously prepared rows, impulses and observations,
and skips a joint already prepared at that boundary, including a failed material
joint which contributed no rows. Repeated ordinary, non-append duplicate inputs
retain their prior duplicate-row behavior. Keys are non-owning and never persisted.

Hash collisions or different allocation addresses cannot reorder body traversal or
floating-point row arithmetic. Retaining the tables trades some bounded-by-peak
workload memory for avoiding repeated linear scans and per-member allocation.

### Joined initial geometry jobs

`RuntimeWorld` borrows `ResourceManager::Jobs()` for its initialized `PhysicsWorld`.
No new worker pool or thread is created. The native
`PhysicsWorld::SetJobSystem(jobs, maxLanes=4)` is configured only at a completed
step boundary; the borrowed service must outlive any joined step using it. The
normal world configuration permits at most four execution contexts **including
the caller**, rather than four extra workers. A native caller can explicitly
choose another limit within the existing job-system bound. This pointer and its
configuration are execution services, not saved physical state or JudasJS policy.

Only eligible ordinary box/sphere initial candidate pairs use the batch. Sensors,
query-only bodies, compound/cooked/terrain geometry and pairs with no active
responding body retain their existing path. At least 256 eligible pairs are
required; a batch exceeding 16,384 eligible pairs uses the serial path rather than
allocating an unbounded manifold array. No pool, a stopped pool, a one-context
limit or a small batch also selects serial execution. This changes where geometry
is computed, not which collision pairs or response rows exist.

The owner freezes the step inputs and prepares orientation data before dispatch.
Joined jobs read that immutable shape/pose view and write disjoint indexed
manifold/touching outputs, claiming small ranges from one cursor. They never
mutate bodies, trees, contact caches, solver rows, event state, scripts or resources.
The owner consumes results in the original candidate order; contact matching,
row construction, impulses, wake changes, all impact-event searches/solves and
callbacks remain serial. This is parallel initial geometry, **not** parallel
physics-world stepping or a parallel contact solver.

Submission/main/worker failures join every submitted task before propagating the
error. Jobs still queued after the caller exhausts the cursor can be cancelled;
no useful output is lost. Outputs incomplete because a job never ran are computed
serially only after all joins. No fallback races a running job, and no job outlives
the step. Scratch capacity is retained within its declared batch bound and counted;
it does not become serialized continuation or a persistent geometry authority.

### Geometry observation and CCD work

Collision search needs represented pose and linear drift, not inertia-dependent
physical angular-velocity transport for every sample. `RigidMotion` therefore
provides geometry-only observations of the same anchored path. Physical solver
and endpoint observations still obtain the full transported angular velocity.
Derived caches use exact pose/time/generation/trajectory identity and are invalidated
at their owning step/trajectory boundary; they never restart integration.

Outward conservative translation and rotating-box sweep bounds can reject a
nonintersecting interval or actual-pose AABB before narrowphase. Their role is
absence certification only: they do not fabricate contacts, replace response
normals or select a different timestep. Unsupported or uncertain cases keep the
existing conservative fallback/query. The simple Boolean contact path shares
ordinary primitive predicates; it cannot turn an approximate overlap into an
accepted contact. Response still uses the complete ordinary manifold.

The impact pass reuses exact unchanged event-time discoveries and derived
membership instead of rescanning equivalent pairs. Completed observation rows
retain replacement/insertion semantics and are stably sorted at publication.
None of the rejected full-step manifold caches, persistent CCD deadline heaps,
private future broadphases or larger cold-solver iteration budgets is installed.
See the experiment record for their actual failures and limitations.

## Profiling boundaries

The profiler profiler now exposes preparation, prediction/wake, velocity integration,
candidate generation, initial contact generation, contact-row preparation,
velocity constraints, contact-history publication, continuous impacts, position
constraints, sleep assessment, proxy refresh and endpoint contact publication.
Child scopes nest inside the existing **Rigid physics** scope.

CCD can call geometry thousands of times in one step. Those operations use
per-step aggregates instead of emitting an event for every predicate:

- CCD search and CCD geometry inclusive milliseconds/calls.
- Impact-island discovery, impact-solve and completed-observation publication milliseconds.
- Initial geometry batch contexts, actual worker-completed manifolds and retained scratch bytes.
- Geometry predicate, exact-fallback, unresolved and numeric-gap-fallback counts, including joined-worker deltas.
- Joint-preparation milliseconds/calls.
- Candidate passes, root queries and overlapping leaves.
- Cooked BVH nodes, triangles and convex tests.
- Contact/joint rows, sleep graph edges/islands and retained sleep scratch bytes.
- Existing impact iterations, sampling fallbacks/caps and geometry-cache counters.

Detailed clocks are active when profiler recording is active. Categories are inclusive;
CCD geometry lies inside search/impact work, and impact solving can contain joint
work. **Do not add these aggregates together as disjoint costs.** Frame scope
history and spikes remain bounded by profiler's existing limits; inspect profiler drop
diagnostics. Per-step CSV is the complete timing series, whereas an exported profiler
report retains bounded history/spikes.

`Physics initial geometry worker lanes` counts permitted contexts, including the
caller; it is not proof that every worker ran. Use
`Physics initial worker completed manifolds` to see actual worker work. Worker
batch scope sums are aggregate thread work and overlap the owner's joined phase;
they are not additional serial elapsed time. Predicate counters merge worker TLS
deltas after join without counting the owner's lane twice.

Script contact-event dispatch now has one enclosing `JavaScript contact events`
scope per dispatch batch. Actual script callback/asset attribution remains;
uninteresting body pairs no longer consume a scope record merely for traversal.
Counter/scope aggregation avoids overflowing a bounded capture on thousands of
pairs, but every exported report must still be checked for dropped/truncated data.

The older `PhysicsWorld::StepStats` fields retain their original boundaries:

- `broadphaseMilliseconds` combines initial broadphase with post-solver proxy and
  endpoint publication work; it is not solely tree traversal.
- `narrowphaseMilliseconds` describes the initial contact pass, excluding later
  CCD geometry calls.
- `solverMilliseconds` includes contact/joint solving, impact handling, position
  correction, history work and sleep assessment.
- `totalMilliseconds` spans the internal step. Benchmark `input_and_step_ms`
  additionally includes external gravity submission and its fixed-step wrapper.

Use detailed scopes/counters to explain those totals. A slow field named “solver”
does not by itself establish that iteration arithmetic is the cause.

## Remaining limits

- Contact/constraint solving, impact-event ordering and world mutation remain
  serial. Only eligible initial primitive geometry has joined worker execution;
  islands do not imply a parallel solver or a job per island.
- The human-live 1,024-piece mixed-shape pile exposed repeated low-speed CCD
  contact-island work. Retained geometry/bookkeeping improvements do not establish
  that this worst-case event cascade is solved or comfortably interactive; final
  measurements and the unresolved limit belong to the follow-up report.
- Dense moving convex contacts and rotating/grazing mesh CCD can still dominate
  real game steps. The coin-pusher workload retains expensive spikes; cheaper
  sleep assessment is not evidence that its active CCD cost is solved.
- Existing CCD bounded-sampling restrictions, deep-contact behavior and numerical
  failure paths remain. physics-scaling does not weaken them to improve benchmark results.
- The existing deep-contact sleep veto still checks geometric proximity against
  dynamic bodies. Many deeply penetrating rows can therefore remain expensive;
  the ordinary-case island improvement is not a new worst-case complexity bound.
- Sleeping cannot make a genuinely moving motor/assembly inactive. There is no
  distance cutoff, hidden collision removal or artificial sleep command.
- Broadphase, narrowphase and solver scaling depend on spatial/contact density,
  not object count alone. These fixtures do not certify every thousand-body scene
  at 60 Hz or every real consumer workload.
- Linux is the acceptance platform. Windows remains informational/nonblocking;
  missing Windows validation must stay explicitly outstanding. Human acceptance
  is separate from builds, synthetic input, screenshots and measured checks.
