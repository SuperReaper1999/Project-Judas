# Three fluid settling comparisons — cause review, 2026-11-01

**The original three assertions remain unchanged and failed. No added damping,
force clamp, threshold change, main promotion or fluid unlock.** The combined
height-sliced candidate remains uncommitted. This investigation establishes an
acceptance-contract problem; it does not certify the whole fluid system.

## Finding

The four-second endpoint-speed condition is not a consequence of the configured
buoyancy/drag model. It rejects a converged, dissipative calm-water solution and
also rejects an independent control using the actual MK2 integrator. Comparing
one instant of an oscillation can rank a more active trajectory as more settled.
The controls do not justify changing the engine simply to force these comparisons
green while claiming to preserve the same model and coefficients.

This is **not** evidence that every remaining fluid defect is a test problem.
The coarse pool has measurable late jitter, described below.

## Independent physical model

Use the fixtures' actual mass, dimensions, release position and independently
observed release water surface. A calm, stationary reservoir and an axis-aligned
box deliberately remove PBF, field reconstruction, changing surface geometry and
rotation. This is an independent control, not a replacement simulation of the
entire original scene.

For box half-height h, volume V, mass m, water density rho and surface H:

    f(y) = clamp((H - y + h) / (2h), 0, 1)
    dy/dt = v
    m dv/dt = -mg + rho*g*V*f(y) - m*k*f(y)*v
    k = 2 s^-1  (the unchanged configured drag rate)

At half-density equilibrium, f=1/2. The local amplitude-decay rate is k/4 =
0.5 s^-1, giving a two-second amplitude time constant. The natural frequency
also depends on box height: omega_0^2 = g/h. The bodies start deeply submerged,
not at their floating equilibria. After four seconds, their velocity depends on
where they are in the resulting bobbing cycle; it need not be below 0.5 m/s.

A binary64 RK4 reference was refined through 960, 3,840, 15,360 and 61,440 steps
across the same four seconds. At the finest level, energy plus integrated drag
loss closes within 1.17e-6 J (initial energies 756–1,000 J). The last two velocity
results differ by less than 3.3e-8 m/s. The continuous drag term is dissipative:

    d(K + U)/dt = -m*k*f(y)*v^2 <= 0

The native control applies the same analytical buoyancy and existing exponential
drag kick through public PhysicsWorld APIs. It keeps the original 1/60 timestep
for the first control, then refines it separately to establish convergence. Those
refinements do not change the timestep of any original production test.

| Case | Converged calm v(4s) | Actual MK2 calm control, 60 Hz | Full sliced-PBF candidate |
|---|---:|---:|---:|
| half_020 | 0.608487 m/s | 0.589536 m/s | 0.632400 m/s |
| half_025 | 0.594577 m/s | 0.586686 m/s | 0.611067 m/s |
| light_80kg | 0.597916 m/s | 0.602412 m/s | 0.679569 m/s |

**Every calm control exceeds the unchanged 0.5 m/s comparison.** Native response
converges toward the independent reference as its timestep is refined. This
rules out blaming these crossings solely on the new particle geometry or a
broken rigid-body integrator. It does not assert identical trajectories between
the calm control and a moving, noisy, three-dimensional PBF pool.

## Actual force-path tracing

Diagnostic copies of ProductionFluidCoupling record gravity-kicked velocity,
pose, angular velocity, submerged centroid, field velocity/acceleration,
predicted flow, buoyancy, torque, drag and drag work. No tracing hook was added
to the shipped production source. Accepted-baseline and candidate runs use the
unchanged test driver and original 240 release steps, 240 warm-up steps and dt.

All **1,440 physical rows** from six traced runs exactly match their uninstrumented
counterparts, excluding timing. The velocity change reconstructed from recorded
forces differs from the actual endpoint by at most **1.90e-7 m/s**. Gravity was
not omitted or counted twice. No unexplained force kick was observed.

For point inverse effective mass W, relative velocity u, drag fraction
alpha=1-exp(-k*f*dt), and impulse J=-alpha*W^-1*u, the intended relative-flow work is

    J.u + (1/2) J^T W J
      = -alpha*(1-alpha/2)*u^T W^-1 u <= 0.

There are **zero positive relative-drag-work steps** in both arms, including
all 12-second continuation runs. This rules out a sign error or hidden negative
damping in that drag impulse. It is not a proof of closed fluid/rigid energy
conservation: sampled moving water can do work, and exterior PBF coupling remains
an approximation with documented ownership restrictions.

## Why the old green result is misleading

For half_020, the accepted baseline ends at 0.113925 m/s and passes; the new
candidate ends at 0.632400 m/s and fails. Over the preceding second, however:

| Metric, 3–4 seconds | Accepted baseline | Candidate |
|---|---:|---:|
| RMS vertical speed | 0.739040 m/s | 0.594756 m/s |
| Maximum absolute vertical speed | 1.229667 m/s | 0.987394 m/s |
| Vertical travel range | 0.336007 m | 0.275026 m |

The passing endpoint therefore does not establish better settling. The revised
wet geometry changes force history and oscillation phase, which the single-frame
comparison mistakes for a simple stable/unstable classification.

## Longer runs and a genuine remaining limit

Continuation begins **after all original four-second assertions have executed**.
Those assertions still determine the exit status: candidate runs continue to
exit with failure, and baseline runs retain their original passes. All initial
240 non-timing rows match the corresponding four-second run exactly. The extra
eight seconds are observations, not delayed acceptance or a settling workaround.

| Candidate, 10–12 seconds | RMS vertical speed | Peak absolute speed | Mean sampled immersion |
|---|---:|---:|---:|
| half_020 | 0.105667 m/s | 0.297335 m/s | 0.498906 |
| half_025 | 0.301411 m/s | 0.562597 m/s | 0.528970 |
| light_80kg | 0.236224 m/s | 0.443885 m/s | 0.512433 |

The bodies remain afloat and motion generally decays; the coarse half_025 pool
still bobs through a **0.210586 m vertical range** in this late window. Its sampled
fluid acceleration has approximately 2.01 m/s^2 RMS in Y in that window. That is
an actual quality limitation to investigate, not something this oracle diagnosis
makes acceptable. The data do not yet establish a justified replacement for its
unsteady pressure/flow coupling. No acceleration term was zeroed or filtered.

## Corrections made and proposed contract review

Added `tests/FluidSettlingReferenceTests.cpp` and registered its normal CMake/
water-suite target. **69 assertions pass** across three physical cases, four
native timestep resolutions, reference work/convergence checks and six negative
controls with missing buoyancy or missing drag. These are independent safeguards;
they do not replace or turn green the three original failures.

No production-force correction was justified by the traces, so none was made.
In particular, no damping coefficient, fluid cadence, particle count, release
position, acceptance threshold or original production assertion was changed.

**Proposal for operator review, not implemented:**

1. Keep the original four-second results as preserved comparisons; do not claim
   them as physical settling certification or retroactively mark them passed.
2. Use independent calm-reference agreement, convergence and dissipative-work
   controls to qualify the intended force/integration mechanism.
3. Define practical settling through motion over a time window and decay across
   several cycles, with bounded state, mass and independent equilibrium-volume
   checks retained. Derive the observation horizon from the configured drag time
   constant rather than an arbitrary endpoint.
4. A concrete candidate for review is an 8–12 second window (four to six amplitude
   time constants here), retaining 0.5 m/s as a **peak** comparison over the window,
   plus early-to-late RMS/range decay. This is a post-investigation proposal and
   would require predeclared fresh qualification. It would **not** automatically
   clear the coarse pool: its 10–12 second peak alone already exceeds 0.5 m/s.

Changing a release gate is an acceptance-contract decision. Until that decision
and the remaining quality work, the original gates stay red, the combined
candidate stays uncommitted, and the Alpha fluid lock stays in place.

## Reproduction and evidence

```sh
cmake -S . -B build
cmake --build build --target judas_fluid_settling_reference_tests judas_production_fluid_tests -j2
./build/judas_fluid_settling_reference_tests
# Each still fails its unchanged four-second endpoint assertion:
./build/judas_production_fluid_tests --case half_020 --output /tmp/settling-half020-fresh
./build/judas_production_fluid_tests --case half_025 --output /tmp/settling-half025-fresh
./build/judas_production_fluid_tests --case light_80kg --output /tmp/settling-light80-fresh
```

Use fresh output directories. Evidence, exact commands, original and diagnostic
sources, parameter provenance, raw force/continuation rows, convergence tables,
and hashes: `docs/evidence/fluid-settling-cause-20261101/`.
Active local data: `.cache/fluid-settling-cause-20261101/`.
The reference test embeds the small recorded parameter fixtures and needs no
cache, research checkout, GUI, network or external Python package.

The geometry candidate remains based on Experimental `90a97547`; main, the Alpha
policy, Basic Water, original production tests, accepted research and unrelated
projects are unchanged. Windows/Android qualification is not claimed.
