# Moving volume, momentum and native ownership

The accepted code here is isolated in `tools/prototypes/fluid_volume_grid`.
Parent source is `40607cd1d23bfc827f6b9225ba364bc759c708db`. There
are no engine routing changes. Linux 7.0.0-42-generic, GCC 15.2.0,
Ryzen 5 5600. Raw outputs were collected 2026-11-03 UTC.
The native archive used in both runners has SHA-256
`170a5306ef4a8187555452fca01aa804366c8fa8a380ef3856fba32a8af36823`.

Reproduce the 705 standalone checks, 28 player checks, 57 rigid checks,
and 26 existing fluid policy checks from the repository root.

```sh
src=tools/prototypes/fluid_volume_grid
build=.cache/2d-stage1-build
out=/tmp/judas-grid-ownership
bash "$src/qualify_moving_grid.sh" "$out"
python3 "$src/run_grid_native_player_tests.py" "$build"
python3 "$src/run_grid_native_mk2_tests.py" "$build" "$out/native"
JUDAS_ENGINE_ROOT="$PWD" "$build/judas_project_fluid_policy_tests"
```

The native Make build tree must have the existing player fluid and
production fluid test targets built. The player and rigid probes relink
their isolated adapters against that native engine. The original
production and player fluid suites still exercise the PBF implementation.
They do not qualify this backend.

## Original release conditions

`settling-controls/` preserves the original warmup, a matched interval
refinement, an equal-volume flat equilibrium, and the original authored
water with gravity disabled. To rebuild the recorded driver, add
`-I"$src"` to its compiler command and use the standalone runner's
`"$out/MovingVolumeGrid.o"` and `"$out/HalfDualVolumeMomentum.o"`
objects. The two resting controls stay exactly still. The authored
omission releases roughly 9.62 kJ of potential energy. At four seconds,
the water still moves at 0.13231 m/s and the refined result is
0.13140 m/s. The original 0.05 m/s diagnostic is preserved. The
cell-speed observation is volume weighted and the grid air domain
is sealed, so the comparison is diagnostic across representations.

The separate release probe starts from that actual evolved state and
applies coupled pressure to the free box with no imposed trajectory.
Run it at each stated cadence. Both executions are expected to exit 3.

```sh
evidence=docs/evidence/fluid-moving-grid-20261103/ownership
c++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror -I"$src" -Itests \
    "$evidence/half-release.cpp" "$out/MovingVolumeGrid.o" \
    "$out/MovingVolumeMomentum.o" "$out/HalfDualVolumeMomentum.o" \
    "$out/CoupledVolumeBox.o" -o "$out/half-release"
"$out/half-release" 60
"$out/half-release" 30
```

At 60 Hz the last accepted body centre is 0.7852244 m with
vertical speed 0.8455715 m/s. The next step crosses the 0.5 m
grid plane with its bottom face. The 30 Hz arm rejects the same
transition. The release probe's exterior observation is a single
grid-based estimate. It does not substitute for independent immersion
measurement or complete the original settling gate.

## Rejected topology experiments

The original traced build is reproduced by applying
`rejected-opening/CoupledVolumeBox-trace.patch` to a disposable copy of
`CoupledVolumeBox.cpp` and compiling that instead of the corresponding
object. Its trajectory matches the original release log through the
failure. A newly opened face can have zero starting mass on both sides.
Pressure cannot divide by that inertia.

The first scratch variant excludes those zero-donor faces from
projection and splits proven axial inflow between newly exposed
half-cells. It still rejects at the same transition. For example,
cell 307 starts with zero free volume, receives enough flow to open
1.58907e-5 m³, and is simultaneously asked to donate 6.05e-10 m³.
That outgoing material has no old owner. Nine levels of subdivision
also fail with the same cause.

A separate active-set scratch solve admits only inbound transfers
at initially empty cells. It can admit individual trial geometries
past the topology event, but the coupled residual jumps around the
event and the full step still rejects. Adaptive subdivision plus
the constrained solve crosses the first event. At 0.2 seconds the
next topology change exhausts a half-dual donor. Neither experiment
satisfies the original release contract. The present evidence points
to missing time and spatial ownership of the material that fills an
opening cut cell. Removing zero-mass links, relaxing tolerances or
choosing smaller steps alone do not solve the full problem.

The fixed-path piston oracle identifies a specific spatial ownership
error independently of the body and pressure solvers. It opens
0.0025 m³ below a midpoint that remains completely solid. The
existing equal-half ledger assigns 0.00125 m³ to the still-solid
upper half and requires the same impossible midpoint transfer if
all new material enters from below. It occurs at any positive
displacement smaller than half a cell. A geometric ledger must use
actual endpoint half capacities, split each transverse transfer
across real openings, and derive midpoint flux from that half's
remaining balance. A blocked midpoint must then have zero flux.

```sh
c++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror -I"$src" \
    "$evidence/piston-birth.cpp" -o "$out/piston-birth"
"$out/piston-birth"
```

For reproduction, apply each patch to its indicated base file from
this checkpoint in a temporary directory, using
`patch -o DEST ORIGINAL PATCH`. The three MovingVolumeGrid variants
are independent patches against the same original. Replace the
corresponding objects in the release command with the resulting
source files, linking the original `MovingVolumeMomentum.o`.
For adaptive runs also apply `half-release-adapt.patch` to the
evidence driver. The donor diagnostic arm uses
`MovingVolumeGrid-trace.patch`, and the constrained arm uses
`MovingVolumeGrid-active.patch`. The scratch patches are historical
negative controls and are never compiled by the qualification runner.

## Cost

`bench-coupled.cpp` uses the same link command as the release probe.
In 60 samples per arm, mean/p95/worst were 1.21/1.29/2.47 ms at
392 cells and 18.15/19.65/36.55 ms at 3,630 cells, with about 5.13
pressure solves per step. It measures a fully filled sealed grid,
one free box and no runtime rendering or collision. These results
establish a scaling concern and a benchmark for future changes.
The [oriented geometry results](../oriented-geometry/README.md) are
separate timings and cannot be added directly to those figures.
