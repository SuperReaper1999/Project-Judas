# Reproducing M93

Run from the repository root on a native Linux desktop with OpenGL 3.3. CMake,
a C++17 compiler and the engine's existing desktop prerequisites are required.
No new runtime package manager, service or mobile dependency is introduced.

## Candidate

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target judas_cook -j2
mkdir -p .cache/m93
# Exact benchmark/test inputs, including source PNGs/OBJ and final cooked bytes:
tar -xJf docs/evidence/m93/FIXTURES.tar.xz -C .cache/m93
python3 scripts/m93_validation.py --build --output .cache/m93/reproduction
python3 scripts/m93_determinism.py
python3 scripts/m93_delivery.py
```

`m93_validation.py` supports `--typescript /path/to/typescript` and
`--model-fixture /path/to/Skateboarder.fbx`. TypeScript is developer-only, reused
from the existing temporary M50/M90 tooling on this machine. The additional M66
import fidelity regression requires its original external Skateboarder FBX; that
unrelated licensed source is not copied into M93. Its path/hash are recorded in
`EXTERNAL_INPUTS.json`. Core M93 tests use the included fixtures. Do not treat a
missing FBX/checker as a passing check.

Use a fresh validation output directory: some observers intentionally refuse to
overwrite an owned project. Delivery similarly requires a fresh
`.cache/m93/delivery` (archive any previous result first). It copies the authoring
project before editor automation, intentionally changes a copied source to prove
stale-export rejection, then exports the original project. It runs the moved game
from an unrelated temporary working directory. Save storage is isolated by
`XDG_DATA_HOME`. The 20-source determinism check recooks to an ignored directory
and compares each output with the actual project artifact.

The optional generators `m93_assets.py` and `m93_lab.py` recreate source fixtures
and lab authoring files. They require Pillow only for authoring. Prefer the exact
archived fixtures for reproducing recorded numbers; PNG encoders/platform math
can produce different bytes across tool versions. The lab generator preserves
its separately authored `Assets/scripts/lab.js`. Recook its four OBJ/sixteen PNG
sources using `judas_cook` after regenerating sources. Normal map/linear-data
roles must be selected explicitly.

## Matching accepted baseline

Baseline is accepted M92 `bb3dbc402629a60c22738da8520e5017998709f1`.
The engine archive and runtime were frozen **before** M93 edits. Their hashes,
hardware, original timestamp and matching link commands are retained under
`baseline/`. Build that commit normally in an isolated checkout to reproduce its
engine library; do not replace the current worktree or protected evidence.

The paired observer is `tests/AssetPipelineBenchmark.cpp`, compiled against each
revision's **own** headers and image decoder. The old TextureData ABI differs.
Do not link the frozen engine to the current `judas_app_icon` library. The helper
`scripts/m93_link_baseline.py` rebuilds the frozen TextureLoader/AppIcon and links
the observer using `.cache/m93/baseline/src`, its frozen engine archive, and
unchanged dependencies from the current build. `FINAL_LINK_COMMANDS.json` records
the exact compiler/linker invocations. The original baseline test source and link
command for the pre-existing shutdown-observer failure are also retained.

```sh
python3 scripts/m93_link_baseline.py
python3 scripts/m93_measure.py
python3 scripts/m93_summarize.py
```

Runs alternate order over two rounds. Each starts a fresh process, loads one mesh
and one 2048² texture, warms 12 frames and records 60 per view configuration.
128 instances, 960×540 main view and independent 480×270 secondary view; ordinary
lighting, no dynamic lights/shadow passes. Four controls: M92 source, candidate
source, cooked full detail and cooked automatic LOD. Source geometry/settings and
camera paths match; the cooked-full control isolates packing from simplification.

Timing includes `glFinish` before the sample ends, excluding swap/pacing. It is
completed-render wall time, not pure CPU time or a gameplay FPS claim. M56 keeps
separate CPU submission scopes and asynchronous GPU timestamp pairs. Its bounded
120-frame history retains fewer first-view samples; sample counts are reported.
Peak process RSS is from `/usr/bin/time -v`, including the common observer,
driver and readback work. OS cache is not flushed: these are cold process/resource
loads, not guaranteed cold-disk measurements. Desktop activity remains; GPU/process
observations before/after each run are retained. No other Judas game was detected.

`view1.png` / `view2.png` capture the main framebuffer in one-/two-view runs.
They do not read the secondary target itself. Independent secondary/split output
is exercised by residency/render-camera tests and actual standalone split-screen
captures. Foreground quality metrics threshold luma difference from each image's
corner background; they are not certified depth silhouettes or Hausdorff errors.

## Development evidence

`development/NOTES.md` classifies actual failures and observer/dispatch corrections.
`DEVELOPMENT_ARTIFACTS.tar.xz` retains available draft source/fixtures/probes, without
executables. Earlier invalid LOD timings are classified as invalid acceptance data;
they must not be used as a speedup claim. Original baseline measurements without
explicit GPU completion are diagnostic only. Final timings use the matched
completed-frame observer.

`FINAL_SOURCE_SHA256SUMS`, `BINARY_SHA256SUMS`, `CHANGED_FILES.txt` and receipts tie
the accepted automated candidate to source, baked assets and actual executables.
The binaries were built from dirty Experimental based on M92 before checkpointing;
embedded Git labels may identify that base. The source manifest is the exact
candidate identity. No human or non-Linux M93 acceptance is implied.
