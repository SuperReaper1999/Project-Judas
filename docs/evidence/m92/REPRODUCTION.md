# Reproduce M92

Run from the repository root. Native desktop GL is required; do not interpret a
sandbox without GPU access as a shader failure. Keep CPU builds and GPU timing
runs serial. No command below stops unrelated applications.

## Build and focused behaviour

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
python3 scripts/m92_validation.py --build --output .cache/m92/reproduction \
  --typescript /absolute/path/to/existing/typescript
cmake --build build --target judas judas_editor judas_export \
  judas_rendering_scale_tests judas_rendering_shadow_scale_tests \
  judas_rendering_lab_performance -j 2
python3 scripts/m92_delivery.py
```

The TypeScript checker is optional and is not a runtime/build dependency. This
machine used `.cache/m90/types/node_modules/typescript` (5.9.3). Delivery writes
only its owned ignored copies and package; preserve/rename a previous
`.cache/m92/delivery` before rerunning because the exporter expects a fresh
destination. The final follow-up also builds/runs `judas_ui_tests`,
`judas_lighting_tests` and `judas_interactable_tests`.

## Frozen baseline

Baseline is `61a9df6acaa102c0a9fcf1006866a01fc3b33a89`. Its source and complete
build must be separate from the working candidate. The original archive/runtime
and observer binaries remain locally in `.cache/m92/baseline`; only their hashes,
commands and observations are committed. To recreate from a fresh checkout:

1. Extract that exact Git commit into a separate temporary directory with
   `git archive`, preserving its dependency sources; configure Release and build
   the `judas_engine` target with two jobs.
2. Copy the resulting `libjudas_engine.a` into `.cache/m92/baseline/`.
3. Extract the baseline `src/` tree into `.cache/m92/baseline/source/src/`.
4. Run `python3 scripts/m92_link_baseline.py` from the candidate root after its
   camera-test target has built. The helper records exact compiler/link arguments.
   It uses **baseline headers** with the baseline archive. Mixing the new
   Renderer object layout with an old EngineHost allocation is invalid.

## Measurements

```sh
python3 scripts/m92_measure.py --output .cache/m92/reproduce-measure --rounds 2
build/judas_rendering_lab_performance .cache/m92/reproduce-watch
python3 scripts/m92_summarize.py .cache/m92/reproduce-measure \
  --watch .cache/m92/reproduce-watch --pixels
```

The summary's optional image comparison uses Pillow, not an engine dependency.
Keep 960×540, 12 warm-up/60 measured frames, source settings and geometry
unchanged. Compare equal-quality 0/5-light and two-shadow cases separately from
the expanded 128-light capability. Watch-tour observations use actual wall time,
360 frames, swap interval one and discard the first 30 frames. Hardware and
desktop load can change results; no universal FPS promise is implied.

Raw evidence is losslessly stored in `raw/observations.tar.gz`; `ARTIFACTS.json`
maps original paths to member hashes. Extract outside the tracked tree if desired.
It includes initial diagnostics, alternating pairs, final-linked pairs, M56
profiles, per-frame CSV, failure/follow-up logs, build logs, editor screenshots,
export receipts and final delivery step inputs. It contains no executables,
static libraries, caches or unrelated assets.

```sh
python3 docs/evidence/m92/verify.py
python3 docs/evidence/m92/verify.py --binaries  # requires this machine's retained builds
python3 docs/evidence/m92/verify.py --git       # check exact staged/index blobs too
```

The package itself is generated output and stays ignored. Developer docs/types
remain available in `docs/JUDASJS.md`, `docs/judas.d.ts` and the current rendering
reference; they are not all copied into a runtime distribution.
