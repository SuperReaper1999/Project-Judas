# Experimental browser player checkpoint — 31 October 2026

Follow-up: human testing found severe Firefox memory growth that the short
automated runs below missed. The original evidence is retained; see the
[subsequent correction and qualification](BROWSER_FRAME_BACKPRESSURE_REPORT.md).

Automated qualification for a bounded presentation/input/lifecycle stage. Human
acceptance is pending. Development is on Experimental; main, published Alpha
packages, release channels and Advanced Fluid policy are unchanged.

Base: `7a22ada94cb64e57fedd164aad74288acaf426e2`.
Main remains `49c4f3d9e29debe669411f01501e0276dea23231`.
The candidate is the commit adding this report. No tag/publication is authorized.

## Delivered

- Responsive canvas and fullscreen, including an exit strip outside the game HUD.
  The existing SDL/WebGL worker remains the rendering owner. Surface resolution
  is capped at DPR 2, 4096 per axis and 2,097,152 pixels; simulation quality and
  fixed-step policy are unchanged.
- DOM focus/visibility is observed before consuming input. Actual worker-owned
  backing dimensions are published after queued SDL events, preventing stale
  logical resize events from becoming renderer dimensions. Relative look uses
  CSS-pixel units on both legacy and device-scoped routes. Capture transitions
  and resize boundaries discard coordinate-system transition motion.
- Requested capture is distinguished from browser-granted pointer lock. Page
  controls cannot steer the game; canvas clicks recover focus/capture without
  taking browser-reserved shortcuts away.
- The existing application suspension/audio path handles hidden tabs. A bounded
  lifecycle wake can observe visibility when RAF stops. A UI-side visibility
  epoch also retains transitions if the worker is frozen throughout the interval,
  so foreground clock reset does not depend on observing the hidden frame.
- An ordinary editable 3D project: `projects/browser_player_lab`. It uses the
  existing first-person template, CharacterMotor, script-owned first/third-person
  camera, MK2 crates, RuntimeUI and durable saves. No HTML gameplay substitute or
  game-specific engine subsystem. Its request IDs are transient, not saved data.
- Pure surface-budget regression, real browser pixel/input/visibility drivers,
  current support documentation and reproducible qualification instructions.

Important paths: `src/Window.cpp`, `src/Window.h`, `src/BrowserSurface.h`,
`platform/browser/shell.html`, `tools/demos/BrowserPlayerLab.cpp`,
`tools/demos/browser_player/player.js`, `tests/BrowserSurfaceTests.cpp`,
`tests/browser/player.cjs`, `tests/browser/visibility.cjs`, and the lab project.
The validation manifest registers the new inexpensive surface gate.

## Final combined checks

| Check | Observed result |
| --- | --- |
| Chromium 156, DPR 1, actual 3D player | 29/29 |
| Chromium 156, DPR 2, actual 3D player | 29/29 |
| Firefox 157, actual 3D player | 29/29 |
| Normal Chromium, real hidden tab, real running audio, released key | 10/10 |
| Beacon Grove win/loss, actual PCM output, cold progress, exit | 23/23 per browser, Chromium and Firefox |
| Relocated Traversal one-way/grid collision and cold save | 8/8 per browser, Chromium and Firefox |
| Native surface-budget arithmetic | 72/72 |
| Native input / rebinding | 40/40 and 19/19 |
| Native pointer, touch and actual resized capture | 33/33 |
| Browser compiler/package Python checks | 33/33 |
| Build organization | 18/18 |
| Validation registration | 9 tests: 7 executed passes, 2 conditional skips |
| Native lab, save then load in a second process | Exact physical endpoint restored |

These are repeated configurations of shared gates, not a claim of hundreds of
independent engine features. No production-suite marathon was run.

The final native cold endpoint was `(1.6666672229766846, 0.917275071144104, 6)`.
Browser cold checks independently restore actual player coordinates and UI
progress. The tests observe actual rendered corner markers, actual browser input,
DOM mouse movement versus script yaw, physical jumps/landings, fullscreen and UI
hits; viewport telemetry alone is insufficient.

Measured DPR-2 surface sizes were 1874×1119, 1339×1565, 2322×903 and 1912×980.
All stayed within the declared pixel budget. A measured 40 CSS-pixel mouse move
produced approximately −0.084 radians at both DPIs. This is a presentation budget
and input-consistency measurement, not a GPU-memory or rendering-speed benchmark.
Chromium software rendering is not native-hardware performance certification.

Three final exports share one compiled engine/source identity. The relocated
Traversal copy runs from a path containing spaces through its own packaged server
outside the source project. The portable preview hash inventory verifies all 21
files. Browser fixtures have their own profiles and durable-save ownership.

## Provenance and evidence

[SUMMARY.json](evidence/browser-player-20261031/SUMMARY.json) records complete
build receipts, actual runtime hashes, exact core-source hashes and result counts.
[SHA256SUMS.txt](evidence/browser-player-20261031/SHA256SUMS.txt) hashes the retained
evidence. Raw compressed logs, successful captures, failed captures and relevant
attempt sources are retained beside it. Browser profiles/build trees are excluded.

The packages truthfully identify the parent plus the frozen working candidate.
Test-harness refinements and this evidence/report followed packaging. Exact core
source hashes bind the tested implementation to this checkpoint; binaries have
not been relabelled as clean post-commit builds. A future editor Build & play
refreshes provenance through the existing strict matching-source workflow.

Playable local exports:

- `.cache/browser-player/final-player-web`
- `.cache/browser-player/final-beacon-web`
- `.cache/browser-player/final-traversal-web`

Serve a complete export with its `serve.py`; do not open HTML through file://.
The normal editor opt-in remains `-ExpBrowserSupport=1`.

## Retained failures and oracle corrections

- The baseline already supported proportional SDL resizing. This stage adds
  available-height/fullscreen/DPI handling; it does not falsely claim the old
  runtime had no resize support.
- A compile-time EM_ASM comma error and a mixed-source export rejection are
  preserved. The provenance guard was retained, not bypassed.
- Chromium's separate headless-shell compositor timed out on some continuous
  OffscreenCanvas screenshots while gameplay continued. Full Chromium passed;
  this does not certify that separate capture backend.
- HTML canvas width attributes can lag transferred worker storage. Early DPR
  checks used the wrong owner. The final checks retain both and verify pixels
  after native viewport publication and a subsequent game-frame boundary.
- Resize clears storage before drawing. Early black captures and stale/clipped
  captures remain preserved. Source review also corrected the candidate's real
  ordering hazard: queued logical size events must not overwrite backing size.
- Firefox's headless fullscreen uses native screen dimensions despite Playwright's
  emulated screen. Calibration happens before the actual fullscreen check;
  resizing emulation during fullscreen would exit it and invalidate the test.
- Ordinary Playwright forces focus/visibility. Actual suspension uses a separate
  owned normal browser with documented `noDefaults:true`, not a forged hidden flag.
- A hidden-page assertion initially used RAF polling and timed out even though
  captured diagnostics showed native inactivity and suspended audio. Timer
  polling fixes that oracle; its five-second deadline remains unchanged.
- Immediate no-motion after returning was an incorrect oracle for Beacon's
  momentum-preserving controller. Its authored braking produced the expected
  short continuation. The final check tests a stationary late endpoint, without
  changing the controller, forces, timestep or velocity.
- The pre-epoch package fails the new visibility-transition retention assertion.
  The final package passes with actual hide/show transitions.

## Limitations and next boundaries

Firefox captures show triangular shadow-like artefacts on untextured lab walls.
Their cause is not isolated. This remains a real visual difference, not a claimed
shadow-quality pass or a reason to disable the scene's shadows. Reported WebGL
device strings may be privacy-masked; exact hardware cannot be inferred from them.

This stage does not qualify every 3D effect, mobile browsers, physical touch or
controllers, Safari, browser networking or automatic GPU context restoration.
Whole-browser forced process-freeze/discard recovery is not independently
qualified; normal hide/show and the retained epoch are tested. The preloaded
universal runtime/download and memory limits remain as documented.
The existing miniaudio ScriptProcessor backend remains experimental/deprecated;
no physical audio hardware certification follows from its measured device state.
Advanced Fluids remain locked/rejected. Native Windows/Android workflows were not
expanded or requalified in this Linux/browser stage.

## Human checklist

1. Open the editable Browser Player Lab or build/play it through the opted-in editor.
2. Resize wide/portrait, enter fullscreen and use its exit strip; inspect markers/HUD.
3. Move/look/jump, push crates, use V; release the mouse and click Test click.
4. Switch tabs while moving, release the key elsewhere, return; check audio and input.
5. K save, close/reopen, L load; inspect the physical endpoint and click counter.
6. Try Beacon Grove and Traversal; inspect Firefox's reported shadow difference.

See [qualification instructions](BROWSER_PLAYER_QUALIFICATION.md) and
[current browser support](BROWSER_SUPPORT.md). Main promotion requires separate
operator acceptance; nothing here grants it.
