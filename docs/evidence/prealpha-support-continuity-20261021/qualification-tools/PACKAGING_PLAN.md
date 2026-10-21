# Source/data evidence packaging plan

Prepared source only. The packager has not been invoked and no evidence has been
copied to the tracked destination by this preparation.

## Destination and retained scope

`docs/evidence/prealpha-support-continuity-20261021`

The existing physics-cost investigation report and evidence remain separate,
unchanged baseline inputs. The new package does not overwrite or recopy them.

Default mappings include:

- Original normal 900/repeat-900/1800/live35 baseline records preserved by
  `prepare_baselines.py`; the copied original core's hash remains in manifests,
  while its static library bytes are excluded.
- Exact support-control source/original source, manifest, recipes and 900/1800/
  live35 data, including the matched unsampled baseline live35 observation.
- Temporal baseline/control900 data, observer source, floor oracle and physical
  comparison. Its original source-monitor warning remains intact.
- Geometry and row prototypes, including the rejected adjacency experiment and
  lifecycle/ownership qualification sources.
- Provisional focused checks, the stale BendingTests object failure, the original
  and relinked-original controls, and the fresh-driver paired bending checks.
- Fresh-driver focused checks, paired warm-guess checks and lifecycle controls.
- Original invalid private cold fixture diagnostic and its exact source, plus
  later valid cold-public-paired-v2 records if present.
- Every immediate qualification tool, source fixture, protocol, manifest and
  preserved source-only failure record under this `measure/` folder.
- Selected final production/test/build sources and the exact room1024 fixture.
- Source-only members of the frozen 517-input source archive, preserving original
  source bytes. The binary font asset is omitted; its original hash remains in
  FROZEN.json. The original archive's exact hash is recorded, its bytes excluded.

The following final labels need explicit mappings once output paths are known:
`final900`, `final1800`, `final-live35`, `final-cold`, `final-integration`.
Missing roots and unmapped final labels are listed rather than implied complete.

## Invocation after root authorizes packaging

First inspect an inventory only (this reads files and prints a plan):

```sh
python3 .cache/persistent-contact-20261021/measure/package_evidence.py \
  --map final900=.cache/persistent-contact-20261021/FINAL_900_FOLDER \
  --map final1800=.cache/persistent-contact-20261021/FINAL_1800_FOLDER \
  --map final-live35=.cache/persistent-contact-20261021/FINAL_LIVE_FOLDER \
  --map final-cold=.cache/persistent-contact-20261021/FINAL_COLD_FOLDER \
  --map final-integration=.cache/persistent-contact-20261021/FINAL_INTEGRATION_FOLDER
```

Replace placeholders with actual frozen run roots. Repeat with `--execute` and
the appropriate `--require LABEL` flags only when the inputs are complete. A
fresh destination is mandatory; existing evidence is never overwritten. Extra
approved source snapshots may be named with repeated `--source REPOSITORY_PATH`.

## Preservation and interpretation

- `AS_RUN.json`, result JSON, console logs and source files are exact byte copies.
- Every `.csv` is stored as `.csv.gz`, gzip level 9, mtime 0 and no filename.
  PACKAGE.json records both original and compressed SHA-256 hashes and sizes.
- Object files, executables, static/shared libraries, archives, caches and build
  directories are excluded by positive source/data selection. Small `.save`
  fixtures are allowed only within the explicitly mapped relevant study roots.
- The frozen source archive is read using validated member names; links, absolute
  paths, parent traversal and oversized members are rejected, not extracted.
- Per-file and aggregate bounds are checked before any destination file is made.
- PACKAGE.json is an inventory, not a test-status aggregator. SHA256SUMS covers
  all stored files except itself. No pass is inferred from a folder name.
- Temporal baseline `AS_RUN.json` has child exit 0, 900 steps, zero nonfinite rows,
  unchanged binary/core/fixture, but source_inputs_unchanged=false. Its wrapper
  exit 1 due to the live source monitor remains a recorded qualification warning.
- The first cold probe's metadata rejection was a private fixture defect, not an
  engine save-restoration failure. Its corrected counterpart must retain its own
  independent result receipt and final source identity.

## Script review status

Syntax was checked by Python `ast.parse`; no packaging execution or workloads
were run during this preparation. Runtime correctness of the packager itself is
not claimed from that syntax check.
