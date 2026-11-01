Optional Utility AI — verified Experimental checkpoint, 2026-11-01

Read ../../GAME_AI_QUALIFICATION.md for implementation, limits, measured costs
and the human checklist. Base source: ecb0ef5aa9eaed047068561de4b58fa96a3e1dd3.
The containing commit is the new checkpoint. Main/Alpha fluid lock unchanged.

FINGERPRINTS.json lists exact changed implementation/content and tested binaries.
It records pre-checkpoint dirty-build provenance, not an assertion that binaries
were rebuilt from the later documentation/evidence commit. Source hashes match
final implementation. STANDALONE_MANIFEST.json inventories the runnable relocated
package currently in .cache/utility-ai-20261101/LittleHaven-standalone (not Git).

Final acceptance evidence:
 SHIPPING_REGRESSIONS.json: 12 focused suite commands, all exit zero
 SHIPPING_APPLICATION.json: real modern saves, same-session and cold process
 SHIPPING_EXPORT.json: complete renamed package, cwd /tmp, no engine-root env
 MODERN_FINAL_COLD.json: modern participant fixture across separate processes
 shipping-*.log: raw final suite, application, and export output
 shipping-core-benchmark.csv: 0/100/1000 native agents, synchronized/staggered
 shipping-script-benchmark.csv: actual 0/100/500-script cases, AI off/on
 shipping-*-benchmark-time.log: whole-process peak RSS and exit status
 api-final.log: AST/declaration/negative/type fixture checks, no runtime enumeration
 registration-final.log, manifest-final.log: native registration infrastructure
 editor-ai-panel.log/.edit.png/.play.png: actual panel + editor Play/Stop smoke
 workshop-final.png: readable 1280x720 game screenshot
 workshop-watch-first.log, workshop-manual-first.log, workshop-first-results.log:
 80/90-second native gameplay/eligibility/physical-travel checks
 application-shipping/expected.json and actual committed .save: independent oracle
 modern-final-cold/save.snapshot: real modern state fixture
 final-source/: exact new/changed files and editable project

Earlier failures/limited checks are retained:
 workshop-first.log: malformed authored save identity; project corrected
 script-first.log: expected immediate teardown; actual safe-boundary contract tested
 cold-read/write.log: legacy state injection is NOT modern native continuation proof
 build*.log: initial compile mistakes plus final clean builds (not runtime evidence)
 core-benchmark-first.csv/resource.txt and other benchmark logs: earlier measurements
 editor-application.log: initial smoke; editor-ai-panel uses correct isolated SDL
 user-data directory. Personal workspace preferences were compared unchanged.

These files are not alternative current acceptance results. No original fixture,
physics expectation or fluid threshold was weakened. No Windows/Android/browser
execution qualification or human acceptance is claimed.

Reproduction from this checkpoint on Linux:
  Follow docs/BUILD_RELEASES.md / normal source prerequisites.
  cmake -S . -B .cache/2d-stage1-build -DCMAKE_BUILD_TYPE=Release
  cmake --build .cache/2d-stage1-build -j2 --target judas judas_editor judas_export \
    judas_utility_ai_tests judas_utility_ai_script_tests judas_utility_ai_editor_tests \
    judas_utility_ai_application_tests judas_utility_ai_benchmark
  Run each target in SHIPPING_REGRESSIONS.json (build those existing targets too).
  judas_utility_ai_script_tests write <new-disposable-path>
  judas_utility_ai_script_tests read <same-path>
  judas_utility_ai_application_tests projects/ai_workshop/AI_Workshop.judasproj <fresh-path>
  judas_utility_ai_application_tests projects/ai_workshop/AI_Workshop.judasproj <same-path> cold
  Use a real desktop GL backend / valid DISPLAY for application/editor runs.
  judas_utility_ai_tests --benchmark
  judas_utility_ai_benchmark
  Run timing processes sequentially without concurrent compilers/tests.
  node scripts/check_judasjs_api.mjs
  python3 scripts/prealpha_validate.py --check-manifest

Native Workshop harness:
  JUDAS_WORLD_STATE=none JUDAS_TEST_SCRIPT=<absolute>/projects/ai_workshop/Tests/watch.txt \
    judas projects/ai_workshop/AI_Workshop.judasproj > watch.log
  Repeat with Tests/manual-inputs.txt, then:
  python3 projects/ai_workshop/Tests/qualify.py --watch-log watch.log --manual-log manual.log

Standalone:
  judas_export projects/ai_workshop/AI_Workshop.judasproj <new-package-dir> <absolute-Release-runtime>
  Move/rename the WHOLE package. Unset JUDAS_ENGINE_ROOT; run its judas executable
  from /tmp or another unrelated directory. Test input can be an external harness
  file; gameplay/module/assets must load exclusively from the packaged content.

Editor smoke on an isolated copy:
  Set XDG_DATA_HOME=<isolated-user-data> (SDL preference location), not merely
  XDG_CONFIG_HOME. JUDAS_EDITOR_AUTOTEST=<output-prefix> runs normal edit/Play/Stop,
  duplication/undo and scene-equality checks and exits. To open the AI panel in
  this test, an isolated version2 workspace's current.panels contains utility-ai.
  Dark/light at multiple scales and authoring/conflict tests live in the dedicated
  editor test target. Never point automated scene saving at a user's game.

Archive has source/log/data/images only, no ELF/library/build caches or research.
Every archived member was read back and hash-compared. ARCHIVE_MANIFEST.json
records hashes/sizes; SHA256SUMS.txt verifies the outer package files. Reproduce
in a disposable directory: test drivers write their own output fixtures.
