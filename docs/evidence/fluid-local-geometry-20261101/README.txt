Local wet geometry and settling — 2026-11-01

Read ../../FLUID_LOCAL_GEOMETRY_SETTLING.md and FINAL_RESULTS.md.
run/final is the combined final-source qualification.
run/ownership-only-final and run/field-basis-only-final preserve the intermediate failures.
run/negative-controls links pre-fix or rejected reconstruction objects against final regression fixtures.
run/coarse contains exact captured inputs around the largest original jumps.
run/coarse-forces.csv.gz and COARSE_TRACE_ANALYSIS retain the original 720-step force replay.
run/MANUFACTURED_ORACLE_CONTROLS.json demonstrates flat-plane observation failure.
run/inflated-gap-control preserves the invalid manufactured-flat expectation; do not cite it as a physics failure.
run/always-sliced-preserved explains the first exploratory output reuse and retained console metrics. The rejected all-query method is also rerun under negative-controls with fresh raw output.
source contains final implementation/tests and registration snapshots.
Compressed text uses deterministic gzip (mtime=0); JSON is uncompressed.
No binaries or unrelated research are bundled. Hashes identify tested binaries.
TASK_PATCH.diff.gz isolates implementation/test changes from the prior dirty candidate.
SHA256SUMS.json covers all files except itself.
No commit, push, main promotion, release or fluid unlock occurred.
