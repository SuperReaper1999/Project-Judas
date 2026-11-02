# Moving-pool measurement audit — 2 November 2026

Continuation from published Alpha v1.2.0 (`b81b39b2`) and documentation checkpoint
`42302816`, on Experimental. **No fluid implementation, force law, timestep,
acceptance threshold or Alpha lock changed.** Independent research is untouched.

## Results

The unchanged `half_025` reproduction still exits 1: original 3–4 s remote-plane
immersion .649617513 against .5 +/- .10. Applied sampled immersion is .5106340657.
The historical four-second speed comparison also remains failed at .5985968113
m/s against .5. Current late-window/decay gates still pass; this is not an entirely
passing production suite or authority to unlock fluids.

Passive capture now includes marker ownership, current owner shapes, material
revision, velocities, mass, gravity and applied load measurements. Before/after
boundaries are distinct: a recorded applied fraction uses the beginning field,
whereas endpoint particles may already have advanced. Private held-field poses
are explicitly **not** captured; these files do not claim exact internal-field
reconstruction.

Capture preserves all **720 physical CSV rows / 22 non-timing columns exactly**.
All eight original complete particle/pose snapshots also remain byte-identical.
The capture run retains the same 2,176 checks and original acceptance failure.

## Independent vertical momentum accounting

The replay tool does not call the wetness sampler. For adjacent actual body
velocities and recorded loads it evaluates, along the uniform-gravity up axis:

    residual = m*(v_after-v_before)
               - dt*(m*g + recorded_buoyancy_force + recorded_drag_force)
               - recorded_contained_fluid_impulse

Production's buoyancy force includes water-acceleration loading. For this uniform
gravity fixture, its static portion is independently separated as
`rho * recorded_displaced_volume * |g|`; the remainder is reported separately.
This checks bookkeeping using the recorded displacement; it does **not** prove
that displacement or force law physically correct. Rigid contact/solver impulses
and binary32 rounding can remain in the residual. Exterior reactions explicitly
not applied by production are reported but never silently added to body impulse.

| Vertical impulse, N s | Original 3–4 s | Late 8–12 s |
|---|---:|---:|
| Actual body momentum change | +15.7627062 | +28.7198672 |
| Gravity | -1059.4801754 | -4237.9207017 |
| Static buoyancy portion | +1082.0133710 | +4291.3417819 |
| Water-acceleration portion | -13.6731277 | -36.9014945 |
| Drag | +6.9026085 | +12.2003426 |
| Contained-fluid impulse | 0 | 0 |
| Unattributed residual | +0.00002985 | -0.00006110 |

Largest per-step residuals are 0.000005764 and 0.000007881 N s. Thus no large
unrecorded/doubled body impulse is demonstrated in these windows. This is body
accounting, not closed water-plus-solid momentum-conservation certification.
The unapplied exterior reactions sum to +839.1285 and +4650.4324 N s and are
deliberately excluded, in agreement with the current one-way exterior coupling.

The body rises from .452646 to .598597 m/s during the original observation window.
It is not static hydrostatic equilibrium. A density-ratio target alone cannot
establish the exact instantaneous wet fraction of this accelerating body in waves.
This does not waive its original failed comparison.

Five force-budget controls cover known Newtonian loading, deliberately wrong
force, exclusion of an unapplied reaction, rotated gravity and invalid records.

## Bounded geometric measurement and rejected interpretation

The independent C++ observer integrates the intersection of a box with a declared
union of oriented geometric cells. Outward-rounded binary64 interval predicates
and a bounded body-local octree give lower/upper numerical coverage bounds. It
uses no native wetness query, density-ratio target or pressure fit. Intersecting
cells are a union, not summed twice. Budget exhaustion leaves explicit unresolved
volume. IEEE arithmetic assumptions, including no fast-math, are documented in
the header. Model/input uncertainty is separate from numerical enclosure.

Its 84 controls cover full/dry, 30/50/70% slabs, overlapping cells, disconnected
gaps, rotations/translations, tilted geometry, nested budgets, replay and invalid
inputs. Dyadic half/gap controls require strict enclosure; transformed analytic
references allow their stated input-construction roundoff. Constant-half output
and convex-hull filling fail the negative controls.

For the captured pool we explicitly try two **proxy models**: mass-equivalent
particle cubes in the authored fixture frame, and their union with recorded
birth cells belonging to the actual observed body. Foreign owners are ignored;
missing history is never invented. Ownership/token/input negative controls pass.

| Endpoint step | Owned cells | Proxy coverage bounds at 32,769 nodes |
|---|---:|---:|
| 181 | 3 | .060276–.093196 |
| 210 | 4 | .095791–.190136 |
| 240 | 4 | .078087–.134720 |
| 481 | 6 | .071518–.137196 |
| 720 | 4 | .074484–.122094 |

The bounds narrow when the budget rises from 4,097 to 32,769 nodes. But the model
measures particle/contact-cell overlap inside a solid from which matter was
excluded. It does not describe the complete displaced water domain. **Reject
using these bounds as actual physical immersion**, just as the earlier remote
plane and unqualified local fit were rejected as general oracles. Numerical
precision cannot restore absent boundary information. No runtime result is
declared wrong merely because it differs from this deliberately incomplete proxy.

## Next engineering boundary

The momentum trace rules out a large net unattributed vertical impulse in the
measured windows; horizontal or cancelling impulses are not excluded. The
remaining qualification problem is establishing a declared, independently tested
free-boundary/pressure observation under displaced solids, with modelling error
and genuinely unresolved coverage explicit. Particle/contact cells alone cannot
select a unique hidden continuum surface. Any replacement observation must pass
known surfaces, wet-history/dry-cavity twins and moving-body controls before it
can adjudicate the original failed gate. Do not silently substitute this proxy,
retune the fixture, add damping, or infer equilibrium from density ratio alone.

The worsening settling measurements, deep-start rotation discrepancies, runtime
reconstruction cost and unsupported active-fluid persistence/origin combinations
remain open. Basic Water is unchanged. No new Windows/Android qualification.

## Reproduction

Configure the ordinary Experimental Release build, then build:

```sh
cmake --build BUILD --target judas_production_fluid_tests judas_fluid_coverage_observation_tests judas_fluid_coverage_replay -j2
BUILD/judas_fluid_coverage_observation_tests
BUILD/judas_production_fluid_tests --case half_025 --capture-immersion --output FRESH_CAPTURE
# Expected production exit: 1. Preserve the original failed assertion.
python3 tools/validation/audit_fluid_force_budget.py FRESH_CAPTURE --output FRESH_BUDGET
BUILD/judas_fluid_coverage_replay FRESH_CAPTURE/half_025_observe210_boundary.json FRESH_CAPTURE/half_025_observe210_particles.csv 32769
python3 tests/test_fluid_force_budget.py
```

The native coverage kernel is registered as an ordinary geometric `water` gate;
it does not replace either known-failing fluid diagnostic. Archive, raw results,
source/binary fingerprints and exact reproduction commands are in
[the evidence directory](evidence/fluid-measurement-20261102/README.md).
