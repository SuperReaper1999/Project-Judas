# Cut-region pressure and material ownership

This packet follows Experimental `85a7fd2f54774ad8334b751fe1d2227f58199584`.
It contains disposable numerical experiments. Production fluid routing
and the Alpha lock are outside these experiments.

## Pressure through a narrow passage

The [captured native failure](../remap-native-continuation/README.md)
uses the actual MK2 tick-28 state. One coarse pressure unknown combined a
vanishing side slit and an opening or closing cap. A tiny lateral
perturbation therefore opened a finite shortcut to the cap. Independently,
a dead-end slit with essentially zero flux inherited a closed-face normal
predictor. The pressure required to stop that predictor added a spurious
side load. In the captured mixed state, the associated lateral loads
were about ±1,036 N at ±1e-14 m. The exact aligned path was −0.24 N.

The independent local oracles distinguish the two errors. For a side
opening with conductance `G_o` and an internal neck with conductance
`G_n`, eliminating the slit pressure produces a cap connection of
`G_o G_n / (G_o + G_n)`. The same slit pressure owns the lateral wall
patch. With retained normal velocity, the zero-width limit also has
to include the normal reaction needed to prevent flow. A cap-only flow
correction leaves that reaction error. Copying an exterior pressure
onto a patch without changing the flux operator breaks work balance.

`pressure/LocalRectCutPressure.cpp` runs an actual global pressure
solve on the captured body dimensions and prescribed displacement.
It uses a filled, initially still, uniform-density chart. Fixed local
cut planes keep the same volume partition for every perturbation. Exact
swept rectangular openings and work-adjoint wall patches retain separate
cap and slit unknowns. Symmetric pressure elimination and pressure
differences recovered relative to an adjacent node avoid loss of
balance in binary64.

The local experiment passed all 11 checks. At ±1e-14 m, the tested
neck carries (2.45\times10^{-18}) m³ in either perturbation and
exactly zero at alignment. Lateral force is ±7.83e-6 N. The vertical
force differs from the aligned path by 0.00052 N in the roughly
27.4 kN uniform-density control. The largest true local flux balance
error is 2.42e-16 m³ and pressure work closes to 7.39e-13 J. These
are continuity controls for the pressure geometry. They do not reuse
the captured mixed material and velocity fields.

The earlier moving-knot version changed its spatial discretization at
alignment and retained a 45.6 N vertical jump. A fixed global overlay
reduced that jump but the recursive CG residual underestimated the
true local imbalance, which reached 3.61e-12 m³. Condensation corrected
the imbalance, at the cost of 33,295 connections. The local overlay
reduces this to about 10,012 connections from about 1,940 before
elimination. Its nine prescribed solves took 12.12 ms median and
13.26 ms worst on the Ryzen 5 5600. This diagnostic cost excludes
material advection, native coupling and nonlinear rigid iterations.
Locality improves the result but the fill-in is still too large to
adopt this exact solver as the game runtime path.

## Shared water and air ownership

The sealed 0.20 m pool rejects at 0.5333 s. Preserving paired half
flows advances a scratch run to 0.7167 s. Feeding exact axial
phase support into the primal partition extends it to 0.9500 s.
At that point the independent axis allocations disagree about where
the water resides. In the retained trial, two neighboring cells
have half inventories that fit each axis separately yet cannot
coexist in one eight-octant geometry.

The joint offline solver finds a deterministic witness with the
other 1,498 cells and all primal phase flows fixed. It changes
32 physical variables in the two cells, verifies the original
phase supports and donor limits, and measures 8.40e-15 m³ raw
balance error against the unchanged 8.45e-15 m³ tolerance. The
solver-only time is about 2.3 ms. The witness reproduced byte for
byte from the archived sample. Common quadrant flow on each face,
swept material timing and persistent shared state remain to be
implemented. The scratch run retains the original rejection because
it never publishes the diagnostic witness.

## Original basin and axis controls

The static geometry compiler represents the five authored overlapping
walls, their exact seams, separate wet traces and half-cell volumes.
The geometry checks pass 46/46 and the composed phase geometry checks
pass 49/49. The fully wet pressure handoff conserves water and passes
1.30e-6 m³ inward through each authored seam. Both matched sealed
faces have exactly zero flux.

All four one-step attempts from the original mixed 9.25 m³ seed still
reject. At 60 Hz the first dry corner donor needs 13.14 times its
starting capacity with authored walls and 20.87 times with the matched
seam sealed. The 480 Hz diagnostic still rejects. Changing the
hydrostatic reference changes the intermediate predictors and leaves
the excessive projected throughput. The original walls, seed and
cadence are retained in the gate.

The sealed 0.25 m chart with gravity cycled from Y to Z preserves the
same dimensions, physical seed and settling observers. It first
changes subdivision at frame 13 and rejects a phase midpoint transfer
at 10.7333 s. This is an axis-order control, separate from both the
successful level proxy and the unchanged native rotated fixture.

A vented atmospheric column shows a potential smaller gameplay scope.
All 14 checks pass in optimized and sanitizer builds, including
moving-piston force and work, conserved water crossing a cell face,
and zero load over a 4 mm dry gap. A separate two-reservoir reduction
passes 30 checks in both builds. Halving the wet aperture from 1 mm
to 0.5 mm cuts flow from 6.54e-4 to 3.27e-4 m³/s in the matched
head control. Closing the aperture gives exactly zero, and an equal
head stays static. At fixed link length the true transit CFL stays
finite as the aperture narrows. Excess liquid transit still rejects.
This mode has no simulated air inventory. Three-dimensional interface
motion and the original authored basin remain qualification requirements.

## Reproduce

From the repository root, run the focused C++ and Python geometry gates.

```sh
e=docs/evidence/fluid-moving-grid-20261103/cut-region-oracles
bash "$e/verify.sh"
```

The four expected authored mixed failures are asserted in that script.
The isolated joint phase oracle requires NumPy and SciPy. The recorded
environment was CPython 3.14, NumPy 2.5.4 and SciPy 1.18.1. Build and
run the cyclic settling driver with the same flags and objects as
[the level driver](../geometric-continuation/README.md).

```sh
gzip -dc "$e/phase/octant-separation-local-sample.txt.gz" > /tmp/judas-octant-sample.txt
OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 python3 "$e/phase/joint_octant_feasibility.py" \
  /tmp/judas-octant-sample.txt joint --patch 544 545 --eliminate-fixed \
  --witness /tmp/judas-octant-witness.npz
cmp /tmp/judas-octant-witness.npz "$e/phase/two-cell-witness.npz"
```
