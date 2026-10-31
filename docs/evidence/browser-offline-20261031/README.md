# Browser offline-game stage — 31 October 2026

Experimental parent: `d1dc3bb8348c0389ca69010ebecc0d3b27b09731`.
Main remains `49c4f3d9e29debe669411f01501e0276dea23231`.
No release, release tag, channel or fluid-unlock changes.

## Correction and results

The miniaudio Web Audio backend attempted DOM/AudioContext operations on the game
pthread, where `window` does not exist. Original and corrected worker probes use
identical engine/mixer/tone code. Original initialization returns -203 in Chromium
156.0.8078.4 and Firefox 157.0. Synchronous MAIN_THREAD_EM_ASM proxies around this
backend's browser operations initialize the real device. Measured mixed sine
RMS is 0.07061 / 0.07043, versus the 0.1/sqrt(2) nominal signal. No JS oscillator,
physics retuning or separate audio implementation is involved.

Final-source game runs passed in both browsers:

- Beacon Grove: two-cell progress, exact saved bytes in a new browser process,
  same actual physical endpoint and inventory, third pickup, two negative beacon
  controls, real animation event/gate destruction, crossing the former gate,
  victory/save/reload, defeat/retry, pause, and ordinary Quit/device teardown.
  Its observer fixture's authored WAV reaches the actual AudioSystem mixer and
  Web Audio output. Final RMS and exact restored positions are in RESULTS.json.
- Traversal: walking underneath, jumping through and landing on the one-way edge;
  blocked grid cells from two directions, a free neighbour, scene switches,
  IndexedDB publication and the actual restored grid pose plus completed load.
- Native AudioTests: 59/59; AudioWorldTests: 65/65.
- Native Beacon: win 22/22, cold 17/17, cold-win 5/5, loss 6/6. The modified demo
  generator was rebuilt, generated a fresh K/L-enabled project, and its full
  native win/save workflow passed 22/22 separately.
- Browser export default-off/policy boundaries: 10/10; JS syntax and diff checks.

The complete-game driver closes and starts the browser process using the same
isolated origin/profile. Hash equality is tested on the actual binary save, not
script state alone. Observers use public JudasJS interfaces; original game policy,
controller configuration, timestep, collision and solver budgets are preserved.

## Provenance and artifacts

`*-BROWSER_BUILD.json` retain exact native-tool/WASM identities and hashes for
all 19 package files; all hashes were verified after the final runs. The runtime
was built from the parent plus the recorded dirty source fingerprint. It was
not relabelled with a later commit. TESTED_SOURCE_SHA256.json independently pins
the implementation, project bindings and test code so the subsequent checkpoint
can be verified without pretending an uncommitted build was a clean Git build.
Evidence/documentation added after those runs do not change the tested logic.

Packages remain locally at `.cache/browser-stage2/beacon-final-web` and
`.cache/browser-stage2/traversal-final-web`. These are observed fixtures, not
public release packages. They include the normal licence/hosting/provenance
files. Beacon includes an original test tone; it is not added to the silent game.
Large WASM binaries, browser profiles and toolchain caches are deliberately not
committed. Source and exact test commands are in
[the qualification procedure](../../BROWSER_OFFLINE_QUALIFICATION.md).

Native commands (ordinary hidden X11 windows; no Xvfb installed):

```sh
cmake --build .cache/2d-stage1-build --target judas judas_export judas_audio_tests judas_audio_world_tests judas_beacon_grove judas_beacon_grove_application_tests -j2
.cache/2d-stage1-build/judas_audio_tests
.cache/2d-stage1-build/judas_audio_world_tests
SDL_VIDEODRIVER=x11 .cache/2d-stage1-build/judas_beacon_grove_application_tests projects/beacon_grove/Beacon_Grove.judasproj /tmp/beacon-run
# Then same output with fourth argument cold, or cold-win; fresh output for loss.
python3 tests/test_browser_export.py
```

Worker probe reproduction: copy audio-worker.c and audio-probe.cjs into
`.cache/browser-stage2`; provide Playwright in `.cache/browser-tests/node_modules`.
Extract the original header using
`git show d1dc3bb:third_party/miniaudio/miniaudio.h` into a separate include directory.
With the documented Emscripten SDK activated, compile each header variant:

```sh
emcc -O2 -pthread -sPTHREAD_POOL_SIZE=2 -sSTACK_SIZE=8388608 -sINITIAL_MEMORY=67108864 -sALLOW_MEMORY_GROWTH=1 -I HEADER_DIRECTORY .cache/browser-stage2/audio-worker.c -o .cache/browser-stage2/audio-VARIANT.html
```

Use VARIANT `original` / `proxy` and the original / current header respectively.
Add a diagnostic BROWSER_BUILD.json marker in that cache folder for the local
server, then run the retained audio-probe.cjs. All browser processes/servers in
the drivers belong to the test and are closed in finally blocks.

## Preserved unsuccessful checks and limits

Early harness failures are preserved rather than called engine defects:

- A six-frame observer first expected two cells after a save that genuinely
  contained three, and coarse software-rendered input later overshot the beacon.
  The final driver observes each frame, waits for an actual stationary endpoint
  and explicitly walks back outside beacon range if necessary.
- Traversal initially presumed ground y=0 and grid spawn (0,0). Authored geometry
  actually gives player centre y=0.1 and grid spawn (0,2).
- The first cross-scene load assertion queried the script's restored old save ID,
  despite the physical pose restoring correctly. The final observer checks the
  separate actual load request, id 2, without changing gameplay or save semantics.
- Initial standalone audio setup needed adequate WASM memory; a later unlock click
  hit an Emscripten sample-page link and navigated away. Those diagnostic logs
  remain locally in `.cache/browser-stage2`; they were not audio-algorithm failures.
- `firefox-baseline.log` has an inherited “Chromium” PASS label typo. It was a
  Firefox run; final drivers record browser/version correctly.

This is Linux browser functional evidence, not a performance comparison or a
hardware listening test. Chromium is software ANGLE/SwiftShader. Native Beacon's
old fixed “SOFTWARE_RENDERED_FRAME” label is not GPU identification or a matched
benchmark. Firefox renderer strings can mask the physical GPU. No browser speaker,
mobile, controller, Windows-browser or Safari acceptance is claimed.

ScriptProcessorNode is deprecated and mixes on the UI thread. AudioWorklet,
background-tab policy, context restoration, streamed/modded worlds, networking,
advanced 3D rendering, responsive surfaces and mobile input remain unqualified.
The 51 MiB-class runtime and preloaded content remain known costs. Main and
published Alpha artifacts stay unchanged; browser support still requires opt-in.

## Human checklist

1. Start either package using tools/serve_browser.py; open its localhost URL.
2. Beacon: collect cells, restore with E, reach the exit; K/L save/load; R retry;
   Esc pause and arrows/Enter Resume or Quit. Reload the page and load progress.
3. Traversal: jump through/land on gold platforms; 2 selects grid, 1 returns;
   try blocked/free neighbours, then K/L across scene changes.
4. With the observed Beacon fixture, click the canvas to unlock sound and listen
   for the quiet test tone; verify Quit stops it. This listening check is pending.
