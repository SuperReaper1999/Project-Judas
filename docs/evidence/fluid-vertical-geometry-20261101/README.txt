Vertical geometry candidate, 2026-11-01

UNCOMMITTED and NOT QUALIFIED. No Alpha unlock. Report: ../../FLUID_VERTICAL_GEOMETRY_CANDIDATE.md

FINAL_PROVENANCE.json identifies final source and binary hashes. candidate/
contains exact final source copies. qualification/ is the complete integration
run of the unoptimized height-sliced implementation; final-*.log, final-cases/,
CONTINUITY_EQUIVALENCE.json and native/ cover the final proof optimization.
query-performance-valid/ is before that optimization; query-performance-optimized/
compares accepted baseline with the final candidate; query-performance-proof-comparison/
is a paired comparison of both candidates. Physical output differs intentionally
from baseline in the layered/cavity cases, so those are cost/correctness comparisons.

All gzip members preserve original bytes. SHA256SUMS.json excludes only itself.
The baseline/ sources are pinned to 90a97547. baseline261.log.gz is an expected
failing negative control. Some early private-storage-changing substitution runs
are diagnostic only, not acceptance evidence. combined-light.log.gz confirms the
connectivity prototype failure after a proper combined CMake rebuild.

The 12-second extended-kernel diagnostic is a DIFFERENT rejected prototype, not
qualification of the final height-sliced source. The rejected unseeded continuity
proof was caught during review and corrected before final physical validation.
qualification-build.log.gz records a build before the new target was configured;
query-performance/ records a failed invocation with an incorrect input path.
These are setup mistakes, not engine failures. Sources/commands remain preserved.

Scratch link scripts infer repository/.cache/task paths and use the existing
local Release build. Do not run them in this evidence directory. Use the normal
CMake targets and commands in the report for clean reproduction. Rejected source
variants need their matching archived private/public headers; do not mix them.
DISPOSABLE_EXECUTABLES.json fingerprints only own obsolete binaries removed to
avoid accumulating cache bloat. No protected research or historical evidence changed.
