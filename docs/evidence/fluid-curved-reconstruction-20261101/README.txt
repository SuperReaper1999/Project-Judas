Curved-surface reconstruction evidence — 2026-11-01

Engineering base: Experimental 03fbd812963d6190f18a881cf68ec44339c4f326.
The containing commit identifies the corrected source. FINGERPRINTS.json records
actual final-source and tested-binary SHA-256 values, not a claim of clean build
provenance after the evidence commit. Main/Alpha fluid policy are unchanged.

Read ../../FLUID_CURVED_SURFACE_RECONSTRUCTION.md first.

Outer files:
  SUMMARY.json             known-answer results, additional shapes, query costs
  QUALIFIED_RESULTS.json   final-source commands, exit codes and elapsed times
  COMPARISON.json          negative-control exit and alternating query runs
  FINGERPRINTS.json        exact final tested sources and binaries
  SHA256SUMS.txt           checksums for the packaged files
  evidence.tar.gz          419 preserved sources, logs, CSVs and receipts

Archive contents:
  GATES.txt                         original gates and continuity refinement
  final-source/src and tests        exact corrected sources used for qualification
  qualified-*.log, qualified-*/*.csv final-source raw tests and measurements
  final-*                           first integration run, including tangent failure
  negative-baseline-hydro.log        new 169-check executable, old sampler: 8 FAIL
  baseline-source                   exact 03fbd812 hydrostatic source
  build-flags.make, build-link.txt    actual Release compile/link inputs
  QueryBench.cpp                     complete fixed-input query benchmark
  compare-recorded.py                original recorded command driver
  compare.py                         reproduction compiles both extra harnesses
  query-[0-3]-before/after.csv        20 batches of 200 queries per shape and version
  additional-baseline/final.csv      independent 64^3 and 96^3 body-volume references
  AdditionalShapes.cpp              complete extra-shape generator and observer
  compare.py, qualified_checks.py    commands for final tests / negative control / cost
  EVIDENCE_MANIFEST.json              per-entry byte count and SHA-256
  LOCAL_BINARY_FINGERPRINTS.json      hashes of local prototype objects/executables

Rejected prototypes retain both source and results. Particularly:
  PartialUnconditional.cpp / PartialSupportFirst.h: false-wet dry-gap regression
  WeightedPatchBeforeLayerFade.h: hard multilayer veto
  PatchBeforeUncertainty.h: weak-donor continuity failure
  Gauss8Rejected.cpp: fixed-rule integration regression
  FluidSolidSupport.h (archive root): first circle clip, before tangent correction
  final-judas_fluid_hydrostatics_tests.log: analytic tangent test caught that defect
  final-source/src/FluidSolidSupport.h: corrected and qualified version

Prototype compile failures and the first runtime output-path setup error are
preserved too. Neither is reclassified as a fluid-engine failure.

The strict original known-geometry diagnostic still fails (36 engine / 80 legacy
observer cases). All 36 engine cases are the preserved fine-grid represented-
contact gaps. Additional shapes retain three >0.10 errors out of 24 measurements:
two tilted-box dome cases and one coarse sphere dome. The original production
immersion assertion remains failed. Full production has two existing scoped
diagnostics; full player qualification has four existing micro-rest diagnostics.
These are NOT all-green or fluid-unlock evidence.

Reproduction:
  Build the targets listed in the report, using Experimental, then run commands
  from QUALIFIED_RESULTS.json with fresh output paths. Absolute paths in those
  receipts identify this machine and should be replaced with your checkout.
  compare.py rebuilds the pinned baseline and links the new test object to it;
  all eight new physical curved-box checks must fail there, while the corrected
  engine passes the 169-check test. Preserve the failed run.
  AdditionalShapes.cpp can be linked to the same engine library as
  judas_production_fluid_tests in place of its test source. See additional.py.

No executable/build archive is committed; their fingerprints and reproduction
sources are retained. Archive decompression was verified against every manifest
entry before checkpointing. No independent research was copied or modified.
