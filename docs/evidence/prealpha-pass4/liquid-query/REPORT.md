# Pass 4 half turn liquid query contract

## Result

The original half-turn origin-query contract **still fails unchanged**. The
failure is a binary32 world-point/owner conversion crossing a discontinuous
finite-volume cell boundary, not lost water, damaged wave state or another bake
failure. No fluid code, numerical threshold or test was changed.

The current combined Linux engine was linked into the unchanged existing
`judas_origin_environment_tests` target. `RESULTS.json` records the exact binary,
engine archive, build identity, source hashes and commands. The relevant liquid
sources and the test are byte-identical to accepted `7da56c2`.

| Existing command | Observed result |
|---|---|
| `--bake-regression knots` | Original assertion fails at check 21 |
| `--query-controls knots` | 23 checks pass; explicitly separate controls |
| `--bake-regression normalization` | 36 checks pass with the previously accepted exact atomicity baseline |

All output directories were fresh. These commands ran native Linux against the
current combined engine; they are not fresh Windows or Android qualifications.

## Actual implementation path

1. `src/ScriptSystem.cpp`, `liquidSample` and `liquidPresentedSample`: `ReadVec`
   converts the supplied JavaScript vector to `glm::vec3` before sampling.
2. `src/LiquidSystem.h/.cpp`, `Sample(glm::vec3)`: promote that already represented
   point to binary64, subtract the owner pose's binary32 position and transform to
   the owner-local frame. The available occupied geometry remains owner-local.
3. `src/LiquidSurface.cpp`, `LiquidSurfaceData::Chart/Cell`: choose a chart column
   and row by flooring the scaled chart coordinate. There is no interpolation or
   hysteresis in cell ownership.
4. `LiquidSystem::Sample`: read the selected cell's level and tangent velocity,
   test its actual tetrahedral occupied geometry, and return its represented cap.
5. `LiquidSystem::ShiftOrigin`: translate owner/parcel binary32 positions and
   invalidate derived optics. Cell allocations, discharges, coordinates and
   immutable geometry are not moved or reset.

The original fixture uses `quat(0,.6f,.8f,0)` and a `(512,-512,512)` chart shift;
this is not a world-Y assumption. The measured owner-local query displacement is
`1.7261876139525506e-5 m`, changing cell `9` to `10`. The returned surface-point
difference is `0.040014658123254776 m`; velocity differs by
`0.015999620780348778 m/s`. The unchanged vector comparison tolerance is
`0.0003`. Complete surface-state bytes and surface energy remain identical.

## Independent controls already exercised

The existing separate query-control mode measures both differently quantized
points **before** any origin shift, on the same unchanged physical state. Those
points already select different cells and return different samples. The shifted
sample then matches the pre-shift sample at that same quantized point. A separate
interior-cell point retains the original strict position/normal/velocity
tolerances. The controls do not replace or relabel the failed boundary-point test.

The normalization positive control also preserves the previously accepted
stronger atomicity check: exact immediate-before/after owner total, unchanged
origin and byte-identical captured participants around the rejected request.
Its earlier legitimate transfer is allowed to change a floating-point sum from
`2.5199999999999996` to `2.52 m3`, while M54 ledger error remains zero. That earlier
summation change is not attributed to the later rejected request.

## Why a local precision edit is insufficient

Write `F` for binary32 representation, `O` for the current chart origin, `c` for
the owner origin and `R` for its orientation. The represented local input follows
approximately

`u = R^-1 ( F(p-O) - F(c-O) )`.

After a shift, independently rounded values need not reproduce the same `u`.
Promoting them to binary64 afterward cannot recover the missing bits. JavaScript
numbers being binary64 does not undo the binding's prior `glm::vec3` conversion.
The `world.toAbsolute/toLocal` helpers likewise cannot recover information already
lost before the conversion.

For the current finite-volume representation, `S(u) = q_i` in cell `i`. Points
arbitrarily close to opposite sides of a cell boundary can differ by
`|q_i-q_j|`, regardless of their spatial separation. Therefore this representation
has no uniform small pointwise error bound proportional to the chart-rounding
error at a disturbed boundary. This explains the failing contract; it does not
authorize weakening it or claiming complete rebasing equivalence.

## Appropriate correction boundaries

A future correction needs an explicit coordinate/reconstruction decision:

- Preserve a stable owner-local or absolute binary64 query identity, together
  with sufficiently precise owner anchors and consistent conversions across
  rebasing. An alternate query API alone would not make this original binary32
  test pass or recover every ordinary actor's lost coordinate bits.
- Or define a consistent continuous reconstruction for physical sampling,
  occupied geometry, rendering, optics, loading and transactions. That is a
  representation/coupling change requiring its own conservation and wet/dry proof.

Changing only the displayed query to bilinear averaging would disagree with
`LiquidSurface::Mesh`, which intentionally draws piecewise caps and exposed
shared-face bands. `Submerged` integrates actual occupied cell envelopes and
modeled per-cell flow. Point-authored transfer and impulse operations also use
this cell selector. Hiding the sample jump alone would not establish cross-system
physical equivalence.

Snapping to a grid, adding a cell-selection epsilon, choosing a previous cell,
resetting waves, changing the rotation, changing the fixture point or raising the
comparison threshold is not a justified correction. No such change was made.
The same chart-cell selection is used for radial surfaces; this report does not
claim a new adversarial radial boundary reproduction.

## Current documentation

`docs/LIQUID_SURFACES.md` still contained the old statement that both complete
rotation commands failed, including normalization's exact-total assertion. That
statement is stale after Pass 3's accepted causal-baseline correction. The root
Pass 4 workstream is updating current documentation and linking the later proof;
protected historical reports and original failures remain unchanged.

## Reproduction

```sh
cmake --build build --target judas_origin_environment_tests -j1
build/judas_origin_environment_tests --bake-regression knots /tmp/NEW-failed-knots
build/judas_origin_environment_tests --query-controls knots /tmp/NEW-query-controls
build/judas_origin_environment_tests --bake-regression normalization /tmp/NEW-normalization
```

Use genuinely fresh paths. The first command after building must still return
nonzero on this candidate. It is not an expected-green gate. FLU-001 remains a
separate deferred legacy-PBF issue. Independent fluid research was not accessed
or modified by this follow-up.
