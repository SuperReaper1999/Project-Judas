# Android browser emulator qualification

31 October 2026. Engine under test: Experimental
`fb77396ccd0a5e18f16784d17be46287eb67abc1`. This follow-up adds qualification
records, not an engine/runtime correction or a release-channel change.

## Scope and setup

Android 16/API 36 Google APIs x86_64 image, Chrome **133.0.6943.137**, Android
Emulator **37.1.11.0**, KVM on the Ryzen 5 5600 / 8 GB Linux host. The successful
session uses two cores, 1536 MiB guest RAM, a 720×1280 display at Android density
320, and host GLES with emulator Vulkan disabled. These are emulator settings,
not a reduction in Judas physics cadence or game rendering settings.

The existing `Judas_M88_API36` AVD is launched with `-read-only -no-snapshot`,
using a task-owned temporary directory. Browser account sign-in is skipped. The
original consumer game is unchanged. An isolated Breakwater copy adds console
observations of public game state, physical paddle/ball poses and viewport;
its rules, controls, state schema, assets and physics settings are unchanged.
The source hashes and executed probe are retained with the evidence.

The initial 2048 MiB guest / 1080×2400 / software-renderer attempt reached the
**3600 MiB cgroup limit before loading Judas**. The guarded unit was killed;
this is an emulator/host-capacity failure, not an engine failure. The smaller
retry remained under that same guard (with at most 256 MiB permitted swap),
although it reached the cap. This is not a useful game-performance platform.
Both attempts and the memory records are preserved.

## Verified behaviour

| Contract | Actual observation and negative control |
| --- | --- |
| Browser startup/input | Actual exported WASM, game content and persistent storage preload. Touch moves the real kinematic paddle to x=6.449999809265137. |
| Durable save | The actual binary save is 39,431 bytes. After save, touch deliberately moves the paddle to a different position. |
| Cold browser restoration | Leave the page and force-stop Chrome without clearing data, then start a fresh process. IndexedDB hydrates the exact saved SHA-256 `8467fdd00729c3175e8e0776db037112d6f69a055ae5ff8a9986ff3ddb3bf725`. Fresh gameplay initially differs from the saved endpoint; ordinary menu Load restores actual paddle position `(6.449999809265137, -9.5, 0)`. |
| Rotation | Android portrait → landscape → portrait changes the native game viewport from 672×572 to 1104×480 and back. The public game viewport agrees with the actual browser surface. Running gameplay survives. |
| Home/background | Genuine document-hidden **and native surface-inactive** state is independently established. Gameplay time remains exactly unchanged during the measured hidden interval. First resumed state advances only 0.0166666675 seconds. |
| Emulated lock/unlock | Sleep/wake/dismiss the unauthenticated emulator keyguard produces real hidden/inactive transitions. Gameplay time again remains unchanged while hidden; first resumed state advances 0.0500000026 seconds. |
| Audio graph | An analyser taps the existing miniaudio device's ScriptProcessor output; no oscillator or replacement sound is synthesized. Launching the real ball produces nonzero native PCM, RMS peak 0.0092868787. Later peak is 0.0141631756. |
| Audio lifecycle | The actual Web Audio context records suspended → running for both Home and lock cycles. It resumes automatically in this run, without a new gesture. Visibility epochs progress 1 → 2 → 3 → 4. No browser exception is observed during the qualified lifecycle pass. |

The original fixed step is retained. The physics observations come from actual
entities through ordinary JudasJS, not an invented display or controller status.
These are bounded functional checks of one existing game and browser/image.
They do not certify every subsystem or establish emulator/native FPS equivalence.

## Test-method failures retained honestly

1. **Invalid rotation oracle.** A portrait phone can have a wider-than-tall canvas
   because browser/page controls consume vertical space. Waiting for canvas
   `width > height` returned before rotation happened. The corrected observer
   waits for actual DOM display orientation and a changed native viewport, then
   checks renderer/game agreement. Cold-save/audio positives from the first
   driver remain valid; its overall result is still recorded as failed.
2. **Forced-visible automation.** Default Playwright attachment enabled focus
   emulation: document-hidden events never arrived, visibility epoch remained
   zero, and gameplay kept advancing on Home. This did not exercise the intended
   hidden contract. The qualified driver uses the repository's existing
   `noDefaults: true` lifecycle method and clears the focus override. It requires
   genuine hidden/native-inactive states before measuring suspension, and keeps
   the original 0.5-second resume bound. No timestep or engine behaviour changed.

This is why the evidence contains failed driver results alongside qualified
results. They are not silently relabelled as passes. The probes are retained as
executed diagnostic sources, not registered as a universally green CI suite.

## Audio and platform limitations

The emulator reports **“Could not init `pa` audio driver”** on the Linux host.
The analyser proves actual mixed PCM reaches the browser audio graph and that
context suspension/resumption works. **Audible output through speakers/headphones
and its quality remain unqualified.** No microphone/capture test was performed.

Chrome 133 on an Android 16 emulator is different evidence from the operator's
Chrome 156 / Android 17 phone testing. This pass does not certify Safari/iOS,
Samsung Internet, other Android devices, Bluetooth/controller support, public
HTTPS hosting, online transports, storage eviction/quota exhaustion or general
renderer-feature coverage. The audio backend remains ScriptProcessorNode;
AudioWorklet modernization and latency guarantees are separate work.

Browser export remains Experimental behind `-ExpBrowserSupport=1`. Main,
published packages, protected history, original games and independent fluid
research are untouched. No fluid work starts as part of this pass.

## Reproduction and retained material

Full records are in
[`evidence/browser-android-emulator-20261031`](evidence/browser-android-emulator-20261031),
including the binary save, engine package receipt, original/probe source hashes,
resource-limit failure, successful session settings, exact executed diagnostic
sources, invalid oracles, successful lifecycle observations and raw console logs.
Logs are losslessly gzip-compressed with hashes of their original bytes.

The local instrumented project and playable export are
`.cache/browser-emulator/project` and `.cache/browser-emulator/web`.
The completed emulator has been shut down and its preview stopped.

For a repeat, use a disposable/read-only Android 16 AVD with Chrome. Boot with
`-read-only -no-snapshot -no-window -memory 1536 -cores 2 -gpu host
-feature -Vulkan -skin 720x1280` under the recorded memory guard; set emulator
`wm density 320`. Provide a localhost game preview, an ADB reverse port and a
forward to `localabstract:chrome_devtools_remote`. Do not use a physical device
serial or an unrelated emulator by accident.

The diagnostic sources read `.cache/browser-emulator/CONNECTION.json` with
`url`, `debugPort`, explicit emulator `device`, and absolute `adb` path. They use
`PLAYWRIGHT_MODULE` to locate Playwright. The first driver's later invalid
rotation check and the default-focus continuation are preserved negative methods;
use the qualified continuation's actual-orientation and no-focus-override method
for lifecycle checks after the recorded startup/save/audio setup. This is an
investigation handoff rather than a newly introduced one-command emulator manager.
