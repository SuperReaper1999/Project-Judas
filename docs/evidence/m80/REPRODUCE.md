# M80 reproduction and manifest notes

Commands below run from the repository root. They are reproduction instructions,
not a request to repeat passing validation merely for checkpointing.

## Focused Release build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2 --target \
  judas_bending_tests judas_bending_application_tests \
  judas_deformable_tests judas_fracture_tests judas_structural_tests \
  judas_joint_tests judas_deformable_application_tests \
  judas_structural_application_tests judas_deformable_bake \
  judas_editor judas judas_export
```

The exact final sequential driver is retained as `RUN_FINAL_CHECKS.py.txt`.
Adjust its repository root and scratch directory for another machine, then run
with Python. It preserves ordinary fixed settings and uses isolated
`XDG_DATA_HOME`, offscreen SDL video and dummy audio. Cold **write** precedes
**read** in a separate process. A read observes full physical state after normal
script initialization but before any resumed fixed step, then resumes motion.
`linux/final-cases.json` records each exit status.

Do not link preserved old engine libraries against current changed C++ headers.
The baseline fixture and original failure logs are under `development/`, with
the required starting revision/matching-header rule in `development/BASELINE.md`.

## Editor and export

Copy the project to a scratch directory before editor automation, since that
check creates a new cook and saves the copy:

```sh
cp -a projects/bending_lab /tmp/judas-m80-editor-project
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  XDG_DATA_HOME=/tmp/judas-m80-editor-data \
  JUDAS_EDITOR_AUTOTEST="$PWD/docs/evidence/m80/linux/editor" \
  JUDAS_EDITOR_AUTOTEST_BENDING=1 \
  build/judas_editor /tmp/judas-m80-editor-project/bending_lab.judasproj

build/judas_export projects/bending_lab/bending_lab.judasproj \
  /tmp/judas-m80-bending-lab-20261013 build/judas
```

The exported runtime is an ordinary no-argument launch from its package. The
normal existing `JUDAS_TEST_SCRIPT` harness can use `export-smoke.txt`; screenshot
paths need changing for another checkout. Launch from an unrelated directory:

```sh
cd /tmp
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  XDG_DATA_HOME=/tmp/judas-m80-package-data \
  JUDAS_TEST_SCRIPT=/home/conner/Documents/GitHub/Project-Judas/docs/evidence/m80/export-smoke.txt \
  /tmp/judas-m80-bending-lab-20261013/judas
```

For an interactive desktop omit the offscreen/dummy environment and test-script
environment variable. See [controls and human checklist](HUMAN_CHECKLIST.md).

Moved package: `/tmp/judas-m80-moved/Bending Lab`; actual packaged application
cold write/read uses `judas_bending_application_tests` with that directory's
`game.judasproj`, modes `write`/`read` and separate evidence output directories.
Logs and exit results are retained under `linux/package-*`.

## API tooling

M80 adds no public binding. A transient pinned TypeScript 5.9.3 installation in
`/tmp` was used, with no permanent project build dependency:

```sh
node scripts/check_judasjs_api.mjs --typescript \
  /tmp/judas-m80-typecheck/node_modules/typescript
```

## Fingerprints and retained traces

- `CHANGED_FILES.txt`: exact intended checkpoint inventory, repository-relative.
- `FINAL_SOURCE_SHA256.txt`: final engine source plus new/affected tests, tools,
  lab assets and current documentation. Evidence and the manifest itself are
  excluded to avoid recursive hashes.
- `BINARY_SHA256.txt`: absolute paths of the tested Release binaries/archive and
  exported runtime. Binaries/packages are not committed.
- `EVIDENCE_SHA256.txt`: all M80 evidence except this manifest itself.

Verify from repository root using `sha256sum -c` on each manifest. Source and
evidence paths are portable within another exact checkout; binary absolute paths
must be adapted while retaining the expected hashes.

Five new M80-captured legacy-lab profiler traces are stored as deterministic gzip
files to avoid needless repository size. `compressed-traces.json` records their
compressed and original hashes/sizes. `gzip -dc file.json.gz` reconstructs exact
original JSON bytes; do not overwrite historical research or evidence. The
underlying tests, commands, input settings, observed failures and thresholds are
retained without reinterpretation as a broader certification.
