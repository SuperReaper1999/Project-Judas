# Reproduce the maintainability checks

Run commands from the repository root. These are developer checks, not game runtime dependencies. Current source must be the Pass 3 candidate described by `FINAL_SOURCE_FILES.json`; frozen before-source files are included here. Do not treat a later source revision as this candidate.

## Source and CMake controls, without a native build

Python 3 and CMake 3.20+ are sufficient:

```sh
python3 docs/evidence/prealpha-pass3/maintainability/tools/verify_refactor.py
python3 docs/evidence/prealpha-pass3/maintainability/tools/verify_cmake.py
python3 docs/evidence/prealpha-pass3/maintainability/tools/cmake_execution.py
python3 docs/evidence/prealpha-pass3/maintainability/tools/verify_origin_format.py
python3 -m unittest discover -s tests -p test_cmake_test_targets.py -v
```

The source tools locate the repository through its ancestor `engine-version.json` and `src/ScriptSystem.cpp`, not an absolute workstation path. Generated reports go under `.cache/prealpha-pass3/maintainability/`; preserved evidence is not replaced. The origin proof is read-only apart from creating that cache parent. The CMake tests execute real declarations with bounded command observers; synthetic Windows/Android conditions do not constitute native platform qualification.

The checked-in patches separate extraction, normal mechanical formatting and the parent's subsequent origin-transaction formatting. For example, `git apply --check --reverse docs/evidence/prealpha-pass3/maintainability/MECHANICAL_ORIGIN.patch` checks whether the exact recorded mechanical change is present. It does not apply a change.

## Matched native observers: Linux prerequisites

`tools/native_comparison.py` is a Linux/GNU-style reproduction helper, not a portable engine build frontend. It requires:

- A Git checkout, Python 3, `/usr/bin/c++`, and the normal Linux engine build dependencies described in the repository README.
- A **Release build using the Unix Makefiles generator**, in the current checkout's `build/` directory. The helper reads `build/CMakeFiles/judas_script_tests.dir/flags.make` and `link.txt`; a Ninja-only build does not provide these files.
- A successfully built `judas_script_tests` target and its static dependencies. This provides `libjudas_engine.a`, profiler/build-identity/icon/material/audio/navigation/QuickJS/text/job libraries, and the platform libraries recorded in `link.txt`.
- SDL/OpenGL support for the actual UI/application cases. The runner requests `SDL_VIDEODRIVER=offscreen`, `SDL_AUDIODRIVER=dummy` and `LP_NUM_THREADS=2`. Missing graphics support is a prerequisite failure, not a passing UI check.
- The baseline `f647d7fd95f60d3a37dba70a51f4e45dc41c2922` engine archive. The original frozen copy was `.cache/prealpha-pass3/baseline/bin/libjudas_engine.a`; it is intentionally not committed. Its recorded SHA-256 is `72abdc886dddded90ee8d5379ec81d2c0bbb7fc1d0df594c60259b7cbe1bfe69`.

For a fresh candidate checkout, build the necessary current target once:

```sh
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DJUDAS_BUILD_CHANNEL=Experimental
cmake --build build --target judas_script_tests -j2
```

If the original frozen archive is unavailable, rebuild the baseline in a separate worktree rather than compiling historical source into the live checkout:

```sh
git worktree add --detach /tmp/judas-refactor-f647 f647d7fd95f60d3a37dba70a51f4e45dc41c2922
cmake -S /tmp/judas-refactor-f647 -B /tmp/judas-refactor-f647/build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DJUDAS_BUILD_CHANNEL=Experimental
cmake --build /tmp/judas-refactor-f647/build --target judas_engine -j2
export JUDAS_BASELINE_ARCHIVE=/tmp/judas-refactor-f647/build/libjudas_engine.a
```

A fresh baseline build can have different bytes because the compiler, dependency builds, paths and provenance differ. Record its real hash; do not replace the original recorded hash or claim byte-identical reproduction without checking it. Use the same supported compiler/dependency configuration for a controlled comparison.

## Observer modes and their order

```sh
python3 docs/evidence/prealpha-pass3/maintainability/tools/native_comparison.py compile-before
python3 docs/evidence/prealpha-pass3/maintainability/tools/native_comparison.py before
python3 docs/evidence/prealpha-pass3/maintainability/tools/native_comparison.py link-after
python3 docs/evidence/prealpha-pass3/maintainability/tools/native_comparison.py after
```

- `compile-before` compiles the four original test entry points and four Application support sources once, verifies original source bytes against HEAD, adds cache-only paths/observation prints, and links against `JUDAS_BASELINE_ARCHIVE` (or the original frozen-cache path).
- `before` executes those baseline observers and records original assertions plus state/physical observations.
- `link-after` reuses the identical observer/support objects and links against current `build/libjudas_engine.a` and the same build dependency paths.
- `after` executes the current observers and records their observations.

The helper now creates all missing cache parents. `OBSERVER_CACHE_PATH_CONTROLS.json` preserves a negative control of the former missing-parent failure and a positive fresh-directory control. Neither control invokes a compiler or a native test.

Compare observations independently of timing:

```sh
python3 - <<'PY'
import json
from pathlib import Path
root = Path('.cache/prealpha-pass3/maintainability')
before = json.loads((root / 'NATIVE_BEFORE.json').read_text())
after = json.loads((root / 'NATIVE_AFTER.json').read_text())
assert len(before) == len(after) == 4
for old, new in zip(before, after):
    assert old['name'] == new['name']
    assert old['exit'] == new['exit'] == 0
    assert old['summaries'] == new['summaries']
    assert old['observations'] == new['observations']
    print(new['name'], new['summaries'], len(new['observations']), 'identical observations')
PY
```

This is a scoped refactor comparison, not a clean-machine performance benchmark: only the engine archive changes between links; observer objects, support objects and dependency paths are shared. The recorded run passed 114 assertions and matched nine state/physical observations. That run preceded the parent's final token-identical origin formatting. Its exact archive/binary hashes and stage are preserved; final post-format original tests are in [the combined Linux results](../final-linux/RESULTS.json). No speedup is inferred from observer wall times recorded during concurrent compilation.

## Final combined candidate evidence

The [combined Linux result index](../final-linux/RESULTS.json) records the 21-contract focused manifest and 21 supplemental records, all successful. Relevant final outputs include script/debug/reload/UI checks, M70/M77 cold-process continuation, [312 cookbook checks](../final-linux/cookbook.log), [473 public symbols / 40 exports / 51 typed examples with live enumeration](../final-linux/api_types_drift.log), [editor Play/Stop with identical authored scene](../final-linux/editor_play_stop.log), and relocated export startup. This does not certify every engine capability or human preview-control acceptance.
