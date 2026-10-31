# Initial optional browser backend, 31 October 2026

Separate Experimental work beyond native Alpha 1.1.0 candidate
49c4f3d9e29debe669411f01501e0276dea23231. These are functional development proofs,
not a browser release or human acceptance. Actual compiled-WASM/native identities
and exported file hashes are in the two BROWSER_BUILD receipts; the source was a
recorded dirty development tree. Preserved unrelated experiments were not staged.

## Results

- Original MK2 headless C++ suite compiled to WASM and ran under Node: PASS,
  zero failures. Not a complete browser physics qualification.
- Explicit developer opt-in: 28 native parser checks; ten Python export controls.
  No SDK/browser dependencies are needed by those native controls.
- Actual Chromium 156, Playwright 1.64.1, ANGLE/SwiftShader software rendering:
  ordinary exported Traversal scene, switch to grid scene, keyboard input and
  real save publication. Screenshot inspected for actual sprites, not HUD alone.
- A copied grid project reports entity(1).transform.position through public
  JudasJS. Native MK2 kinematic movement reaches x=-1. Two F6 saves finish.
  An entirely new Chromium process recovers the exact 197655-byte save from
  IndexedDB. After moving from x=0 through x=-1 to x=-2, F7 restores x=-1;
  the fresh save service reports its actual load request completed. The final
  JSON records exact hash, y=2, z=0 and measured service timings. Timings are
  software-browser diagnostics, not performance acceptance or latency promises.
- Same-game competing tab, denied IndexedDB and missing COOP/COEP: fail closed.
  The final hosting preflight explains headers before creating pthread workers.
- Browser native TCP Listen/Connect reject with WebSocket guidance, no bound
  socket/connection and no phantom events after idle Poll; actual compiled WASM.
- Shared native regressions: sprite rendering23, animation rendering12,
  pointer/harness33, script/editor workflow70 (including separate aliased-root
  qualification). Native application scene switching, actual x=-1 endpoint,
  screenshot and save pass. Native editor opt-in launch, Play/Stop (IDENTICAL
  authored state), save and shutdown pass. Synthetic UI/input are not human tests.
- Build organization18 tests pass. Registration8 tests pass, two platform-tool
  controls skipped because Ninja/Visual Studio are unavailable locally.

## Failed attempts retained

1. ICU's platform classifier did not recognize wasm32. Static-only generic make
   rules retain the real wasm target/compiler. Native data tools remain separate.
2. Host pkgdata attempted ELF assembly. Portable C data packaging (-w) fixed the
   actual cross-format boundary; the failed diagnostic is compressed, not erased.
3. External asset package lacked exported FS helpers; boot failed explicitly.
4. SDL2 EGL tried to create a context on the UI thread after canvas transfer.
   The worker now owns WebGL through the supported platform API; SDL owns input.
5. HUD rendered but all scene meshes failed GL_INVALID_OPERATION. A disposable
   draw probe found active unsigned joint attributes disabled with float defaults.
   Correct integer defaults restore sprites; native renderer behavior is unchanged.
6. First cold driver expected a serialized UI request ID to remain a live service
   handle across processes. That was a bad oracle: actual physical restore passed.
   Final telemetry observes the new service's load request independently.
   A later short fixed delay also missed UI telemetry; bounded completion polling
   replaced that timing assumption. Neither changed engine acceptance conditions.

## Reproduction

See ../../BROWSER_SUPPORT.md for SDK 6.0.13 install, matching native tools and
strict export commands. Native host compiler/tools, all engine source and pinned
archives are required. Runtime is genuine shared C++/QuickJS/MK2/GLES, not a game
rewritten in JavaScript. Node 20.20.2 successfully queried the actual WASM identity.

Optional isolated browser tooling:

    npm install --prefix .cache/browser-tests playwright@1.64.1
    node .cache/browser-tests/node_modules/playwright/cli.js install chromium
    mkdir -p .cache/browser-sdk
    cp docs/evidence/browser-initial-20261031/*browser.cjs .cache/browser-sdk/
    cp docs/evidence/browser-initial-20261031/boot-final.cjs .cache/browser-sdk/

The recorded boot driver expects .cache/browser-proof/traversal-licensed; its package
is made from the ordinary committed Traversal project with tools/build_browser.py.
It owns localhost:8792 and an isolated headless context. The cold driver takes a
package path, owns localhost:8793 and a fresh persistent profile, closes Chromium
completely, then reopens that same profile. The negative driver owns ports8794/5.
No ordinary user browser/process is touched.

To reproduce cold telemetry, copy projects/2d_traversal_workshop outside itself,
exclude Saves/.cache, select Scenes/grid.judas as startup and replace controls.js
with observable-controls.js from this folder. Its only additions use public
entity/console/saves observations; gameplay movement is unchanged. Export the copy
with the same matching tools, then run:

    node .cache/browser-sdk/cold-browser.cjs /absolute/path/observable-web
    node .cache/browser-sdk/negative-browser.cjs /absolute/path/observable-web

Full intermediate builds, scripts, original logs and generated sites remain in
.cache/browser-sdk, .cache/browser-build and .cache/browser-proof. The generated
sites used for transient JS probes were disposable; final proofs used newly
exported source-matched sites, not manually patched generated JS.

## Boundaries

Linux compiler host only; WebGL2/SharedArrayBuffer/isolation/IndexedDB/Web Locks
required. The roughly 51 MiB uncompressed universal WASM and 256 MiB initial memory
are an early foundation. No Firefox/Safari/mobile, controller, audible-audio,
all-3D-effect, streaming-archive or production performance certification. No
browser editor, native TCP shim, automatic update or Advanced Fluids unlock.
Browser tooling stays behind -ExpBrowserSupport=1 and is not in native Alpha 1.1.0.

The final web packages include 19 hashed files, readable engine/font and pinned
SDL/GLM notices, and the exact Emscripten/musl/LLVM runtime licence texts. Missing
runtime notices reject packaging. The Windows diagnostic log confirms the native
canonical-versus-lexical asset-registration failure; its repaired release candidate
is kept separate from this browser implementation.
