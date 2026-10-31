# Browser game export feasibility — 31 October 2026

Investigation of Experimental at 70fe8ff4 plus the accompanying usability edits.
**Not an implemented or qualified platform.** No Emscripten compiler was found on
PATH; no WebAssembly build or browser gameplay was attempted. Current desktop
and Android exports are not HTML games. No release or channel changes proposed.

## Recommendation

Build an optional **Emscripten / WebAssembly / WebGL 2 game runtime**, with the
existing native desktop editor cooking assets and exporting a web directory.
Keep the same scenes, JudasJS/QuickJS runtime, MK2 physics and renderer. A small
HTML loading/start screen hosts the canvas. Do not replace the engine with a JS
rewrite, remote streaming, a second 2D renderer or a browser-hosted editor.

This is feasible in principle, but a substantive platform port, not a packaging
flag. The lowest-risk first proof is Beacon Grove and Traversal Workshop, not
planetary worlds, every renderer effect or online multiplayer.

## Actual source and necessary adaptations

| Area | Observed implementation | Required browser work |
|---|---|---|
| Build | CMakeLists.txt selects Android GLES or desktop GLAD; no Emscripten branch. cmake/JudasTextDependencies.cmake otherwise configures ICU explicitly for Linux. | Add an optional cross target, SDL Emscripten port, explicit runtime-only targets, host tools vs target libraries, cross-built ICU/data/Freetype/HarfBuzz. Preserve Unicode rather than silently downgrading text. |
| Graphics | src/GraphicsApi.h, Window.cpp and Renderer.cpp select ES3 shaders only for Android. Existing ES path replaces texture-buffer bone palettes. | Share the ES capability path with WebGL2 without pretending the platform is Android. Audit formats, framebuffer completeness, floating-point attachments, texture readbacks, mapped buffers, shader precision and extension-dependent profiling. Reuse existing Android fallbacks where valid. |
| Scheduling | src/Application.cpp has an owning while-loop; JobSystem creates worker threads and waits on condition variables. Resource/save operations consume it. | Browser-scheduled frame callbacks and explicit lifetime/shutdown; deliberate worker architecture. Do not simply block the browser main thread or replace asynchronous services with synchronous stalls. |
| Assets/saves | Native filesystem paths and cold-save service, src/SaveStorage.cpp / SaveService.cpp and existing export dependency closure. | Mount packaged cooked assets, browser persistent storage, asynchronous startup/flush boundaries and quota/errors. Report success only after durable browser synchronization; support export/import of save backups. |
| Input/audio | SDL input/windowing, native mouse capture, miniaudio backend. Vendored miniaudio explicitly contains a Web Audio backend. | Canvas resize/DPI, focus loss, pointer lock/fullscreen, touch and gamepads; click-to-start/audio permission handling. Test actual browser behaviour. Source support is not validation. |
| Networking | src/NetworkTransport.cpp opens native TCP sockets and has host/listen behaviour. | A WebSocket-capable transport/server endpoint or explicit unsupported diagnostics. Existing desktop hosting is not automatically available in a tab. Keep networking out of the first offline proof. |
| Export | Native executables/runtime distributions and asset dependency manifests. | index.html, JS loader, wasm and cooked data; MIME/compression/hosting instructions, checksums and exact provenance. Web packages must not contain editor tools or unrelated source assets. |

WebGL2 builds on ES3, but is not all of desktop OpenGL. The official
[Emscripten GL guidance](https://emscripten.org/docs/optimizing/Optimizing-WebGL.html)
explains its restrictions and animation-frame scheduling. Renderer calls need a
capability audit even when already guarded by Android macros.

## Threads and hosting decision

A threaded first prototype is the closest match to the current job architecture.
Run engine work off the browser UI thread, use bounded prestarted workers, and
return graphics/input operations through the supported browser/SDL path. Validate
wait/proxy behaviour before adopting it. Emscripten pthreads require cross-origin
isolation (COOP/COEP); this constrains hosting and embedded portals. A separately
qualified cooperative job backend could support non-isolated hosts, but it must
preserve cancellation, publication and save completion semantics. Do not promise
both merely because compilation succeeds. See [official pthread requirements](https://emscripten.org/docs/porting/pthreads.html).

The first deliverable should state exactly which hosting headers it needs, with
a runnable local server and deployment example. No WebGPU rewrite is needed to
prove current sprite games. More advanced effects can be qualified separately.

## Persistence and network boundaries

In-memory filesystem writes disappear on refresh. Browser persistence needs an
explicit persistent mount and completed synchronization; see [Emscripten filesystem overview](https://emscripten.org/docs/porting/files/file_systems_overview.html).
Test cold reload, denied/quota-limited storage and interrupted writes, not only
same-session saves. Backups and content identities remain useful.

Browsers cannot directly open arbitrary TCP connections. [Emscripten networking](https://emscripten.org/docs/porting/networking.html)
describes WebSockets and proxy constraints. A browser client needs a compatible
endpoint; peer hosting/dedicated servers stay native initially. Do not advertise
M99 compatibility through unchecked socket emulation.

## Proposed bounded sequence and acceptance

1. **Toolchain and boot proof.** Cross-build actual dependencies, runtime and
   Unicode text; render a native sprite, run QuickJS and MK2, receive keyboard/
   pointer input and unlock audio in Chromium and Firefox. Record download size,
   memory and frame times. No feature claims from compilation alone.
2. **Complete offline game.** Export the unchanged Beacon Grove gameplay and
   Traversal Workshop through the desktop tools. Verify tilemap, animation/events,
   sword/query helpers, parallax, one-way CCD, grid movement, touch where offered,
   scene changes and persistent saves across page reload. Compare native physical
   acceptance with web tolerances justified by actual measurements; do not assume
   cross-platform bit-identical trajectories.
3. **Developer export workflow.** Browser export preset, missing-toolchain/preflight
   diagnostics, relocatable hosted files, start/loading/error screen and tested
   hosting configuration. No network/editor-process APIs silently exposed.
4. **Broaden only with evidence.** Mobile browsers, optional no-threads build,
   advanced rendering and WebSocket clients are subsequent qualification scopes.

Each step can fail independently. Threading/dependency boot is the highest-risk
unknown to prototype first; download/memory cost and browser graphics behaviour
must be measured before promising broad game compatibility. No defensible timing
estimate or browser performance claim is available from this source review.
