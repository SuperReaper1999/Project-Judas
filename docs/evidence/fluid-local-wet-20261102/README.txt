Local physical wet reconstruction — Linux, 2 November 2026
=========================================================
Report: ../../FLUID_LOCAL_WET_RECONSTRUCTION.txt
Base:941832fe9a4544a937dfc1b08894ed2c989b91d6, Experimental.
Main/published Alpha remain b81b39b22ac305bcd250b1cef0fe9ff28e097633.
No release, tag, main promotion, Advanced Fluid unlock or human acceptance.

SOURCE_FINGERPRINTS.json describes final implementation, tests and registration.
BINARY_FINGERPRINTS.json records exact tested dirty-base Linux engineering
binaries and embedded provenance. They are not post-commit release builds.
The commit containing these manifests identifies the selected source checkpoint.

raw-evidence.tar.xz preserves exact raw inputs/results, failed alternatives,
commands, sources and member hashes. All members were read back and verified.
No executable/object/build tree, unrelated project or independent research is
included. Paths retain .cache/fluid-root-reconstruction-20261102/ prefixes.

Authoritative selected evidence:
- final/RUNTIME_SOURCES.json and RESULTS.json:17 sequential final-source commands,
  no unexpected exits. The original known-geometry diagnostic still exits1.
- final/: root34, local support290, hydro346, wet153, vertical335, drag185,
  lifecycle38, supported containers4340, flat/radial49, boundary24, cavity7 cases,
  rigid coupling, project fluid policy26 and Basic Water56 all pass.
- final/player:5661 checks,zero acceptance failures;4 existing diagnostics still fail.
- final/production:13801 checks,zero acceptance failures;4 diagnostics still fail.
  Original historical4s comparisons remain5 total,2 failing (1 formerly mandatory).
- final/known.csv: unchanged360-case original;48engine/80observer failures,
  0frame failures,0reference failures. It remains failing and unchanged.
- final/BODY_TRAJECTORY_PARITY.json:22 physical fields exactly match941 across
  4560 played rows in all11 production cases. Full suite includes2400 warmup steps.
- benchmark/baseline-root.log: same34 assertions with exact941 hydrostatic module
  override still fail14, confirming the regression detects the missing mechanism.
- benchmark-final/TIMING_SUMMARY.json and RAW_MEASUREMENTS.json:7 alternating pairs,
  16passes per frozen field/run,64queries per volume,4 exactfields. Before/corrected
  mean7.807/8.334ms,median7.317/8.333,p9515.106/16.055,worst17.581/20.192.
  Correct sampling adds~6.8% mean cost here. These are frozen CPU sampling calls,
  not solver/renderer/frame times, and not a speedup. Outputs intentionally change.
- CMake helper22 and registration9 tests pass (2 platform skips); docs-unit10 pass.
  registration-final.log is after the final finding-prose correction. An initial
  misnamed module matched0 tests; its empty log is preserved and is not a pass.

Experiments and mathematical controls:
- fixture-analysis/: exact captured neutral/swim inputs are hashed; analytic
  raw/short-pair/lens tests and predeclared rules are preserved. They are static
  Boolean geometry experiments, not native moving-body qualification.
- Whole local hulls can fill a connected ring's cavity. Pair-hull hard activation
  introduces a finite support jump. The lens-clipped alternative tends to0.
- native-v1/: the actual native candidate passed root/neutral/swim controls but
  failed4 rotated vertical-support assertions. Taking sqrt of a tiny rounded
  positive tangent radicand magnified coordinate error into false wetness.
- native-v2/: guarded tangency passes those selected native cases but its predicate
  and sqrt radii differ. The shifted-cutoff analytic counterexample rejects that
  mismatch; final code uses the same uncertainty-eroded sphere for both.
- prototype/ is an intermediate experiment, NOT the selected implementation.
  Use final-source/ for exact selected code; no unqualified mode is exposed.
- unit/: independent source for the original288 assertions; final regression adds
  2 candidate-collection controls, making290. tests/ source in final-source/ is
  authoritative. All34 original root fixture inputs/limits remain unchanged.
- combined/ and initial-integrated/: earlier passing native stages, before the
  final conservative candidate-predicate clarification. final/ supersedes them.
- HOST.json: compiler, CPU, OS, RAM and timestamp. Benchmark runs deliberately
  launch no other task-owned heavy job; shared-host scheduling is not excluded.
- inputs/ and INPUT_MANIFEST.json preserve all8 original captured CSVinputs with
  verified hashes. The Python investigations preserve original local paths;
  substitute their src root with inputs/ while retaining relative subpaths.
  The native benchmark consumes the portable benchmark/frozen-fields.json.

Reproduce from the checkpoint and fresh output directories:
  cmake -S . -B BUILD -DCMAKE_BUILD_TYPE=Release
  cmake --build BUILD --target judas_fluid_local_wet_support_tests judas_fluid_root_admission_diagnostics judas_fluid_hydrostatics_tests judas_fluid_wet_support_tests judas_fluid_vertical_support_tests judas_player_fluid_integration_tests judas_production_fluid_tests judas_fluid_known_geometry_tests -j2
  BUILD/judas_fluid_local_wet_support_tests
  BUILD/judas_fluid_root_admission_diagnostics
  BUILD/judas_fluid_hydrostatics_tests
  BUILD/judas_fluid_wet_support_tests
  BUILD/judas_fluid_vertical_support_tests
  BUILD/judas_player_fluid_integration_tests --output FRESH/player
  BUILD/judas_production_fluid_tests --output FRESH/production
  BUILD/judas_fluid_known_geometry_tests --output FRESH/known.csv
Expected exits:0,0,0,0,0,0,0,1. Strict modes and original historical comparisons
remain available; diagnostic-only failures are printed rather than erased.

Exact local build/link arrays are provenance. Substitute checkout/build roots to
reproduce elsewhere. benchmark/build_and_measure.py shows complete941 module
override builds preceding libjudas_engine.a, the same-driver negative control,
and original paired timings. benchmark-final/run.py relinks the corrected driver
against final engine code, reuses the pinned baseline executable, and records the
final paired measurements. No compiler ran during its timed section.

Scope: actual non-solid admission and owned-fallback samples; no public API,
project/save schema, physics cadence, controller tuning or iteration changes.
The force control proves sampler sections and existing quadrature wiring, not
independent continuum volume/pressure/energy conservation. Exact arbitrary wet
3D topology, raw-cube/quadrature transitions and binary32 resolution remain limits.
Active PBF cold saves/suspension/live origin shifting remain unsupported. Alpha
policy is unchanged; native tests here run Experimental, not a rebuilt Alpha
package. No Windows, Android or browser qualification is claimed.
