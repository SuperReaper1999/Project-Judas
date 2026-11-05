# Renderer V2 portability qualification — 5 November 2026

This is compiler-only qualification for Windows/Android and an actual browser-driver test of the changed rendering owners. It is not Windows/Android runtime qualification or a complete browser application test.

The starting committed source is `6ac95d7ba6e95cc7f12be4f586c8a297637a4ba9` plus the Renderer V2 candidate working changes. `SUMMARY.json`, the per-command hashes in `compiler-only/RESULTS.json`, and the linked proof's `webgl2/BUILD.json` identify the actual source. All recorded source hashes still matched after validation. The parent Renderer V2 report owns the final candidate commit identity.

## Results

| Check | Result | Actual scope |
|---|---|---|
| Emscripten compiler | 12/12 translation units passed | Renderer, Window, WorldPresentation and every current `src/render/*.cpp`; no full application link |
| Windows MinGW-w64 compiler | 12/12 translation units passed | Cross compiler only; no Windows link, execution or hardware claim |
| Android ARM64 compiler | 12/12 translation units passed | NDK target `aarch64-none-linux-android26`; no APK, emulator or hardware claim |
| Actual WebGL2 | 33/33 checks passed | Changed production rendering owners linked with Emscripten and executed in Chromium 156.0.8078.4 using software ANGLE/SwiftShader |

The browser probe compiled in **8.78 seconds** with production exception handling enabled (`-fexceptions`). Its transient service completed in **3.224 seconds**, consumed **2.914 CPU seconds**, and peaked at **501.5 MiB**, with **zero swap**. These are qualification-process costs, not representative game-rendering performance measurements.

The Wasm memory was fixed at **33,554,432 bytes** with growth disabled. The complete browser process group was limited to **768 MiB**, zero swap and 60 seconds. There was one page and one WebGL context, no application loop, no persistent browser profile and no long-running observer. The program explicitly destroyed its graphics context; the observer closed Chromium and its HTTP server. No page or GL errors occurred.

## What actually executed in WebGL2

The probe directly compiles the production `GraphicsCapabilities`, `ShaderLibrary`, `ViewUniforms`, `ParticlePass`, `StreamArena` and `ComputeDevice` implementations. It does not substitute JavaScript reconstructions of those owners.

- Actual canonical mesh vertex/fragment sources are transformed by the production view-block adapter, prepared for ES300 and compiled/linked by the browser driver.
- Canonical Legacy ambient-only, actual Unlit and actual nonmetal PBR materials render known-answer colours. The PBR oracle independently computes quarter-strength ambient, luminance tone mapping and sRGB encoding (124/255).
- Independent RGB channels test the camera, ninth shadow matrix and projection through the real 720-byte std140 UBO. Rebinding after another pass does not add an unchanged upload; changing the ninth shadow updates its expected channel with one upload.
- Production compact billboards render known alpha colours. Reversed input retains depth ordering; reversed physical depth changes the image; equal depth retains original-index tie ordering. Uploads remain 32 bytes per particle.
- The real stream owner uploads a known GPU colour payload and falls back to portable storage even when persistent storage is requested. Invalid input cannot publish a stale slice.
- Forged desktop capability flags cannot enable compute or allocate a compute buffer on the compiled WebGL2 backend.
- Explicit shutdown retires resources before context destruction.

`tests/RendererPortableWebTests.cpp` is the test source. `webgl2/RESULTS-chromium.json` and `console-chromium.json` retain every assertion and driver result. The screenshot is a machine-result display, not visual acceptance of a game.

## Preserved failures and prerequisites

The first compiler-only invocation passed browser compilation but encountered 20 missing-SDL-header failures across the Windows/Android configure-only caches. Those caches contained generated `SDL_revision.h` and platform-specific `SDL_config.h`, but SDL had not populated its public `SDL2/*.h` forwarding directory. This was a test prerequisite, not a renderer compile defect.

The initial commands and failures remain in `first-prerequisite-failures/`. The rerun used isolated test-only header overlays pointing at each target's actual cached SDL source headers and its generated `SDL_config.h`. No dependency was built, no target configuration changed and no engine source was modified for this prerequisite. `compiler-only/HEADER-OVERLAYS.json` records each source and hash. `compiler-only/reproduce.py` recreates that setup in `.cache` and runs one compiler process at a time.

Browser startup also exposed two infrastructure constraints, preserved in `SUMMARY.json`: the restricted tool process could not access the user service bus, and the first external service could not find nvm's Node through its inherited PATH. The successful command uses the actual absolute Node path and an explicit project working directory. The tracked build helper now generates those paths in `COMMANDS.txt` rather than relying on an interactive shell.

One pre-existing browser-only unused-parameter warning remains in `Window::DispatchRumble` (`device`, source lineage `1f52d45c6`). It was not hidden or changed in this task.

## Reproduction

From the repository root, with the existing configured cross-toolchain caches:

```sh
python3 docs/evidence/renderer-v2/portability/compiler-only/reproduce.py
python3 tools/validation/renderer_web_proof.py build
```

Run the newly generated `.cache/renderer-v2/web-proof/COMMANDS.txt` command after compilation and performance workloads have finished. It selects the installed Node path, uses a bounded transient user service, and invokes `tools/validation/renderer_web_observer.cjs chromium`.

The retained successful invocation is in `webgl2/COMMANDS.txt`. Build flags, source hashes and artifact checksums are retained alongside the browser results. The compiled JS/Wasm/HTML remain in `.cache/renderer-v2/web-proof/` and can be reproduced; unnecessary binary copies are not added to the repository evidence.

No Windows or Android runtime was started. No physical GPU, controller, touch, audio, complete game export, scene lifecycle or sustained browser-memory qualification is inferred from these checks. Existing platform acceptance remains separate. Firefox was not run in this bounded pass.
