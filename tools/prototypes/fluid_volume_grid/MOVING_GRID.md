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

The two-phase owned path uses the same half-cell mass for pressure,
residual gravity and momentum transport. Both rigid feedback and
transport run inside the evaluated trial step for a body crossing the
waterline. Only the converged candidate is published.

A separate geometry path clips oriented boxes exactly at step endpoints
and integrates the moving openings and wall moments over time. It
verifies cell-volume changes against face sweeps and solid wall motion
independently. The prescribed-motion solver uses this geometry to
exchange linear and angular pressure work in a sealed, uniformly wet
or air-filled chart. Waterline crossings and free rotation are still
outside its supported scope. The surface extraction probe clips the
evolving axis-aligned grid interface against the same occupied volume
used by the solver. It returns triangles independently of the renderer.

## Validation boundary

The standalone run covers 874 checks. It includes exact free-volume
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
- The coupled waterline with momentum transport closes the rigid
  impulse to 2.58e-9 N·s, projected work to 1.98e-9 J, water volume to
  1.34e-14 m³ and transport momentum to 1.80e-13 N·s.
- The 10-step aperture reopening run closes rigid impulse to 7.81e-9
  N·s and conserves water to 1.78e-15 m³. This fixture reopens a face
  between cells that retain material. Fully uncovered cells are a
  separate case.
- Prescribed oriented motion transfers 0.82 J to the fluid in the
  combined translation and rotation case. The work ledger closes
  to 1.21e-10 J. A wet-to-air comparison changes the dynamic work by
  the physical density ratio.

A separate 28-check native consumer uses the existing PlayerController
and native physics archive. It samples authoritative occupied volume
through the grid query. In level and oblique charts the player enters
evolving water, responds to actual jump input while immersed and exits
dry. The playerless replay is bit-identical. The adapter approximates
the player with a chart-aligned box and samples local flow.

```sh
python3 tools/prototypes/fluid_volume_grid/run_grid_native_player_tests.py .cache/2d-stage1-build
python3 tools/prototypes/fluid_volume_grid/run_grid_native_mk2_tests.py .cache/2d-stage1-build /tmp/judas-moving-grid-native
```

The 57 native MK2 checks apply pressure impulses through the existing
constraint participant before CCD. A separate geometry rebuild verifies
the pose published by MK2. The represented-pose and rounding-only
reconciliation probes each complete 16 substeps on a freely translating
body. They reject a coarse chart whose representable displacement
cannot conservatively fit the projected water. These probes cover a
free rigid body. General CCD, contacts, joints and orientation still
require one authoritative native motion prediction and publication path.

The original half-density fixture leaves extra empty space around the
submerged placeholder. The grid proxy's warmup speed is 0.13231 m/s at
the original 0.05 m/s comparison. A same-volume flat control stays
exactly still, and halving the original warmup intervals changes the
release speed by 0.00092 m/s. The released box exposes another failure
at about 0.18 seconds, when a face opens between cells that started
with zero free volume. Rejecting the step preserves published state.
See the [ownership evidence](../../../docs/evidence/fluid-moving-grid-20261103/ownership/)
for the original probe, traces and rejected repairs.

The [cell-exposure probes](../../../docs/evidence/fluid-moving-grid-20261103/cell-exposure/)
resolve the geometric part of that failure. New water and air volume
are assigned to the actual half-cells. A separate single-phase
pressure probe joins a new cell to a directly connected old material
owner during projection and recovers the fine-cell transfer from
continuity. The filled piston cavity satisfies every cell balance
and sends no flux through its blocked midpoint. This is a reduced
pressure metric with no separate inertance for its internal link.
The [geometric continuation probes](../../../docs/evidence/fluid-moving-grid-20261103/geometric-continuation/)
now join that ownership to wet/dry transport, free-body feedback and
momentum. The sealed 0.25 m pool proxy completes 12 seconds with
3.36e-13 m³ peak water-volume drift and a late RMS body speed of
0.01053 m/s. Its initial preparation still exceeds the preserved
0.05 m/s diagnostic. The 0.20 m proxy rejects a later unsupported
half-cell transfer at 0.5333 seconds. These are grid proxies for the
original fixture, whose wall geometry and full native motion remain
qualification requirements. The additional native probes cover a
mixed aperture opening and dry gap through one MK2 tick, and the same
28-check level and oblique player path with geometric ownership.

The [remap and native continuation probes](../../../docs/evidence/fluid-moving-grid-20261103/remap-native-continuation/)
specialize staggered transport to one momentum component and reproduce
all logged release observables over 720 steps. The repeated native
probe verifies 96 exact published pose/velocity boundaries. A longer
native run reaches an aperture and traction discontinuity at tick 28.
Separate contact and CCD probes show why the existing velocity callback
cannot certify the path used by the pressure solve.

The [cut-region oracles](../../../docs/evidence/fluid-moving-grid-20261103/cut-region-oracles/)
verify separate pressure ownership for a thin slit and a cap, a common
water inventory across the three half-cell axes, and exact clipped
geometry for the authored basin. They preserve the finer pool, cyclic
gravity and original authored mixed transport failures. Separate vented
liquid controls pass without a simulated air phase. They qualify a
piston column and a fixed wet aperture, leaving the three-dimensional
free surface and native composition open.

Runtime adoption requires a conservative treatment of newly exposed
volume, native contact and CCD composition, representative terrain and
surface publication, cold persistence, and full original gameplay
qualification. The isolated backend leaves the Alpha policy intact.
