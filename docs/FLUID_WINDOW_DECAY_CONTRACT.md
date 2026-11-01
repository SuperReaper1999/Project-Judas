# Fluid settling: authorized window/decay contract

**Current checkpoint disposition:** [Experimental fluid progress](FLUID_PROGRESS_CHECKPOINT.md). The report below preserves the findings and uncommitted status of that earlier investigation; it is not the current unlock status.

**Subsequent local-geometry investigation:** [Wet ownership, field frames and settling](FLUID_LOCAL_GEOMETRY_SETTLING.md). Earlier measurements below remain historical observations of their exact candidate.

2026-11-01. Experimental base `90a97547e7148f2ba179fb5d6a547d1e94a21188`.
**Uncommitted combined candidate. Qualification still fails. Main and the Alpha
Advanced Fluid lock remain unchanged.** This task changes test acceptance and
observation only; it adds no damping, clamps or production physics changes.
The earlier uncommitted vertical-geometry implementation remains present.

## Contract and scope

The operator approved replacing a single four-second speed observation with a
window/decay contract after the [cause review](FLUID_SETTLING_CAUSE_REVIEW.md).
The declaration was saved before fresh qualification; its exact bytes are in
`evidence/fluid-window-contract-20261101/DECLARED_CONTRACT.json`.

For the unchanged drag acceleration `-k*f*v`, equilibrium immersion `f_eq` and
small vertical displacement, the amplitude time constant is

    tau = 2 / (k * f_eq)

Here `k=2 s^-1`, `f_eq=0.5`, so `tau=2 s`. Observe early motion over `(tau,2*tau]`
and late motion over `(4*tau,6*tau]`: nominally 2–4 s and 8–12 s. The existing
binary32 `1/60 s` timestep gives 120 early and 240 late observations. Rounded
step boundaries and actual times are reported, not substituted timesteps.

Require all of:

- Every late vertical speed strictly below **0.5 m/s**.
- Late RMS vertical speed and vertical travel range each at most half the early
  value, with only binary32 representation-scale floors.
- Late mean sampled immersion and independently estimated immersion each within
  the existing **0.10** tolerance of half-density equilibrium.
- Complete, finite observation windows, including every independent late sample.

Window centers are 3.5 amplitude time constants apart. Ideal linear amplitude
would decay by `exp(-3.5)` (about 0.03); the declared factor 0.5 allows nonideal
moving water without fitting acceptance to the measured late speeds.
This derivation sets observation times, not a guarantee that a nonlinear PBF
reservoir must satisfy the contract. Failures remain failures.

The four resolved floating fixtures use this contract. Neutral, sinking,
zero-gravity, common-freefall and explicitly sub-particle diagnostics retain their
original scopes. Original all-step finite-state/mass checks and 3–4 s immersion
checks remain active. No mass, release pose, solver budget, cadence, coefficient,
or physical timestep changed.

## Results

Linux Release, GNU C++ 15.2, Ryzen 5 5600. These are correctness runs, not isolated
performance measurements. Full raw logs, source snapshots, provenance and hashes
are in [the evidence directory](evidence/fluid-window-contract-20261101/README.txt).

| Check | Result |
|---|---|
| Test-contract positive/negative controls | 46 checks, zero failures |
| Independent calm model and native MK2 controls | 81 checks, zero failures |
| Vertical geometry / hydrostatics / lateral support | 261 / 124 / 109 checks, zero failures |
| Full production window qualification | 13,801 checks, **six mandatory failures**, two existing diagnostic failures |
| Exact legacy four-second mode | 8,021 checks, **three mandatory failures**, two existing diagnostic failures |
| Build target registration | 18 tests passed |
| Validation suite registration | Nine tests, seven passed and two environment-dependent skips |

The native calm controls satisfy the new contract with the original drag and
also reject undamped native bobbing. Synthetic negative controls detect a lucky
endpoint phase, persistent low-speed oscillation, late spikes, separate RMS/range
failures, wrong immersion, missing observations and invalid/overflowed values.
The original converged and native four-second velocities remain recorded.

### Fresh candidate measurements

Speed units m/s; range units m. Independent immersion is the existing exterior
surface estimate integrated over the body, not the engine's wet sampler.

| Fixture | Early / late RMS | Late peak | Early / late range | Sampled / independent late immersion | Failing checks |
|---|---|---|---|---|---|
| half_020 | .784196 / .129339 | .376893 | .484601 / .098923 | .508748 / .645513 | Independent immersion |
| half_025 | .785575 / .321804 | .630705 | .478813 / .244748 | .509820 / .625049 | Peak, range decay, independent immersion |
| rotated_half | .992720 / .179188 | .478254 | .627413 / .132367 | .516967 / .667909 | Independent immersion |
| light_80kg | .843732 / .213154 | .443885 | .502611 / .142805 | .516973 / .629207 | Independent immersion |

All four pass observation coverage, RMS decay and sampled late immersion. The
coarse pool's range limit is .239407 m, below its measured .244748 m. Nothing
was rounded into a pass. All six failures remain authoritative exit failures.

The four independent-immersion mismatches are **unresolved**, not yet four
established engine defects. This oracle fits a flat height from external surface
columns; late nonplanar water may bias that estimate. It must be checked against
actual local wet geometry before attributing the entire disagreement to either
implementation. The gate has not been suppressed or widened.

### Numerical baseline comparison

A diagnostic executable links the new observer against the pinned `90a97547`
hydrostatic object, ahead of otherwise identical candidate engine libraries.
This isolates the changed hydrostatic reconstruction; it is **not** represented
as a fresh full-checkout baseline build. Source/object/binary hashes and the link
recipe are preserved.

| Fixture | Baseline late peak | Baseline late independent immersion | Baseline failures |
|---|---|---|---|
| half_020 | .306659 | .613330 | Independent immersion |
| half_025 | .344791 | .573208 | Range decay (.170842 > .152281) |
| rotated_half | .885991 | .590915 | Late peak |
| light_80kg | .396370 | .573805 | None |

Thus the baseline is not uniformly passing either. Candidate results worsen
some measurements and improve others. In particular, three late independent
checks that pass in this diagnostic baseline fail in the candidate. This is an
unresolved reconstruction/oracle difference, not grounds for declaring the
geometry candidate qualified.

## Preservation and reproduction

Across all eleven fixtures, the new legacy-mode run matches all **2,640 original
non-timing physical CSV rows** exactly. The first 240 observations of the default
run match too. Only `fluid_step_ms` and `total_coupling_ms` are excluded; physical
pressure-reconstruction RMS columns are included. All **66** original release,
step-10 and step-240 body/particle snapshots are byte-identical across the old,
legacy and window runs. The optional strict legacy flag also leaves all 720
physical rows identical and adds the original failure (one to two for half_020).
Both modes still exit nonzero because the new independent-immersion gate fails.

From the repository root, using the existing configured build directory:

```sh
cmake --build .cache/2d-stage1-build --target judas_production_fluid_tests judas_fluid_settling_contract_tests judas_fluid_settling_reference_tests -j2
.cache/2d-stage1-build/judas_fluid_settling_contract_tests
.cache/2d-stage1-build/judas_fluid_settling_reference_tests
.cache/2d-stage1-build/judas_production_fluid_tests --output /tmp/fluid-window-current
.cache/2d-stage1-build/judas_production_fluid_tests --legacy-four-second --output /tmp/fluid-legacy-current
.cache/2d-stage1-build/judas_production_fluid_tests --strict-legacy-settling --case half_020 --output /tmp/fluid-strict-legacy
```

The last three commands are expected to return nonzero on this candidate.
`--strict-diagnostics --case small_mass_125x` retains its existing diagnostic
failure; it has not been converted to a supported-scale pass.

## Files and next boundary

This task adds `tests/FluidSettlingContract.h`,
`tests/FluidSettlingContractTests.cpp`, extends `tests/ProductionFluidTests.cpp`
and `tests/FluidSettlingReferenceTests.cpp`, registers the new contract test in
`CMakeLists.txt` and `tools/validation/suites.json`, and updates current fluid
reports. The reference test and pending geometry registrations originated in
the previous uncommitted investigation. Production fluid code is unchanged by
this task; hashes verify that boundary.

Next work should validate the late independent wet-volume oracle and investigate
coarse-pool persistent motion, preserving this contract and all original traces.
No fluid unlock, publication, main promotion or claim of human validation is
justified by this result. The combined candidate is left unstaged/uncommitted.
