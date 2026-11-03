# Shared cut-cell water and finite-air pressure

This Linux prototype keeps the pressure operator, phase apertures, moving-wall
volume source, gas mass transport and dynamic wall reaction on one cut chart.
It lives outside the runtime. The release lock, Basic Water and the original
production tests were untouched. Parent source is
`f41545df297e61f6a7f6ececebf0e844e52a5527`. The local run used GCC 15.2,
x86-64 Linux 7.0.0-42 and Ryzen 5 5600.

## Numerical contract

Water is incompressible. Each gas cut keeps finite mass and linearized
compressibility about its initial volume, using a reference density and sound
speed. A midpoint pressure head difference drives each connected phase port.
With phase density `rho`, common head scale `P = rho_w*g`, face area `A`,
port length `L` and step `dt`, its signed volume transfer is
`dt*A*u0 + dt²*A*P*(hL-hR)/(2*rho*L)`. Gas transfer uses mass divided by
reference density. An exterior port has zero gauge pressure and finite
conductance.

An interior water surface couples its water and gas nodes with conductance
`2*A_interface`. The linear gas capacitance is `P*Vgas/(rho_air*c²)`;
the midpoint row uses twice that value. The volume source for each phase
is its start volume less its end capacity at the frozen initial interface.
The compact graph uses a binary64 SPD solve with IC(0) preconditioning.
Floating components require compatible sources before pinning, and all
original rows are audited after the solve.

The step measures both phase apertures and wall patches on the same moving
geometry. Wall reaction uses the pressure work transpose. The solve
independently recomputes signed port fluxes, material ledgers and work.
End storage is compared with actual geometry and all outputs remain
transactional on rejection. The wall load here is the dynamic gauge
contribution. Equilibrium hydrostatics, general water advection, phase
topology changes and native body integration are separate requirements.

## Reproduction

From the Judas checkout root, with Bash and a C++17 compiler:

```bash
s=tools/prototypes/fluid_volume_grid
b=docs/evidence/fluid-moving-grid-20261103/shared-cut-regions
e=docs/evidence/fluid-moving-grid-20261103/hydroacoustic-coupling
out=$(mktemp -d /tmp/judas-hydroacoustic.XXXXXX)
bash "$s/qualify_moving_grid.sh" "$out"
c++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror -I"$s" -I"$b" \
  "$e/AuthoredHydroacousticProbe.cpp" \
  "$out"/{CutHydroacousticGraph,CutHydroacousticPressure,CutHydroacousticStep,CutRegionGasTopology,CutRegionGeometry}.o \
  -o "$out/authored"
"$out/authored"
sha256sum -c "$e/SOURCE_SHA256SUMS.txt"
(cd "$e" && sha256sum -c SHA256SUMS.txt)
```

The authored executable exits zero when both expected rejections leave
published state intact. It is a negative control. The separate project
policy suite used `JUDAS_ENGINE_ROOT="$PWD"` and the configured Alpha-locked
Linux build's `judas_project_fluid_policy_tests`. The 49 hydroacoustic checks
were also compiled with `-O1 -g -fno-omit-frame-pointer
-fsanitize=address,undefined` and run with ASan leak detection disabled
under the runner. Trailing whitespace is normalized in the two combined logs.

## Results and limits

- All 1,030 checks across 26 suites passed, including 49 new
  pressure/geometry checks and 14 new graph checks. See
  `suite-counts.tsv` for the full moving-grid run. The 26 project policy
  checks passed and verified the release lock rejects attempts to enable
  Advanced Fluids.
- Actual translating-wall controls in two chart orientations recover
  `-5.47487367 N` on the gas-only wall and `-16.94493572 N` on the mixed
  wall along the tested axis. Continued wall-work residuals stay within
  `1.1e-18 J`. These are manufactured cases with known prescribed motion.
- Both unchanged authored basin inputs, 9.25 and 9.21600041198731 m³,
  reach a missing shared-face storage path before their first step.
  The broader opposed-trace inventory found 428 and 1083 faces,
  respectively. Three rotated apertures were smaller than a neighboring
  rectangular section. These counts also include traces near a cut seam.
  The archived strict pure-phase inventory found 320 and 29 exact
  coincidences with water below air. The remaining 108 and 1,054
  opposed faces have water above air. Their gravitational storage has
  negative curvature, so treating them as stable hydrostatic columns
  would change the model. Original-basin native settling and swimming
  remain open.
- `shared-interface-controls.tar.gz` validates signed storage across a
  full-section port in 22 independent checks. Its ledge control shows
  that equal pressure head applied to a larger receiving section can
  double water volume, while preserving volume with a whole-cell plane
  can wet a dry shelf. Shared boundaries need footprint-aware storage
  and a conservative phase/momentum transition.
- `native-pressure-pair.tar.gz` tests actual MK2 with a manufactured
  two-chamber reduction of 72 cut regions. Both media pass four
  substeps. Exact wet reaction rejects at step seven of the longer test
  when two adjacent binary32 states map to each other. A separate
  bounded scalar representability control reruns the fluid at the
  chosen native endpoint and accounts for an impulse difference of
  `1.88572e-8 N·s`, within the derived `1.91928e-8 N·s` bound. Both
  phases pass 32 substeps with that control. This is neither the
  authored basin nor a general angular/contact coupling. The packet
  also preserves prescribed full-adapter results and failures.
- `sparse-graph-controls.tar.gz` compares the same manufactured pulses
  on the original rotated geometry at matched original-row residual
  tolerance. The four individual compact solves took 62–92 ms
  versus 162–236 ms for the previous graph. A core replay reaches
  physical bounds instead of timing out, then rejects negative local
  water volume in near-empty cuts. These are diagnostic singles,
  before phase events, not runtime frame times.
- `endpoint-bounds-negative.tar.gz` tests an endpoint-only bounded
  pressure solve on four manufactured pulses on that same geometry.
  It closes its rows within `9.61e-20 m³`. The independent donor and
  gas-mass audit still rejects every case. In the worst pulse, a gas
  donor overspends by `2.49e-8 m³`, and three cuts end with positive
  gas mass but zero gas volume. Endpoint bounds alone cannot qualify
  the phase transfer.
- `nonlinear-finite-air-negative.tar.gz` preserves the fifth original
  level interval where a separate nonlinear finite-gas solve
  converges on frozen openings. Its own water motion changes the
  aggregate opening conductance by 7.50×. The updated solve stalls at
  a water row residual of 1.32e-10 m³. It publishes no fifth step.
  Neither frozen apertures nor an instant atmospheric pressure
  merge qualifies as a repair for this interval.
