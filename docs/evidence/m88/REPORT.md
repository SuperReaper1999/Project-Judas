# M88 — Judas Learns to Build for Android 16

## Delivery and acceptance boundary

Starting checkpoint: `16c40f08986486ef3d88aca50cefe0dde572b95b`, Experimental and
main aligned. This is a Linux-validated **Experimental** implementation, with
native Android 16 x86_64 emulator execution. **M88 human, ARM64-device and physical
phone/tablet/controller acceptance remain pending.** Main is not promoted.
Windows M88 compilation/runtime was not run; existing Windows branches remain
compiled conditionally and optional Android dependencies are not loaded there.

Deliverables (generated, not committed):

- `.cache/m88/Breakwater.apk`: signed development APK, ARM64 + x86_64,
  103,924,246 bytes; SHA-256
  `a88e57b08e9b163ac29aa70e9f2bbd385175e1c0eee3305fc3f58fd7c8355603`.
- `.cache/m88/Breakwater-Linux-x86_64.zip`: portable desktop export;
  `.cache/m88/Breakwater-desktop/judas` runs it directly.
- Ordinary editable project: `projects/breakwater/breakwater.judasproj`.
- Setup/export instructions: [ANDROID.md](../../ANDROID.md).
- [Provenance](provenance.json), [binary identities](binaries.json),
  [exact changed files](CHANGED_FILES.txt), [final source hashes](final-source.sha256)
  and [evidence hashes](evidence.sha256).

## Native architecture

The existing Application, RuntimeWorld, Judas-owned rigid-body/contact/CCD solver,
QuickJS-NG, resources, Unicode text/UI, miniaudio and game scripts cross-compile
into `libmain.so`. SDL2 supplies the native Activity, EGL window, Android input and
controller adapter. Java prepares an immutable private content generation and
forwards platform lifecycle/audio-focus observations. It does not implement the
game, simulate physics, render a web page or stream desktop frames.

Android is selected only by the NDK CMake toolchain. Desktop builds retain system/
vcpkg dependencies and GLAD/OpenGL 3.3. Android uses GLES 3.0, precision-qualified
GLSL ES 300, four-texel-wide RGBA32F skin palettes, ES buffer/FBO readback and
explicitly unavailable ES timestamp queries. An unchanged existing `pose_demo`
project was separately packaged and executed: [GLES skinning capture](screens/android-skinning.png).
Its source/assets were not changed. Breakwater exercises ordinary lit geometry,
Unicode UI, real resource loading, audio initialization and physics on Android.
Heavy weather/planetary/portal/render combinations are not comprehensively mobile-certified.

Private content is verified by per-file size/hash into a staging generation before
publication. Saves are separate. Native file walks start at the trusted Android
private root (including its OS-provided canonical alias), retaining descendant
`openat/O_NOFOLLOW`, bounded slot checks and atomic/checksummed save publication.
The Linux/Windows storage rules are preserved. No external storage permission,
network service, Node runtime, package manager or remote client is required.

Backgrounding cancels input, stops stepping/rendering and suspends the audio device.
Resume resets the clock rather than simulating elapsed background time. Worker
writes already holding immutable snapshots may finish; owner/load/resource
publication waits for foreground. Rotation uses the actual SDL window dimensions.
Focus-only audio interruptions atomically mute the final mix while consuming media;
Activity pause stops the device/cursors. No Java callback touches physics or voice tables.
A reported graphics-context loss exits cleanly; seamless GPU-context reconstruction
and guaranteed recovery of unsaved progress are not claimed.

New public API is limited to `input.touches` and `input.pointer`, bounded normalized
snapshots documented/typed/inventoried with existing logical input. Standard OS
InputDevice gamepads use the same M35 bindings. Mobile controls remain project JS.

## Build/package workflow

Pinned source archives: SDL2 2.32.10 and GLM 1.0.1 with hashes/licences under
`third_party/android`. Existing ICU 76.1/FreeType/HarfBuzz/QuickJS/Recast sources are
reused. ICU builds matching host data tools plus static per-ABI libraries, avoiding
Android's private ICU ABI. The SDL archive stays byte-identical; two asserted
staging-only Java receiver export-flag adaptations address API 33+ registration.

`tools/build_android.py` uses ordinary M38 dependency validation/export, NDK r28c,
SDK/build-tools 36, javac/D8/aapt2, APK alignment and signing. ARM64 and x86_64
Release libraries compiled/linked. All six packaged native libraries are stored
uncompressed and have 16 KiB ELF load alignment; APK `zipalign -P 16` and v2/v3
signature checks passed. [Packaged native hashes/alignment](android/packaged-native.json),
[manifest/ABI inspection](android/apk-badging.txt), [signature receipt](android/signature-alignment.log).
The emulator actually uses 4 KiB pages; 16 KiB execution is still a hardware test.

The default APK is **debuggable/development-signed**, not a Play Store release.
Own-key non-debuggable signing is supported; no key/password is committed. Linux
is the first supported APK build host. Complete JDK 17+ and Python 3.11+ are build
requirements. A JRE alone is insufficient. Build caches are transaction-locked;
the negative concurrent-export control exits 2 without creating an APK.
Desktop and SDK-free editor/runtime Release builds completed as well.

## Breakwater game

Clear 42 physical ceramic blocks before losing three lives. A native sphere bounces
against static boxes and an ordinary kinematic paddle. Real contact callbacks
retire hit blocks and score. Scripts add authored steering impulses after genuine
paddle bounces. No travelling ball is teleported or given scripted wall reflection.
The ready/unlaunched ball follows the paddle before launch; loss/restart is game policy.

Every fourth hit releases a real gravity-driven capsule. Capsule/paddle contact
cycles wider collider (15 seconds), two independently simulated extra balls and
a slowing impulse. JS owns all game rules, touch/camera mapping, HUD, pause,
win/loss, save/load and reload. Four original PCM effects and existing licensed
font resources are normal project assets. No game name, block/paddle/weapon
component or gameplay special case was added to engine C++.

Controls: drag the lower playfield, tap Launch; mouse drag/A/D/arrows and Space
on desktop; left stick/South on gamepad. Escape/Android Back/Start pauses;
R/Back resets. Menus offer resume/new game/save/load/exit. UI uses portrait design
anchors and fits landscape too. See [project guide](../../../projects/breakwater/README.md)
(path from this evidence directory: repository `projects/breakwater/README.md`).

## Focused validation

All successful checks below used actual runtime mechanisms, not public-symbol
counts as behavioural proof. No full unrelated production-suite run was performed.

| Linux check | Result |
|---|---:|
| Input / paired input / paired script input | 40 / 43 / 21 checks passed |
| Touch bounds, ID retirement/reuse, reset, two devices, UI cancellation | 17 passed |
| Script runtime / mod content / secure save storage | 35 / 127 / 36 passed |
| Audio / render-camera-resource / Unicode text rendering | 59 / 52 / 48 passed |
| Breakwater normal Application, shadows on | 31 passed |
| Same game/physics, shadows off | 31 passed |
| Existing save-lab cold write/read, actual physical continuation | 13 / 20 passed |
| TypeScript 5.9.3 + API drift | 34 exports, 387 symbols, 270 native ops; 44 examples typechecked; 4 negative drift controls passed |
| Final desktop export, moved launch from `/tmp` | Passed; launch/pause/resume and frame captures |
| Desktop runtime/exporter/editor Release builds | Passed |

Game application tests prove UI does not consume/fire launch incorrectly, background
freezes fixed steps, resume avoids catch-up, touch moves the actual paddle,
contacts score/remove physical blocks, save/load restores the real collision
population and one paddle, reload cleans state, all three physical power-ups,
full win and three-miss loss. Some application fixtures position bodies to create
specific contacts; these are focused mechanism tests, not an automated human
playthrough. [Raw Linux receipts](linux/focused-results.json).

Native Android emulator checks:

- API 36/x86_64 process runs GLES with native C++/QuickJS and initialized AAudio;
  real touchscreen input launches, hits/removes a block and scores.
- Android predictive Back opens/closes the project menu, rather than terminating
  or immediately undoing itself. Portrait/landscape resize and Home/resume work;
  foreground/background retains the process. Native logs contain no startup,
  shader, script or save exception in the successful final run.
- Two kernel multitouch slots travel through Android MotionEvent → SDL → public
  `input.touches` → JS → native paddle. First held contact stays selected; release
  hands control to the second. [Measurement](android/android-multitouch.json),
  [captures](screens/android-multitouch-handoff.png).
- A test-only uinput standard controller travels through Android InputDevice/SDL,
  moves the physical paddle, launches via South and is removed normally. These
  are synthetic controller-route checks, **not Bluetooth/physical-pad acceptance**.
- Real M61 save in app-private storage: one hit, score 100, two lives, ready ball,
  41 blocks. Force-stop/fresh process → load → re-save preserves score/lives/phase,
  physical entity IDs, 44 static + 1 kinematic + 1 dynamic bodies. Independent
  envelope/checksum/scene decoding validates actual persisted participants;
  normal elapsed `state.time` advances. [Final reproduction](android/android-cold-results.json),
  [ready state](android/android-cold-ready.png). Corrupted checksum negative control rejects.
- Both ABIs packaged; ARM64 execution, physical 16 KiB pages, actual speaker output,
  transient call/audio-focus perception, cutout/navigation-bar variations, tablets,
  real controller pairing and thermal/battery behaviour remain device tests.

Native input/probe captures were taken during implementation against the same
unchanged final input/skin code; final APK lifecycle/cold-save/performance checks
ran after the last native change. The final synthetic gamepad probe was rebuilt
from its recorded source. API/type checks verify declaration synchronization,
not arbitrary gameplay semantics or every Android engine feature.

## Measured performance

Host/toolchain/emulator configuration is in [provenance](provenance.json). Linux
benchmark is a real-clock ordinary application with a logical-input pilot,
2 seconds warm-up + 8 seconds sampling, hidden 1024×768 native GL window and
uncapped swap. **Throughput is not visible display refresh or phone performance.**
M56 capture is enabled. No CPU affinity/frequency lock or GPU isolation was used.

| Workload | Frame median / p95 / worst ms | Fixed step median / p95 / worst ms |
|---|---|---|
| Linux host, shadows on, 7,719 frames / 480 steps | 0.987 / 1.344 / 3.501 | 0.099 / 0.173 / 1.022 |
| Android emulator ready, shadows on, 62 frames / 247 steps | 63.085 / 273.030 / 404.922 | 0.068 / 0.169 / 0.571 |
| Same APK/ready board, shadows off, 94 frames / 93 steps | 16.674 / 18.546 / 19.387 | 0.129 / 0.291 / 0.403 |
| Android emulator active, shadows off, 95 frames / 94 steps | 16.672 / 19.541 / 19.947 | 0.160 / 0.450 / 1.363 |

Linux peak RSS 127,135,744 bytes; up to 46 bodies during measured play, last score
500/hits5/lives3. Android PSS/RSS are retained verbatim in each summary (ready-off
85,155/188,196 KiB). Emulator GPU memory accounting is unavailable/zero in dumpsys;
no physical GPU-memory claim. The retained active ES scope mean is 2.192 ms for
main-camera submission, 0.116 ms for JS fixedUpdate on frames present. GPU timestamp
queries are explicitly unavailable; these CPU submission/wait timings cannot be
reported as independent GPU execution time. Linux full samples and Android bounded
120-frame histories are retained; this is a short sanity check, not a long-session gate.

The measured emulator bottleneck was **shadow submission/driver blocking**, not
physics: ready-on mean shadow submission 75.597 ms versus 0.00069 ms disabled.
Ready-on hit the existing fixed-step cap three times and discarded 0.363 seconds;
ready-off and active-off had no caps/discarded time. Cold shader/driver/scheduler
spikes are visible, not hidden behind averages. Optional Android shadow maps use
512²; the practical default is off. Same-binary debug override allows an identical
42-block A/B. Desktop shadows stay on by default. The fallback changes rendering
only—same timestep, solver, mass, material, interactions and gameplay.
[Raw profiles](android/profiles), [Linux samples](linux/linux-performance.json),
[bounded scope attribution](performance-attribution.json).

## Preserved failures and corrections

[Development receipts](development) are separate from final passes:

1. Initial project call omitted the required full pose/duration for `moveKinematic`;
   corrected project use of the existing API, no native convenience binding.
2. First Android run hit SDL Java's target-36 receiver export-flag exception;
   exact checked staged receiver adaptations fix it, upstream archive unchanged.
3. Opening `/` for the secure save walk returned EACCES inside Android's sandbox.
   Generic Android private-root anchoring fixed the real failure; actual cold
   saves now work. This did not relax descendant symlink/slot checks.
4. Initial SwiftShader/Vulkan emulator process segfaulted and SystemUI became
   unresponsive. Host-GL/Vulkan-off AVD works; no Judas app-crash claim from that
   host-QEMU failure. Large raw core/build outputs are not committed.
5. First performance driver dereferenced a lazy script system before publication;
   corrected test driver. Initial zero fixed timings were disabled observation,
   not measured zero-cost physics; that draft remains preserved.
6. New shadow-pass assertion first assumed caller-reset Stats were per-frame;
   corrected the fixture to reset before observing one frame. Production was not
   changed to satisfy the wrong assertion.
7. Save-lab runner was initially invoked without its write/read arguments (exit2);
   correct unchanged-fixture invocations passed 13/20 checks.
8. An accidentally overlapping Android cache build was stopped and invalidated its
   output; a fresh sequential build passed. The exporter now rejects concurrent
   use of one cache. No hashes were regenerated to hide a runtime failure.

## Limitations and one human checklist

Mobile editor, app bundles/Play distribution, asset packs, seamless EGL-loss
reconstruction, external-storage/SAF import UI and Windows-host APK creation are
out of scope. The APK uses minimum API26, but execution was tested on API36 only.
ARM64 compiled; physical acceptance is pending. GLES heavy render combinations,
16 KiB execution, actual audio/controller behaviour and power/thermal limits need
hardware testing. The planar game removes destroyed blocks, uses authored steering
impulses, and has no arbitrary-fragment fracture or commercial-content scope.

1. Open Breakwater on desktop; move/launch, observe real bounces and independent blocks.
2. Catch each capsule: wider physical paddle, multiball and slowing effect.
3. Win/lose a round, restart; pause/resume without accidental launch.
4. Save non-default progress, close, reopen/load; check score, lives and remaining blocks.
5. On an Android 16 phone/tablet when available, sideload APK and repeat with touch.
6. Use two fingers, rotate, Home/resume and Android Back; confirm no stuck input/catch-up.
7. Check audio interruption/resume, display cutouts/system bars and real paired gamepad.
8. Play long enough to assess phone frame pacing, heat/battery; compare shadows off/on.
9. Confirm ARM64 and, where available, 16 KiB devices; these are outstanding, not synthetic passes.

No main promotion, milestone tag, physical hardware acceptance or new milestone
is implied. Protected historical evidence, accepted fluid/research work and other
projects are unchanged. Exact source/evidence manifests distinguish this candidate
from the unrelated pre-existing untracked working files.
