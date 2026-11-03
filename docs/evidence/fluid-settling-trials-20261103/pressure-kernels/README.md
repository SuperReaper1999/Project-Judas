# Rejected pressure-kernel candidates

Engine baseline `2fccc514bb7bd2e384c4bf726393c273d0b44beb`. Only a scratch
`FluidWorld.cpp` replacement object was linked ahead of the exact current
native Release engine archive. Production and player test sources, settings,
thresholds, rigid collisions, particle contact, Alpha policy and tracked files
were unchanged.

The legacy unit-integral Poly6 pressure density overestimates a nominal
Cartesian lattice at the default support-to-spacing ratio 2.1 by 1.48015%.
For normalized compact radial quartic pressure density,

    W(r) = (3465 / (512πh³)) (1 − r²/h²)^4, 0 ≤ r < h
    W(r) = 0, r ≥ h

the error is 0.0044266%. The constant follows from integrating the kernel
over its spherical support. The initial successful scratch variant uses
the existing Spiky PBF projection direction and independent Poly6 velocity
smoothing. The clean patch reports pressure-kernel density in the diagnostics.
It does not set an arbitrary startup exception, gravity mode, threshold or
extra damping.

## Results

- At 2.1 support ratio, the 125-particle, .2 m aligned rest block's first
  kinetic energy falls from 34.90366 J to 0.00004434 J. Rotated result is
  0.00004502 J. With 5% linear compression, the candidate still releases
  4,443 J; aligned bulk speed is 1.95e−8 m/s. Similar rest improvements
  are measured at .05, .25 and .65 m. At ratio 2.5 the new kernel introduces
  a small rest kick absent in Poly6, so this is not a universal correction.
- The unchanged zero-gravity rigid-pool release improves from 0.097213 to
  0.000255231 m/s and the .25 m aligned half-density pool improves from
  0.0868477 to 0.0361876 m/s. Both pass strict, as does the rotated .2 m
  half-density test at 0.0396818 m/s. Full Production strict has 13,801
  checks, zero mandatory failures and one strict diagnostic
  (`small_mass_125x` warmup at 0.0626414 m/s). The full production run
  used the variant retaining the old final diagnostic density only.
- Full Player strict on exact HEAD has 5,661 checks, zero mandatory and
  four diagnostic failures. The clean quartic variant has seven mandatory
  and seven diagnostic failures. In the .2 m, 2,500-particle, 5 m-deep
  pool, bulk at release worsens from 0.00850171 to 0.0525769 m/s; the
  gate is 0.05 m/s. Last-second vertical bulk standard deviation grows
  from 0.00464 to 0.15314 m/s. This is an observed persistent pressure
  oscillation and not just an isolated sampled endpoint.
- Both zero-gravity swim timed-jump checks also fail in quartic. The first
  fluid update is 1/60 s, then updates are 1/30 s. In a separate direct
  footprint control with these intervals, the HEAD .2 m cloud expands
  from z≥−.9 m to z≥−2.9003 m by the jump, and the .25 m cloud expands
  from z≥−.875 m to z≥−3.0130 m. Quartic leaves both near their original
  footprints (z≥−.9041 / −.8787 m); the actual controller jump occurs at
  z≈−2.495 m and receives zero swim acceleration. The unchanged jump
  expectations rely on water still being present at their authored time.
- Using the exact derivative of quartic rather than Spiky gives five
  mandatory half-density failures. Using the exact Poly6 derivative with
  a lattice-calibrated target passes zero gravity and common freefall
  but gives four mandatory aligned half-density failures and five
  rotated half-density failures. The latter preserves the gradient of
  the pressure density but changes the response of the legacy approximate
  diagonal constraint solve.
- Direct 1,200-particle, 80 independent first-step samples for rest and 5%
  compression yield median 7.65→7.93 and 8.18→8.25 ms, respectively.
  These runs overlapped other validation and are diagnostic only.

## Reproduction

From the repo root, with current native Release `.cache/2d-stage1-build`
configured and the two existing test targets built:

1. Create ignored `.cache/fluid-zero-settling-kernel/clean`. Copy the
   included `base.cpp`, `probe.py` and `block-control.cpp` to its parent, `build.py` to
   `clean/`, and copy the known native engine archive to `clean/engine.a`.
2. Run `python3 .cache/fluid-zero-settling-kernel/probe.py` for the
   focused cases and gradient controls. Then run
   `.cache/fluid-zero-settling-kernel/probe --strict-diagnostics --output <fresh>`
   for the full production comparisons.
3. Run `python3 .cache/fluid-zero-settling-kernel/clean/build.py player` to
   generate the clean replacement object and relink the Player and Production
   executables. Run each with `--strict-diagnostics --output <fresh>`.
4. To reconstruct the baseline Player, import `build.py` from the clean
   directory and call
   `link('judas_player_fluid_integration_tests','player-baseline',work/'player.o')`.
   To build either direct control, use `compile(source, 'judas_production_fluid_tests', obj)`,
   then `link('judas_production_fluid_tests', name, obj)` for the baseline
   or provide `work/'candidate.o'` as the final parameter for quartic.
5. The analytic Poly6 control is generated by the separate `exact/probe.py`
   in its encoded scratch directory. Its result files retain the exact
   production failures without touching accepted evidence.

The archive of trajectories contains only generated CSVs, unchanged serialized
fixtures and JSON measurements. Compiler flags and dependency locations come
from the existing CMake build; binary and input identities are recorded
separately. These prototypes are rejected and must not be integrated based
on the rigid-pool result alone.
