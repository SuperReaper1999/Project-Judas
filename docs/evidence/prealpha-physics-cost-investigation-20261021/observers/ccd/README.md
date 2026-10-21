# Private CCD observer

Diagnostic copies only. No production implementation, `PhysicsWorld::Impl`
layout, collision return value, query ordering, or acceptance setting is changed.
No query is cached, omitted, approximated, or replaced. The actual existing
`ActualTruth` three-entry cache remains unchanged; its hits/misses are counted.

## Files and include resolution

- `CCDAudit.h`: header-only, process-wide observer; disabled by default.
- `WorldImpactQueries.h`: observational wrappers around the actual `PairAt`,
  `PairTouches`, and `PairPenetrationExceeds` implementations.
- `WorldCCDSearch.cpp`: complete original search with an observation scope around
  `ImpactTime`, request metadata capture, and local truth-cache hit/miss counts.
- `WorldInternal.h`, `WorldHotAccessors.h`: byte-exact current production copies.
  They contain no added fields or altered declarations.
- `SOURCE_MANIFEST.json`: original/private hashes and byte-copy checks.
- `SOURCE_REVIEW.md`: key completeness, collision risks and interpretation review.
- `analyze_ccd.py`: summarize future `ccd.csv` files by kind/origin, verify counter
  accounting, report duplicate/result/timing data without inferring cache profit.

Quoted includes resolve in the including file's directory first. Merge these
files into the same private `buildsrc` directory as all copied World translation
units and headers, rebuild **all** World TUs and their selected solver TUs
against it, and put original `src/physics` and `src` later in the include search
path. In particular `WorldInternal.h` includes the sibling private
`WorldImpactQueries.h`, which includes the sibling `CCDAudit.h`.

Do not mix original and observed definitions of these inline member functions
in one executable. A `-I` override alone does not redirect an original source's
quoted `WorldInternal.h`. Do not force-include this copied header before the
original header: their separate paths defeat the desired `#pragma once` identity.

## Driver API and bounded sampling

The driver alone enables observation. Environment variables never enable it.

```cpp
#include "CCDAudit.h"

ccd_audit::Header(csv);
for (std::uint64_t step = 0; step != steps; ++step) {
    // Read the desired inclusive start/end from the driver's environment.
    // Planned sample window: 300..900. No extra physics calls are needed.
    ccd_audit::Reset();
    ccd_audit::SetEnabled(step >= sampleStart && step <= sampleEnd);
    world.Step(dt);
    if (ccd_audit::Enabled())
        ccd_audit::WriteStep(csv, step);
}
ccd_audit::SetEnabled(false);
```

`Reset()` clears every per-step record/counter and map entry, retaining bounded
bucket capacity. It preserves configuration/enabled state. Call it for every
step. Observe one authoritative world on its owner thread; the maps are not
concurrent instrumentation. `OriginScope` is thread-local and nestable:

```cpp
ccd_audit::OriginScope origin(ccd_audit::Origin::Reschedule);
// Execute the existing, unchanged native scheduling operation.
```

Available origins: `Unknown`, `InitialSchedule`, `Reschedule`, `Domain`,
`Support`, `Other`. Origin attribution is copied into each query/request scope;
outer scopes resume after a nested scope is destroyed. All total counters also
have origin-specific columns.

Optional settings, read the first time the driver calls `SetEnabled(true)`:

- `JUDAS_CCD_AUDIT_MAX_KEYS`: combined geometry-entry/request-pair limit,
  default 262,144; clamped to 1..2,000,000. Unique input keys beyond this cap
  increment `overflow`/`untracked`. Existing keys remain observable. No physics
  query is affected. Memory is proportional to the configured cap; this is an
  instrumentation cap, not a physics limit.
- `JUDAS_CCD_AUDIT_TIMING=1`: per-kind wall-clock timings and observer overhead.
  Disabled by default; use count-only observation separately from timing runs.

## Meaning of the measurements

### Pair geometry

Each exact key contains canonical body slots, generations and primitive indices;
the original directed call-order bit; both absolute represented poses (14 raw
binary32 values); the raw binary32 margin; the raw binary64 penetration target;
and query kind. Shape data must remain immutable within the observed step.

`Manifold`, `Touches`, and `PenetrationExceeds` keys are separate. A Boolean
answer is never equated with a full positive-margin response manifold. The
directed call-order bit prevents reversed witness ordering/normal signs from
being counted as identical directed inputs. Signed zeros and quaternion signs
are retained exactly.

`repeat`, `repeat_true`, `repeat_false`, and `repeat_points` describe native
execution repeated at a previously observed identical input during this step.
`same_anchor`/`cross_anchor` compare with the **first observation's** raw
`impactQueryAnchorTime`. They distinguish event-time anchors, not a unique event
serial: simultaneous events can share an anchor, and a repeated query at an
already-seen later anchor remains `cross_anchor` relative to its first call.

All repeated results are checked. Boolean hashes encode the exact Boolean.
Manifold hashes enumerate count, uncertainty and every valid `Contact` field:
hit, world point/normal/penetration, precise normal/separation, local anchors and
witnesses, anchor-frame flags, and separation state. No padding or unused slots
are hashed. The map stores two stable 64-bit digests, not manifold copies.
`mismatch` must remain zero before describing identical-input repetition as an
observational reuse opportunity. Hash equality is a bounded diagnostic, not a
formal proof or authorization to introduce a production cache.

### Impact requests

`ImpactScope` observes every invocation, including recursive interval splits.
It captures request metadata only where the existing code has already evaluated
the two start poses. Early whole-interval bound rejections are counted separately
under `early_bounds`; recursive wrappers can be `uncaptured`. This avoids any
observer-only motion evaluation/cache mutation. Request counts therefore need
not equal the engine's post-split `stats.impactQueries`.

The last completed captured request for each directed primitive pair is retained.
`same_input` compares exact start/finish/target, geometry, rates, and both active
motion-segment descriptions. Segment descriptions include begin/end, anchored
position/orientation, linear/drift rates, prescribed state and inverse mass.
Equal current poses and rates alone cannot certify the same future represented
path when the integrator has been re-anchored. `same_input_mismatch` compares raw
returned impact-time bits for fully matching observed inputs.

`changed_velocity_same_geometry`, `changed_interval_same_geometry`, and
`changed_motion_anchor_same_geometry` compare with that pair's previous completed
request. They are diagnostic categories, **not** claims that a previous no-impact
certificate survives altered velocity/interval/anchor. `same_input_cross_anchor`
is the stricter same-request observation across a different event-time anchor.

`local_truth_hit/miss` counts existing `ActualTruth` hits/misses, split by true
and false results, including native mesh-budget predicates. Mesh-budget truth
is not inferred to be interchangeable with the other Boolean query kinds.

### Timing and overhead

All `*_native_ns` columns are steady-clock wall time. Query timing begins after
its existing start poses were acquired and the observer key was prepared; it
ends before output hashing/map bookkeeping. Observer bookkeeping is measured
under `observer_ns` and subtracted from enclosing query/Impact scopes when nested.
`clock_calls` and a 32-pair minimum `clock_pair_floor_ns` expose clock overhead.
The calibration occurs only on the first explicit enable and outside Step.
Clock cost and wrapper/code-generation effects cannot be removed perfectly.

Penetration predicates can call the instrumented manifold path; request timings
contain their geometry work and nested requests. **Do not sum inclusive query
kinds or request time as independent CPU work.** Repeat timing identifies work
worth investigating; it is not an uninstrumented speedup estimate. Use a separate
unobserved binary/run for authoritative frame timings and physical digests.

## Verification status

The child agent created these diagnostic files and reviewed the text/diffs only.
It did not compile, run tests, execute benchmarks, alter production source,
commit, or push. Parent owns compilation, physical-digest comparison, sampling
window selection and performance interpretation.
