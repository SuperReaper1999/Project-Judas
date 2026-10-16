# Reproduce M89

Linux working desktop GL session, existing ordinary Release dependencies. No
controller hardware is needed for automated validation. Do not overlap profiling
with native builds or emulator startup. TypeScript is a developer-only external
module, not a game/runtime/build requirement.

```sh
python3 scripts/m89_validation.py --build --regressions \
  --typescript /path/to/typescript --output .cache/m89/repro
python3 scripts/m89_validation.py --performance --export \
  --typescript /path/to/typescript --output .cache/m89/repro-package
```

The latter also reruns the small new focused checks; individual commands are
recorded in `linux/commands.json`. Performance executable arguments are
`2|4 shared|split output.json`; M56 environment variables are ordinary profiler
controls. Real-clock pilot work is bounded to two seconds warm-up/five measured,
with default project physics, uncapped swap and a hidden 1024×768 GL window.
Raw samples, body counts, RSS, final game state and M56 scope data are retained.
These are throughput measurements, not display-refresh or all-hardware guarantees.

The game can be opened normally in `build/judas_editor
projects/core_scramble/core_scramble.judasproj`. Automated Play/Stop uses a private
copy of the project with `JUDAS_EDITOR_AUTOTEST=/absolute/output` and checks that
its authored scene is identical after Stop. See the editor log for actual results.

```sh
python3 tools/build_android.py projects/core_scramble/core_scramble.judasproj \
  --id org.projectjudas.corescramble --name 'Core Scramble' \
  --cache .cache/m89/android --output .cache/m89/CoreScramble.apk
```

Optional `--icu-host-tools /absolute/matching/icu76.1/host-build` avoids rebuilding
host data tools; it is not required. Android setup is in [ANDROID.md](../../ANDROID.md).
For emulator input only, compile `tests/android/LocalGamepads.c` with the NDK's
`x86_64-linux-android26-clang -O2 -o .cache/m89/local-gamepads`. On an isolated rooted
API36 AVD at 720×1280/density240:

```sh
python3 scripts/android/check_local_multiplayer.py \
  --adb "$ANDROID_HOME/platform-tools/adb" --serial emulator-5554 \
  --apk .cache/m89/CoreScramble.apk --gamepads .cache/m89/local-gamepads \
  --output .cache/m89/android-validation
```

The script rejects non-emulator serials. It uses native MT slots (inspect
`getevent -lp`; pass `--touch-device` if different), real Android InputDevice/SDL
virtual controllers, screenshots and logcat. It does not add a game/native test
hook. Physical controllers, phone/tablet usability, audible output and sustained
thermal/performance acceptance remain human/hardware work.
