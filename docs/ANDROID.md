# Native Android games

<!-- judas-release-status:start -->
Source identity is **Judas Engine Alpha v2.0.0** (`2.0.0-alpha.1`).
[Source notes](ALPHA_V2_0_0_NOTES.md) · [Published packages](https://github.com/SuperReaper1999/Project-Judas/releases) ·
[Support boundaries](ALPHA_SUPPORT_MATRIX.md).

Source identity and publication are separate. The
[publication record](RELEASE_STATUS.json) is a dated receipt.
Check the release page for subsequently published packages.
<!-- judas-release-status:end -->


Android is an **optional runtime target**. Linux and Windows keep their existing
build/export workflow and do not need an Android SDK. Android builds use the same
C++ Application, RuntimeWorld, Judas-owned physics, QuickJS gameplay, resources,
Unicode UI and miniaudio engine. SDL supplies the native Activity/window/device
adapter; this is not a browser, remote desktop or streaming client.

## Build and export

Install CMake 3.20+, a C/C++ host compiler, make, Python 3.11+, a complete JDK
17+ (including javac/keytool), and Android SDK command-line tools. Android
Studio/Gradle/npm are not required. Set `ANDROID_HOME` to your SDK and
`JAVA_HOME`/PATH to the JDK. Install/accept the SDK licences normally:

```sh
"$ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager" \
  'platforms;android-36' 'build-tools;36.0.0' 'ndk;28.2.13676358'
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUDAS_BUILD_CHANNEL=Experimental
cmake --build build --target judas judas_export -j4
python3 tools/build_android.py /path/to/Project/breakwater.judasproj \
  --id org.projectjudas.breakwater --name Breakwater \
  --output .cache/android/Breakwater.apk
```

Use the installed versioned command-line-tools directory if it is not named
`latest`. Download/SDK installation requires network; CMake uses only the pinned
local source archives afterward. The build includes host ICU data-generation
tools and ABI-specific native ICU libraries, preserving the existing Unicode
implementation rather than linking Android's private ICU ABI. Caches are large
and belong on disk, not a small RAM `/tmp` filesystem.

The exporter validates/copies normal export project content, cross-compiles the native
runtime for both ABIs, compiles SDL/Judas Java bootstrap code, creates DEX/resources,
strips staging copies of native libraries, aligns and signs an installable APK.
`--shadows off` is the mobile default; `--shadows on` enables 512² maps.
This is renderer quality, not a simulation setting. `--abis arm64-v8a` or `--abis x86_64` builds one ABI. `--cache`, `--sdk`, `--jobs`,
`--exporter`, `--desktop-runtime`, `--id`, `--name`, `--version-code` and `--version-name` customize
the workflow. Use a distinct stable application ID per game. Desktop and mobile
read the **same ordinary project scripts/scenes/assets**.

Default signing uses a local cached **development** key and a debuggable manifest.
It is suitable for sideload testing, not publication. Never commit that key.
The development preset derives its default app ID/name from the project filename
and uses version name `development`; it no longer assigns every project the touch-input
Breakwater identity. Keep explicit app identity/version arguments when updating an
existing installation.

`--preset release` refuses to build without an explicit app ID, name, version code,
version name, keystore and alias. Supply the signing password through its named
environment variable; it is not written to manifests or command arguments:

```sh
python3 tools/build_android.py projects/my_game/my_game.judasproj \
  --preset release --channel Experimental \
  --id com.example.mygame --name 'My Game' --version-code 2 --version-name 1.1 \
  --keystore /private/release.p12 --key-alias release \
  --password-env JUDAS_SIGNING_PASSWORD --output /outside/repository/MyGame.apk
```

Keep the signing key and application ID to update an installed game. A changed key
cannot update the old installation; uninstalling deletes private saves. App
bundles/Play Store submission, asset delivery packs and Windows-host APK generation
are not implemented by this workflow.

### Build identity and reproducibility

The engine channel (`--channel Alpha|Beta|Experimental|Stable|Local`) is independent of the game
version and signing preset. The default is Experimental. The checked-in engine
version is used unless `--engine-version` is explicit. Default caches are separated
under `.cache/android/<channel>`; `--cache` can reuse an existing native cache.
To install two channels side by side, give them distinct application IDs; Android
then treats them as separate apps/storage. Engine channel metadata does not rename
a game or change its save identity automatically.

Native ABI builds and desktop export tools must match source/version/channel.
Rebuild the desktop `judas` and `judas_export` for the same unchanged candidate
first. The APK embeds `assets/judas-build-info.json`, and Android metadata exposes
the engine version/channel. `<name>.build.json` records exact ABI and desktop-tool
identities, content hashes and APK hash; `<name>.sha256` covers APK and receipt.
The removed desktop executable's provenance is not misrepresented as Android's.

Generated ZIP/JAR entries have stable timestamps and ordering. `SOURCE_DATE_EPOCH`
may select an epoch in ZIP's 1980–2107 range (default 1980). This improves repeatability;
it is not a promise of byte-identical native output across compilers, NDKs, signing
keys or different source roots. Signing verification and 16 KiB alignment checks
still run. See [distribution](DISTRIBUTION.md).

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
the existing profiler profiler, launch with `--ez judas.profile true`, play, then use
Exit Game for a clean native shutdown. A debug build allows:

```sh
adb exec-out run-as org.projectjudas.breakwater cat files/judas-profile.json > android-profile.json
```

GPU timestamps are explicitly unavailable on this ES 3 path; CPU/fixed/script
measurements remain real. Emulator frame timings do not certify device performance.

## Input, lifecycle and storage

- Standard Android InputDevice gamepads use the existing input logical bindings.
  Projects still author controls; no built-in mobile gameplay controller is added.
  Bluetooth pairing is OS-owned. Raw Bluetooth HID features requiring extra
  permissions are not enabled by this manifest. Physical pad testing is pending.
- [`input.touches` and `input.pointer`](judasjs/input.md#touch-and-pointer-snapshots)
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
- save-slot slots live separately under private `files/judas/games/<save-id>/Saves`.
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

## Local multiplayer

For repeated builds `--icu-host-tools /absolute/path/to/matching/icu-host-build`
may reuse ICU 76.1's existing native data tools. The default still builds its own;
SDK/ICU caches are developer artifacts, never packaged runtime dependencies.

## APK verification

The builder checks one complete JDK 17+ (`java`, `javac`, `keytool`) before
configuring native code. An explicit `JAVA_HOME` is authoritative; an incomplete
JRE or older compiler fails with an actionable error. The resolved JDK directory
is used consistently by Java compilation, DEX tools and development signing.
Desktop exporter/runtime identities are compared before configuration, and each
configured ABI is compared before its expensive native build.

APKs contain `assets/judas-build-info.json`, `assets/judas-content.json` and
`assets/judas-native.json`. The native inventory hashes the **stripped libraries
actually shipped**, including `libc++_shared.so`; the external `.build.json` also
retains input-library hashes separately. An independent stdlib-only verifier
checks all content/native entries, exact library closure, source identities,
architecture and every library's 16 KiB ELF load alignment:

```sh
python3 tools/verify_android.py /outside/game.apk --receipt /outside/game.build.json --output /outside/verification.json
```

This does not replace `apksigner verify`, `zipalign -c -P 16 4`, emulator or
device testing. The builder performs both Android-tool checks before publication.
`DT_NEEDED` imports must resolve to the shipped ABI libraries or the verifier's
explicit public Android library set; the builder additionally verifies those
system names against the actual API-26 NDK stubs. Private/vendor dependencies are
refused. This is import-name closure, not a full symbol-by-symbol/minimum-device
certification. Older APKs without `judas-native.json` cannot retroactively satisfy
this stronger inventory contract; preserve their original validation scope.
Corruption, missing/extra content, changed native libraries, duplicate ZIP entries
and mismatched identities fail. Verification has a 2 GiB total unpacked budget;
larger custom packages need an explicit application-specific verification policy.

The APK, receipt and checksum are prepared together and restored to their previous
bytes on an ordinary publication error. The three renames are not power-loss atomic;
verify the checksum pair and do not read or publish one output concurrently.
Outputs must be ordinary `.apk` files with ordinary receipt/checksum companions.
No keys, passwords or phone installations are part of artifact verification.

When using downloaded or locally built Alpha desktop tools, pass `--channel Alpha`
to `build_android.py`. The default remains `Experimental`; it does not infer or
switch release channels from an executable. A channel-only mismatch now shows
both values and suggests the matching flag, rather than advising an ineffective
rebuild. Other provenance differences remain strict errors and list the fields
that differ. `Stable` is reserved for GA, not an Alpha release.

## Game orientation

Android exports accept `--orientation any|landscape|portrait|sensorLandscape|sensorPortrait`.
The default `any` omits an activity orientation restriction, preserving normal
device rotation. Use `--orientation sensorLandscape` for Hinge-style landscape
games that should support either landscape direction, or `sensorPortrait` for
either portrait direction. `landscape`/`portrait` request the corresponding fixed
orientation. This is an APK build option, not a `.judasproj` field. Record it in
your project's build command; the `.build.json` receipt records the selected policy.

```sh
python3 tools/build_android.py path/to/hinge.judasproj \
  --orientation sensorLandscape --output /outside/Hinge.apk
```

The exporter sets `android:screenOrientation` on the native activity, which
preserves it when SDL initializes its resizable window. With `any`, SDL retains
its existing user-rotation behavior. Android
may override requests in multi-window or device-specific compatibility modes.
The API-36 large-screen default has an exception for apps categorized as games,
as Judas packages are; see [Android's documented exceptions](https://developer.android.com/about/versions/16/behavior-changes-16#exceptions).
Keep layouts responsive through UI anchors and
`world.viewport`; an orientation policy is not a guarantee of a fixed surface
size on every device. Desktop aspect tests are described in
[the application harness guide](TEST_HARNESS.md#window-size-and-rotation-layout-tests).

### Orientation acceptance for the Alpha v1.0.4 patch

### Development whole-frame benchmark APKs

A development APK can run the same ordinary-application capture used by
`judas_whole_engine_benchmark`, without a synthetic simulation clock:

```sh
python3 tools/build_android.py /private/project/game.judasproj \
  --output .cache/android/GameBenchmark.apk --abis arm64-v8a --channel Alpha \
  --id org.projectjudas.gamebenchmark --name 'Game benchmark' \
  --orientation sensorLandscape --benchmark --benchmark-key key:F4 \
  --benchmark-seconds 20 --benchmark-warmup 10
```

The desktop export tools and Android channel/source must match. Choose the
launch control belonging to the project; the engine contains no vehicle launch
logic. The benchmark waits for resource preparation, requests interval-zero
presentation, holds that control after warmup, records the actual application
loop, and exits after the measurement. It does not change physics quality,
fixed cadence, catch-up limits or discarded-time policy. Use a project with
appropriate automatic or authored controls; normal touch input is isolated
while the diagnostic runner supplies its reproducible input.

Each launch writes a new private `files/benchmark-TIMESTAMP-UUID/` directory:
`frames.jsonl` retains every measured frame (including fixed costs, native
counters, backlog and discarded/clipped time), `run.json` records identity,
drawable size, renderer and presentation settings, and the existing profiler
retains its bounded detailed timeline separately. Read the complete JSONL when
investigating early impacts, rather than relying on the last 120 frames.
Retrieve only this app's files with `adb exec-out run-as APPLICATION_ID ...`.

The debuggable-only `judas.benchmark` intent extra can enable/disable this mode;
matching `.key`, `.seconds` and `.warmup` extras override its bounded defaults.
Durations are 2..120 seconds and warmup 0..30 seconds. Release/private-key APKs
reject the benchmark preset. The bootstrap accepts no arbitrary output path or
environment injection.

Android may enforce compositor pacing even after an interval-zero request.
Inspect the recorded swap result/interval and drawable resolution when comparing
phones with desktop throughput. GPU timestamp calibration is unavailable on this
ES path and is never fabricated. Compare the same authored physical workload
and report rendering, scheduling, thermal and capture-policy differences.
