# PHY-002 continuation: measured remaining CCD work

Status: **isolated optimization passed the existing Linux timing gate**. These are diagnostic isolation results, not acceptance of the final combined candidate or native Windows performance. The original M77 corrective-step failure is a separate unresolved problem at this point.

## Fixed workload and measured cause

The observer retains the original imported 13-body articulation, four independent rigs, the actual cooked 8,192-triangle floor, original dimensions/materials/constraints, 60 Hz cadence and 900 steps. No bodies are forced asleep. Box-floor controls are unchanged.

The original follow-up four-rig trace has 78/900 steps above 16.67 ms. Its slowest 45 steps average 23.438 ms, including 22.209 ms in the contact/joint solver stage. Wall time correlates with CCD query count (0.960) and advancement iterations (0.969). This identifies the measured path, rather than a new rendering or broadphase suspicion.

Temporary instrumentation divides that path. In the retained expensive step (profiler frame 2938), continuous impacts consume 32.796 ms of a 34.207 ms fixed step:

| Continuous-impact phase | Inclusive time |
|---|---:|
| Impact-time queries | 22.523 ms |
| Island discovery | 4.956 ms |
| Actual event contact rows | 3.359 ms |
| Joint preparation | 0.146 ms |
| Physical impulse solve | 0.640 ms |
| Other continuous-impact work | 1.172 ms |

The query phase spends **20.408 ms** inside pair geometry (1,770 calls); island discovery spends **4.065 ms** in pair geometry (371 calls). Full response-row geometry costs 3.303 ms (184 calls). The step tests 12,704 cooked triangles / 12,344 convex SAT pairs / 31,639 BVH nodes. All original CCD requirements remain active.

The profiler reports no dropped events, exhausted labels/nodes/threads, incomplete scopes or truncated records. Seven open scopes are the normal export-time enclosing scopes. Geometry timers include pose preparation; they do not claim all measured cost belongs to SAT arithmetic alone. The original CSV's `geometry_predicates=0` tracks primitive `ContactGeometryDiagnostics`, not cooked `CollisionGeometryStats`.

## Small equivalent-work correction

Boolean contact observations previously built complete cooked contact manifolds: all witnesses, physical anchors and deterministic contact coverage reduction. The isolated correction shares the **same accepted-witness evaluator** with full manifolds and stops a Boolean observation after the first accepted witness. SAT, clipped-face witness gaps, edge-fallback SAT gaps, margins, sidedness, active-edge policy and BVH remain unchanged. Full impulse rows still receive complete manifolds. No cadence/CCD limits/contact fidelity or rig properties change.

The instrumentation stays in owned copies. Acceptance timing below uses an **uninstrumented** isolated archive.

| Case | Before median / p95 / maximum | Isolated existence correction median / p95 / maximum |
|---|---:|---:|
| Box, one rig | 0.154795 / 0.234744 / 4.312341 ms | 0.154875 / 0.244063 / 5.723246 ms |
| Box, four rigs | 0.636415 / 0.855523 / 17.960147 ms | 0.630396 / 1.024949 / 18.698935 ms |
| Mesh, one rig | 0.331528 / 12.233121 / 25.841217 ms | 0.324363 / 9.754852 / 20.523983 ms |
| Mesh, four rigs | **1.040693 / 18.875046 / 33.793832 ms** | **1.032763 / 14.963051 / 23.999817 ms** |

The four-rig p95 decreases 20.73%, below the unchanged 16.67 ms gate. Its original gate also requires median <8 ms and maximum <50 ms. These isolated numbers pass all three; they must be rerun on the final combined candidate.

Every recorded per-step contact/CCD/sleep observation matches the before trace across all four 900-step cases. Four mesh rigs still have 45,936 contact rows, 69,190 CCD iterations, 28 bounded-search limits, minimum bone-center Y 0.0443587974 m and 39/52 bodies asleep. This preserves observed physical behavior; the CSV is not a complete bitwise dump of every body pose and velocity.

## Receipts and limits

- `../concave-existence-isolated.log` / `.json`: uninstrumented isolated runtime result.
- `phase-attribution.json`: temporary instrumented phase split.
- `existing_trace_analysis.json`: original slow-step analysis.
- `existence-trajectory-parity.json`: all 900 per-step observations in each case.
- `parity.md`: independent full-manifold and Boolean predicate verification, including preserved failed probe assertions.
- `MEASUREMENT_FINGERPRINTS.json`: SHA-256 inventory of sources, recipes, receipts, binaries and raw measurement inputs.

No Windows timing claim is made here. No fluid code or acceptance threshold changes are involved. Final combined physics/contact/CCD/gravity/save/application validation remains the root task's responsibility.
