# Pass 5: derived impact-candidate reuse

## Result and limit

A bounded, frame-local cache removes repeated **sorted broadphase candidate discovery**
without reusing a contact, TOI, collision response or sleeping decision. The live
1,024-piece fixture makes 981,074 candidate requests; the cache performs 143,207
actual tree queries, an 85.4% reduction. An independent original-tree query checks
**every modified call**, including its candidate order: zero differences.

Three predeclared before/after observations preserve every one of the 270 physical
step records and the endpoint geometry. Their p95 costs are 8.3–10.6% lower and
whole-run wall time is approximately 2–4% lower. Median changes are small and mixed.
**The severe tail is not solved:** one candidate run has a 1,957.994 ms maximum,
compared with a 1,838.908 ms maximum in its paired baseline. No improved worst-case
bound, 60 Hz capability for this pile, or memory saving is claimed.

This is one independently testable cost reduction, not certification of all Pass 5
work or human acceptance. Parent-coordinated final production regression receipts
are separate from these frozen prototype observations.

## Provenance and workload

- Accepted source: `4c141e9bb6c68a50c63dec263dcec398200328ad`.
- Frozen engine archive SHA-256:
  `6e77a9f799ade7f6ce3ad741687689b8cae6d5efa6fd82e472923749ee21b441`.
- Clean baseline executable SHA-256:
  `2d6b5a4dcdb54a1ac40eb4fdb026f9875bfd33d95ce8d9946000c0eda902a433`.
- Measured candidate executable SHA-256:
  `c5c2312cf36522e82f28b7121ac91e7d42aa4cacc90a7215e9b74faffed16aca`.
- Scene: `projects/physics_stress_lab/Scenes/room1024.judas`, SHA-256
  `a2caf62a295243d301149202c8a0a5eedafff256d68de0e3cda988846b2e05bf`.
- 1,024 authored loose pieces; the unchanged scene contains 1,285 total bodies.
- Release, GNU C++ 15.2, `-O3 -DNDEBUG`, Linux x86_64, Intel i7-4790.
- 270 fixed steps, `dt=0.016666667`, ordinary scene/scripts, profiling enabled,
  hidden desktop window, no tour or periodic changes. Full actual arguments and
  timestamps are in every `runs/*/RESULTS.json`.
- Root serialized CPU-heavy jobs and paused the Windows VM for the unobserved
  runs. Observers are excluded from timing comparisons. These are sequential
  observations on one host, not randomized trials or universal bounds.

`baseline/FROZEN.json` and its build-info file describe the clean baseline actually
used. `baseline/PRECONFIGURE_INPUTS.json` separately preserves 803 source/build/test
inputs and the **older inherited build identity before clean-4c reconfiguration**.
That older identity is not relabelled as the measured clean baseline. Isolated
candidate builds replace one object in a copy of the frozen engine archive;
their embedded identity remains baseline while `builds/*/BUILD.json` records the
actual changed translation unit, helper, archive and executable hashes. No
executables, archives or object files are committed as evidence.

## Profile-first investigation and rejected approach

In baseline step 270 the profiler records 51 impact events, 501,599 TOI queries,
46,950 search iterations, a peak 973-body impact island and 2,517 impact rows.
The geometry counter is 626,049 calls / 837.683 ms inclusive; island discovery is
591.100 ms inclusive and impact solve 160.255 ms inclusive. Nested categories
**overlap** and must not be added together. The raw final profile is preserved.

The first proposed exact manifold memo was instrumented without skipping calls.
Across 270 steps it found only 19,397 equal-pose repeats among 5,672,004 primitive
geometry calls (0.34%). The worst step has only 2,637 repeats among 520,167 calls.
No repeated manifold fingerprint differs, but this hit rate cannot plausibly
remove the dominant work. This approach was rejected before production changes;
its observer source, raw results, build commands and run are retained.

The same observer shows 461,647 changed trajectory endpoints but only **926 actual
fat-proxy changes**. In the worst step 38,936 endpoints change while two fat boxes
change. Rebuilding sorted tree candidate lists for every endpoint is therefore
unnecessary derived work even when exact contact geometry cannot be reused.

## Generic correction and dependency proof

`src/physics/ImpactCandidateCache.h` is private physics machinery. The two call
sites are in `src/physics/WorldImpactEvents.cpp`. No public API, physics budget,
CCD policy, event order, collision mask or solver setting changes.

Let `F_a` be the querying body's current fat tree box, `Q` the requested box and
`F_b` a current other leaf box. The original result is the ascending sequence
`S(Q) = { b | F_b overlaps Q }`. If `F_a.Contains(Q)`, then filtering `S(F_a)` by
that **same inclusive overlap predicate** gives exactly `S(Q)`, in the same order.
If containment is not established, the original query runs.

The cache retains `S(F_a)` by slot and the global existing reinsertion revision.
An unchanged own box alone is insufficient: **any other proxy moving invalidates
all retained lists**. `CoverStepReach` increments that revision whenever it moves
a proxy, including moves within an event loop. The cache also checks the stored
own box. Each subset query filters against current other fat boxes.

Within this guarded owner-thread pass, body creation/destruction, authored filter
changes, origin changes and gameplay callbacks do not run. Coupling callbacks
finish before this pass. `Wake` changes sleep/island state without changing the
tree. Nested geometry and solve operations do not reenter the cache or QuerySlots;
returned storage is consumed before the next outer query. The cache lives only
through one `AdvanceImpacts` call and is discarded on return or exception.
Future changes to these ownership/callback boundaries must re-establish the proof.

`CanRespond`, child expansion, complete primitive identities, actual geometry,
TOI and solver calls continue at their original points. No response truth is
cached. Hash-map iteration never determines physics order: only the copied,
originally sorted vectors are iterated.

Admission is bounded by 4,096 slots and 262,144 **retained ID capacity** entries
(1 MiB of ID payload). Map metadata and filtered scratch are additional bounded
memory; this is not a claim of a 1 MiB total allocation cap. Oversized supersets
fall back to the exact original requested query. Once a budget is saturated,
subsequent misses avoid a second preliminary superset query until a revision
change. Allocation failure retains ordinary exception handling, not truncation.
Observed peak retention is 24,628 IDs and 980 cached slots.

## Detecting controls

`tests/ImpactCandidateCacheTests.cpp` has 17 checks using the actual dynamic AABB
tree: inclusive tangency, exact ordering, unchanged-tree reuse, a strict fat-only
candidate excluded from a tight query, oversized-superset fallback, budget
saturation, another proxy entering/leaving, own movement, enclosure escape,
retired slot reuse, zero budgets, sparse identities and fresh response filtering.

Two controls deliberately remove necessary mechanisms:

- `--omit-revision`: **7 checks / 1 expected failure**, exit 1, because another
  proxy enters the cached box while the querying body's own box is unchanged.
- Test-only filtering mutation: **17 checks / 4 expected failures**, exit 1.

An independent source reviewer identified that the first 14-check draft lacked a
strict fat-only exclusion case and an oversized-superset/subset fallback case.
Both were added before acceptance, with the filter-disabled negative above. That
review is preserved as a source review; it is not counted as executed tests.

The full live observer then checks all **981,074** modified calls against a fresh,
original `QuerySlots(Q)` and compares exact ordered vectors: **zero mismatches**.
At the worst baseline step, 88,458 requests require 1,943 actual cache tree queries
(97.8% fewer). Copies separate observer oracle scratch from borrowed references.
Observer overhead is not used in performance evidence.

## Matched measurements

All six unobserved runs are retained; no run was discarded or repeated to obtain
a favourable maximum. A1/B1 is the preliminary pair; A2/B2/A3/B3 was explicitly
declared before execution to assess uncertainty.

| Run | Median ms | p95 ms | Maximum ms | Wall s | Peak RSS KiB |
|---|---:|---:|---:|---:|---:|
| A1 baseline | 25.646 | 361.145 | 1838.943 | 52.494 | 413100 |
| B1 candidate | 25.371 | 322.786 | 1722.175 | 50.485 | 413140 |
| A2 baseline | 25.367 | 363.919 | 1838.908 | 51.748 | 414224 |
| B2 candidate | 25.581 | 326.411 | **1957.994** | 50.641 | 385448 |
| A3 baseline | 25.873 | 364.067 | 1833.120 | 51.759 | 386192 |
| B3 candidate | 24.904 | 333.756 | 1704.814 | 50.797 | 385796 |

p95 is the existing runner's sorted sample at `int(.95 * (n-1))`. RSS varies with
runtime/driver allocation and does not support a memory reduction claim.

Every unobserved run, and both diagnostic observer runs, matches all 270 records
for body/sleep counts, candidates, contact counts, impact events/queries/iterations,
peak counters, complete trajectory digest and maximum linear/angular speeds.
All have zero nonfinite fields and final digest `192f0f4bed728953`. Endpoint
observation timing is the only differing field in `quality.json`: geometry remains
9,327 candidate pairs, 2,475 contact points and 0.007217109 m deepest penetration,
with zero uncertain/unsupported pairs. Actual geometry and TOI counts do not drop.
The unchanged baseline's four additional free/pile/active/sleeping controls are
preserved; final production control/regression receipts are recorded separately.

## Reproduction and inventory

- `reproduction/verify_delivery.py`: read-only manifest, lossless compression, all
  physical runs and optional exact production-source verification (`--root TREE`).
- `reproduction/run_workloads.py`: unchanged as-run workload driver, copied from
  Pass 4 without editing the historical copy. Requires explicit binaries/output.
- `reproduction/build_*.py`: exact isolated one-object build/relink commands as run;
  these refer to the owned `.cache/prealpha-pass5` layout and frozen archive.
- `reproduction/WorldImpactEvents.*.cpp`: exact observer and measured sources.
- `reproduction/ImpactCandidateCache*`: measured helper, checks and negative control.
- `observations/ANALYSIS.json`: all paired results and exact physical comparison.
- `observations/ADOPTION_PROOF.json`: namespace/include/comment/format-only adoption
  from measured prototype to the readable production helper and test.
- `independent-review/`: dependency, lifetime and budget review with exact hashes.
- `runs/`: raw steps, logs, resource measurements, profiles and endpoint observations.
- `COMPRESSED_INPUTS.json`: lossless gzip payloads and original uncompressed hashes.
- `MANIFEST.json`: exact committed evidence inventory; excludes itself to avoid
  self-reference. `SOURCE_FINGERPRINTS.json` identifies the adopted source snapshot.

For a fresh comparison, use separate accepted-4c and candidate source/build trees,
configure Release with `JUDAS_ENABLE_VALIDATION=ON`, build
`judas_physics_stress_live_benchmark` and `judas_physics_scaling_diagnostics`, then
run the copied driver with `--root TREE --live BINARY --scaling BINARY --output NEW`.
The actual command history contains the exact flags and limits; build and run one
heavy workload at a time. To run the tight helper control after configuring the
candidate, build/run `judas_impact_candidate_cache_tests`; its missing-revision
negative intentionally exits 1. Rebuilding on a different compiler need not
produce the same executable bytes or cross-compiler trajectory fingerprint.

## Remaining work

The unchanged number of CCD geometry queries and event reschedules remains the
dominant dense-contact limit. This patch does not make the 1,024-piece pile real
time. Broader derived adjacency or trajectory-dependency work needs a separate
proof and meaningful measured benefit; geometry truth must not be reused merely
because a fat proxy stayed unchanged. No budgets, fidelity, cadence, fixture or
acceptance limits were loosened. No Windows performance claim is made.
