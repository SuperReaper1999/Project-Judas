Finite-height fluid support corrections, Linux, 2026-11-02
========================================================

Start: c27a7ec110a40b564f0e4adf23022de3471a5640 (Experimental).
Main/published Alpha remain b81b39b22ac305bcd250b1cef0fe9ff28e097633.
Advanced Fluids stay locked. No independent research or M54/M55 edits.
Report: ../../FLUID_FINITE_SUPPORT_REPAIR.md.

raw-evidence.tar.xz contains exact raw inputs, results, disposable experiments,
reproduction programs, old source module overrides and the final candidate source
copies. No executable, object, build directory or external asset is included.
SOURCE_FINGERPRINTS.json and BINARY_FINGERPRINTS.json distinguish validated dirty
source from embedded build provenance. These are engineering checkpoint binaries,
not new release packages. Readback verification checks every archived member.

Current authoritative results:
- final/RESULTS.json: 12 sequential native commands; original container invocation
  exits2 because its required fresh --output was omitted. Corrected supported
  container command is in EXTRA_RESULTS.json and passes4340 checks.
- final/production: original unchanged full suite,13801 checks, still exits1.
  Original half_025 remote-plane immersion failure remains .631789144.
- final/known-geometry.csv:360 cases,36 engine/80 observer failures preserved.
  KNOWN_COMPARISON.json:all360 engine values exactly match preceding checkpoint.
- final/player:5661 checks,zero acceptance failures,four existing diagnostics.
- negative/old-physics-new-controls.log:old modules fail20/346 assertions in the
  actual new driver. negative/COMMANDS.json gives exact override builds.
- OBJECT_PARITY.json:all three relevant engine module objects remained identical
  after final comment/include-only source cleanup. FINGERPRINT_CHECKS.json reruns
  the expanded hydrostatic,wet,vertical,policy and Basic Water checks afterward.
- synthetic/:original finite-owner positive/dry twins,failed first short-radius
  assumption,repair,analytic ray controls and corrected new native oracle.
- replay/:exact frozen query,old/alternative partitions,rejected point-slice
  variant,raw alternating before/after timing. Runtime/solver performance is NOT
  inferred from these microbenchmarks or changed coupled trajectories.

Source archive paths retain their repository-relative .cache/... layout. Exact
local compiler/link arrays are provenance; for another checkout substitute its
source/build roots and use its generated CMakeFiles/*/flags.make and link.txt.
Native standalone override objects must precede libjudas_engine.a. Use complete
module overrides with consistent headers; never mix changed class layouts.

Portable final tests from a configured repository build:
  cmake --build BUILD --target judas_fluid_hydrostatics_tests judas_fluid_vertical_support_tests judas_fluid_wet_support_tests judas_production_fluid_tests -j2
  BUILD/judas_fluid_hydrostatics_tests
  BUILD/judas_fluid_vertical_support_tests
  BUILD/judas_fluid_wet_support_tests
  BUILD/judas_production_fluid_tests --output FRESH_DIRECTORY
  BUILD/judas_fluid_known_geometry_tests --output FRESH_DIRECTORY/known.csv
Expected exits:0,0,0,1,1. Do not overwrite an existing evidence directory.

Direct offline replay:
  compile replay/FrozenReplay.cpp with current engine flags/includes and root -I;
  replace the driver object in judas_production_fluid_tests' generated link line,
  then run it with tests/fixtures/fluid/support-band-334.json.
The direct diagnostic driver does not expose --build-info-json; record the exact
source/binary hashes and enclosing build identity rather than attributing another
program's embedded identity to it.

No human acceptance,Windows/Android qualification,fluid unlock,main promotion,
patch publication or universal conservative pressure claim is made.
