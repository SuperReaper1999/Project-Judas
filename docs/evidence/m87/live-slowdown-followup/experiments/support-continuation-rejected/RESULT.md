# Rejected: event-time persistent-support projection

## Decision

**Reject; not integrated.** The proposed continuation mechanism changes physical trajectories and adds an extra normal solve. It did not improve the original dense fixture. No further tuning or acceptance testing is justified by this result.

The complete source, base source, patch, preparation/build scripts, build log and provenance are preserved here. Raw fixed-step/frame CSVs and diagnostic logs preserve both exact same-binary controls. The executable/object remain outside the repository; the binary hash is in `SUMMARY.json`. No binaries, objects or generated build directories are included.

## Identical input and settings

Both runs used the original current 1024-loose-body scene, 1285 total bodies, 1152 dynamic bodies, original materials/shapes, fixed input cadence argument `0.016666667`, 155 completed fixed steps, no tour, hidden rendering, no swap wait and no profiler recording. Both used trace logging and exited zero. See `commands.sh` for exact original invocations. Only the private experiment enable flag differed.

| Measured fixed-step result | Original control | Continuation projection |
|---|---:|---:|
| Sum over 155 steps (ms) | 9040.047365 | 9372.892863 |
| Median (ms) | 42.038408 | 26.290832 |
| p95 linear percentile (ms) | 115.904311 | 86.413570 |
| Maximum (ms) | 583.790625 | 2326.404732 |
| Worst fixed step | 153 | 153 |
| Impact events | 3780 | 4435 |
| Impact queries | 726869 | 1188480 |
| Nonfinite physical fields | 0 | 0 |

These are diagnostic rejection timings, including identical trace logging, and must not be substituted for the final production before/after measurements.

## Physical divergence and local diagnostics

The first physical digest divergence occurs at step **142**. 141 of 155 step digests match. Full first-divergence input/result records are preserved in `SUMMARY.json`; changed trajectories were expected because this is a policy change.

Across the projection run:

- Attempts: 3802; accepted projections: 1368.
- Extra solved rows: 120567; positive-gap rows: 10399.
- Additional component-member inclusions: 27965 (repeated inclusions across events, not unique bodies).
- Energy-guard rollbacks: 0; unsupported-case skips: 0.
- Largest measured positive kinetic-energy rise: 5.94457588932e-07 J (within the existing relative engineering guard for that component).
- Largest momentum residual after fixed-boundary reactions: 1.86598111256e-06 kg m/s.
- Largest angular-momentum residual after fixed-boundary reactions: 1.07269612981e-05 kg m^2/s.
- Largest remaining normal-velocity constraint violation: 0.038671515882 m/s.

These are narrow local diagnostics, **not** physical-quality acceptance. Finite fields or a passing energy guard do not establish correct complete mechanical behaviour, penetration bounds, game behaviour or persistence. The nonzero normal residual is retained explicitly. Ordinary CCD remained active as the nonlinear/finite-iteration safeguard.

## What was changed experimentally

The true zero-margin `ImpactSolver` remains unchanged. A separate, zero-warm, normal-only support projection refreshes previously accepted support anchors at event time, with target `-max(gap,0)/(dt-now)`. It includes existing hard-joint impact rows while excluding additional motor/spring/constitutive efforts. Uncertain geometry, incompatible anchors, newly reached sleeping/deferred-acceleration bodies and moving infinite-mass boundaries retain the original path. The complete original CCD invalidation/rescheduling, support exemption policy and event-cap behaviour remain unchanged.

This adds **10 extra iterations per accepted projection**, and applies predictive normal response to existing positive-gap supports, just as the existing persistent-support concept permits. It is not an exact optimization and is not a universal no-impulse-across-gap model. Positive-gap rows do not publish collision callbacks. No physical cadence, contact fidelity, ordinary CCD search, fixture threshold or damping value was reduced.

Compilation/linking succeeded with one cosmetic prototype-only misleading-indentation warning preserved in `build.log`. Since the measured mechanism was rejected, it was not polished, retuned or merged. The validated production cost reductions and remaining dense-pile limitation are separate from this experiment.
