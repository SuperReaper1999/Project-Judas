# Advanced Fluids settling follow-up

Experimental baseline `a6eeedbe0d7fae39d0dcbb57b5569a2e9853046b`.
The two original warmup diagnostics remain active. This note records causal
experiments. Advanced Fluids remain locked in Alpha.

The separate [stationary-boundary trace](FLUID_STATIC_SETTLING_FOLLOWUP.md)
explains why restoring the predecessor's stale contact rows would make the
half-density diagnostic green for the wrong physical reason. The [prescribed
wedge correction](FLUID_PRESCRIBED_WEDGE_PROJECTION.md) addresses a different
normal-response problem.

## Zero-gravity open basin

The unchanged fixture reproduced 727 checks with one strict warmup diagnostic
failure and zero mandatory failures. Six archived snapshots and all 240
physical trajectory rows matched the baseline, excluding elapsed time columns.

The authored lattice starts at rest, but its discrete Poly6 kernel sum at
the interior is `ρ̂/ρ₀ = 1.014801537` for support radius divided by spacing
`h/s = 2.1`. This positive numerical compression generates 2,034 J of
kinetic energy during the first pressure reconstruction, with nearly zero
net vertical momentum. The basin contacts then redirect the expansion upward.
They contribute 858.080 of 858.172 N·s of vertical fluid momentum by release.
204 of 1,152 particles have passed above the open rim and carry 150.919 of
163.772 J.

Release mean speed is 0.097213 m/s and bulk speed is 0.093118 m/s. The latter
already exceeds the 0.05 m/s gate. A momentum-preserving neighbour smoother
cannot bring the mean speed below that bulk speed. Its neighbour connectivity
remains intact in the last second. The local relative velocities are small
because the plume is moving coherently.

A scratch lid retained the original first-substep pressure kick but reduced
release mean speed to 0.012475 m/s by intercepting the plume. In a separate
scratch operator, using the analytical lattice normalization removed the
startup pressure error in both aligned and rotated rest blocks. It still
responded to an independently compressed block. The unchanged zero-gravity
fixture then passed all 727 checks at 4.71e-7 m/s. That operator also changed
wet classification materially and introduced a mandatory early immersion
failure in `half_025` (0.611921 versus the upper bound of 0.60).
The global normalization was rejected as a production change.

The historical open-basin diagnostic is retained. A production pressure
correction must distinguish discrete rest-density bias from real compression
and requalify wet support and hydrostatics across resolutions and frames.

[Measured phases and snapshot verification](evidence/fluid-zero-g-followup-20261103/).
Raw snapshots, commands, instrumentation, failed candidates and hashes are in
`.cache/fluid-zero-g-followup-20261103/` on the development machine. Reproduce
the unchanged production case with `judas_production_fluid_tests --case
zero_gravity --strict-diagnostics --output <new-output>`.
