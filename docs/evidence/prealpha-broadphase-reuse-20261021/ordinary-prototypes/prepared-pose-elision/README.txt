PRIVATE SOURCE-ONLY EXPERIMENT: PREPARED POSE ELISION
==================================================
Base: 80f7b2a5f96cdf1d56c5c88043c16edefe70a4c3.
No production source, public API, Impl layout, build or workload changed here.
PhysicsWorld.cpp is the exact base file plus prepared-pose-elision.patch.
Not compiled or executed. No measured benefit claimed.

Mechanism
---------
WorldContacts.cpp:89-99 limits initial prepared outputs to non-asset Box/Sphere,
non-sensor/non-query-only pairs that are not both sleeping/static. Each has one
primitive. Workers already constructed both PrimitivePose values, native manifold
and touching truth (WorldContacts.cpp:140-153). The owner consumes that output in
PhysicsWorld.cpp:181-229, but still calls PrimitiveAt twice before choosing it.
Narrowphase.cpp:14-16 copies Shape and RigidBody for each such call. Shape includes
vector, shared pointers and string, and RigidBody includes inertia/accumulators.
Those owner copies are not used by any prepared branch.

This private variant uses disengaged optional storage on prepared pairs and only
constructs primitive poses on the original serial/fallback path. It retains the
outer childA lifetime and one-childA-per-part order for compound pairs. Native
geometry calls and their order, margin, normals, anchors, manifold copy, cache
matching, solver/contact observation order and all arithmetic are unchanged.

Expected source-level work reduction: exactly two redundant PrimitiveAt calls per
prepared pair, zero changed manifold calls. This does not assert optimized machine
code cost: compiler elision may already reduce some copies; measure independently.
Optional construction may add a move on serial fallback, so include that control.

Qualification plan (root owns build/CPU lane)
-------------------------------------------
1. Compile only this alternative PhysicsWorld.cpp using the normal target flags,
   create a fresh private library replacing the PhysicsWorld object, and relink
   fresh driver objects. Keep normal core archive unchanged. No inline/header or
   Impl layout changes are needed.
2. Run existing ParallelNarrowphaseTests with serial, 2-lane and 4-lane controls.
   It already compares exact physical states, ordered contacts, geometry diagnostics,
   mixed/rotated/cooked paths, filtering, slot reuse, sleeping, worker shutdown,
   unrelated failed jobs, cold saves and shared resource-service ownership.
3. Compare base/variant fixed-step physical rows and work rows on unchanged room1024
   for the normal short and late windows. Expect identical rows except wall times.
   Initial worker completed count must be nonzero; a serial-only run is not a
   positive control for this change. Also compare jobs-null/lanes=1 fallback.
4. Compare median/p95/worst narrow-phase and full fixed-step duration in matched
   runs. Keep only a reproducible improvement, and reject if tiny/noisy/regressive.
5. If extending fixture coverage, exercise >=256 eligible pairs mixed among compound,
   cooked, disabled, sensor/query-only, sleeping/static and pivot-offset simple pairs.
   Both zero manifolds and positive/speculative manifolds must be consumed. Keep
   exact geometry diagnostics, physical endpoints and ordered callback observations.

Other source findings, not included in this prototype
---------------------------------------------------
* FinishTouches (WorldContacts.cpp:32-51) rebuilds all candidates for any enabled
  sensor/kinematic, then discards ordinary pairs. Querying only relevant endpoints,
  retaining CanCollide and canonical sort/unique order, can avoid that scan in games
  with sparse sensors. The unmodified room1024 authoring contains no sensor,
  kinematic or CharacterMotor, so this is not established current scenario-3 cost.
* PrepareInitialContactGeometry's extra zero-margin existence test currently builds
  a complete manifold. Existing PrimitiveHasContacts (Contacts.cpp:647-689) shares
  exact accepted-contact predicates and diagnostics for simple zero-margin pairs
  without response witnesses. This is a separate plausible work reduction, and
  must be compared independently and validated rather than assumed profitable.
* Initial geometry uses up to four total lanes by default, including owner, not
  every hardware worker. RuntimeWorld.cpp:339 attaches the resource JobSystem;
  EngineHost.cpp:53 supplies it in the normal app. Actual worker-completed counter
  is the positive evidence, not merely requested lane count. Do not attribute the
  remaining source-serial owner assembly cost to missing parallel geometry.
* broadphaseMilliseconds is not pure candidate/tree time: PhysicsWorld.cpp:481-482
  adds the entire endpoint/proxy/cache-stat publication tail. First prediction
  candidate generation occurs before its start timestamp. Profile named scopes,
  not this mixed counter alone, when deciding what remains expensive.
