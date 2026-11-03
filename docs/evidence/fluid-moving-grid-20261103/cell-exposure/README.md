# Geometric half-cell ownership and exposed-volume pressure

Parent checkpoint `ec72a2fa586e29a73f6843ef34e7f30626331c49`.
The work is isolated in the prototype tree. Linux 7.0.0-42-generic,
GCC 15.2.0, AMD Ryzen 5 5600, 2026-11-03 UTC. The original particle
suite and fluid lock are unaffected.

The new half-cell geometry measures actual endpoint storage and
integrated midpoint and transverse openings without refining the
whole grid. It also reconstructs water and air ownership from the
existing local phase planes. The geometry agrees with explicit
spatial refinement to 8.68e-19 m³ for volume and 1.39e-17 m² for
area. A synthetic single inlet and its reversal validate the
half-cell balance independently of pressure. Misrouting flow into
a solid half is rejected even when the primal volume would balance.

A separate prescribed-motion pressure probe groups an initially
empty cell with a directly connected old material owner across a
real opening. The pressure solve uses the sum of each group's
individual endpoint capacity changes. After solving, the group's
internal transfer is recovered using the original cell's continuity
equation. Group-external material transfers and the original
per-cell endpoint balances remain checked. The probe uses one
uniform phase, and validates the mass and work ledgers for its
reduced pressure model. It does not assign a separate inertia to
the internal link or transport the new phase momentum. There is
no proof here that the absolute force is exact. A different
discretization can produce different force at the same resolution.

The 64-cell piston opens 0.01 m³. Its measured axial inlet contributes
0.00893 m³ and the rest enters through real lateral openings. The
oracle uses the total analytic cavity volume. This also exercises
the recovered lower-half balance with the upper half completely
solid. Water inventory error is zero in this run, wall/pressure
work agrees to 7.96e-13 J, and the pressure kinetic-work ledger
agrees to 5.69e-13 J.

At equal physical motion and timestep, the group probe passed at
cell widths 0.25, 0.125, 0.0625, 0.04167 and 0.03125 m. The
unmerged FacePrism control passed only at 0.25 m and requested
outgoing material from an empty initial cell at the other widths.
The three finest group impulses are −265.74, −265.26 and
−265.86 N·s. These measure a spatial refinement trend, not
agreement with an independent force oracle. The separate fixed-grid
time refinement has different limiting impulses in the grouped and
unmerged models, about −319 and −394 N·s respectively. This is
why the change is restricted to a probe. A suitable native runtime
scheme also needs the newly admitted material's actual inertia and
momentum, and stable transfer during mixed-phase changes.

## Reproduce

```sh
src=tools/prototypes/fluid_volume_grid
evidence=docs/evidence/fluid-moving-grid-20261103/cell-exposure
out=/tmp/judas-grid-exposure
bash "$src/qualify_moving_grid.sh" "$out"
for probe in piston-refine piston-space; do
    c++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror -I"$src" \
        "$evidence/$probe.cpp" "$out/MovingVolumeGrid.o" \
        "$out/HalfDualVolumeMomentum.o" -o "$out/$probe"
    "$out/$probe"
done
```

The previous 705 standalone check outputs and 85 native outputs
were reproduced byte for byte. The added geometry and pressure
tests contribute 43 passing checks. The preserved half-density
pool still needs the pressure and geometric transport changes
joined with moving rigid-body feedback. The original warmup
diagnostic and settling criteria are unchanged.

Both new suites also passed with AddressSanitizer and
UndefinedBehaviorSanitizer. Leak detection was disabled for those
runs because LeakSanitizer could not inspect this restricted process.
