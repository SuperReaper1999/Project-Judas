# Elastic-energy / Ubuntu 24.04 investigation evidence

Input source `04388ffd1f9590bda0fa8114f235f5057b1c1a7c`, Linux x86-64,
2026-10-24. Production physics source is unchanged. All earlier failure artifacts
remain untouched at `docs/evidence/prealpha-linux-readiness/elastic-limit/`.

## Reproduce the unchanged energy failure

Build `judas_mk2_diagnostics_tests` in a Release Makefiles build. Compile
`elastic/original-probe.cpp` against its matching headers and the libraries from
`build/CMakeFiles/judas_mk2_diagnostics_tests.dir/link.txt`; replace the test object
arguments with this probe. Run it, then run the earlier
`check_recorded_energy.py` on the **new output**. The expected result is failure,
not an accepted pass. `elastic/baseline.log` is the matching 16-case measurement.

`gameplay-envelope.cpp` uses the same link procedure and runs 64 material/rate
combinations, 120 steps each; the script does not retune any acceptance fixture.
`creation-order.cpp` changes only body creation order to match the ordinary JS
scene. `rotation-isolation.cpp` compares the two existing motion-ledger rotation
maps with no contacts. These are live native experiments, not source-enumeration
or cached-controller tests.

## Stage instrumentation and rejected experiments

The `.cpp.patch` files are unified differences against the **input checkpoint's**
`src/physics/WorldMk2Solver.cpp`. Apply a patch only to an owned copy, compile
that module with the matching engine flags and `-I src/physics -I src`, then link
its object **before** `libjudas_engine.a`, replacing that archive member.
`JUDAS_TRACE_ELASTIC=1` enables the temporary energy observations. Production
sources do not contain that instrumentation.

- `stage-isolation.cpp.patch`: additional arrival-before/after boundary samples;
  corresponding `isolated-stages.log` and `isolated-native.log`.
- `frame-only-experiment.cpp.patch`: momentum transport attempt;
  `frame-stages.log`, `frame-only.log`. Rejected: still creates energy.
- `pullback.cpp.patch`: temporary-body-frame pullback attempt;
  `pullback-stages.log`, `pullback.log`. Rejected: worse total energy.
- `exponential-midpoint.cpp`: standalone consistent-exponential trial, linked
  directly with `src/RigidBody.cpp`. Its four finite-rate controls conserve
  energy; `exponential-midpoint-1000.cpp` preserves its failed extension.
- `pullback-exact.cpp.patch`, `RigidMotion-prototype.patch`, `Exact.h` and
  `exact-helper.cpp`: combined rejected trial. Copy the engine headers into the
  owned experiment directory (preserving their relative hierarchy), apply the
  motion-header patch there, force-include `Exact.h`, and link the module and
  helper before the engine archive. `pullback-exact-stages.log` shows surviving
  gains and nonconvergence. This trial is **not** an engine implementation.

The original helper's finite-difference Newton derivative and continuation are
retained as tried. Do not silently turn a failed nonlinear solve into a physical
state or hide its failure by adding iterations. These prototypes are evidence,
not supported APIs, installed runtime code or new default CI pass requirements.

## Older-userland build

Use the documented `tools/build_linux_baseline.py` workflow. The exercised
bubblewrap root uses the checksum-verified official Ubuntu Base 24.04.3 archive;
`linux/ubuntu-base-SHA256SUMS` retains the upstream list. `linux/ubuntu-install.log`
records package setup, including harmless single-user namespace ownership
warnings; the initial rejected apt-helper-user attempt is not a completed setup.
No Windows/Android environment was started.

`linux/ubuntu-build.log` is the initial 395-action Release SDK build, with all 17
identity probes. Final helper receipts retain exact configure/build/qualification
commands, installed packages, source identity and binary/manifest hashes.
Additional records cover actual editor create/export and separate Play/Stop,
90-step relocated execution with source/SDK mounts absent, native physics checks,
ABI-negative controls and the existing 26.04 loader failure.

Software rendering is actual Ubuntu 24.04 Mesa llvmpipe/Xvfb. It is not physical
GPU/audio-device qualification. The Linux kernel remains the host kernel; this
is older **userland/ABI** validation, not a booted Ubuntu 24.04 machine.

`SOURCE_FINGERPRINTS.json` covers the runtime source envelope and task code/docs;
`BINARY_FINGERPRINTS.json` covers the qualified older-userland SDK. Receipt hashes
are not a substitute for the visible open physics failure.
