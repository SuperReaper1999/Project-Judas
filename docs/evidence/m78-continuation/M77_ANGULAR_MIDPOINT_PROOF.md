# M77 angular drift: independent mathematical and application proof

Scope: the unchanged M77 timestep, public impulse, authored rig and acceptance
criteria. No PhysicalBalance/controller change was made. All failing isolated
variants remain preserved. This is a numerical mechanism investigation, not
an adjustment of control gains or of a success threshold.

## Governing torque-free equation

Let `R0` map body coordinates to world coordinates, `B = I_body^-1`, and
`m0 = I_body R0^T omega0` be initial body angular momentum. Between external
torque/contact/control impulses the body Euler equation is

```text
m' = m × (B m)
L_world = R m
E = 1/2 m^T B m
```

The explicit candidate predicts a world angular generator from initial
angular acceleration and uses the existing normalized quaternion drift.
It then transports world momentum into the new inertia orientation.
Transporting momentum preserves `L_world`, but does not itself preserve `E`.
The independently measured same-input full-horizon errors below demonstrate
this mismatch without a controller or a contact acceptance predicate.

## Midpoint/Cayley identity

Solve for midpoint momentum `u` at anchored duration `h`:

```text
F(u) = u - m0 - (h/2) [u × (B u)] = 0
m1 = 2u - m0
omega_mid_body = B u
omega_generator_world = R0 omega_mid_body
```

The Newton Jacobian acting on any vector `v` is

```text
J v = v - (h/2) [v × (B u) + u × (B v)]
```

Let `W v = omega_mid_body × v`. Normalizing the quaternion
`(1, h omega_mid_body/2)` produces the Cayley rotation

```text
Q = (Id + h W/2) (Id - h W/2)^-1
R1 = R0 Q
m1 = Q^T m0
```

The last equality follows directly from
`m1 - m0 = h [u × omega_mid_body] = -h W (m1+m0)/2`.
Therefore `R1 m1 = R0 m0`: world angular momentum is unchanged.
Because `B` is symmetric,

```text
E1 - E0 = (m1-m0)^T B (m1+m0)/2
        = h [u × (B u)] · (B u) = 0
|m1|^2 - |m0|^2 = 2 (m1-m0) · u = 0
```

These are algebraic identities, not controller-dependent fitting. Floating
point quaternion/velocity storage leaves rounding error. The positive-definite
ordinary inertia path uses a bounded 12-iteration Newton solve; nonconvergence is
reported explicitly, without falling back to the unconserved predictor.
Singular/locked-axis inertia retains its separately documented existing path.

## Same inputs, independent scalar arithmetic

`probes/check_midpoint_algebra.py` uses scalar Python arithmetic only, with
actual logged box inertia, orientation, angular velocity and duration. It does
not call Judas or PhysicalBalance. `midpoint-independent-algebra.json` retains
the full selected source records and results.

| Actual state | Explicit full-horizon ΔE | Implicit full-horizon ΔE |
|---|---:|---:|
| failing trajectory, step 807 / body 10 | −0.000166499 J | 0 J at double precision |
| pre-failure trajectory, step 558 / body 11 | +0.0000115692 J | +4.44e−16 J |
| passing trajectory, step 561 / body 8 | +0.00000287829 J | +8.88e−16 J |

The normalized midpoint residual is between `7.24e−17` and `3.72e−15` in
these cases. Independently reconstructed Cayley momentum agrees with
`2u-m0` within `1.86e−16` to `7.43e−15` relative norm.

## Actual-application observers

Both observers run the unchanged compiled `BalanceLabTests.cpp` object,
ordinary project copy, logical `J` action, 29 N s impulse and 60 Hz cadence.
Original source/archive/object/executable/project hashes are in the adjacent
`*-balance-app.json` and `*-observer-build.json` receipts.

`measure_anchored_energy.py` observes the proposed full anchored horizon.
`measure_consumed_energy.py` observes completed motion segments after the
actual TOI schedule has truncated any affected segment. It reads each actual
segment endpoint and retains only aggregate statistics and its worst 32
segments. It adds no impulses, mutation or solver feedback.

Both observed application traces are byte-for-byte equal to the respective
uninstrumented trace; the summary JSON files record both hashes. The explicit
archive still fails 15 / 1 and the implicit archive passes 15 / 0. Instrumented
wall-clock times are not performance measurements.

### Full proposed horizon, fixed world momentum

| Method | Segments | Largest absolute ΔE | Largest relative ΔE |
|---|---:|---:|---:|
| explicit + right limit + observations | 8,939 | 2.21805e−4 J | 7.51446e−5 |
| implicit + right limit + observations | 8,680 | 1.66232e−8 J | 1.94954e−7 |

### Actually consumed, including shortened TOI horizons

| Method | Actual TOIs | Largest absolute ΔE | Largest relative ΔE | Sum of local absolute ΔE |
|---|---:|---:|---:|---:|
| explicit + right limit + observations | 83 | 0.0119482 J | 0.00539342 | 0.0325073 J |
| implicit + right limit + observations | 37 | 7.13776e−5 J | 0.000159848 | 0.000288658 J |

Each application executes 1,248 physics steps. These are different resulting
trajectories, not equivalent workloads for a speed claim. Summed local errors
are not net articulated energy conservation: real contact/control/external
impulses occur between the separate segments.

The largest explicit consumed error is during the eventual fall, after the
controller failure. A real earlier local error also exists at step 558 / trace
frame 33: −3.94741e−4 J; another occurs at step 762 / frame 237:
−4.51643e−4 J, before the unchanged failure at frame 247. This distinguishes
pre-failure numerical error from merely reporting the later crash trajectory.

## Honest dense-output limitation

The implicit algebraic invariants apply at the anchor's full horizon. CCD
observes the existing anchored curve before that horizon; an actual TOI can
truncate it. The constant anchored generator is not recomputed at each
observation, so shortened samples are not exactly energy preserving.

At the **same** failed step-807 state, shortened-horizon errors are
−0.0119481 J explicit and −0.0118124 J implicit. At the **same** pre-failure
step-558 state they are −3.94729e−4 J and −3.95645e−4 J. Thus the much lower
aggregate consumed error on the passing trajectory does not prove direct
elimination of dense interpolation error. It reflects the different resulting
trajectory and the invariant full-horizon integration correction. No claim
that every dense TOI sample conserves energy is made.

The retained generic change corrects the independently proven full-horizon
Euler-top energy defect while preserving world momentum and passing both the
unchanged Linux C++ and actual-application cases. It does not remove arbitrary
gravity/contact/constraint dynamics, replace the controller, or retune M77.

## Final combined checks available at this report

- Linux C++ unchanged controller: **119 checks / 0 failures**.
- Linux actual authored GL/JS corrective-step application: **15 / 0**.
- Linux rigid motion: **10,239 passing checks**, including independent free-flight
  energy and orientation convergence.
- Linux contact lifetime/completed observations: **1,076 / 0**, preserving the
  original FTFT4A fixtures and adding bounded actual-TOI lifetime coverage.

See `midpoint-final-controller.log`, `midpoint-final-application-step.log`,
`midpoint-final-motion.log` and `midpoint-final-contact_lifecycle.log`.
The parent continuation report supplies final affected Linux/Windows checks,
performance and final-source identities; isolated observers are not a
substitute for those final combined checks.
