# Remaining dense-step cost: bounded read-only follow-up

This note uses the already captured profiles and source. No new benchmark, timer,
production correction or performance gate is introduced.

## Measured residual

Both confirmation candidates retain exactly the baseline's step-270 physical work:
501,599 TOI queries, 46,950 search iterations, 51 impact events, 130,189 pose
evaluations and 78,508 prepared parent orientations. The corresponding aggregate
geometry counter is 626,049 calls. Candidate B3 reports 827.278 ms geometry,
500.620 ms island discovery and 164.648 ms impact solve; B2 reports 934.266,
571.097 and 188.687 ms respectively. These are **inclusive overlapping counters**,
not independent costs to sum. B2's 1,957.994 ms maximum is preserved.

The original observation, which did not skip geometry, separately counts 520,167
rich zero-margin primitive `PairAt` calls from discovery and response in this step:
130,219 hits and 389,948 misses. Thus the repeated per-event geometry remains a
large residual even after tree candidate retrieval is cheaper. Its exact-pose
repeat rate is only 0.51% in this step, ruling out a broad simple exact-manifold
memo as a promising reduction.

The existing actual-AABB-rejection counter is 344,326, but it covers overlapping
query paths. It cannot safely be subtracted from either aggregate call total to
claim a count or cost for unique full SAT/manifold construction. The profiles do
not yet establish which individual primitive geometry operation dominates.

## Actual implementation constraints

- `WorldImpactEvents.cpp` grows the event island through current physical contact,
  rechecks current geometry, and retains a canonical discovered manifold for
  response when available. Current contact connectivity is not proved by unchanged
  broadphase proxy boxes.
- `WorldImpactQueries.h::PairAt` includes current tight-bound rejection, prepared
  orientations, primitive extraction and full manifold generation. A Boolean
  replacement must preserve uncertainty and response ordering, not only hit counts.
- `PairTouches` already uses the public internal primitive/cooked Boolean path
  where appropriate. Blindly substituting it for discovery loses the explicit
  reliable-miss/uncertainty distinction and can force a second manifold call on
  positive response pairs.
- `WorldCCDSearch.cpp::RemainingImpactBound` already caches an enclosing bound per
  body and anchored trajectory, with later-subinterval reuse. Real velocity changes
  invalidate that enclosure. Caching it merely by unchanged fat box would be wrong.
- `ImpactPoseAt` already separates immutable event-anchor and bisection samples and
  retains exact prepared orientations. Cross-event poses are usually different;
  the rejected repeat observer demonstrates this in the actual workload.

## Appropriate next bounded investigation

Before another production optimization, an isolated `WorldImpactEvents` observer
could discriminate discovery versus response `PairAt` calls, narrow-bound rejection
via the existing counter delta, successful canonical versus reversed discovery,
and per-category elapsed time. It must retain every original call and compare all
270 physical records. That would test whether fresh manifold construction or the
repeated contact-graph predicate walk is the practical next target. Observer costs
must remain separate from accepted before/after measurements.

This is a proposed diagnostic boundary, not an established fix or a claim that
unchanged proxy connectivity is sufficient to retain contact truth. No concurrent
mutation, CCD relaxation, contact cap, cadence reduction or body-count reduction
is proposed. The extreme dense pile remains an explicit open limit.
