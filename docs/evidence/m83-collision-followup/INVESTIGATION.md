# Finite-triangle response-normal follow-up

## Observed mechanism

Claude reported a skateboard contact whose nearly lateral SAT axis acquired a small component opposite a mesh face and was snapped into a much deeper face response. Independent current-source reproduction established the same mechanism. This is **not** a reproduction of the entire external game/session; it isolates the generic contact bug.

`3102bf6c6e4b5c57eb3ab66a71fa502f8fc423f6` introduced the guard in `visitCookedPolyContacts` to prevent a deep overlapping thin body from being driven through a mesh. It correctly keeps SAT separation rejection ahead of any response correction and requires actual overlap. However, its `dot(normal, front) < -1e-3` predicate also classifies almost lateral axes as backward face responses, contrary to the stated lateral-axis preservation intent.

The independent fixture is one finite triangle at `(0,-1,-1)`, `(0,-1,1)`, `(0,1,-1)`, with front `(-1,0,0)`. A box with half dimensions `(0.23,0.4,0.1)` is centred at `(-0.01,-0.5,-1.01)` and rotated -0.002 radians about local Y. Its correct finite-edge SAT axis is approximately `(0.001999999,0,-0.999998)` and its independently projected overlap is 0.090020031 m. The original guard instead creates a face response `(-1,0,0)` with 0.219980464 m depth. At zero angle, it preserves the 0.09 m lateral response; a very small orientation change therefore creates a 13 cm depth and direction jump. This is a selected **body face axis**, so excluding only cross-edge SAT axes would not solve it.

`original-geometry.txt/.json` preserve the original angles, SAT axes, public manifolds and source/binary fingerprints. `original-contract-failure.txt/.json` preserve the actual expanded public test linked against the original engine; it fails after 19 checks. No original acceptance threshold or historical evidence was changed.

## Narrow generic correction

Keep the existing face correction only when the unit normal's backward face component exceeds its entire tangential component:

```text
backward = -dot(normal, selectedTriangleFront)
tangentMagnitude = sqrt(1 - backward^2)
correct face response only if backward > tangentMagnitude
                         equivalent to dot(normal, front) < -sqrt(1/2)
```

This rear 45-degree cone distinguishes predominantly backward response from predominantly lateral minimum translation. It is an explicit bounded response policy, **not a mathematically unique continuum contact law**. At the equal-component boundary the lateral SAT response is retained. The existing face-axis fallback still protects clearly opposing deep axes. No SAT axis, finite-edge rejection, witness clipping, culling, contact margin, solver budget, physics cadence or public API changes.

The chosen rule is independent of gravity/world-up and uses the selected front, including the reversed front for the back of a two-sided mesh.

## Isolated policy comparison

Two single-object variants were linked against the same existing engine archive and unchanged `ImpactRobustnessTests.cpp` object before changing production source:

| Policy | Unchanged impact checks | Deepest support corner | Result |
|---|---:|---:|---|
| Dominant backward component: 45-degree rear cone | 4,412 | 0.0179729 m | Pass; original 0.03 m bound retained |
| Claude suggestion: 60-degree rear cone / dot < -0.5 | 4,412 | 0.0179729 m | Pass; original 0.03 m bound retained |

Both retain clearly opposing -0.8/-1.1 radian cases. The 45-degree rule also retains a -0.6 radian lateral axis because its backward component (0.565) is smaller than its lateral component (0.825); the 60-degree rule unnecessarily replaces it. No demonstrated protection regression required the broader cone. `variant-receipts.json` records exact compile/link arguments, source/binary hashes, exit codes and runtimes. These isolated variant results are not presented as final rebuilt-engine validation.

## Focused contract coverage

The ordinary collision-geometry test now independently projects all eight body and three triangle vertices. It covers: zero/nearly perpendicular axes; lateral-dominant axes; both sides of the chosen angular boundary; genuinely opposing deep response; arbitrary whole-fixture rotation; one/two-sided winding and back-side reversal; full manifold/Boolean agreement; reversed pair order; negative/zero/positive margins; and a box crossing the infinite plane but separated from the finite edge. It leaves the earlier geometry, contact and fast-spin fixtures/tolerances untouched.

`guard-removed-contract-failure.txt/.json` preserve the complementary independently compiled negative control. Removing only the guard lets all the early lateral cases pass, but fails after 79 checks at the first dominantly backward case. The original overbroad guard fails after 19 checks at the nearly lateral case. The same public contract therefore detects both the erroneous overcorrection and absent deep-overlap protection. Compile/link arguments and source/test/archive/binary hashes are retained.

The correction does not prove every external game's contact configuration. Existing finite precision, approximate response policy and bounded physics contracts remain. Final actual-engine validation is recorded separately from the isolated comparison receipts. Windows validation remains informational/nonblocking and is not inferred from Linux results.

## Final actual-engine regressions

The corrected production engine and rebuilt standalone test sources pass:

- Collision geometry: 1,303 checks, including both independent negative controls described above.
- Unchanged impact robustness: 4,412 checks; deepest support corner 0.0179729 m within the original 0.03 m bound.
- Sixteen affected regression executables: rigid contact, collision, kinematic, ragdoll, physical animation, impact timing, impact solver, impact mesh cost, physics casts, physics continuation, M83 material/connection/JS, wheels, powertrains and bending. All exited zero; exact commands, durations and binary hashes are in `final-regressions.json`.

The default mesh-cost fixture ran all six original scenarios, 300 steps each, twice for deterministic digest checking. Its dense trough measured 21.48 mm maximum penetration and 60.48 ms worst step on this host. These are diagnostic measurements, not tightened acceptance promises. All 311 original checks passed. Existing fast-box tunnelling counts remain visible in the receipt; this response-normal correction does not claim to repair them. The exact external Skate game acceptance recipe was not supplied or run; the public deterministic geometry fixture independently proves the reported mechanism.

Windows has not been rerun for this follow-up and remains informational/nonblocking. Runtime/editor/export relink and the combined M83 application follow-up are recorded separately when completed. Original `docs/evidence/m83` and prior milestone evidence are preserved.
