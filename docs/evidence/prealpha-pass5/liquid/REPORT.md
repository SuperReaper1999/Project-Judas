# Pass 5: half-turn liquid query identity and occupied geometry

## Results

**No production fluid correction is shipped.** The unchanged original half-turn
contract remains failed. New end-to-end controls establish why and separately
exercise physical loading and persistence that the original assertion prevented
that command from reaching.

| Command / independent oracle | Result |
|---|---|
| Original `--bake-regression knots` | **FAIL, check 21**, unchanged |
| Original separate `--query-controls knots` | 23 checks pass |
| Original `--bake-regression normalization` | 36 checks pass |
| Half-turn identity/geometry/loading/restore/full-dry proof | 38 checks pass |
| Separate radial identity/geometry/loading/restore/full-dry proof | 39 checks pass |
| Evidence-only absent-cap negative | Fails at check 20, as intended |

The added 38/39 totals include the unchanged fixture setup; they are not counts
of distinct newly certified engine features. Native Linux Release provenance,
commands, binary/archive SHA256 and elapsed times are in [RESULTS.json](RESULTS.json).
The actual binary reports clean Experimental `4c141e9bb6c68a50c63dec263dcec398200328ad`.
The linked engine archive is SHA256
`6e77a9f799ade7f6ce3ad741687689b8cae6d5efa6fd82e472923749ee21b441`.
[Source pins](FROZEN_SOURCES.json) prove all seven fluid/geometry/original-test
files remain byte-identical to accepted `4c141e9`.

| Measured physical quantity | Original half-turn inputs | Additional radial inputs |
|---|---:|---:|
| Owner-local input separation | 1.7261876e-5 m | 3.0517578e-5 m |
| Pre-shift cap separation | 0.040000035 m | 0.039512646 m |
| Pre-shift flow separation | 0.015999620 m/s | 0.014554033 m/s |
| Independent triangle-cap error, input A/B | 1.90e-8 / 1.03e-8 m | 1.51e-6 / 1.81e-6 m |
| Partial-solid displaced-volume change under real shift | 1.30e-15 m³ | 9.02e-16 m³ |
| Ledger error after legitimate full/dry transactions | 0 m³ | 0 m³ |

Both aliases are passed through the actual `RuntimeWorld` origin transaction.
The single resulting sample can match one prior point but cannot match both.
Ordinary capture/build/prepare/restore also retains exact wave bytes, actual
shifted query endpoint and ledger; this is a separately constructed runtime in
the same OS process, **not** a newly qualified cold-process save contract.
The probe does not claim an optical, full fluid suite, Windows or Android rerun.
No platform-dependent fluid code or binding changed.

## Scope and outcome

This investigation preserves the original half-turn command, fixture, timestep,
inputs and `0.0003` vector tolerance. No production liquid code, binding, quantity,
solver setting or protected research is changed. The original command remains a
failed contract. The additional checks below isolate the meaning of that failure;
they do not replace it with an expected-green test.

## Mechanism

`LiquidSystem::Sample` converts the represented binary32 world point to the
owner-local frame, chooses one finite-volume chart cell, and reads that cell's
actual occupied cap and modeled tangent velocity. `LiquidSurface::Mesh`, solid
submersion integration, optical paths, transfers and impulses use the same cell
allocation and geometry. The half-turn fixture includes a conserved local
withdrawal, so adjacent cells legitimately have different levels.

Let `F` mean binary32 rounding and `d=(512,-512,512)`. The existing fixture already
supplies two original-coordinate inputs:

```
p_a = point
p_b = F(F(point-d)+d)
```

They are distinct before the shift, lie in different represented cells, and
produce different physical caps and flow. Nevertheless:

```
F(p_a-d) == F(p_b-d)
```

A deterministic stateless `Sample(vec3)` receives exactly the same input after
translation. If the two original outputs are separated by more than `2*epsilon`,
no one output can be within `epsilon` of both (triangle inequality). Promoting
already-rounded coordinates to binary64 cannot recover which input was intended.
This establishes a precision/representation contract conflict, rather than a
corrupted reservoir, missing wave state or an additional rotated-baking failure.

The independent probe ray-intersects the actual generated mesh triangles rather
than accepting the sampling implementation as its own geometric oracle. It also
checks real partial-body submersion, ordinary persistence reconstruction, exact
wave bytes, the owner ledger and a separately selected radial alias. The radial
queries are additional inputs and are not mislabeled as the original half-turn
fixture inputs.

## Reconstruction boundary

The occupied representation is:

```
q_i = C_i^-1(V_i)
Omega_i = G_i intersect { Q_h(x) <= q_i }
```

Here `V_i` is the allocated cell quantity, `G_i` its available tetrahedral cavity,
and `Q_h` the implemented affine gravity coordinate within a tetrahedron.
Separate paired transfers can legitimately fill one cell and empty its open-face
neighbour before the next solve. For this selected full/empty state, preserving
the exact existing occupied cell volumes inside a bounded cavity requires a
physical interface step. Simply averaging heights changes either the actual
occupied local volumes or their relationship to the conserved allocations.

This does **not** show that continuous conservative free-surface methods are
impossible. A redesigned reconstruction could have different subcell degrees of
freedom, wet/dry treatment and consistent integration. It would require one
coherent contract for mesh geometry, loading, optics, wet-face fluxes, transactions
and queries. A query-only interpolation or tiny cell-selector epsilon would hide
the observed difference while reporting geometry the other consumers do not use.
Neither is implemented here.

## Recommended correction boundaries

1. Preserve an explicit, stable owner-local/absolute binary64 query identity when
   callers require the same material-space point through an origin transition.
   Converting an ordinary binary32 actor point afterward cannot restore lost bits.
   An additional precise API would not retroactively pass the unchanged failing
   world-`vec3` assertion; that distinction must remain visible.
2. If smoother physical surface sampling is required independently, design and
   validate a conservative continuous reconstruction across all occupied-geometry
   consumers, including arbitrary gravity, nonlinear storage and wet/dry limits.
3. Keep the existing failed oracle and its exact threshold until its intended
   coordinate identity is explicitly reviewed. Do not silently substitute a
   nearby point, smooth just the query, reset waves or change the tolerance.

FLU-001 remains separately deferred and failed. Independent research was neither
accessed nor modified.

## Reproduction and preserved failed attempt

Build the existing native Linux Release `judas_origin_environment_tests` target
first. The portable wrapper requires that target's Unix Makefiles metadata and
links only one evidence translation unit against the existing engine archive:

```sh
python3 docs/evidence/prealpha-pass5/liquid/reproduction/reproduce.py \
  --repo "$PWD" --build "$PWD/build" --output /tmp/NEW-pass5-liquid
```

The wrapper refuses changed pinned fluid/original-test sources and an existing
output directory. Its `expected_exit: 1` records are preserved failures, not
original acceptance passes. The probe is generated from the original fixture;
its authored fixture and ordinary preparation bodies are exact, checked in the
receipt. `JUDAS_LIQUID_ALIAS_PROBE` and the absent-mesh switch exist only in this
evidence translation unit, never in engine/demo/runtime source. The absent-mesh
negative removes the oracle's generated triangles and verifies that its independent
geometry check detects their absence.

The first additional radial probe incorrectly assumed that any rounded original
point would cross a chart cell. Its height rounding stayed in the same cell, so
it correctly failed that assumption at check 19. [attempt-1](attempt-1/) retains
its exact generated source, generator, receipt and output. The corrected
additional radial setup explicitly searches opposite tangent-side inputs that
both alias after translation and actually select distinct cells. The original
half-turn point, delta, fixture, assertion and tolerance are never modified.

The current [source map](../../../../src/LiquidSystem.cpp) includes `Sample`,
`Submerged`, paired-transfer endpoints and `OpticalPaths`;
[LiquidSurface.cpp](../../../../src/LiquidSurface.cpp) owns the actual finite-volume
caps and exposed face bands. The [previous investigation](../../prealpha-pass4/liquid-query/REPORT.md)
remains unchanged.

## Disposition

- Half-turn world-query identity: **open**, now supported by an explicit aliasing
  counterexample and independent geometry/control proof.
- Conserved quantity, retained surface state, represented shifted submersion and
  reconstructed represented endpoint: passing within this specific scope.
- No public API, physics equation, solver setting, fluid geometry, authored asset,
  original test or threshold changed.
- FLU-001: carried forward as deferred/failed; not rerun, changed or reclassified.
- No protected historical artifact or independent research was changed.
