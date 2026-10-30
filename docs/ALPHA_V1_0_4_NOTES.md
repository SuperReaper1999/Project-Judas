# Judas Engine Alpha v1.0.4

Machine version: **`1.0.4-alpha.1`**. Alpha, not Stable/GA.
Source, candidate preparation and publication are separate states. The immutable
release manifest and protected publication record identify exact shipped bytes.
Previous release tags/packages remain unchanged.

## Changes

- Android APK exports accept `--orientation any|landscape|portrait|sensorLandscape|sensorPortrait`.
  Default `any` preserves existing behavior. An explicit activity policy survives
  SDL's native-window initialization, which otherwise overrides the manifest.
  The build receipt records the selected policy. This is an export option, not
  a new `.judasproj` field or JudasJS API.
- Desktop startup accepts `JUDAS_WINDOW_SIZE=WIDTHxHEIGHT`; the default remains
  1024×768. Strict dimensions from 1 to 8192 reject malformed/oversized requests.
- The application harness adds `RESIZE width height frame` and
  `EXPECT_VIEWPORT width height frame`. Resize reaches the real window and
  published gameplay viewport before input/UI callbacks. On Mesa/Xvfb it also
  completes the presentation boundary before the next capture, avoiding the
  old-size backbuffer and black strips. This is not a sleep or image repair.
- RGB screenshot readback handles odd-width rows and preserves the caller's
  OpenGL packing alignment.
- The pixel regression covers seven repeated landscape/portrait/odd-size
  requests; the test loop uses a reference rather than copying pairs.
- CI routing permits Android tooling checks to overlap native validation while
  still requiring every excluded path to have a lightweight route. Android
  source/tool changes continue to require native gates. No gate is waived.

No JudasJS binding was added or renamed. Existing declarations, API inventory and
reference remain the contract; the UI/camera guides link the new testing workflow.
No physics, game logic, Android touch/input policy or save compatibility was changed.

## Evidence and acceptance

- The original Xvfb resize regression reproduces 20/21; the expanded test linked
  against the original implementation fails six of 33 checks. The corrected
  version passes 33/33 on Xvfb/Mesa26 and real X11/NVIDIA; ordinary application
  regressions pass 42/42 on both.
- Hinge's original portrait capture had 440 black rows. Corrected portrait/wide
  captures and a relocated standalone have no black rows or columns. The initial
  before/after landscape PNG is byte-identical.
- Android tooling tests pass 12/12 with JDK available, alongside real SDK36
  manifest compilation and actual JudasActivity/SDL Java compilation. Claude's
  independent Mesa25.2.8 verification also reports the corrected checks passing.
- The operator confirms the rebuilt Hinge phone APK works, following the
  landscape-directions/pause-resume checklist. Claude identifies it as
  `d5f02448` with `sensorLandscape`; its APK bytes/device identity were not
  independently collected here. This is not universal device/controller testing.
- Exact-source desktop package qualification is recorded by the preparation
  workflow and release artifacts; it must succeed before publication. Current
  claims do not relabel the earlier focused binaries as release SDKs.

Detailed source/failure evidence: [orientation/aspect work](HINGE_DISPLAY_FEATURES.txt),
[headless correction](HINGE_HEADLESS_RESIZE_CORRECTION.txt).
[Controls](TEST_HARNESS.md#window-size-and-rotation-layout-tests) ·
[Android orientation](ANDROID.md#game-orientation) ·
[Support matrix](ALPHA_SUPPORT_MATRIX.md) · [Publication process](RELEASING.md).

## Retained limits

**Advanced Fluid Dynamics remains compiled-locked in Alpha.** Basic Water and
swimming remain available. Private pressure/transport research is not a released
backend and this patch cannot enable it.

SDL's offscreen backend cannot resize its backing surface mid-run and rejects
`RESIZE`; use a real desktop backend/Xvfb or separate startup-size runs. Android
owns its surface size and rejects desktop resize commands. Android may override
orientation requests in some window/device compatibility modes; keep responsive
layouts. `EXPECT_VIEWPORT` checks dimensions, not image contents; the separate
pixel regression verifies the backing surface. No automatic installer or
project overwrite is introduced.
