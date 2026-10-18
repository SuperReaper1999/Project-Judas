# Pass 5 — CI inventory check repair

## Observed failure

Eight successive GitHub runs, from `39138629167` through `39156910751`, failed in
the same tooling test: `test_shared_inventory_contains_every_current_public_tool`.
The direct job logs in `remote/` show the same five names, not eight independent
native-engine failures. The earlier `39073963008` resource-gate failure is a
separate historical issue; it is not reclassified as this inventory failure.

The inventory test assumed every target remained a literal `add_executable` in
the root build file. Pass 3 moved four declarations into included CMake modules
and used the established target helper for Windows readiness. The targets remained
present, while the source-layout-dependent verification went stale.

## Changes

- `scripts/prealpha_packaging_checks.py` follows reachable local CMake includes and
  recognizes literal declarations through both existing declaration mechanisms.
  Comments, inert quoted/bracket text and dynamic helper parameters cannot invent
  targets. Missing declarations and missing local includes still fail.
- `.github/workflows/prealpha.yml` executes the SDK's packaged inspector instead
  of the source-tree copy. Host, glibc and OS checks remain enabled, followed by
  strict SDK verification from `/tmp`; no gate is removed.

No engine, physics, native CMake target, package inventory or runtime implementation
changes are part of this correction. The scanner verifies authored cross-platform
declarations; it does not substitute for actual native platform builds.

## Evidence and results

- `local/before/`: unchanged suite reproduces the exact failure, 24 tests, one
  failure, exit 1; original source snapshots and fingerprints preserved.
- `local/after/`: corrected packaging suite passes 26 tests, and existing real
  CMake helper/module contract checks pass 12 tests, both exit 0.
- `local/negative-controls/`: the actual inventory assertion fails as required
  when the Windows helper target is removed or the RenderingAssets include is
  removed while its module remains on disk. Each detecting run has exactly one
  expected assertion failure; the outer verifier requires both detections.
- `other-local/`: five remaining lightweight CI tooling commands pass, with exact
  command/exit receipts and raw logs. These are not native engine qualification.
- `remote/`: original run/job receipts, annotations and compressed raw logs. Raw
  compression retains original byte hashes in `COMPRESSED_INPUTS.json`.

The local correction is verified. A subsequent actual GitHub workflow result must
be recorded separately; local passes alone do not establish hosted CI success.

See `local/REPORT.md` for exact scope, scanner boundaries and reproduction commands.
Top-level manifest generation and the combined checkpoint are owned by the parent
agent. No full production suite or native engine rebuild is needed for this
verification/workflow-only change.
