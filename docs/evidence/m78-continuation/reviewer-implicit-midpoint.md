# Independent retained-midpoint review

This append-only review records a source/algebra/preservation check during the continuation. It does not replace the root's combined Linux/Windows validation, and no production edit, build or game simulation was performed.

## Angular integration

The retained algorithm is mathematically consistent at a complete force-free segment endpoint. With body momentum `m0`, it solves `m_mid = m0 + h/2 (m_mid × I⁻¹m_mid)` and rotates by the corresponding Cayley step. The endpoint body momentum is `Q(h)ᵀm0`; the quadratic kinetic energy and momentum norm are preserved to Newton/rounding error. World angular-momentum transport uses the endpoint inertia rather than leaving angular velocity unchanged. The twelve-iteration cap and explicit failure remain visible; there is no silent unconserved fallback. Force/torque kicks and contact/constraint integration remain approximate.

**Remaining numerical limitation:** the anchored CCD dense output uses the generator computed for the entire segment at an interior time. Exact endpoint energy conservation therefore does not imply exact interior or early-TOI-split energy conservation. An independent double-precision calculation, using Gaussian elimination and Rodrigues rotation rather than production GLM code, demonstrates the distinction:

| Inertia diagonal; initial angular velocity | h = 1/60 | h = 1/120 | h = 1/240 | h = 1/480 |
| --- | ---: | ---: | ---: | ---: |
| (1.06, .85, .29); (1, 2, 3) | −1.7456e−4 | −4.3358e−5 | −1.0803e−5 | −2.6961e−6 |
| (1, 2, 3); (12, 15, 20) | −6.8253e−3 | −1.9036e−3 | −4.9993e−4 | −1.2787e−4 |

Entries are relative energy errors at `t = h/2`, with the full-duration generator fixed. Full endpoints in these calculations conserve energy to approximately 1e−15; the interior errors converge approximately quadratically under refinement. These idealized algebra cases do not claim a measured game/contact failure or exact binary32 results. World momentum remains preserved by transport at each sample. A split may retain interior energy error, so constrained/CCD integration must remain described as approximate. Recomputing a midpoint for every query would change the anchored geometric trajectory and its bounds; no such correction or threshold change is proposed in this batch.

Raw equations, additional angular-velocity cases, Newton residuals, refinement ratios and the review's source hashes are preserved in `reviewer-midpoint-dense-output.json`, reproduced by `probes/review_midpoint_dense_output.py`. This is a local review snapshot, not final candidate fingerprints.

## Contact and CCD source review

The exact accepted-witness Boolean optimization remains byte-identical to the separately verified complete-manifold parity implementation. Complete physical manifolds, actual zero-margin witness tests, filtering, the original geometry, bounds and bounded CCD search remain intact. Right-limit event state and completed support observations are separate generic corrections; revision and generation checks reject stale observations. The anchored geometric generator remains distinct from transported physical angular velocity, so bounds continue to describe the actual sampled quaternion path. No new source evidence here warrants reducing fidelity or weakening tests.

## Protected preservation

`reviewer-protected-preservation.json` records bytewise checks against the frozen original baseline: **35,323 historical evidence/research/prototype files, 1,910,079,101 bytes, zero missing or changed**. Original M78 audit: **600/600** match. Previous follow-up manifest: **432/439** match; seven differences are current authorized source/tests (`CollisionGeometry.cpp`, `PhysicsWorld.cpp`, `RigidBody.cpp/.h`, `RigidMotion.h`, `CollisionGeometryTests.cpp`, `RigidMotionTests.cpp`), with **zero historical artifact differences**. Old manifests and original failures remain untouched. This does not claim a before/after hash for every unrelated ignored user asset.
