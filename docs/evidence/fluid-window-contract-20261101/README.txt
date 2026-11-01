Fluid window/decay contract — 2026-11-01

Read ../../FLUID_WINDOW_DECAY_CONTRACT.md for interpretation.
run/DECLARED_CONTRACT.json predates fresh qualification.
run/qualification/RESULTS.json records expected failures and binary hashes.
run/qualification contains complete raw per-step observations and snapshots.
run/baseline contains the diagnostic reconstruction-substitution comparison.
run/BASELINE_LINK_RECIPE.json describes that limited baseline, not a full old build.
run/LEGACY_EQUIVALENCE.json and run/STRICT_FLAG_EQUIVALENCE.json preserve replay evidence.
run/before retains the original production driver and 69-check reference source.
run/reference-final.log.gz records final 81-check native/reference result.
run/*.py.gz preserve executable qualification/comparison scripts; decompress before running.
source/ preserves final test, registration and report sources.
FINAL_PROVENANCE.json identifies unchanged production code and tested binaries.
All non-JSON text snapshots are gzip with mtime=0. No binaries are bundled.
SHA256SUMS.json covers all files except itself.

Expected remaining failures: four late independent-immersion mismatches,
plus coarse half_025 late peak and range decay. Original four-second
failures remain reproducible. No tolerances retuned after observing results.
No main changes, commit, push, unlock, publication or research modifications.
