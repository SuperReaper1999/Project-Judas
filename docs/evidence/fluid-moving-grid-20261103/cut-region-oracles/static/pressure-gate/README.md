# Authored wall pressure handoff

This disposable test pins the unmodified pressure code to
`8fd9a2de1156b262782d8e252f9426d4320a6e91`. It passes composed cut-cell
capacities, real shared open and wet faces, independent local phase planes,
and actual half-cell material masses into the existing pressure kernel. The
driver includes that pinned implementation in its translation unit to use
the private projection entry point. The diagnostic copy adds logging only.

The fixture reproduces the five authored walls, `half_025` initial box
dimensions and pose, `.25f` parcel spacing, `.36f` omission rule, and
`float(1/60)` cadence from `tests/ProductionFluidTests.cpp`. At chart
`h=.125 m`, the 592 authored parcels become 4,736 full cells for exactly
`9.25 m³`, with the exterior initially dry. No water was clipped, added,
redistributed or settled by a preparatory model. The original observed PBF
release surface `1.7094379663467407 m` is retained as the pressure reference
height, distinct from the authored parcel fill. The two pressure steps
are separate one-interval witnesses from the same seed. One holds the
body still, the other prescribes `+0.25 m/s` for one interval. The test
does not replay the production warmup or integrate a trajectory.

## Results

All four original mixed-water/air attempts failed the unchanged donor
capacity gate, including the first stationary preparation attempt. The
matched seam control changes only the ±Z wall half-height from `1.499f`
to `1.5f`; it preserves the tiny X/Z corner gaps.

| Walls and motion | First donor | Capacity (m³) | Outgoing (m³) | Donor CFL |
| --- | --- | ---: | ---: | ---: |
| Authored, stationary | (4,4,3) | 1.56260766e−5 | 2.05299382e−4 | 13.1383 |
| Authored, moving | (4,4,3) | 1.56260766e−5 | 2.02140233e−4 | 12.9361 |
| Seam sealed, stationary | (4,4,3) | 3.49245965e−10 | 7.29042711e−9 | 20.8748 |
| Seam sealed, moving | (4,4,3) | 3.49245965e−10 | 7.27942113e−9 | 20.8433 |

These are dry, positive-capacity corner donors. Their geometry is valid.
The pressure solve demands much more mixture through-flow than the explicit
starting inventory can supply in one interval. It refuses publication.
At `1/480` the first failures move to other cells at the residual corner
gaps and still reject. Authored CFL is `1.3277` stationary and `1.1366`
moving. Matched-seam sealed CFL is `6.1359` and `5.3647`. This is a separate
diagnostic and does not change the original timestep gate.

The `height=0` gauge control removes large dry-link intermediate velocity
predictors (up to `302.47 m/s` on the traced cell) and changes reported
perturbation pressures. The first authored stationary outgoing volume
remains `0.00020529938232884343 m³`, versus `0.00020529938233259848 m³`
in the original split. The dry-link predictor is part of hydrostatic
reference bookkeeping. Changing the reference does not repair the
post-projection narrow-cell through-flow or its donor budget.

A separate positive control saturates the entire chart with water, uses
zero gravity, and drives the same body through the same prescribed motion.
Both wall variants publish. The authored wall passes real, physically
supported water inward by `1.301793445e−6 m³` at each measured seam.
Both matched-sealed faces have exactly zero mixture and water flux.
Total water change is zero at printed precision, maximum local mixture
balance residual `5.74e−17 m³`, and maximum dynamic pressure-work mismatch
`1.78e−15 J`. Pressure plus geometry and work mapping take about 52 ms
per measured control on this Linux host. Peak RSS for the complete gate
process was 32,336 KiB. This confirms that the handoff can represent and
transport through the actual seam when the solver admits the material
state. It does not infer mixed-pool seam transport from rejected steps.

The driver also rejects a physically separated body whose swept hull shares
a coarse cut cell with a static wall, because combined pocket topology has
not been qualified for this gate. The previous output remains intact.
This test covers mixture pressure, conservative coarse phase flux and
pressure mapping. It does not execute the half-momentum remap, the
warmup, MK2 coupling, or the settling gate. No engine source was edited.

## Reproduction

From the repository root:

```sh
d=.cache/fluid-moving-grid-20261103/static-wall-union
g="$d/pressure-gate"
p="$g/pinned"
build() {
  source=$1
  binary=$2
  shift 2
  g++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror "$@" -I"$p" \
    "$d/GridStaticUnion.cpp" "$d/phase-geometry/GridStaticMovingPhase.cpp" \
    "$g/$source" \
    "$p/GridHalfCellGeometry.cpp" "$p/GridGeometricMomentum.cpp" \
    "$p/GridPhaseFluxRepair.cpp" "$p/GridConservativeRemap.cpp" \
    "$p/HalfDualVolumeMomentum.cpp" -o "$g/$binary"
}
build StaticPoolPressureGate.cpp gate
/usr/bin/time -v "$g/gate"

build StaticPoolPressureGate.cpp gate-diagnostic -I"$g/diagnostic"
"$g/gate-diagnostic"

build StaticPoolPressureGate-short.cpp gate-short -I"$g/diagnostic"
"$g/gate-short"

build StaticPoolPressureGate-gauge.cpp gate-gauge -I"$g/diagnostic"
"$g/gate-gauge"
```

All four runs are expected to exit `1` for the preserved mixed-pool
failures. The uninstrumented base reports `17 checks, 4 failures`.
The shortened and gauge controls each report `9 checks, 4 failures`.
