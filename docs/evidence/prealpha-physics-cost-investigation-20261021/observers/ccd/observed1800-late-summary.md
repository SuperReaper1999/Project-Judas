# CCD observation summary

Identical inputs are an investigation lead, not evidence that a cache would be profitable.

## observed1800-late

Source: `/home/conner/Documents/GitHub/Project-Judas/.cache/physics-exhaustive-profile-20261021/observed1800-late/ccd.csv`
Observed steps: 900 (901..1800); timing rows: 900.
Consistency: no observed mismatches or accounting errors; overflow calls: 0; observer wall time: 7796.021 ms.

| Origin | Query | Calls | Repeats | Repeat fraction* | Cross-anchor | Output mismatch | Native wall ms† | Repeated wall ms† |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| all | manifold | 25593710 | 1428 | 0.01% | 1418 | 0 | 10631.687 | 0.539 |
| all | touches | 5848355 | 3215 | 0.05% | 1196 | 0 | 1613.341 | 2.252 |
| all | penetration | 0 | 0 | n/a | 0 | 0 | 0.000 | 0.000 |
| unknown | manifold | 16 | 10 | 62.50% | 0 | 0 | 0.056 | 0.026 |
| unknown | touches | 0 | 0 | n/a | 0 | 0 | 0.000 | 0.000 |
| unknown | penetration | 0 | 0 | n/a | 0 | 0 | 0.000 | 0.000 |
| initial | manifold | 63423 | 0 | 0.00% | 0 | 0 | 67.637 | 0.000 |
| initial | touches | 103002 | 851 | 0.83% | 0 | 0 | 69.883 | 0.642 |
| initial | penetration | 0 | 0 | n/a | 0 | 0 | 0.000 | 0.000 |
| reschedule | manifold | 5701917 | 0 | 0.00% | 0 | 0 | 3822.805 | 0.000 |
| reschedule | touches | 5745353 | 2364 | 0.04% | 1196 | 0 | 1543.459 | 1.610 |
| reschedule | penetration | 0 | 0 | n/a | 0 | 0 | 0.000 | 0.000 |
| domain | manifold | 19541331 | 1398 | 0.01% | 1398 | 0 | 6388.379 | 0.485 |
| domain | touches | 0 | 0 | n/a | 0 | 0 | 0.000 | 0.000 |
| domain | penetration | 0 | 0 | n/a | 0 | 0 | 0.000 | 0.000 |
| support | manifold | 287023 | 20 | 0.01% | 20 | 0 | 352.809 | 0.028 |
| support | touches | 0 | 0 | n/a | 0 | 0 | 0.000 | 0.000 |
| support | penetration | 0 | 0 | n/a | 0 | 0 | 0.000 | 0.000 |
| other | manifold | 0 | 0 | n/a | 0 | 0 | 0.000 | 0.000 |
| other | touches | 0 | 0 | n/a | 0 | 0 | 0.000 | 0.000 |
| other | penetration | 0 | 0 | n/a | 0 | 0 | 0.000 | 0.000 |

* Fractions are lower bounds when records overflow. Per-origin history includes prior calls from other origins.
† Instrumented wall time. Inclusive scopes overlap and must not be summed.

Impact requests: 5794691; captured: 5725178; no impact: 99.87%; same complete input as previous pair request: 0; returned-time bit mismatches: 0.
Same geometry with changed velocity: 428; changed interval: 428; changed motion anchor: 428.
Local truth cache: 183675 hits / 5848355 misses (3.04% hits).

## Interpretation limits

- Duplicate means an identical directed geometry input was observed earlier in the same step; it does not establish a profitable cache.
- Shape contents must be immutable and exactly one world must be observed between Reset calls; world tokens and shape contents are not part of keys.
- Input map hash collisions cannot produce matches because all raw key words are compared. Manifold output equality uses two explicit-field 64-bit digests, not retained byte-for-byte outputs.
- Per-origin repeats are against the shared per-step history, including other origins; per-origin unique means first observed globally in that origin.
- Anchor classification compares with first observation, not the previous call. Different events can share the same anchor time.
- Overflow/untracked calls make duplicate counts lower bounds; missing keys are never assumed unique.
- Impact same-input compares only the previous completed captured request for a pair; it is not an exhaustive all-request duplicate search.
- Timing is instrumented wall time, not CPU cycles. Nested penetration/manifold and Impact scopes are inclusive and must not be summed as independent work.
- New native time includes untracked calls as well as newly observed keys. Repeat time is an observed-work category, not a speedup estimate.
- No benchmark, engine build, physics test or cache experiment is performed by this analysis script.
