Fluid contact, initialization and query-cost checkpoint — Linux, 2 Nov 2026
=======================================================================
Report: ../../FLUID_CONTACT_AND_QUERY_REPAIR.txt
Base: d71af6e007b5c39a822f0f563f4dd108040cea4b, Experimental.
Main/published Alpha remain b81b39b22ac305bcd250b1cef0fe9ff28e097633.
No Advanced Fluid unlock, release package, human acceptance or platform expansion.

raw-evidence.tar.xz contains exact selected/rejected sources, frozen inputs,
raw numerical traces, logs, compiler/link arrays and readback-verified member
hashes. No binary/object/build tree, independent research or unrelated asset.
SOURCE_FINGERPRINTS.json describes the final scoped implementation and tests.
BINARY_FINGERPRINTS.json describes the exact tested dirty-base Linux binaries,
not a claim that the later checkpoint hash was embedded in those executables.
The commit containing these manifests identifies the selected source checkpoint.

Raw paths retain .cache/fluid-support-continuation-20261102/ as their prefix:
- qualified/RESULTS.json:17 sequential final-source native commands. Ordinary
  physics/water/player/production regressions exit0. Known geometry exits1.
- qualified/EXTRA_RESULTS.json: original historical4s failure exits1; exact-zero
  root diagnostic exits1; final explicit-source counterpart exits1 because its
  observer still fails80. The first copied-driver compile failed because moving
  it out of tests lost a relative include; corrected command adds -I tests.
  Both logs are preserved; this setup failure is not an engine test failure.
- qualified/root-exact-tolerances.log:34 assertions,14fail. This new manual
  diagnostic preserves all22 original dry/owner assertions plus12 footprint
  controls. It is not wrapped in an expected-failure pass. No accepted test was
  removed, demoted or weakened. See FLU-ROOT-ADMISSION in suites.json.
- qualified/player:5661checks,0 acceptance failures,4 unchanged warmup diagnostics.
- qualified/production:13801checks,0 acceptance failures,4 diagnostic failures.
  Two historical4s comparisons still fail. legacy-four-second.log preserves
  the actively rerun original half_025 .582806m/s vs .5m/s failure (exit1).
- qualified/known.csv: original360 cases,48engine/80observer failures, no frame
  failures. The prior36 engine failures and the additional12 conservative
  contact-gap results remain visible. This is not improved diagnostic coverage
  being represented as complete fluid correctness.
- qualified/known-owned.csv: same physical geometry with actual pre-cut source
  ownership supplied,0engine/80observer failures,0frame failures (still exit1).
- QUALIFIED_PARITY_AND_TIMING.json: every one of22 physical CSV fields is
  identical over720 coupled steps before/after caching. Mean35.95->25.11ms,
  median36.73->25.35,p9549.29->35.06,worst57.73->41.20. Before is the corrected
  raw-height version before caching, NOT the preceding accepted checkpoint.
  Timing is CPU coupling; it excludes rendering and ordinary rigid stepping.
- wet-topology/TIMING_RAW.json and TIMINGS.json:7 alternating pairs of3000 frozen
  queries,196.14->117.41us median batch means;16,387 kernel comparisons,
  762 rejected invalid cache reuses and frozen-query field parity preserved.
- initialization/: original8/36 exceptions, new355 world/38 lifecycle assertions,
  old-engine/new-driver46 failures, authored27particle scenes, sources/hashes.
- vertical-witness/: exact dry .055m lateral/.325m vertical gaps, actual .005m
  contact positive, failed guard-only sliver, raw-endpoint .125 repair.
- prepared-reuse/: rejected solid preparation reuse (8,505,000 comparisons,
 195.55->194.89us; no material measured benefit). It is NOT in production.
- final/ is a REJECTED global halo ban (4 player regressions). Read
  rejected-global-halo-ban/STATUS.txt; its full source snapshot is preserved.
- bounded-halo/: actual-only and nominal/bounded experiments. Actual-only fixes
  missing-bin water but fails neutral drift and2 hydro controls; bounded selected
  passes do not correct nominal-bin admission. These remain unqualified.
- qualified-source/: exact selected source copies. HOST.json: CPU, compiler,
  timestamp,OS/RAM. Original failures and setup mistakes are preserved.

Reproduce from the checkpoint, with a configured Release build and fresh outputs:
  cmake -S . -B BUILD -DCMAKE_BUILD_TYPE=Release
  cmake --build BUILD --target judas_fluid_world_tests judas_fluid_hydrostatics_tests judas_fluid_wet_support_tests judas_fluid_lifecycle_qualification_tests judas_player_fluid_integration_tests judas_production_fluid_tests judas_fluid_known_geometry_tests judas_fluid_root_admission_diagnostics -j2
  BUILD/judas_fluid_world_tests
  BUILD/judas_fluid_hydrostatics_tests
  BUILD/judas_fluid_wet_support_tests
  BUILD/judas_fluid_lifecycle_qualification_tests
  BUILD/judas_player_fluid_integration_tests --output FRESH/player
  BUILD/judas_production_fluid_tests --output FRESH/production
  BUILD/judas_production_fluid_tests --case half_025 --legacy-four-second --output FRESH/legacy
  BUILD/judas_fluid_known_geometry_tests --output FRESH/known.csv
  BUILD/judas_fluid_root_admission_diagnostics
Expected exits:0,0,0,0,0,0,1,1,1. Default diagnostic-only failures are printed;
use original strict modes to enforce them. Do not label them passes.

Exact local compile/link arrays use absolute paths for provenance. Substitute
source/build roots to reproduce elsewhere. qualified_extras_remaining.py shows
how to compile the copied owned-source diagnostic using normal current headers,
-I tests and the generated known-geometry flags/link line. Complete module
objects precede libjudas_engine.a in the older disposable override experiments;
never mix changed internal class layouts across those old prototypes.

Limitations: root/halo admission unresolved; finite quadrature/topology, nonplanar
observation, native initialization contact ties and expensive reconstruction still
need qualification. Active PBF cold saves/suspension/live-origin shifts remain
unsupported. Alpha lock policy source is unchanged; this Experimental run did not
retest a published Alpha binary. No independent research/M54/M55 changes.
