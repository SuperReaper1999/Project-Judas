# Support-certificate subdivision completeness

## Demonstrated mechanism

The original `SupportContinuationSafe` constructs a conservative lower-clearance proxy over a time interval `T`:

`L_T(t) = relative_center_projection(t) - max_extent_A(T) - max_extent_B(T) + existing_allowance * |normal|`.

Its rotational extents are independently bounded over the entire interval. Consequently `L_T(t)` is a lower bound on actual clearance, not actual clearance itself. The interval returned by `gap(T)` encloses this lower-bound proxy. Its upper endpoint is **not** an upper bound on actual time-varying clearance.

The old recursion nevertheless stopped when `bound.hi <= 0`. Tightening the time interval can reduce the worst-case rotational extents and increase the lower-clearance proxy, even when the original interval's entire proxy is nonpositive. This pruning could prevent all useful subdivision.

The minimal correction removes that internal pruning condition. It preserves:

- the original native Float trajectory and contact predicates;
- the existing 5 mm penetration allowance and shape-size restriction;
- the existing maximum subdivision depth of three (at most fifteen nodes);
- the endpoint precheck, segment boundaries and all unsupported-path fallbacks;
- solver iterations, cadence, restitution and friction.

No physical threshold is increased and no contact geometry is replaced.

## Exact captured replay

Seventy-eight preserved stages for pair 595/851 were reconstructed from their exact recorded bits: fifty scheduled stages, fourteen before-impact stages and fourteen post-trajectory replacements.

The original actual engine method rejected all seventy-eight. Its branch observer matched every result. Every rejection visited exactly one root subdivision node: the nonpositive-upper-proxy condition prevented subdivision. Endpoint lower-clearance margins were nevertheless positive, between 2.275 and 3.997 mm.

The corrected actual method certified all seventy-eight with three to nine nodes, within the unchanged depth-three budget. The diagnostic arithmetic again matched all actual decisions.

A separate 257-sample native manifold inspection measured at most 0.547 mm penetration in these captured paths. This sampling is diagnostic only; the corrected accepted interval leaves provide the continuous certificate.

## Independent detecting controls

The permanent candidate fixture calls the actual internal method without copying its numerical implementation. It includes stationary and co-moving boxes rotating about their separating normal. Their actual projected extent is invariant while whole-interval quaternion enclosures lose correlation. The original bad-prune implementation misses both certificates; the corrected method closes both in seven nodes.

Two physical negative controls remain rejected:

1. Closing boxes producing 16.67 mm endpoint penetration.
2. A rotating thin box producing 9.90 mm interior penetration while its endpoint is completely clear.

The second case prevents endpoint-only acceptance from masquerading as a continuous certificate. The corrected fixture passes eleven checks; the same source linked to the original engine genuinely exits nonzero with four detecting failures. No expected-failure argument is present in the proposed permanent test.

## Artifacts

- `RESULTS.json`: original seventy-eight-stage actual/diagnostic branch replay.
- `SUMMARY.json`: original failure classification and source hashes.
- `corrected/RESULTS.json`: corrected actual/diagnostic replay.
- `CORRECTED_BUILD_RESULTS.json`: exact source/object commands and control outputs.
- `corrected/LIVE_BUILD.json`: full ordered archive reconstruction with one changed member.
- `permanent/SupportContinuationTests.cpp`: portable-header numerical regression candidate.
- `permanent/BUILD_RESULTS.json`: same-source original-failure/corrected-pass evidence.
- `original-detecting-failure.log`: original implementation's genuine nonzero regression result.

The original first wrapper compilation failure and the rejected coherent-double architecture remain preserved separately. Native stress measurements and final production adoption are owned by the parent integration task; component proof success alone does not establish complete game cadence or release acceptance.
