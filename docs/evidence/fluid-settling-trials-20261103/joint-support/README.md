# Joint pressure and support experiments

Source baseline `2fccc514bb7bd2e384c4bf726393c273d0b44beb`. This is a scratch
investigation. It modifies no fixture, shipped code or release policy.
The unchanged production tests use ordinary scene creation, stepping and
release. The optional `--warmup` driver stops directly after the original
warmup check, with its original 240 steps and 0.05 m/s threshold.

## Independent two-particle column

In one spatial dimension, calibrate each of two identical particle masses as
`1000/(W(0)+W(.05))` for the authored Poly6 kernel. Place the lower particle
on the floor's 18 mm preferred offset, and the other 50 mm above it. With
gravity 9.81 m/s² and the reported time step, exact quiescent support requires
zero velocity and (2mg\Delta t) wall impulse. No rigid callbacks, hidden
settling, moving geometry or free-surface estimation enter this control.

At step 600 the baseline's upper position is .067517221 m and does not change
over the printed final steps, but its stored speed is .0164774 m/s downward.
Support is .165230706 N·s versus .165230811 N·s analytically. This isolates
persistent inconsistency between published displacement and velocity despite
correct aggregate support. The position update is pressure-projected after
preferred geometry alters the trial spacing. The velocity update excludes
geometric displacement and resolves wall contacts only on the individual
supported particle. Position-only joint tangents reduce the equilibrium
spacing error but retain .0163624 m/s spurious downward velocity. Joint
velocity projection reduces that speed to .00392346 m/s and brings the
position within 15 nm of its calibrated height, but remains inexact. This
witness establishes a defect in the split update. It does not establish
that all pool motion comes from the same source.

## Original fixture results

| Operator | Warmup mean speed | Mandatory half-density / neutral / dense failures |
| --- | ---: | --- |
| Baseline | 0.0868477 m/s | 0 / 0 / 0 |
| Exact Poly6 Jacobian, original Jacobi structure | 0.108138 m/s | Not run |
| Joint velocity, 2 sweeps | 0.0525197 m/s | 4 / 1 / 0 |
| Joint velocity, 8 sweeps | 0.0533939 m/s | Not run |
| Joint tangent position, 2 sweeps | 0.0511198 m/s | 3 / 1 / 0 |
| Joint tangent position, 4 sweeps | 0.0633328 m/s | Not run |
| Joint position plus velocity, 2 sweeps each | 0.042237 m/s | 4 / 1 / 0 |
| Joint velocity with one-sided support mobility | 0.0668626 m/s | Not run |

Each new density rate uses the exact Poly6 Jacobian, inverse masses and
central, equal-and-opposite pair impulses. It corrects approach toward
compression, releasing multipliers where applicable. Joint variants add the
finite-feature contact rows already admitted by the engine. Their endpoint
supports are modeled as prescribed. Zero wall-point velocity alone does
not prove the body is prescribed, so the scratch joint velocity path
cannot ship without a coherent native moving-body ownership interface.
Velocity and position changes affect particle packing and the resulting
independent immersion and float-decay measurements.

The combined variant passes the original pool quiescence diagnostic but
introduces mandatory early and late immersion, late RMS speed decay and
late travel-decay failures in `half_025`, and a mandatory neutral drift
failure. Its unchanged zero-gravity warmup reaches .102072 m/s and fails
the original 0.05 m/s diagnostic. The corrected-header baseline and all
three primary candidates passed the existing direct FluidWorld test
binary. That direct coverage does not override the authored-scene failures.
These prototypes are rejected for production.

## Reproduction

Copy these files to an ignored `.cache/<experiment>` directory. From the
checkout root, use a Release GNU make build at `.cache/2d-stage1-build` with
`judas_production_fluid_tests` and `judas_fluid_world_tests` built. The
scripts read compiler and link flags from that build but write only into
their own directory. The original experiment relinked current-source
FluidWorld and test objects against the existing engine archive. The
relinked baseline reproduces the original 0.0868477 m/s warmup. Recompile
test objects when relinking because older headers need not share a binary
layout with the current FluidWorld.

```sh
python3 .cache/<experiment>/reproduce.py baseline --warmup
python3 .cache/<experiment>/reproduce.py joint-2 half_025 neutral_025 dense_025
python3 .cache/<experiment>/reproduce.py position-2 half_025 neutral_025 dense_025
python3 .cache/<experiment>/reproduce.py both-2 half_025 neutral_025 dense_025 zero_gravity
python3 .cache/<experiment>/column.py
python3 .cache/<experiment>/fluid-world.py
```

Original logs, candidate source, and column data remain in
`.cache/fluid-static-settle-20261103`. Raw hashes identify the host copies.
