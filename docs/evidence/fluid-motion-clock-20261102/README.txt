Fluid motion-clock repair and lightweight GPU diagnostics — Linux,2Nov2026
Report: ../../FLUID_MOTION_CLOCK_REPAIR.txt
Parent:5e0692e4728fcd8af819bcdf912686ca995043c0, Experimental.
The containing commit identifies the selected source. Tested binaries retain
their actual parent/dirty-source build metadata; no release identity is invented.
Main/published Alpha and the fluid lock remain unchanged.

raw-evidence.tar.xz contains exact source snapshots, inputs, commands and raw
passing/failing results, with .cache/fluid-pool-impact-20261102/ prefixes.
Every member is readback-verified against MEMBERS_SHA256.json. ELF executables,
objects and build libraries are excluded; their recorded hashes are retained.
No unrelated project or independent research directory is included.

Selected evidence:
- baseline-source/ and final-source/: actual coupling, host, test and gate source.
- neutral/cadence-before.log: same48 new controls fail8 on the old engine;
  cadence-after.log and cadence-gate-final/: final48/48 and verified registration.
- pool-probe-original.cpp, impact-trace.log, step-trace.csv: exact1440-particle
  input and original escaped particle IDs484/486/626; original zero-escape gate
  exits1. Original project fixture/thresholds are not modified.
- phase-probe.log / clock-phase-probe.log and associated disposable sources:
  pressure/smoothing/wall-response attribution. Logging thresholds only select
  output; they are not physical thresholds or a controller.
- clock-probe.log: isolated clock-only prototype,0 escaped at endpoint.
- pool-final-probe.log / pool-final-peak.log / impact-trace-corrected.log and
  step-trace-corrected.csv: final original probe passes, count/mass exact; high
  contact transient at step471 remains explicitly recorded.
- neutral/REPORT.txt, SUMMARY.json and raw controls: independent fully wet
  buoyancy checks, actual neutral fixture, thrown-body trace, rejected initial-
  overlap setup. No buoyancy correction was inferred from floor contact alone.
- final/RESULTS.json:12 sequential final-source tests; all exit0. Production
  and player acceptance pass while2/4 diagnostic failures remain respectively.
  All8 container cases pass7014 assertions; flat/radial demonstration49 passes.
- legacy-final/: both original4s reproductions pass unchanged,731 checks each.
  The old failure sources/results remain in earlier protected evidence.
- gpu/RESULTS.json and off/on/flood.log: opt-in/off, deliberate error, filtering,
 256-message cap, async mode and lifecycle controls. These intentionally produce
 labelled synthetic API errors; they are not real game failures.
- gpu/actual-renderer.log:44 existing renderer assertions pass with logging on.
  gpu/pool-application.log and relocated-final.log: actual project scene/UI/
  swimming and relocated standalone checks; no unexpected callback errors.
  kernel-fault-query.log is empty for the bounded run query since18:05.
- export-final.log:4assets/2scenes/58481754bytes; final package moved to
  /tmp/Judas Fluid Clock Final 20261102, no JUDAS_ENGINE_ROOT override used.
- cadence-gate/: initial stale-provenance rejection retained, not counted as a
  test pass. Reconfigured cadence-gate-final/ passes normally.
- validation-helper-final.log:15/15; cmake-tests-final.log:22/22.
- alpha-registration.json: active cadence test is unavailable in Alpha, not
  passed. The implementation still rejects locked advanced-fluid activation.

Reproduction from the repository root using a matching Experimental build:
  cmake --build BUILD --target judas_fluid_cadence_motion_tests judas_gpu_diagnostics_tests -j2
  BUILD/judas_fluid_cadence_motion_tests
  BUILD/judas_gpu_diagnostics_tests off
  BUILD/judas_gpu_diagnostics_tests on
  BUILD/judas_gpu_diagnostics_tests flood
GPU test controls require a native GL context; unsupported KHR_debug returns77.
Default-off ordinary engine operation remains supported without that extension.
The original pool probe compiles with the recorded ordinary engine-test link
set; use final-source for the repaired candidate, baseline-source for a negative
control in an isolated copy. Preserve original fixtures and fluid-lock policy.

No new human/Windows/Android/browser acceptance, whole-engine certification,
GPU-crash resolution, general fluid unlock or runtime speedup is claimed.
