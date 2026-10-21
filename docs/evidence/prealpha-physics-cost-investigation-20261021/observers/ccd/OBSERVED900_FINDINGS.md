# CCD and repeated-input findings: steps 300–900

## Scope and verification

Source: `f5624f0bb4f92ece7211199ca8fe51abc08a2b19`.
Input: `../observed900/ccd.csv`, summarized by `analyze_ccd.py` into
`observed900-summary.json` and `.md`. This child executed lightweight CSV analysis
only; it did not run builds, simulations, or benchmarks.

There are 601 observed steps, no map overflow, no counter-accounting errors, no
aborted observed calls, and no repeated-result disagreements. Parent's preserved
`../parity-observed900.json` reports identical trajectory digests/workload counts
against the sampled baseline for every step in the window. These observations
describe this window, not the later live approximately 1 FPS state.

## Exact repeated-input memoization is not the missing speedup

| Query | Native calls | Exact repeated inputs | Repeat fraction | Repeated-input native wall time, total |
|---|---:|---:|---:|---:|
| Full manifold | 11,530,621 | 113 | 0.000980% | 0.237 ms |
| Boolean touching | 2,636,150 | 1,743 | 0.066119% | 1.473 ms |
| Penetration-threshold Boolean | 0 | 0 | Not exercised | 0 |

Only 2 repeated manifold inputs and 377 repeated touching inputs have a different
event-time anchor from their first observation. All complete repeated
`ImpactTime` request comparisons are zero. The preceding exact manifold-cache
experiment's rejection is consistent with these new counts: there is almost no
literal identical-input work to recover in this window, while map lookup and
storage cost remain. This does not evaluate other certificate representations.

### What really repeats

- 3,365,067 `ImpactTime` invocations produce 4,780 finite impact candidates and
  3,360,287 no-impact returns: **99.857952% no impact**.
- Initial scheduling accounts for 1,027,976 requests, of which 838,743
  (**81.591691%**) are rejected by the existing remaining-sweep AABBs.
- Rescheduling accounts for 2,337,091 requests; 2,334,988
  (**99.910016%**) return no impact. Every rescheduled request reaches existing
  start-pose acquisition; none is newly rejected by its inner AABB check because
  the dirty-discovery path already did that same bound test.
- 1,511,831 captured requests have a previously completed request for that pair
  in this step. Only **four** have exactly the same represented start geometry;
  all four also have changed velocities, interval and motion anchors.

Thus the repeated operation is **searching nearby pair identities again on new
anchored trajectories**, not re-running an identical pure query. Retaining an old
`false` result without a supporting proof would be invalid.

## Why inputs change and old no-impact answers cannot simply survive

`src/physics/WorldImpactEvents.cpp:610–615` treats any exact velocity-bit change as
a trajectory change. For every such endpoint, lines 684–696 append a new motion
segment at the current chronological event time, invalidate impact pose/bound
samples, and update broadphase reach. Dirty discovery at lines 716–749 then
collects incident events and nearby candidate pairs, deduplicates identities
within that event, and schedules them again.

Two things change even for the same candidate identity:

1. The search now begins at a later event time, so ordinary linear/angular drift
   changes its represented start position/orientation.
2. An actual velocity update replaces its future motion segment. Even equal
   instantaneous positions/rates would not prove an identical future float path
   after re-anchoring (`src/RigidMotion.h:28–52`).

The current heap already retains unaffected endpoint events; it is not blindly
rebuilding the entire event schedule. `src/physics/ImpactEventSchedule.h:13–17`
retains no-impact identity but no heap or endpoint adjacency entry. Those misses
reappear through overlapping broadphase neighborhoods after dependent endpoint
changes. Existing identity deduplication removes duplicates within one event;
it cannot certify a new trajectory by itself.

## Existing bounds reject what they can, but do not carry pair proofs

`src/physics/WorldCCDSearch.cpp:5–50` builds remaining-motion shape AABBs from a
conservative sphere enclosure tightened by translation or dynamic box rotation
bounds. An unchanged path can reuse its earlier enclosure for a subinterval.
Actual endpoint trajectory changes invalidate it.

`WorldImpactEvents.cpp:733–738` rejects separated remaining AABBs before primitive
expansion. This explains the zero inner AABB rejections during rescheduling.
Pairs that survive have overlapping *axis-aligned enclosures*, not necessarily
intersecting geometry. Crowded, rotated nearby boxes can overlap those enclosures
while an oblique separating plane or negligible closing motion proves no hit.

The later `ImpactTime` computes geometry/relative rates and returns only a time
or `finish + 1`; it does not return a reusable separating witness, conservative
slack, expiry, or the reason for a no-impact answer. Consequently no-impact is
not a first-class certificate that can be cheaply revalidated under a changed
trajectory.

## It is not a long spin/CCD iteration explosion here

There are 2,588,879 advancement iterations for 2,526,324 requests that reached
start-pose acquisition: **1.024761 iterations per captured request**. There are no
`PairPenetrationExceeds` calls in the window. Existing local `ActualTruth` reuse
already avoids 109,508 of 2,745,658 truth requests (3.9884%).

The reschedule origin makes 2,386,961 touching queries and 2,355,258
positive-margin manifold queries. Of those manifolds, 2,325,418 are empty. The
common path is approximately one actual-contact Boolean followed by one
positive-margin/proximity query at a new represented pose, not thousands of
temporal samples per request.

Source: `WorldCCDSearch.cpp:681–685` keeps actual zero-margin truth separate from
proximity; lines 735–756 then obtain a full margin manifold, select one minimum
separation/normal and calculate an advance. `SpinsThroughSupport` at lines
399–445 and the target path at lines 619–620/676 exist, but are not the measured
threshold-query bottleneck in this window. Preserving this distinction prevents
optimizing a dramatic but unexercised branch.

The no-impact return count includes multiple meanings, including an already
touching pair left to the contact solver (`WorldCCDSearch.cpp:685–695`), empty
proximity, no closing motion and bounded-search misses. It is **not** proof that
99.86% of calls could be discarded. A reason-specific observer would distinguish
these certificates from sampled misses before implementing reuse.

## Most observed manifolds are response-domain discovery

8,874,179 manifolds (**76.961848%**) originate in `Domain`, outside `ImpactTime`.
They produce 2,125,645 nonempty and 6,748,534 empty answers. Their instrumented
native wall time is 2,611.299 ms across the window. Rescheduled CCD manifold and
touching work contributes 1,623.934 + 648.001 ms. Inclusive timing categories must
not be summed with enclosing Impact/solver time.

`src/physics/WorldIncrementalImpact.cpp:31–67` deduplicates candidate identities
within a trial and constructs actual manifolds; lines 69–92 expand a reached
body's nearby candidates, and lines 111–132 grow the physical response domain
when the solver activates more bodies. The exact-input map shows these are
generally new event-time geometry samples too. Faster identical-input lookup
cannot eliminate their underlying work.

The response-domain/solver observer must establish how often and why that growth
is necessary before choosing a representation change. This CCD data alone does
not prove that a large physical response domain can be arbitrarily truncated.

## The next justified architecture boundary

The existing architecture's reusable object is primarily a scheduled time tied
to complete endpoint trajectories. A more useful boundary to investigate is a
**pair contact/separation episode with an explicit validity certificate**:

- Separated with a conservative witness and expiry.
- Actual continuing contact with valid finite features/local anchors.
- A scheduled new impact or an unknown state requiring full search.

For a fixed separating direction `n`, define

`g_n(t) = min_{x in A(t)} dot(n,x) - max_{y in B(t)} dot(n,y)`.

A strictly positive conservative lower bound for **all** times in the remaining
interval certifies separation. Reusing the old witness direction is potentially
cheap, but after a velocity/anchor change the interval bound must be recomputed
or its lost slack bounded. Old Boolean truth is insufficient. If the proof is
uncertain/nonpositive, retain the current full path. Existing
`TryPrimitiveSweepProjection` / `TryRelativeCentreProjection` in
`src/RigidMotionBounds.h` and the proof structure in
`WorldCCDSearch.cpp:448–539` are relevant building blocks, not measured fixes.

This suggests tracking a small pair witness/feature state with endpoint
dependencies instead of forgetting every no-impact reason. It also gives a
well-defined place to connect continuing contacts with incremental response
rows. Finite-feature exit, departure/reimpact, restitution, mesh sidedness and
topology changes must remain explicit episode boundaries. A sampled miss cannot
be relabeled as a separation proof.

To test this direction without another blind cache experiment, first measure:

1. No-impact reasons and retained candidate identity recurrence.
2. How often a prior separating direction can certify the **new** trajectory
   interval with cheaper interval projection than a complete search.
3. How often proof slack survives the actual velocity update, with exact failure
   fallback, and its memory/revalidation cost.
4. How much response-domain discovery is retained contact topology versus new
   finite features, and what physically forces solver propagation.

These are architectural hypotheses with source-supported interfaces. No measured
speedup, valid production cache or complete episode redesign is claimed here.
