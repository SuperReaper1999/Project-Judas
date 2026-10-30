# Tilemap Stage 4 evidence

These are Linux automated results for the source listed in FINGERPRINTS.json,
based on 051518ff. No human acceptance, main promotion or patch publication.
RESULTS.json records exact as-built identities and binary hashes for eight selected
native gates. Source hashes include new untracked implementation and project files
that the generic tracked-diff field alone cannot capture. The package was built
from that source snapshot; its receipt may say dirty and does not pretend to have
been rebuilt from the later checkpoint commit.

Reproduction from repo root (Release, Experimental channel):

```sh
cmake --build .cache/2d-stage1-build --target judas judas_editor judas_export judas_tilemap_tests judas_tilemap_editor_tests judas_tilemap_render_tests judas_tilemap_application_tests judas_tilemap_benchmark --parallel 2
python3 scripts/prealpha_validate.py --build-dir .cache/2d-stage1-build --test tilemap --test tilemap-editor --test tilemap-render --test tilemap-application --test planar-physics --test sprite-animation --test editor-preferences --test mk2-physics --output .cache/2d-stage4-rerun --expected-channel Experimental
.cache/2d-stage1-build/judas_tilemap_tests .cache/2d-stage4-rerun/tilemap --cold
.cache/2d-stage1-build/judas_tilemap_benchmark projects/tilemap_workshop/Tilemap_Workshop.judasproj
node scripts/check_judasjs_api.mjs
python3 -m unittest discover -s tests -p test_cmake_test_targets.py
python3 -m unittest discover -s tests -p test_validation_registration.py
```

The cold command must follow the producer test in its own process. It compares
actual restored native body state, not cached project status.

Editor test: copy the workshop and isolate XDG_DATA_HOME. Run judas_editor on that
copy with JUDAS_EDITOR_AUTOTEST=<output-prefix> and
JUDAS_EDITOR_AUTOTEST_TILEMAP=1, using a real desktop/X11. The checked screenshots
are dark 100% and light 135%. These tests move/restore the mouse and exit; don't
run them over an operator's ongoing edits. Existing editor preferences test covers
the general registry; screenshots and pointer assertions cover actual painting.

Export:

```sh
.cache/2d-stage1-build/judas_export projects/tilemap_workshop/Tilemap_Workshop.judasproj /tmp/tilemap-package .cache/2d-stage1-build/judas
```

Move that complete package elsewhere and run `./judas game.judasproj` there.
For the recorded tour, JUDAS_ENGINE_ROOT pointed to a missing directory and
JUDAS_TEST_SCRIPT named final-tour.txt (adjust its absolute screenshot destinations).
XDG_DATA_HOME was a writable /tmp directory, not the source checkout. The first
relocated-tour.log failed because sandbox protection blocked the default home save
directory; relocated-tour-writable.log is the successful rerun, no engine fix.
Native desktop execution needs access to the desktop; offscreen GL tests use
Mesa llvmpipe, editor screenshots use NVIDIA GTX970. No Windows/Android inference.

Retained failed editor-tests.log: incorrect seven-cell expected count for an
inclusive negative-coordinate span (eight is correct). editor-ui-build.log retains
an intermediate build. editor-dark/editor.log preserves the first pointer-injection
failure. desktop-dark and desktop-light use corrected actual SDL input and pass.
No production assertion was weakened. The speculative empty test discovery is not
counted: the real build-organization module ran 18/18; registration ran six passing
checks and two unavailable-generator skips (raw unittest reports eight, skipped=2).

Benchmark five-sample p95 equals worst. It measures bounded compile cost and a
settled physical grid, not adversarial collisions or overall FPS. Full runtime
application frame timings are software GL on a shared host and exclude waits/swap.
No additional GPU performance claim is made.
