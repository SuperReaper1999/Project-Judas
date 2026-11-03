# Transported interfaces and pressure inertia

This checkpoint follows Experimental `508bf41e3f9dca1fe2b0ba664275db2573dbe3ee`.
It extends the occupied-volume prototype. The Alpha fluid lock remains in
force, and the original authored qualification cases retain their clocks,
geometry and acceptance checks.

## Geometry and transport

A cut region can retain a signed interface axis. When an initially pure
region receives the other phase, the accepted face transfers identify the
dominant inlet. The new phase begins at that inlet, and the next pressure
and transport solve both read the resulting geometry. An already mixed
region retains its orientation. Opposing or ambiguous arrivals can still
require more geometry than one slab can represent.

Plane reconstruction traverses from the smaller phase and retains its
depth relative to the occupied slab. This preserves a 6.74e-19 m³ air
volume at a coordinate where the absolute plane rounds to the solid
boundary. It also places the saturated interface at the true free-fluid
limit when part of a cut is solid. A fully dry cut carries zero wet
support even when the solid opens a new aperture.

The pressure metric measures wet and dry volumes in the normal columns
connected to each side of the aperture. A solid stops a column at its
first boundary. The pressure gravity predictor uses the same measured
volume. For translating boxes, the intervals are split at geometric
crossings and integrated exactly for the resulting piecewise polynomial.
A four-way shared aperture measurement gives exact joint support for
water, gas and both one-sided transitions. The gas graph reads those
same planes and connects only through positive dry apertures. It rejects
unsupported dry splits within a cut.

A numerically admissible circulation can sometimes assign a trace phase
to a relay whose accepted receipts contain none of that phase. The
cut-region path can attempt a second partition with those unowned routes
closed. It retains the pressure flow and original geometric bounds, then
audits actual sources and both constituent inventories again. A required
route with no source remains a rejection.

## Native evidence

A captured MK2 hydrostatic control produced 2.59 N·m of spurious pressure
torque when two mirrored cuts differed only by a trace of air. The
free-fluid saturation correction reduces that same input to 1.95e-5 N·m.
A separately reflected remap input reduces it to 2.83e-11 N·m. The raw
source and both comparisons are preserved in the native evidence packet.

A second replay revealed that a trace of air could switch the pressure
calculation from one axis shortcut to another and change a mirrored
right-hand mass from 0.0003683 to 0.05351 kg. Using actual connected fluid
columns gives 0.0001575639 kg on both faces, reduces the predictor
difference to 4.92e-21 m³, and drops the recorded torque from 0.2514 N·m
to 3.09e-11 N·m. The projection diagnostic closes pressure work to
2.04e-14 J. These are fixed-input replays of the projection.

A separate fully submerged oriented-box control used real MK2 angular
pressure impulses and verified all four substeps of a 60 Hz tick.
Omitting the angular impulse triggered the independent endpoint check.
The first wet angular impulse was −0.522540 N·m·s. The original authored
wet basins remain separate failures before publication.

## Closed gas and the rotated basin

The original level seed encloses 0.534 m³ of gas around the body. The
rotated seed contains a 0.168 m³ region that is connected to the exterior
through a bottleneck of only 2.17e-18 m². Positive geometric connectivity
alone cannot decide pressure equilibration.

The standalone pneumatic graph uses the original rotated cut geometry
and a prescribed volume perturbation. Its finite aperture conductance
retains an 8.4035 Pa compression response at both the 60 Hz and MK2
substep durations. A manufactured open control produces 0.0855 and
1.1871 Pa respectively. Both mass and work checks pass. The full sparse
model also produces localized pressure peaks that a single chamber
pressure cannot represent. IC(0) reduced the two localized solves to
roughly 55–62 ms, which remains expensive for a game substep.

The separate level wet/gas scratch solve uses the shared phase geometry
and applies bounded outgoing water directly in its pressure equations.
With explicitly first-order gas splitting, it stages four stationary
60 Hz intervals with zero ordinary-donor overspend, water drift below
5.16e-14 m³ and gas work residual below 2.4e-12 J. The next interval
produces a gas merge. This scratch model lacks qualified water momentum,
moving MK2 coupling and a practical step cost. It is archived outside
the executable prototype.

## Reproduce

From the repository root, the prototype qualification uses

```sh
bash tools/prototypes/fluid_volume_grid/qualify_moving_grid.sh /tmp/judas-interface-qualification
```

The native replay packet and the three research archives carry their
own reproduction scripts and hashes. The pneumatic graph and wet/gas
experiments are isolated controls with their own supported inputs.

The combined prototype passed 967 checks across 24 executables on
native Linux. Project policy passed 26 checks. Trailing spaces from
successful empty error fields are normalized in the qualification logs.

The exact current connected-prism native dry control completed all four
MK2 substeps. The separate mixed hydrostatic control accepted one
substep with zero measured water drift, pressure work residual
1.25e-16 J and a 0.0405 J decrease in measured mechanical energy.
Its next trial is rejected by the phase ownership or geometry gate.
The level and rotated authored wet warmup both reject before
publication. In the direct probes, level rejects phase feasibility and
rotated exhausts the 2,048-iteration pressure solve. The retained
scratch retry resolves four unowned phase routes on a candidate and
then reaches a distinct infeasible phase partition at a subsequent
displacement. It publishes no additional native step.

The two-chamber hydro-acoustic control couples wet storage to finite
gas pressure and momentum through the same shared physical face. It
compares the symmetric and antisymmetric modes with closed-form
solutions and verifies separate water volume, gas mass and piston work.
All 36 controls pass. This is a linear, stationary-geometry
qualification of the coupling algebra. Its compression input is
prescribed and moving MK2 remains outside this control.

```sh
s=tools/prototypes/fluid_volume_grid
e=docs/evidence/fluid-moving-grid-20261103/interface-ownership
c++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror -I"$s" \
  "$e/research/HydroacousticPair.cpp" "$s/CutRegionGeometry.cpp" -o /tmp/judas-hydroacoustic-pair
/tmp/judas-hydroacoustic-pair
```

The preserved gas merge audit supports a finite-pressure coupling. The
fifth stationary level interval proposes joining two chambers with an
8.23 kPa pressure difference. With the measured opening history, its
isolated pneumatic comparator retains a 608–846 Pa jump by the end of
the interval. Instant equalization would erase that difference. The
authored settling, mixed native coupling, new-backend swimming and
rotated trajectory remain unqualified.
