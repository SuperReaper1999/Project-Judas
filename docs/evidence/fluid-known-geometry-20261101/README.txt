Known-answer fluid geometry and connected-slab correction, 2026-11-01
Read ../../FLUID_KNOWN_GEOMETRY_COMPARISON.md.

This is an Experimental correction and an honestly FAILING geometry diagnostic.
The known geometry diagnostic exits 1. The original coarse production immersion
check remains failed at 0.6341740926. Advanced Fluids remain locked in Alpha.
No main promotion, release, tags or human acceptance.

source/ records exact candidate implementation, tests and registry.
run/GATES.txt.gz records the staged, declared experiments and comparison scales.
run/ contains baseline/candidate CSVs, failures, source-isolated ablations,
query-reference equivalence, fixed workload timings, fault-injected negatives,
physical replay comparison, native/Python results and exact reproduction code.
No binaries/objects/build trees or independent research are bundled.
Text is gzip with mtime=0; JSON and PNG are uncompressed. SHA256SUMS.json covers
all files except itself. Copies/decompressed bytes were checked against originals.
PROVENANCE.json identifies compiled sources, native binaries and core library.

Important results:
- Baseline 335-assertion vertical suite fails six newly added controls; candidate
  passes all 335. Existing 317 assertions were not weakened.
- Height-sliced reference versus final interval integral: 720 exact equal query
  results, including dry classification and velocity/acceleration.
- Small query benchmark: five batches of 200 queries per field. The reference
  and final checksums match exactly. These are batch means, not step p95 values.
- Known geometry: 360 comparisons; candidate still has 76 engine, 80 observer
  errors >.10 and three engine frame errors >.001. Some fine-grid errors arise
  from centre-only solid exclusion leaving a contact gap; the additional aligned
  input is a CONTROL, not a replacement for failing inputs.
- An always-half sampler fails 48/60 flat comparisons.
- All 720 original coarse physical rows remain equal. All 300 archived native
  ruler measurements replay exactly after the behavior-preserving extraction.

Setup failures are separate from engine findings:
- First target build needed CMake regeneration.
- First rotated/translated observer setup omitted its zero-reservoir-origin
  adapter. That source and wrong-frame outputs are preserved, not counted as
  actual engine/observer failures.
- A disposable fault-injection link initially omitted the registered target's
  main=JudasValidationMain definition; the corrected harness succeeds.
- Initial registry metadata used water plus known-failures; existing tests
  required known-failures ONLY and an explicit two-manual-entry inventory.
  Final validation passes those unchanged policy constraints.

The original 300-frame ruler replay inputs are already preserved in
../fluid-immersion-observer-20261101/native-capture.tar.gz; not duplicated here.

Reproduce in an Experimental Release build:
  cmake --build BUILD --target judas_fluid_known_geometry_tests \
    judas_fluid_vertical_support_tests judas_production_fluid_tests -j2
  BUILD/judas_fluid_known_geometry_tests --output NEW_DIRECTORY/known.csv
  BUILD/judas_fluid_vertical_support_tests
  BUILD/judas_production_fluid_tests --case half_025 --output NEW_DIRECTORY/production
Expected exit codes: 1, 0, 1. Default known-geometry comparisons stay failing.
Python checks:
  python3 -m unittest discover -s tests -p test_fluid_immersion_observer.py -v
  python3 -m unittest discover -s tests -p test_cmake_test_targets.py -v
  python3 -m unittest discover -s tests -p test_prealpha_validation.py -v
  python3 -m unittest discover -s tests -p test_validation_registration.py -v

Causal scripts record this machine's configured .cache/2d-stage1-build paths.
For reproduction, extract to a disposable directory and adapt those build/output
paths. Never install no-admission.cpp or other rejected variants into src/.
