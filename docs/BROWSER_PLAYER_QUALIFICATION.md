# Browser player qualification

This is an Experimental-only presentation/input/lifecycle stage. It adds no
second renderer, controller, physics loop or save format. Main and published
Alpha packages are outside this change.

## Reproduce

Build `judas_browser_player_lab` and generate a fresh project, or open the editable
`projects/browser_player_lab/Browser_Player_Lab.judasproj`. Export using the
opt-in compiler procedure in [Browser support](BROWSER_SUPPORT.md). Never replace
an existing edited project or reuse an old browser package as a new-source build.

The optional browser tests need Playwright 1.64.1 with Chromium and Firefox,
Node.js and Python Pillow. These are test-host tools, not game dependencies.
Outputs must be fresh directories. Run browser workloads sequentially.

```sh
PLAYWRIGHT_MODULE=/absolute/node_modules/playwright \
  node tests/browser/player.cjs /absolute/browser-package /fresh/results chromium 1
# Repeat with chromium 2 and firefox 1.
PLAYWRIGHT_MODULE=/absolute/node_modules/playwright \
  node tests/browser/visibility.cjs /absolute/browser-package /fresh/visibility
```

The visibility test opens its **own** normal Chromium process/profile and connects
using the documented `noDefaults:true` option. Unlike ordinary Playwright pages,
this does not force focus/visibility. It requires a working desktop display and
closes only its own browser. Run it with the observed Beacon Grove audio fixture
to exercise an actually running audio device as well as simulation.

## Meaningful gates

- Real pixels: all four RuntimeUI markers after landscape, portrait, wide, odd
  dimensions and fullscreen. A public viewport number alone is insufficient.
- Match CSS aspect while bounding physical rendering to DPR 2, 4096 per axis and
  2,097,152 total pixels. Unit tests include invalid/non-finite and huge dimensions.
- Acknowledge the new native viewport, then a subsequent game-frame boundary before
  reading pixels. UI callbacks precede drawing; resize clears WebGL storage.
- The worker owns transferred OffscreenCanvas storage. HTML width attributes can
  lag during SDL resize callbacks; keep both in evidence rather than treating
  HTML attributes as authoritative GPU dimensions.
- Real pointer lock and DOM movement versus actual script yaw; wait for input
  acknowledgement before measuring the next event. Test both ordinary and high DPI.
- Page-control focus must not move the player; returning must not stick keys.
  Jump/land and a real resized UI button exercise the motor and pointer route.
- Genuine fullscreen entry and its visible exit control. Firefox headless screen
  size is selected **before** entry; changing its emulated viewport during
  fullscreen exits fullscreen and would invalidate that check.
- Durable save completion, then a fresh browser process restoring the actual
  physical endpoint and persistent UI progress. Request IDs are transient game
  state, not serialized progress.
- Actual hidden tab: native active flag becomes false, script/fixed time stops,
  visibility transitions are retained, foreground resumes with its clock reset.
  Running native audio must suspend/resume when present. Do not count an absent
  audio device as an audio pass.

## Scope and caveats

These are functional tests on Linux desktop browsers. Chromium's software backend
is not a GPU performance benchmark. Its separate headless-shell compositor timed
out on some continuous OffscreenCanvas captures; full Chromium is used for the
pixel gates. The preserved unsuccessful captures are not deleted or called passes.

The lab proves ordinary 3D primitives, a CharacterMotor, script-owned cameras,
MK2 bodies, UI and saves. It does not certify all 3D effects. Firefox's tested
WebGL capture showed triangular shadow-like artefacts on the lab walls;
their cause is not isolated. This is a visible rendering difference, not a
claimed shadow-quality pass. Reported WebGL device strings can be privacy-masked;
they do not establish exact hardware qualification.
Mobile browsers, physical touch/gamepads, networking, context restoration and
complete-engine browser compatibility remain separate work. No physical audio
hardware certification follows from observing a Web Audio device state.

For human review: resize and use fullscreen; move/look/jump/push crates; use V;
release the mouse to click Test click; save with K, close/reopen and load with L;
switch tabs and return. Test the existing 2D games as well as this small 3D lab.
