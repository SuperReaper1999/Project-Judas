# Static and moving phase geometry handoff

This is an executable geometry experiment in the ignored cache. It uses the
qualified parent directory's static partitions and does not modify the
prototype or production source tree.

`ReconstructStaticMovingPhase` takes authoritative water volume and the
current stationary snapshot of every moving box. For each cell it combines
the static partition's phase-axis cuts with the dynamic box edge heights.
Free volume is piecewise linear in the plane elevation between those cuts.
The implementation integrates one clipped slab at a time and inverts its
storage. Empty and full cells publish their exact coarse endpoints. The
reported capacity uses the same arithmetic as the composed pressure geometry.

`BuildStaticMovingHalfStorage` clips the fixed free volume to each physical
half and its reconstructed water plane. It subtracts the exact solid
intersection from each moving box. `AverageStaticMovingWetApertures` and
`AverageStaticMovingHalfApertures` do the corresponding calculation on
shared and midpoint rectangles. The two donor traces use the respective
cell plane, including the one-sided trace at a horizontal phase boundary.
The fixed free clip is time independent. The moving contribution uses the
existing event-partitioned analytic average for a linear AABB path.

## Evidence

- The manufactured same-cell fixture uses one fixed wall and one separated
  moving box. Analytic water volumes reconstruct two different adjacent
  heights exactly. Analytic coarse opening, left wet and right wet areas
  are `0.71875`, `0.328125` and `0.3984375 m²`. The upper transverse half is
  dry on one side and has `0.0703125 m²` of water support on the other.
- The original `half_025` five walls are reproduced using their authored
  binary32 center and half extents promoted to binary64. The chart is
  `24×32×24`, `h=.125 m`, origin `(-1.5,-.5,-1.5) m`. A separate physical
  box inclusion-exclusion calculation provides the manufactured water
  inventory for a specified global plane. The inverse is per cell.
- The largest partial-height error is `1.11e-16 m`. Canonical capacities
  and total coarse openings match the composed pressure geometry exactly.
  The worst half/coarse opening closure is `1.73e-18 m²`. Half water and
  capacity closure are also checked across every cell and component.
- Both authored seams have `0.00099999999999988987 m²` wet area with the
  partial seam fill and `0.0020000934600830078 m²` at the measured release
  surface. Changing only the Z wall half-height to `1.5f` makes both wet
  traces exactly zero in the matched seam control.
- The authored box's prescribed vertical motion reaches an independent
  partially immersed endpoint. It reconstructs the known `2.2094379663467407 m`
  chart water plane from the manufactured endpoint inventory and matches
  the composed endpoint capacity. This is a geometry witness.
- Over-capacity water and a swept wall intersection reject without
  replacing the previous result.

The full chart diagnostic takes roughly 27–28 ms on one Linux host for
the coarse phase, all three half-storage and aperture directions, and
the comparison loops. These are single diagnostic samples, not isolated
geometry timings or a real-time runtime claim.

## Scope

Static and moving solids must remain strictly separated. Combined free
pockets in a mixed cut cell are still an admission responsibility. A
future caller must supply its genuinely transported endpoint inventory.
During a sweep, the local water planes are held fixed. This experiment
does not run the pressure solve, move fluid, integrate MK2, establish
equilibrium settling, or authorize enabling Advanced Fluids.

## Reproduction

Run from the Project-Judas repository root.

```sh
d=.cache/fluid-moving-grid-20261103/static-wall-union
p="$d/phase-geometry"
g++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror \
  -Itools/prototypes/fluid_volume_grid \
  "$d/GridStaticUnion.cpp" "$p/GridStaticMovingPhase.cpp" "$p/GridStaticMovingPhaseTests.cpp" \
  -o "$p/static-phase-tests"
/usr/bin/time -v "$p/static-phase-tests"

g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer -no-pie \
  -Itools/prototypes/fluid_volume_grid \
  "$d/GridStaticUnion.cpp" "$p/GridStaticMovingPhase.cpp" "$p/GridStaticMovingPhaseTests.cpp" \
  -o "$p/static-phase-tests-asan"
ASAN_OPTIONS=detect_leaks=0 "$p/static-phase-tests-asan"
```
