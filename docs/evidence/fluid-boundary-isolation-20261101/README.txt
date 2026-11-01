Exact-boundary isolation, 2026-11-01
Engine base: Experimental db90a17bee89a97dde17a07c6fcafc2cbd753297.
No production implementation changes, fluid unlock, main promotion or independent
research changes. Read ../../FLUID_BOUNDARY_ISOLATION.md first.

The archive preserves the exact sources and all four successive native runs,
including predeclared gates and later explicitly recorded experiment extensions.
Only the final run has all eight modes, both fine-grid controls and the dry-hole
negative. Prior runs are not extra accepted coverage.

Files:
  ANALYSIS.json       exact native equivalence, frame checks, ablation results
  TWINS.json          identical particle-state counterexample, continuum domains
  RUN.json            final diagnostic command, exit code, elapsed time
  FINGERPRINTS.json   unchanged production source and diagnostic source/binary hashes
  SHA256SUMS.txt      packaged checksums
  evidence.tar.gz     55 source/log/CSV/receipt entries, with internal MANIFEST.json

Archive root:
  GATES.txt                 chronology of predeclared controls and extensions
  Diagnostic.h              isolated observation/mode data, not an engine API
  prepare.py                instrumented source generation from unchanged engine
  HydroDiagnostic.cpp       exact generated source compiled for the final run
  BoundaryIsolation.cpp     unchanged Evaluate path, analytic boundary and references
  run.py                    actual build/link/run driver
  results/cases.csv          416 measured rows (52 combinations x eight modes)
  results/samples.csv        per-query fits, confidence, rejection counts and early reason
  results/dry-hole-negative.csv  native dry / unsafe bypass falsely wet, both frames
  twins.py, TWINS.json       two domains, identical 32,268-particle arrays
  prior-native.csv           previous checkpoint's native additional-shape results
  analyze.py                bit-identical comparison, covariance and ablation checks
  native-hydro/vertical/wet.log  unchanged production regression results
  ENVIRONMENT.json          compiler, host, native regression binary hashes
  first-run/second-run/third-run/  preserved earlier sources and raw results
  build-flags.make, build-link.txt actual compile/link inputs

Mode legend:
  0 unchanged native, trace only
  1 exact-boundary oracle, current body-volume sampling
  2 exact cap, retained admission
  3 exact cap, skip late hull guard only
  4 exact cap, skip late contact guard only
  5 native fit, skip both late guards
  6 exact cap, skip both late guards
  7 exact cap, skip both late guards and early particle-range rejection

Modes 2-7 are causal probes, NOT qualified runtime repairs. Modes 2 and 4 fail
the unchanged .001 frame comparison (maximum .00241208). Mode 7 falsely wets
the real dry-hole negative with fraction .5 instead of zero. These failures are
preserved, not treated as successful physical behavior. The harness expects the
unsafe negative control to fail physical correctness, proving it catches the
omitted admission mechanism.

Reproduction, in a disposable copy of this evidence, not the preserved archive:
  Use engine source db90a17bee89a97dde17a07c6fcafc2cbd753297, or verify production
  hashes first. Extract under .cache/fluid-boundary-isolation-20261101 within
  that checkout. Configure/build the engine and judas_production_fluid_tests in
  .cache/2d-stage1-build (Release), then:
    python3 .cache/fluid-boundary-isolation-20261101/prepare.py
    python3 .cache/fluid-boundary-isolation-20261101/run.py
    python3 .cache/fluid-boundary-isolation-20261101/twins.py
    python3 .cache/fluid-boundary-isolation-20261101/analyze.py
  The scripts contain recorded local build paths; adapt them if using a different
  build directory. The binary is linked against the existing engine library with
  the isolated diagnostic object overriding only this translation unit. Original
  target objects/library and runtime feature policy are not overwritten.

Expected final result: 26 harness controls pass, 24 prior native results reproduce
bit-for-bit, exact-boundary and all-admission-bypassed paths agree bit-for-bit.
This is not a green fluid-unlock qualification. The original 36 known contact-gap
failures, three additional body errors and production immersion failure remain
unchanged. Binary hashes are supplied; large build artifacts are not packaged.
