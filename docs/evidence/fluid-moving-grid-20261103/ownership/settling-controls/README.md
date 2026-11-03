# Original coarse-pool warmup diagnostic controls

This disposable runner reproduces the original half_025 grid proxy and
changes one physical condition or the numerical timestep in each arm.
The production tests and solver were left untouched. Snapshot headers
and optimized Linux Release objects were copied from the repository
prototype and ../fluid-moving-grid-20261103/final2 at the time of this
experiment. The original arm matches the earlier original-half-energy.log
through release for every physical observable shown there.

Build and run in this directory:

    c++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror run.cpp MovingVolumeGrid.o HalfDualVolumeMomentum.o -o run
    for mode in original half-step flat zero-g; do ./run "$mode" > "$mode.log"; done

The authored omission mask leaves 592 parcels (9.25 m³) and reserves 48
parcel cells around the 0.216 m³ stationary solid. The grid proxy treats
all six outer boundaries as sealed. It is a close basin/clock proxy,
with exact cut-box support, but does not reproduce the PBF skin or
the original side walls' sub-mm seams. The flat control redistributes
the same absolute water volume to its analytic hydrostatic level,
2.36650000644 m. This control is separate from the authored fixture.
The zero-g control preserves the exact authored water volumes. Its
energy columns retain the standard-gravity reference for comparison,
even though gravity is disabled during integration.

Columns in numeric rows are update, volume-weighted cell mean speed,
peak over all cells, water centre-height proxy, admitted substeps,
rejected attempts, elapsed milliseconds, volume difference, owned
kinetic energy, exact reconstructed-potential excess over the analytic
flat equilibrium, total excess, volume-weighted RMS speed, volume-weighted
vertical absolute speed, lower-layer mean speed and upper-layer mean
speed. The lower-layer aggregate covers cells 0–4 in chart y.

All arms end at the same elapsed warmup time. Both flat and no-gravity
arms retain exact zero velocities. Two explicit half-intervals per
original update change release mean speed by less than one percent.
That result is a timestep control only. Spatial convergence and exact
continuum dissipation are outside this reproduction. The proxy still
fails the unchanged production diagnostic's numerical target at its
release clock, with that comparison limited by the differing speed
observations and backend dissipation. No authoring change is proposed.
