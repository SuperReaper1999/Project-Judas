# Independent combined-candidate review

This is a read-only review of the production integration after the isolated geometry experiment. No build, simulation or production edit was performed for this review. The root investigation produced the combined run cited below. Exact file hashes and raw CSV comparisons are in `combined-readonly-review.json`.

## Geometry and CCD fidelity

- Current `src/CollisionGeometry.cpp` SHA-256 is `554caa6eb6823fe1e9fa33d8c16472e6d1757faa315532768e6233316ca67de0`; `src/CollisionGeometry.h` is `5b8b12373b7d4fbcf98c9d87a6487d8e440e2b12d80e0d6aae352aba8ef22198`. Both are byte-identical to the isolated implementation that passed 286,091 checks over 43,980 samples, including bitwise complete-manifold parity against the preserved original implementation.
- The shared traversal retains original SAT axes, sidedness, triangle adjacency/active edges, BVH candidates, clipping, witness-gap acceptance and deterministic witness order. Its full-manifold consumer visits every accepted witness, constructs the same contact fields and invokes the same reduction. The Boolean consumer only stops once an accepted witness establishes `count > 0`.
- `PhysicsWorld::PairTouches` samples the same represented impact poses and child geometry. It uses the Boolean path only for zero-margin CCD truth and contact-island discovery. Sphere/capsule cooked contacts and primitive contacts retain their original complete-manifold dispatch.
- Proximity advancement and physical event-contact rows still call `PairAt`, constructing complete manifolds. Collision filters, broadphase candidates, current mesh geometry, all 32 bisection updates, 64 conservative-advance iterations, bounded sampling policy and impact response are retained. The optimization does not remove contact points from a physical solve.
- The combined candidate also corrects event-boundary state selection and completed contact observations. These intentionally change the physical trajectory relative to the original failing candidate. The isolated optimization's unchanged per-step counters must not be represented as bitwise trajectory parity for the entire combined fix.

## Same unchanged four-rig workload

The preserved observer source SHA-256 is `f0ce020eb47613ae82bb2619cdc03e6b477782b744576be2b9a36a52f5079136`. It still loads the actual original thirteen-body rig, duplicates it four times, cooks the same 8,192-triangle floor and runs 900 steps at 60 Hz. Original body geometry, constraints, authoring, gravity, impulses and physics cadence were not changed.

| Candidate | Median (ms) | p95 (ms) | Maximum (ms) | Steps above 16.67 ms |
| --- | ---: | ---: | ---: | ---: |
| Original failed follow-up | 1.040693 | 18.875046 | 33.793832 | 78 / 900 |
| Exact Boolean optimization only | 1.032763 | 14.963051 | 23.999817 | 30 / 900 |
| Combined corrected event state + optimization | 0.734328 | 3.297413 | 23.341687 | 3 / 900 |

These are same-workload observations, with the combined physics correction yielding a different legitimate trajectory. They are not an isolated attribution of the combined speedup to the Boolean optimization. The isolated row provides that attribution separately. The combined p95 passes the existing 16.67 ms gate, but three individual steps still exceed it; the maximum remains visible rather than hidden by averages. Combined mesh-4 reports 48,984 contact rows, 14,252 CCD iterations, one bounded search limit and 39 sleeping bodies. All positions passed the unchanged finite/penetration sanity guard. The other three unchanged observer cases also completed.

Raw evidence: `../combined-concave.log`, `../combined-concave.json`, `../combined-concave-probe-build.json` and `.cache/m78-continuation/concave-combined-data/`. The build receipt pins archive `05f396fdfded872573f44f1cd995273b5ecc088b7721d3d5458f39a22b07a228` and executable `ee0a509892b95c6fb602ee81d49d8bfbc7de6ee3f81ad0b224ee094f2f00aed8`. The immutable compact observer still produces its original misleading-indentation compiler warnings; it was not edited to remove historical warnings.

## Reproduction command prepared for the root operator

The root already compiled and ran the combined observer. No repeat is needed solely for this review. If a later source correction requires an affected rerun, choose fresh output names and compile the same observer against the newly built combined archive:

```sh
python3 docs/evidence/m78-continuation/probes/compile_probe.py \
  --name combined-concave-recheck-2 \
  --source docs/evidence/m78-followup/probes/concave_final.cpp \
  --archive .cache/m78-followup/release/libjudas_engine.a

SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
XDG_DATA_HOME="$PWD/.cache/m78-continuation/data" \
XDG_CONFIG_HOME="$PWD/.cache/m78-continuation/config" \
  .cache/m78-continuation/combined-concave-recheck-2 \
  .cache/m78-continuation/concave-combined-recheck-2-data
```

Capture a fresh run receipt, stdout/stderr, source/archive/executable hashes and compare the CSVs with the original `.cache/m78-followup/concave-final-data`. Do not overwrite old inputs or claim identical physical counters when testing a state-correction mechanism. This preparation did not execute these commands.

## Historical preservation

The previous 439-file candidate manifest was checked in full. Exactly five entries differ, all authorized current continuation source/tests: `src/CollisionGeometry.cpp`, `src/PhysicsWorld.cpp`, `src/RigidMotion.h`, `tests/CollisionGeometryTests.cpp`, and `tests/RigidMotionTests.cpp`. The other 434 entries match, including every recorded historical evidence artifact. Its manifest remains unchanged at SHA-256 `38892f9979dc8eadf91c6a397c7f86ae60678bce83e48b5927aafac7f4c829e5`.

The original M78 audit's 600-file manifest was separately rechecked: all 600 match, zero missing/changed artifacts. Its manifest remains `fe4d7221e165d26cf79836a62f4fe000eff8700661d06b1e21b0ffc938cb0e27`. The original numerical/performance failures and invalid bonus-assertion follow-up remain preserved. This check does not replace the root's final broader tracked-file preservation check or Linux/Windows combined validation.
