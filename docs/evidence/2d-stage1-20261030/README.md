# Stage 1 evidence

See [report](../../2D_STAGE1.md). `SUMMARY.json` identifies scope/platform/counts.
Raw failures are preserved alongside follow-ups; unavailable/wrong-fixture runs
are not counted as passes. The authoritative 2D manifest group was qualified with
validation-enabled native entrypoints; `registered-qualified/RESULTS.json` contains
exact binary/source identities for that run. `integration-checkpoint.log` adds the
three final orthographic LOD controls (51 total), with separate-process restore.

The 3D comparison is six byte-identical PNG hashes. Its baseline recompiles only
the starting Renderer.cpp / WorldPresentation.cpp against shared current headers
and other modules. The reproduction script and compiler commands record this
boundary; it is not a full old-engine rebuild. Historical source objects/binaries
remain local in `.cache/2d-stage1-evidence/render-baseline/`, not committed.

The initial standalone proof uses the earlier same-renderer authored arrangement;
its movement log selects the then-current explorer ID 188. The final lab puts the
explorer first (ID 1) and groups backdrop sprites. Final exported/candidate-head
qualification is retained locally under `.cache/2d-stage1-evidence/final-head/`
after the checkpoint, avoiding a self-referential commit/provenance record.

Source file fingerprints are in `SOURCE_SHA256.json`; binary identities are in
raw gate JSON/logs. No binaries, caches or external research are archived here.
Main and published artifacts remain unchanged.
