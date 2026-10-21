# Late CCD observation: actual steps 901–1800

Input: `../observed1800-late/ccd.csv`, 900 enabled rows. Analysis outputs:
`observed1800-late-summary.json` and `.md`. The child ran CSV analysis only, no
physics workloads or builds.

## Observer validity

- Zero result mismatches, aborted scopes, counter-accounting errors or overflow.
- Maximum combined retained keys: 227,353, below the 262,144 cap. Negligible
  duplicate counts are not the result of capacity loss.
- Direct comparison with `../long1800/steps.csv` over all 900 common actual steps
  finds zero trajectory-digest differences and zero differences in body/awake/
  sleeping/candidate/contact/event/query/iteration/peak/nonfinite counts.
- The source-risk notes in `SOURCE_REVIEW.md` still apply: one world, immutable
  shapes per reset; full input-word equality; manifold outputs compared by two
  explicit-field digests; inclusive wall times do not add independently.

## Comparison with the preceding observation

| Measurement | Steps 300–900 (601 rows) | Steps 901–1800 (900 rows) |
|---|---:|---:|
| Manifold calls | 11,530,621 | 25,593,710 |
| Exact repeated manifold inputs | 113 (0.000980%) | 1,428 (0.005579%) |
| Touching calls | 2,636,150 | 5,848,355 |
| Exact repeated touching inputs | 1,743 (0.066119%) | 3,215 (0.054973%) |
| All repeated geometry native wall time | 1.711 ms total | 2.791 ms total |
| ImpactTime calls | 3,365,067 | 5,794,691 |
| No-impact fraction | 99.857952% | 99.874333% |
| Rescheduled requests | 2,337,091 | 5,680,677 |
| Rescheduled no-impact fraction | 99.910016% | 99.926611% |
| Exact complete repeated ImpactTime input | 0 | 0 |
| Same start geometry as previous pair request | 4 | 428 |
| Penetration-threshold queries | 0 | 0 |
| Advancement iterations per captured request | 1.024761 | 1.012562 |
| Domain manifold share | 76.961848% | 76.352084% |

All 428 same-start-geometry requests in the late window have changed velocity,
interval and motion anchor. They are not equivalent future-trajectory requests.
There are 3,782,464 recurrent captured pair requests, so even this narrow
same-geometry category is only 0.011315% of recurrent pair requests.

Every one of the 5,680,677 rescheduled requests reaches the existing start-pose
acquisition; the separate dirty-discovery swept-bound check already rejected
nonoverlapping enclosures. Initial scheduling is now only 114,014 requests,
including 69,513 early-bound rejections. Work has shifted toward repeated
*re-certification of new incident trajectories*, rather than initial setup.

Domain growth performs 19,541,331 manifolds, of which 5,527,347 are nonempty and
14,013,984 empty. Its native manifold wall time is 6,388.379 ms. Rescheduled CCD
manifold native wall time is 3,822.805 ms. These are instrumented per-kind costs,
not additive totals with enclosing response/Impact scopes.

## Matched normal timings, not observer timings

Over actual steps 901–1800, the existing **normal** `long1800` run reports:

| Metric | Mean | Median | p95 | Worst |
|---|---:|---:|---:|---:|
| Fixed step, ms | 63.624 | 60.547 | 115.929 | 297.795 |
| Physics, ms | 62.380 | 59.218 | 114.742 | 296.578 |
| Solver, ms | 50.553 | 47.337 | 103.048 | 284.738 |

The observed run's fixed-step mean is 95.950 ms, with 525.940 ms worst. The
observer is intrusive and must not be used as the normal performance claim.
Its explicit CCD bookkeeping alone averages 8.662 ms per step; other observer
layers, memory/cache effects and wrapper/code-generation overhead also apply.

Only 3.694 actual impact events occur per step on average, compared with 4.787
in steps 300–900. Yet manifold calls rise from 19,185.7 to 28,437.5 per step and
impact queries from 5,599.1 to 6,438.5. Event count alone does not explain rising
cost; response/discovery and recertification work per event matters.

## Interpretation

The late window reinforces, rather than reverses, the prior result:

1. Literal geometry memoization cannot explain a meaningful missing speedup.
   Even the complete observed repeated-geometry native cost is only 2.791 ms
   across 900 steps, before any lookup/storage cost.
2. High no-impact frequency is not permission to reuse stale Boolean answers.
   Represented poses, chronological intervals and anchored trajectories differ.
3. Inner CCD iteration explosion and spin-threshold searching are not the
   measured mechanism in this window. Most requests perform approximately one
   Boolean/proximity advancement attempt.
4. Most manifold work remains response-domain discovery. Persistent finite
   contact episodes and cheap conservative recertification of retained separating
   witnesses are the relevant architectural boundary to investigate; neither is
   an established speedup until validity, hit rate and cost are measured.

No production correction or workload alteration is made or inferred here.
