# Physics V2 — current architecture and qualification status

**Implementation checkpoint, 14 November 2026. Final SDK and performance qualification pending.**
The work starts from `d1ce11c7c14d498755e3437292f3a72d022d5783`. The production-default routine suite has completed: **179/179 passed** on the NVIDIA Linux desktop. Extended native contracts, final matching SDK, repeated gameplay measurements and remote CI are being qualified separately; this is not human acceptance or promotion to main.

## Implemented architecture

- **One fixed-step owner, four fresh native intervals.** Each interval prepares contacts at its current geometry and solves/advances once. This replaces preparing one manifold set and carrying it through four increasingly stale intervals; it does not increase physical interval count. Forces, source impulses, coupling rounds, motion history, contact publication and sleep retain their appropriate macro/interval ownership. An explicit outer `StepCoupled` subdivision composes with these intervals.
- **Support and geometric anchors remain physical.** The existing matched contact cache carries its steady support estimate across the macro interval; impact spikes are not mistaken for reusable support. Warm impulses scale using the duration of the kernel that produced them, separately from public observation duration. Spherical contact support follows sphere geometry rather than a rotating arbitrary surface-material anchor; friction continues to use physical angular point velocity.
- **Immediate arrivals precede unnecessary future searches.** Certain native box/identity-child compound contacts establish current-time approaching events first. Strictly future roots can remain dirty while those exact-time events run. All eligible current-time candidates are considered before event selection, deferred roots are completed before time advances, and trajectory revisions invalidate stale observations. Unsupported/uncertain cases retain conservative ordinary queries. There is no small-speed/slop collision skip.
- **Bounded independent geometry workers.** `JUDAS_MK2_QUERY_LANES=1|2|4` tunes query lanes; the production default is four, limited by the supplied worker pool and actual batch width. Only frozen native geometry queries run concurrently. The owner participates, joins every helper, and publishes in original stable pending order; the event heap retains deterministic time/key ordering. Small/unsupported work remains serial. Borrowed owner anchors cannot bypass allocation of lane-private bound storage.
- **Successful impacts continue.** The arbitrary fourth-distinct-arrival body freeze is removed. A native interval still has a shared 256-arrival work guard; failed responses and exhausted searches remain explicit conservative holds. This is bounded CCD, not a promise of unlimited collisions.

Search, advancement and geometry timings aggregate lane CPU work when workers
participate. They are not wall latency and must not be added to nested solve
scopes or GPU time. Use fixed-step/presented-frame duration and frozen-query
batch wall time for critical-path comparisons. After the frontier's 256-event
guard, terminal retirement is separately bounded by the world population;
the guard is not a promise of exactly 256 total contact operations.

Integration qualification also exposed a physical-balance recovery latch: a
brief upright sample could permanently disable further authored get-up attempts
before a real stand. The controller now uses current posture until measured
standing succeeds, retaining its original torque, effort and attempt budgets.
Accepted character steps record their true starting position, including an
initial trigger overlap that a lifted preparation probe previously omitted.

## Evidence obtained before final-default qualification

Correctness controls and whole-frame pilots have different scopes. The procedural eager/lazy and serial/two/four-lane proof retained complete exact per-step state, contact impulses, touch history and motion paths across nine ordinary scenes plus five precise frozen-ledger scenarios, with two worker repetitions. It passed with actual helper/deferred work, not silent serial fallback. The production-default registered scheduling test now passes all 14 scenarios across serial/two/four lanes. The character/controller correction passes 77 checks, borrowed-anchor queries 34 checks, and separation predicates 3,992 checks. The complete routine build and 179 routine gates passed; extended qualification and final SDK/CI remain in progress.

One NVIDIA desktop four-way pilot, 1280×720, uncapped/VSync off, 20 seconds after 10 seconds warmup:

| Query policy | Eager worst frame | Lazy worst frame |
|---|---:|---:|
| Serial | 125.96 ms | 81.71 ms |
| Four lanes | 96.15 ms | 76.25 ms |

These are representative single-trial wall-clock observations, not a completed three-repetition performance qualification or a universal speedup. Exact-state equivalence comes from matched fixed-step controls; wall-clock frame grouping differs.

Removing the body hold changes physical outcomes and must **not** be presented as a matched average-FPS gain. A subsequent four-lane pilot reported zero path holds versus the old body's peak of 36, with maximum fixed step **36.05 ms**, worst frame **85.80 ms**, and eight consecutive >16.67 ms frames spanning **357.31 ms**. No simulation time was reported discarded in that capture. It still fails the 60 FPS exceptional-impact budget and the aggressive 5 ms combined-engine goal. The capture dropped 349 profiler events: outer/fixed-step timing remains available, but missing internal attribution is not complete evidence.

Independent elastic-rattle controls passed **45 checks**. The old body guard allowed only about 15.6 m of travel: the corrected solver follows the 16.6667 m / 33.3333 m analytic paths at 1,000 / 2,000 m/s, with endpoint errors about 0.90 / 1.80 μm and zero reported relative energy error. It does not reset physical momentum to disguise truncation.

## Explicit remaining limits

The >256-arrival stress control still demonstrates the shared guard's limit: 200,000 m/s travel truncates (822 m versus a 3,333 m analytic path), five guard holds occur, and an unrelated later collision can be starved. Bounded work is necessary, but this is a documented correctness/workload limitation, not a passed real-time guarantee.

Massless controller translation now contributes swept observations for **static** gameplay sensors along accepted fixed-orientation movement, coalesced at publication. Rejected probes, penetration recovery and teleports are not genuine swept travel. Moving-sensor relative timing and rotating capsule sweeps are not inferred by this API; ordinary rigid-body completed-ledger trigger coverage is separate.

Final work remains: run the production-default regression/authoring/cold-save gates, preserve permanent separation/scheduling/empty-lane controls, repeat affected whole-game captures, rebuild and qualify the matching SDK, and finish actual CI. Windows hardware GL, Android and browser execution must be labelled unperformed unless executed. Human acceptance remains the operator's decision.

## Evidence locations

All local proof material is under `.cache/physics-v2-audit-20261113/`:

- `lazy-futures-fixture-results3.log`: exact procedural A/B and worker proof.
- `final-routine-ctest-v2.log`: 179/179 routine gates, including 77 character/controller checks.
- `lazy-futures-frame-proof/{eager-serial,lazy-serial,eager-four,lazy-four}/`: identities, raw frames and pilot summaries.
- `global-guard-frame-proof/{body-four,global-four,global-serial}/`: separate changed-physics capture, including raw drop counters.
- `arrival-guard-bounce-results.log` / `arrival-guard-bounce-stress.log`: analytic success and preserved negative stress control.
- `tiny-approach-proof/tiny-approach-analysis.json`: evidence rejecting a general tiny-speed collision shortcut.

Source ownership: `PhysicsWorld.cpp`, `physics/WorldMk2Solver.cpp`, `physics/WorldMk2Continuous.cpp`, `physics/WorldCCDSearch.cpp`, `physics/WorldImpactQueries.h`, `physics/WorldContacts.cpp`, and the character/step-climb translation callers. No second vehicle/deformable chassis solver was introduced.
