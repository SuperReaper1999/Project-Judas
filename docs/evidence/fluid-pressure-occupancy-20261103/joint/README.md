# Pressure transport and geometry closure experiments

Baseline `103f4bf5`. Scratch source only. The Alpha lock and all test
conditions are unchanged. The stand-alone test override links the
candidate FluidWorld against the existing GNU Release archive and rebuilt
test objects. These are rejected investigations, not implementation.

## Pre-advection pressure plus wall solve

Write each predicted particle velocity as mass-normalized momentum
coordinates. The objective is minimum change from the gravity and
existing XSPH predictor. Density constraints use the exact derivative
of Poly6 at the starting positions and the inequality
`J v <= (1 - rho / rho0) / dt`. Prescribed support halfspaces use
finite box-contact witnesses and the available normal gap. Cyclic
projected coordinate minimization solves the dual. Each move admits or
releases a pressure or wall multiplier, and the result drives advection.
The existing hard-contact stage still certifies positions and processes
physical velocity contacts. The prototype does not implement finite rigid
mobility. Identical endpoints alone are insufficient to infer runtime
ownership. A deliberately conservative static-scene dispatch uses original
endpoints and no resolved motion ledger. Sampled poses alone misclassify
some static boxes due to interpolation roundoff.

The exact two-particle column exposes the baseline with stationary
positions and downward published speed at the upper particle.
Pre-advection coupling reduces the latter from 0.0164774 to
1.049e-7 m/s at step 600, restores the top height within 0.16 micrometre
and delivers the expected 0.165230811 N s of floor impulse.

On the .025 production pool the static single-predictor-geometry version
settles at 0.0652595 m/s, with 331/360 projection calls at the diagnostic
64-sweep work bound. Correcting the lattice bias brings the zero-gravity
warmup to 3.39e-7 m/s and leaves the ordinary pool at 0.0655516 m/s. Both
versions introduce a mandatory late immersion mismatch.

Pressure movement itself exposes many finite wall faces absent from the
gravity-only trial. The discovery prototype verifies the pressure-driven
path, adds new finite box rows and repeats the same solve. It observes
48,408 additional face admissions by call 720, reduces the original
.025 pool warmup to 0.0317279 m/s and passes the unchanged 0.05 m/s
check. It fails mandatory early half-density immersion and neutral drift.
The neutral body settles at 0.30116 m instead of remaining within 0.20 m
of the 0.7 m release position. Dense sinking passes. The old half-density
legacy comparison also fails. Geometry discovery adds substantial work
and leaves some residual. The unchanged .20 deep-player smoke passes warmup at 0.0213682 m/s but
fails starting immersion and neutral drift. The player moves downward
0.60444 m in the release interval. The reduction in visible noise does
not justify replacing the production operator.

## Smaller isolated smoothing mechanism

At step 600 in the known-answer column, reconstructed bottom/top velocities
are -0.34347713 and -1.04308e-7 m/s. XSPH converts them to -0.32699981
and -0.01647741 before the wall projects only the bottom particle to
zero. This directly isolates exchange of the bottom particle's impossible
inward floor velocity as the source of the upper particle's erroneous
settled motion.

The isolated variant samples support-feasible particle velocities for the
smoothing exchanges but applies those pair corrections to the original
predictor. It publishes no provisional wall impulse. The final ordinary
contact solve still owns actual impulse. It removes the column speed
(to approximately 1.135e-7 m/s at step 600) without removing all spacing
error (approximately 0.425 mm). On unchanged .025 it worsens warmup to
0.102062 m/s and fails early immersion. On the unchanged .20 deep-player
smoke it fails strict warmup at 0.0782709 m/s with zero mandatory failures.
It passes the ordinary .20 half-density box fixture. This is a real
local mechanism with insufficient evidence for a production patch.

## Reproduce

Run from the pinned checkout root with the Release GNU make directory
`.cache/2d-stage1-build` holding built production and direct fluid targets.
Copy the files to an ignored scratch directory first. The Python drivers
read compiler/link flags from the existing build and write their own
overrides and outputs. Use fresh case/output folders to preserve old
evidence. Optional `--warmup` exits immediately after the original
warmup diagnostic. All full fixtures are unmodified.

```sh
python3 .cache/<experiment>/probe.py implicit --warmup half_025
python3 .cache/<experiment>/probe.py owned half_025
python3 .cache/<experiment>/probe.py normalized --warmup zero_gravity half_025
python3 .cache/<experiment>/probe.py discover half_025 neutral_025 dense_025
python3 .cache/<experiment>/probe.py xsph half_025 half_020
python3 .cache/<experiment>/probe.py trace --warmup nonexistent
python3 .cache/<experiment>/probe.py xsphtrace --warmup nonexistent
python3 .cache/<experiment>/column.py
python3 .cache/<experiment>/column-traces.py
python3 .cache/<experiment>/player.py xsph
python3 .cache/<experiment>/player.py discover
```

The three unified diffs describe source deltas. The Python generators
preserve the additional variations and phase-only instrumentation. The
original logs and artifacts are in the parent scratch folder.
