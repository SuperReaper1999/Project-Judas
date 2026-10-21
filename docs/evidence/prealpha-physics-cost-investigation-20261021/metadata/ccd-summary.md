# CCD observation summary

Identical inputs are an investigation lead, not evidence that a cache would be profitable.

## observed900

Source: `/home/conner/Documents/GitHub/Project-Judas/.cache/physics-exhaustive-profile-20261021/observed900/ccd.csv`
Observed steps: 601 (300..900); timing rows: 601.
Consistency: no observed mismatches or accounting errors; overflow calls: 0; observer wall time: 3195.613 ms.

| Origin | Query | Calls | Repeats | Repeat fraction* | Cross-anchor | Output mismatch | Native wall ms† | Repeated wall ms† |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| all | manifold | 11530621 | 113 | 0.00% | 2 | 0 | 4529.092 | 0.237 |
| all | touches | 2636150 | 1743 | 0.07% | 377 | 0 | 769.399 | 1.473 |
| all | penetration | 0 | 0 | n/a | 0 | 0 | 0.000 | 0.000 |
| unknown | manifold | 218 | 97 | 44.50% | 0 | 0 | 0.496 | 0.216 |
| unknown | touches | 0 | 0 | n/a | 0 | 0 | 0.000 | 0.000 |
| unknown | penetration | 0 | 0 | n/a | 0 | 0 | 0.000 | 0.000 |
| initial | manifold | 208544 | 14 | 0.01% | 0 | 0 | 167.824 | 0.019 |
| initial | touches | 249189 | 771 | 0.31% | 0 | 0 | 121.398 | 0.651 |
| initial | penetration | 0 | 0 | n/a | 0 | 0 | 0.000 | 0.000 |
| reschedule | manifold | 2355258 | 0 | 0.00% | 0 | 0 | 1623.934 | 0.000 |
| reschedule | touches | 2386961 | 972 | 0.04% | 377 | 0 | 648.001 | 0.822 |
| reschedule | penetration | 0 | 0 | n/a | 0 | 0 | 0.000 | 0.000 |
| domain | manifold | 8874179 | 2 | 0.00% | 2 | 0 | 2611.299 | 0.002 |
| domain | touches | 0 | 0 | n/a | 0 | 0 | 0.000 | 0.000 |
| domain | penetration | 0 | 0 | n/a | 0 | 0 | 0.000 | 0.000 |
| support | manifold | 92422 | 0 | 0.00% | 0 | 0 | 125.540 | 0.000 |
| support | touches | 0 | 0 | n/a | 0 | 0 | 0.000 | 0.000 |
| support | penetration | 0 | 0 | n/a | 0 | 0 | 0.000 | 0.000 |
| other | manifold | 0 | 0 | n/a | 0 | 0 | 0.000 | 0.000 |
| other | touches | 0 | 0 | n/a | 0 | 0 | 0.000 | 0.000 |
| other | penetration | 0 | 0 | n/a | 0 | 0 | 0.000 | 0.000 |

* Fractions are lower bounds when records overflow. Per-origin history includes prior calls from other origins.
† Instrumented wall time. Inclusive scopes overlap and must not be summed.

Impact requests: 3365067; captured: 2526324; no impact: 99.86%; same complete input as previous pair request: 0; returned-time bit mismatches: 0.
Same geometry with changed velocity: 4; changed interval: 4; changed motion anchor: 4.
Local truth cache: 109508 hits / 2636150 misses (3.99% hits).

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
