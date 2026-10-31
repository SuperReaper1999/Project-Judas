# Wet-classification diagnosis and repair evidence

See ../../FLUID_WET_CLASSIFICATION_REPAIR.md for scope, results and limits.

*.gz are lossless compressed raw captures/source; decompress to a fresh external
path before reproducing. Originals remain in the task's active local cache.
SHA256SUMS.json fingerprints the retained artifacts. PROVENANCE.json records
source and qualification-binary hashes; it deliberately identifies the actual
pre-checkpoint dirty build, not a fictitious clean release. A post-checkpoint
runtime identity rebuild may change its binary hash without changing physics.

FOCUSED_RESULTS.json records final v3 commands, exit codes and elapsed wall time.
final-*.log.gz are their raw outputs and Basic Water native/cold-process checks.
FLAT/RADIAL_BODY_TRANSFERS are original 120-step direct-load/contact traces.
*_V3_FINAL are final full-scene traces; each includes an independent run with
water forces suppressed as a negative control. The production game has no delay
or suppression. FINAL_SCENES.json summarizes every dynamic body, not only the ship.

CAPTURED_INPUTS and COLUMN_TRACE reproduce the baseline column-weight diagnosis.
The generated TraceFluid.cpp/header rename the production class only to observe
its query internals in isolation; selected fractions/accelerations are asserted
exactly equal to the recorded production samples. They predate the correction.
TraceDriver contains the analytic dry controls and source-acceleration ablations.
These are diagnosis artifacts, not an imported research solver.

BASELINE_EXPECTED_FAILURE.log preserves old-classifier failure under the new
regressions. baseline-hydrostatics-abi-adapter.cpp is cafe797a's original arithmetic
with only the extra unused query argument needed for the candidate test ABI.

Rejected connectivity and invalid-positive-fixture logs are explicitly retained.
The earlier qualifier failed its light-body check; it is not the final candidate.

QUERY_PERFORMANCE.json and query-*.csv contain matched query timings, with exact
frozen input data and QueryBench.cpp. They are not full fluid/application times.
LOCK_SOURCE_CHECK.json verifies the existing Alpha policy files are unchanged.
No independent research directory was modified and no research code was adopted.

Raw text logs were subsequently losslessly compressed to avoid whitespace checks
misreading their verbatim terminal output as source. The exact qualification scene
is retained as terrain_flat_comparison-tested.judas.gz; its working copy differs
only by removed trailing spaces. No authoring tokens or physics changed.
