# Starter legacy HUD leak — 2026-10-25

Operator screenshot: a modern starter playing its ordinary rotating-box script
shows historical ship/player debug text and inactive legacy shortcuts.

Root cause: InteractivePlay::Frame gated the old HUD on drawHud, HUD visibility
and authored UI preferences, but omitted world.legacyGameplay. The starter
correctly disabled legacy gameplay; rendering alone still exposed its old HUD.
The engine did not add UI content to the user's project.

Correction: require legacy gameplay before drawing that HUD. Project-authored UI
retains its separate draw path; explicitly legacy projects retain their HUD and
debug-overlay preference. No project files, controls or gameplay changed.

Release editor and UI test targets built successfully. The focused native Linux
UI suite passes **88 checks**. Added rendered-frame controls cover modern and
legacy worlds, no authored UI, authored UI and debug-overlay preference. The
identical test linked with original InteractivePlay.cpp from `bbd031bc` fails
exactly the two modern-HUD checks (**88 checks, two failures**); raw logs retained.
An initial test draft incorrectly assumed authored UI disables the legacy HUD
despite its explicit debug-overlay preference. The final test checks both values.

Reproduction:

```sh
cmake --build build --target judas_ui_production_tests judas_editor -j 2
SDL_AUDIODRIVER=dummy build/judas_ui_production_tests /tmp/judas-hud-isolation
sha256sum -c docs/evidence/starter-hud-isolation/SOURCE.sha256
```

Operator accepted text/folder corrections were promoted unchanged to main at
`bbd031bc6642ce26ee11ed74b631af4cabe9278d`. This subsequent HUD correction is an
Experimental follow-up awaiting human acceptance. Existing editor sessions and
the operator's new project remain untouched. Restart into the rebuilt editor to
inspect it; a running older binary cannot acquire this native-code correction.
