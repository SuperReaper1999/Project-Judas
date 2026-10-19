# Reproducing this scoped cleanup

Run from the matching full repository checkout. Linux requires the documented
SDL/GLM/CMake/compiler prerequisites, Python 3.11+, and functioning OpenGL.
Networking checks require actual loopback sockets. Offscreen software GL is a
functional alternative, not GPU-performance or pointer-capture evidence.

Use a fresh owned directory instead of overwriting an earlier receipt:

```sh
cmake -S . -B /tmp/judas-cleanup-build -DCMAKE_BUILD_TYPE=Release \
  -DJUDAS_BUILD_CHANNEL=Experimental -DJUDAS_ENABLE_VALIDATION=ON
cmake --build /tmp/judas-cleanup-build --target judas_prealpha_tests \
  judas judas_editor judas_export judas_script_check judasjs_examples_tests \
  judas_script_tests judas_script_developer_tools_tests \
  judas_script_tools_editor_tests judas_script_developer_consumer_tests \
  judas_material_tests -j2
SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 \
  ctest --test-dir /tmp/judas-cleanup-build -L prealpha --output-on-failure
```

The first run here passed 43/47; all four socket tests failed in a restricted
namespace. `SANDBOX_SOCKET_PROBE.json` establishes EPERM independently. The same
four binaries then passed natively, with all test settings unchanged:

```sh
python3 scripts/prealpha_validate.py --build-dir /tmp/judas-cleanup-build \
  --group network --output /tmp/judas-cleanup-network
```

Do not hide those failures or claim a single green CTest run from this split result.
`focused-gates/` retains every native wrapper receipt and raw log, and `ctest.xml`
retains all 47 original observations. `native-network/` is the explicit follow-up.

## API, type, documentation and packaging controls

```sh
npm ci --ignore-scripts --prefix tools/verification
node tests/TypeScriptToolingTests.mjs
node scripts/check_judasjs_api.mjs
node docs/evidence/prealpha-doc-cleanup-20261019/reproduce_type_controls.mjs
python3 -m unittest discover -s tests -p test_current_docs.py -v
python3 scripts/check_current_docs.py --require-tracked
python3 -m unittest discover -s tests -p test_prealpha_validation.py -v
python3 -m unittest discover -s tests -p test_validation_registration.py -v
python3 -m unittest discover -s tests -p test_cmake_test_targets.py -v
python3 scripts/prealpha_packaging_checks.py --output /tmp/judas-cleanup-packaging.json
python3 scripts/prealpha_distribution_checks.py --output /tmp/judas-cleanup-distribution.json
```

The preserved baseline declaration and baseline manifest-loader function are
real pre-cleanup sources, not invented mutants. The same type fixture detects
three rejected valid camera calls and one unsafe material dereference in that
baseline; current declarations have zero diagnostics. Six malformed manifests
accepted by the baseline loader are rejected by the current implementation.

Execute examples through the ordinary VM, then compare its actual surface:

```sh
SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 SDL_AUDIODRIVER=dummy \
  /tmp/judas-cleanup-build/judasjs_examples_tests /tmp/judas-cleanup-cookbook
node scripts/check_judasjs_api.mjs --runtime /tmp/judas-cleanup-cookbook/surface.json
```

The full cookbook requires its matching consumer projects/assets. A curated
source-engine archive deliberately does not include every consumer or historical
fixture; use the full checkout for this command. Individual cases can be selected
as the second argument (`surface`, `minimal`, `authoring-runtime`, etc.).

## Editor and relocated package

The captured `validate*.py`, `finish.py` and `editor_native.py` record the exact
workspace automation; their original `.cache` location is significant. The direct
commands below avoid depending on those cached paths. Use a private project copy
because editor tests deliberately author/save within their owned workspace:

```sh
cp -a projects/character_demo /tmp/judas-cleanup-editor-project
JUDAS_EDITOR_AUTOTEST=/tmp/judas-cleanup-editor \
  JUDAS_EDITOR_AUTOTEST_AUTHORING=1 \
  /tmp/judas-cleanup-build/judas_editor /tmp/judas-cleanup-editor-project/character_demo.judasproj
/tmp/judas-cleanup-build/judas_export \
  /tmp/judas-cleanup-editor-project/character_demo.judasproj \
  /tmp/judas-cleanup-export /tmp/judas-cleanup-build/judas
python3 scripts/m60_package_smoke.py /tmp/judas-cleanup-export \
  /tmp/judas-cleanup-moved-receipt unique_cleanup_label
```

The last command uses Linux/X11 tools and libX11 to close only its own window.
It makes the moved installation read-only and runs from unrelated `/tmp`.
Provide the desktop's normal DISPLAY and isolated XDG_DATA_HOME when needed.
Editor screenshots establish actual framebuffer output, not human UX acceptance.

## Preserved unresolved contract

Expected exit **1**, actual original grid-query assertion still failed:

```sh
python3 scripts/prealpha_validate.py --build-dir /tmp/judas-cleanup-build \
  --group known-failures --output /tmp/judas-cleanup-known-failure
```

This is not a green engine result. FLU-001 remains deferred and was not retuned or
rerun here. Existing dense measurements, legacy-save history and Windows/Android
receipts retain their original scope.

Binaries are represented by actual hashes/embedded identities, not committed ELF
files. The recorded build uses dirty `be1b7d63` inputs; a subsequent source commit
is not retroactively embedded in those artifacts. Reconfigure/rebuild after
changing source or Git provenance before guarded gates.
