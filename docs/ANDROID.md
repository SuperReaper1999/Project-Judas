# Native Android games (M88)

Android is an **optional runtime target**. Linux and Windows keep their existing
build/export workflow and do not need an Android SDK. Android builds use the same
C++ Application, RuntimeWorld, Judas-owned physics, QuickJS gameplay, resources,
Unicode UI and miniaudio engine. SDL supplies the native Activity/window/device
adapter; this is not a browser, remote desktop or streaming client.

**Current scope:** Android 16 / API 36 target, minimum API 26, OpenGL ES 3,
arm64-v8a phones/tablets and x86_64 emulators. Linux is the supported APK build
host for this first toolchain. An editor on Android is not included. Physical
phone/tablet, thermal/battery, vendor-driver and real controller acceptance remain
pending. Emulator results are recorded separately in [M88 evidence](evidence/m88/REPORT.md).

## Build and export

Install CMake 3.20+, a C/C++ host compiler, make, Python 3.11+, a complete JDK
17+ (including javac/keytool), and Android SDK command-line tools. Android
Studio/Gradle/npm are not required. Set `ANDROID_HOME` to your SDK and
`JAVA_HOME`/PATH to the JDK. Install/accept the SDK licences normally:

```sh
"$ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager" \
  'platforms;android-36' 'build-tools;36.0.0' 'ndk;28.2.13676358'
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target judas judas_export -j4
python3 tools/build_android.py projects/breakwater/breakwater.judasproj \
  --output .cache/android/Breakwater.apk
```

Use the installed versioned command-line-tools directory if it is not named
`latest`. Download/SDK installation requires network; CMake uses only the pinned
local source archives afterward. The build includes host ICU data-generation
tools and ABI-specific native ICU libraries, preserving the existing Unicode
implementation rather than linking Android's private ICU ABI. Caches are large
and belong on disk, not a small RAM `/tmp` filesystem.

The exporter validates/copies normal M38 project content, cross-compiles the native
runtime for both ABIs, compiles SDL/Judas Java bootstrap code, creates DEX/resources,
strips staging copies of native libraries, aligns and signs an installable APK.
`--shadows off` is the mobile default; `--shadows on` enables 512² maps.
This is renderer quality, not a simulation setting. `--abis arm64-v8a` or `--abis x86_64` builds one ABI. `--cache`, `--sdk`, `--jobs`,
`--exporter`, `--desktop-runtime`, `--id`, `--name` and `--version-code` customize
the workflow. Use a distinct stable application ID per game. Desktop and mobile
read the **same ordinary project scripts/scenes/assets**.

Default signing uses a local cached **development** key and a debuggable manifest.
It is suitable for sideload testing, not publication. Never commit that key. For a
release-signed, non-debuggable APK, supply your own keystore/alias and put its
password in the named environment variable:

```sh
python3 tools/build_android.py projects/my_game/my_game.judasproj \
  --id com.example.mygame --name 'My Game' --version-code 2 \
  --keystore /private/release.p12 --key-alias release \
  --password-env JUDAS_SIGNING_PASSWORD --output /outside/repository/MyGame.apk
```

Keep the signing key and application ID to update an installed game. A changed key
cannot update the old installation; uninstalling deletes private saves. App
bundles/Play Store submission, asset delivery packs and Windows-host APK generation
are not implemented by this workflow.

## Install and run

```sh
"$ANDROID_HOME/platform-tools/adb" install -r .cache/android/Breakwater.apk
"$ANDROID_HOME/platform-tools/adb" shell am start \
  -n org.projectjudas.breakwater/org.projectjudas.runtime.JudasActivity
```

An Android 16 x86_64 AVD can use the SDK's `system-images;android-36;google_apis;x86_64`.
Use hardware virtualization and a supported graphics backend. This host's emulator
37.1.11 software Vulkan path crashed **the host emulator process**; the host-GL
backend with Vulkan disabled is recorded as a separate follow-up. Such emulator
failures are not evidence of phone-driver compatibility.

For native logs, use `adb logcat -s Judas.Native SDL`. The bounded native stdout/
stderr bridge retains existing script/resource/renderer diagnostics. To capture
the existing M56 profiler, launch with `--ez judas.profile true`, play, then use
Exit Game for a clean native shutdown. A debug build allows:

```sh
adb exec-out run-as org.projectjudas.breakwater cat files/judas-profile.json > android-profile.json
```

GPU timestamps are explicitly unavailable on this ES 3 path; CPU/fixed/script
measurements remain real. Emulator frame timings do not certify device performance.

## Input, lifecycle and storage

- Standard Android InputDevice gamepads use the existing M35 logical bindings.
  Projects still author controls; no built-in mobile gameplay controller is added.
  Bluetooth pairing is OS-owned. Raw Bluetooth HID features requiring extra
  permissions are not enabled by this manifest. Physical pad testing is pending.
- [`input.touches` and `input.pointer`](judasjs/input.md#touch-and-pointer-snapshots-m88)
  expose bounded normalized snapshots. Primary touch also drives existing UI.
  Secondary contacts are independent, not duplicate UI clicks. Back becomes
  ordinary Escape/logical UI-back input; project JS decides menu behaviour.
- Android uses the actual native window size, immersive fullscreen and resize
  events. Project UI anchors/`world.viewport` handle rotation; camera policy remains
  project JS. Do not cache startup dimensions forever.
- Backgrounding stops authoritative stepping/rendering and suspends the audio
  engine. Audio-focus-only interruption mutes the final mix through an atomic
  observation while consuming media, avoiding delayed one-shot bursts; Activity
  pause suspends the device/cursors. No Java callback mutates physics or voice tables.
  Resume cancels held input and resets the clock; elapsed background time
  is not simulated as catch-up. Already-dispatched immutable save writes can finish on workers while backgrounded;
  queued captures, load/resource work and owner publication wait for foreground.
  Save timing remains project policy.
- The APK's hashed content manifest is unpacked into a private, versioned staging
  generation, verified, then published before engine startup. Interrupted extraction
  is retried. This initial packaging path bounds content to 50,000 files / 1 GiB
  and temporarily needs space for current plus staging generations. Upgrades retire only engine-owned content generations. Project
  content remains filesystem-compatible; no external-storage permission is used.
- M61 slots live separately under private `files/judas/games/<save-id>/Saves`.
  They survive process death, rotation and same-key APK reinstall/update. Existing
  content-identity checks still apply; edits can make old saves incompatible.
  Uninstall/clear-data removes them. Backup/cloud sync is not enabled.
- Android normally preserves EGL over pause. A reported graphics-context loss
  terminates the native session cleanly rather than using stale GPU handles.
  Relaunch rebuilds resources; unsaved progress is not guaranteed. Seamless context
  loss reconstruction is an explicit limitation, not a claimed automatic save.

## Rendering and practical mobile budgets

OpenGL ES 3 shaders use explicit precision and a 2D RGBA32F skin palette; desktop
keeps GL 3.3 texture buffers. Mesh/texture diagnostic readback uses ES-supported
mapping/framebuffer reads. Android export defaults to no shadow passes, retaining ordinary lighting. Optional
mobile shadow maps are 512² rather than desktop 1024². `JUDAS_SHADOWS=0` also
provides this generic renderer fallback on desktop; its default remains on.
A debuggable APK permits `--ez judas.shadows true|false` on a fresh launch for
same-binary profiling. This never changes physics settings or forces.
The existing HDR framebuffer failure fallback remains available. HDR/weather
float-target support and expensive modern scenes require device-specific review;
not every desktop quality combination is certified on mobile. ES timestamp
extension support is deliberately not assumed.

Start with modest lights/shadows, bounded particles, low weather quality and
reasonable texture/mesh budgets. Graphics settings never reduce physics quality
or silently change game forces. Breakwater has 42 physical blocks and a bounded
number of real dynamic balls/capsules. Heavy desktop labs are not mobile benchmarks.

## Dependency provenance

`third_party/android` contains pinned SDL 2.32.10 (zlib licence) and GLM 1.0.1
(MIT) source archives and their notices/hashes. Packaging applies two checked,
staging-only Java receiver-registration adaptations for Android 13+ export flags:
USB/custom permission broadcasts are not exported; protected Bluetooth system
broadcasts are exported. Pinned upstream bytes are preserved. Normal Android
InputDevice gamepads work without granting raw Bluetooth permissions.

See [SDL's Android integration](https://wiki.libsdl.org/SDL2/README-android),
[Android 16 SDK setup](https://developer.android.com/about/versions/16/setup-sdk),
and [Android's 16 KB page-size requirements](https://developer.android.com/guide/practices/page-sizes).
NDK r28c builds 16 KB-compatible ELF loads; the packager verifies APK 16 KB native
alignment and signature. Physical 16 KB hardware validation remains separate.

## Focused emulator reproduction

The development APK permits normal `adb run-as` access. These tools target only
an isolated Android emulator; they do not certify real touch/controller hardware:

```sh
python3 scripts/android/check_breakwater.py --adb "$ANDROID_HOME/platform-tools/adb" \
  --apk .cache/android/Breakwater.apk --output .cache/android/cold-save
python3 scripts/android/profile_game.py --adb "$ANDROID_HOME/platform-tools/adb" \
  --shadows off --mode active --seconds 5 --output .cache/android/profile
```

The UI fixture expects a 720×1280 portrait AVD. Set that test device explicitly
with `adb shell wm size 720x1280`, `adb shell wm density 240` and
`adb shell wm user-rotation lock 0` if needed. The check resets only Breakwater's
running round, writes its test save, cold-launches, reloads and compares real
entity populations plus non-default score/lives. The profile retains M56's bounded
120-frame history, excludes paused frames and reports fixed-step caps/discarded time.
Use ready mode with shadows on/off for identical-content rendering comparisons.

Build caches are locked per export transaction. Concurrent exports must wait or
use separate caches. The game/APK is a generated deliverable; source, instructions
and evidence are committed, SDK/build caches/signing keys are not.
