# Experimental browser offline-game qualification

This stage qualifies bounded offline 2D gameplay, not every Judas capability.
Main and published Alpha packages remain separate. Browser export still requires
`-ExpBrowserSupport=1`, the pinned toolchain and the hosting/storage requirements
in [BROWSER_SUPPORT.md](BROWSER_SUPPORT.md).

## Reproduction

Use a source build of `judas_scene_author`, `judas`, `judas_export`, and an
Emscripten browser runtime with matching provenance. Work in fresh directories:

```sh
python3 tests/browser/prepare_offline_fixture.py projects/beacon_grove /tmp/beacon-observed build/judas_scene_author
python3 tests/browser/prepare_traversal_fixture.py projects/2d_traversal_workshop /tmp/traversal-observed
```

Export each copied project using the normal `tools/build_browser.py` command.
Install Playwright 1.64.1 and its Chromium/Firefox browsers in an isolated tooling
folder. It is a test dependency, not a game/runtime dependency. Run sequentially:

```sh
export PLAYWRIGHT_MODULE=/absolute/test-tools/node_modules/playwright
node tests/browser/offline_game.cjs /absolute/beacon-web /tmp/beacon-chromium chromium
node tests/browser/offline_game.cjs /absolute/beacon-web /tmp/beacon-firefox firefox
node tests/browser/traversal.cjs /absolute/traversal-web /tmp/traversal-chromium chromium
node tests/browser/traversal.cjs /absolute/traversal-web /tmp/traversal-firefox firefox
```

Outputs must be fresh. Each driver owns a loopback server (8796 or 8797) and its
own isolated browser processes. Do not run drivers sharing a port concurrently.
Beacon's profile is restarted by closing the entire browser process, not just
reloading a tab. A real server/origin/profile is required for cold persistence.

## Meaningful controls

Beacon keeps the original game policy byte-for-byte in `game-policy.js`, with a
public JudasJS subclass observing pose, actual entity validity, audio and save
status. It adds an original PCM tone through an ordinary authored AudioEmitter;
the real shipping game stays silent. Web Audio analysis taps the actual mixer
output; it never substitutes a JavaScript oscillator. The test covers pause,
real trigger pickups, early-interaction rejection, resources without proximity,
animation-timed gate removal, walking through the former gate, victory, defeat,
retry, binary save recovery across browser processes, physical endpoint recovery
and ordinary UI Quit/audio-device destruction.

Traversal keeps its original movement and UI policy and adds public observations
to its controls script. It checks walking below a one-way edge, jumping through
and landing above, collision from two sides of a blocked grid cell, a free route,
scene switching, durable save publication and actual grid pose restoration. Its
second service request is the load transaction: inspect `saves.status(2)` after
restoration, rather than confusing the old saved script request ID with that load.

Observation timing must follow real endpoints. A six-frame observer can miss
pickups during software rendering; the final observer reports each UI frame and
waits for released movement to become stationary. No controller speed, collision
shape, gameplay threshold, physics timestep or engine solver budget is altered.
Authored ground centre is y=0.1 and the grid spawn is (0,2); assertions use the
actual authored geometry, not a presumed origin.

## Boundaries and evidence

Recorded runs and failures are under `docs/evidence/browser-offline-20261031`.
The original worker-audio negative control must fail with -203; corrected audio
must produce a running context and nonzero native-mixed PCM. Automated analysis
is not human speaker/headphone acceptance. Native audio and Beacon regressions
provide a separate desktop preservation check.

Chromium uses software ANGLE/SwiftShader; Firefox uses its normal isolated Linux
backend. These are functional tests, not matched performance benchmarks or
Windows/mobile/Safari certification. Headless browser identity strings may mask
the GPU; do not infer hardware from a Firefox renderer string. Hidden-tab
scheduling, context-loss recovery, streamed/modded worlds, networking, advanced
3D effects, responsive surfaces and mobile input remain separate qualification.

Next work: practical editor export/hosting UX and explicit preflight diagnostics,
then broader rendering, lifecycle and device qualification. No web editor or
unconditional platform enablement is implied by this stage.
