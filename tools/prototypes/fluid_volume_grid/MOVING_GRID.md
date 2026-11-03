# Moving occupied-volume prototype

This isolated C++17 prototype uses absolute water volume and cut-cell
capacity. The existing fixed-grid prototype and engine fluid code are
separate. Alpha remains locked. This code has no engine startup, packaging
or runtime dependency.

Run the reproducible core qualification from the repository root.

```sh
bash tools/prototypes/fluid_volume_grid/qualify_moving_grid.sh /tmp/judas-moving-grid
```

The script builds from source and runs the strict phase-flux gate. The
[recorded outputs](../../../docs/evidence/fluid-moving-grid-20261103/) include
the original dry-gap and donor-flux failures and the first coupled run.

## Physical state and ownership

`MovingGridGeometry.h` computes the beginning and ending free volumes
and exact time averages of shared face openings and wall patches for
translating axis-aligned boxes. An ordinary positive-axis solid sweep
satisfies `C_end - C_start = div(solidSweep)` per cell. Moving box
owners must be unique and their swept bodies must remain separated
and strictly inside the chart. Geometry and solver publication are
transactional in the supported domain.

The pressure core stores one flux per physical face. Its mixture-volume
constraint is `div(Q) = C_start - C_end`. The paired transfer partitions
that same flux into water and air. All material mass-flux values derive
from that partition. The work-adjoint wall reaction is the pressure on
time-averaged solid surface patches. Its work cancels the moving-boundary
pressure work in the fluid. Energy tests separately account for the
implicit pressure correction. This discretization uses the same energy
projection principle as [Batty, Bertails and Bridson](https://www.cs.ubc.ca/~rbridson/docs/batty-siggraph2007-variationalcoupling.pdf).
Swept capacity is an explicit discrete constraint, consistent with the
geometric conservation requirement discussed by [Seo and Mittal](https://pmc.ncbi.nlm.nih.gov/articles/PMC3156558/).

The equilibrium pressure of an exactly flat interface uses the
geometric wet length along a vertical pressure link. A solid in the
same partially wet cell owns only the actual wetted parts of its wall.
The evolving step reconstructs a horizontal interface in each cell from
its water inventory and uses a shared hydrostatic reference plus local
residual gravity. Departure slabs bound which phase a face can receive.
A mixed transfer can change composition to satisfy all donor inventories
but cannot invent a phase outside its departure support. This is a
low-order interface reconstruction. It has one level per cell and
cannot represent independent subcell pockets.

If a prescribed step exhausts a donor, the interval method can try
conservative substeps along the original path. Pressure, geometry and
material publication must succeed throughout the interval. The returned
mean force and torque integrate the accepted substeps; the final field
describes the last substep. Malformed input, impossible sealed
components and exhausted subdivision return a failure without a
partial result.

The coupled box solves its pressure impulse and displacement together.
The fully submerged constant-density path removes hydrostatic pressure
analytically, then solves the dynamic part. The evolving surface path
adds actual reference-phase hydrostatic work and repeats the moving
pressure evaluation to close the rigid impulse. Rotation is constrained.
Box extents must each exceed one cell. A smaller body could have zero
velocity pressure while remaining entirely within one cell.

Momentum transport has an algebraic staggered dual ledger using the
same material mass flux. The joined fixed-topology route uses the same
dual mass for projection and advection. The separate half-dual ledger
retains side ownership when apertures close and reopens them with a
momentum-preserving merge. Pressure impulses, advective exchange and
stationary wall impulses are checked independently.

## Validation boundary

The standalone run covers 588 checks. It includes exact free-volume
and wet-geometry controls, a 4 mm same-cell dry gap, an actual moving
waterline, free rigid flotation and sinking, flow-driven motion, rotated
charts, linked material and momentum transfer, and transaction failures.

- The earlier same-cell dry-gap control showed a false upward load of
  184.77 N. The solved reference produces zero water-induced load
  against the matched air-only control on all three chart axes.
- The original strict waterline transport case lost 0.002103 m³ into
  unreachable dry departure slabs. The new gate measures zero leakage.
- The larger preserved 12 mm entry rejects in one step and succeeds
  with two accepted substeps. The maximum donor fraction used is .532.
- The freely moving 650 kg/m³ box enters the water in 16 coupled steps.
  Rigid impulse closes to 5.58e-9 N·s, pressure work to 4.36e-9 J and
  water volume to 1.34e-14 m³.
- A nonplanar cyclic chart comparison agrees within 5.21e-18 m³ per
  cell and 6.67e-15 m/s per face.
- The six-step full-water pressure, rigid and momentum composition
  retains volume and measures rigid impulse error below 6.56e-12 N·s.

A separate 28-check native consumer uses the existing PlayerController
and native physics archive. It samples authoritative occupied volume
through the grid query. In level and oblique charts the player enters
evolving water, responds to actual jump input while immersed and exits
dry. The playerless replay is bit-identical. The adapter approximates
the player with a chart-aligned box and samples local flow.

```sh
python3 tools/prototypes/fluid_volume_grid/run_grid_native_player_tests.py .cache/2d-stage1-build
```

The standalone solver's translation state is an independent reference
for the native MK2 handoff. The production renderer, native
contact/CCD combinations, terrain, arbitrary rotation of a solid within
the chart, persistence and full original gameplay qualification remain
separate gates. Passing these tests does not imply an Alpha unlock.
