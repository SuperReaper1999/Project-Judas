# Broadphase reuse evidence package plan

Source preparation only. `package_evidence.py` has not been invoked and no package
has been copied to the proposed destination.

Destination: `docs/evidence/prealpha-broadphase-reuse-20261021`.

## Selected inventory

- Accepted80f baseline900, unsampled/profile-off normal live35, sampled live35
  with raw PCs/maps/module ranges and exact symbol/bucket analyses.
- Existing-profiler stage900 JSONL, full raw scope/counter hierarchy, worker
  records, diagnostic limits and both all-step/500–900 analyses.
- Prediction-reuse900/live35 and instrumented candidate-demand900.
- The same-flags original-code1800 control, final900/1800/live35 and both final
  analysis tables are now explicit default mappings. The layout-noise control
  and exact common-prefix comparisons remain alongside their raw rows.
- Final-stage-build/final-stages900 (including500–900 analysis), final fresh-driver
  qualifications, earlier failed qualifications and helper snapshots, active34-
  check original/broken controls, fresh application-driver build receipts, cold
  M70/M77 records and integration results are explicit default mappings. HOST.json,
  CORE_MEMBER_COMPARISON.json and root build/qualification logs are included by
  the exact immediate-record selection.
- Original, unconditional-reuse negative control, guarded-reuse, empty-manifold,
  prepared-pose and combined private prototype sources, exact build recipes and
  receipts. Rejected experiments stay explicitly distinct from production.
- The initial helper compile failure, successful independent sampler/stage
  builds, private guard-control failures/follow-ups and their unmodified logs.
- All immediate measurement helper sources, tool fingerprint receipts, parity
  records and aligned normal-run tables.
- Full baseline source hash inventory, build identity, relevant selected baseline
  source files, current coordinator/test/build wiring, exact room1024 scene and
  its project scripts. Do not copy the full516-source snapshot or core archive.
- Other independently owned qualification roots can be supplied with `--map`.
  Existing defaults can be replaced explicitly if root chooses new final folders.

## Root execution after final data freeze

Inspect a plan first. The mappings below are explicit existing defaults and can
be replaced if root chooses new frozen output paths:

```sh
python3 .cache/support-followup-20261021/package_evidence.py \
  --map final900=.cache/support-followup-20261021/final900 \
  --map final1800=.cache/support-followup-20261021/final1800 \
  --map final-live35=.cache/support-followup-20261021/final-live35 \
  --map qualifications-final=.cache/support-followup-20261021/qualifications-final \
  --require final900 --require final1800 --require final-live35 --require qualifications-final
```

When the inventory is correct, repeat the same command with `--execute`. The
destination must not already exist. Additional approved final source files can
be included using `--source REPOSITORY_PATH`. Additional qualification roots use
independent `--map LABEL=PATH` entries; no generic recursive repository scan occurs.

## Preservation policy

- JSON receipts, logs, recipes and source remain exact byte copies.
- Every CSV/JSONL is losslessly gzipped with zero mtime and no filename.
  PACKAGE.json records original/stored SHA-256 hashes and byte counts.
- Positive source/data extension selection excludes binaries, objects, static
  libraries, executables, archives, temporary assets and user-data/cache folders.
  Temporary cold-test `project/` copies are excluded; exact test sources, original
  versioned content references, outputs, receipts and physical endpoint JSON stay.
  Their hashes remain in original build/run receipts when those receipts record
  them. The final package has a fresh SHA256SUMS inventory.
- Per-file64MiB and total uncompressed256MiB bounds prevent accidental large
  inclusions; all selected data are read/bounded/hashed before output creation.
- Missing final groups remain explicit, not implied complete. `--require` checks
  only their availability; actual acceptance comes from retained test results.
- Normal performance comparisons must use profile-off/sampler-disabled runs.
  Sampled and instrumented timings are diagnostic, never substituted for normal.
- Existing support-continuity and physics-cost reports/evidence remain untouched.

## Tool review

The source has been syntax-checked with Python AST parsing only. No packaging
execution, builds or workloads were performed for this preparation. Its eventual
inventory/output verification remains root's final packaging step.
