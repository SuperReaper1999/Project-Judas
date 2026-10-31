# Experimental browser game builds

This is a separate development effort beyond the Alpha 1.1.0 release candidate.
It is **not** a browser-support certification or part of the already-frozen 2D
release. See BROWSER_EXPORT_REVIEW.md for architecture and qualification stages.

## Deliberate opt-in

The editor hides the browser-export guidance unless started with:

```sh
judas_editor -ExpBrowserSupport=1 MyGame.judasproj
```

`-ExpBrowserSupport = 1` is also accepted by the editor. `=0`, or omitting the
option, leaves browser UI hidden. The option is per launch, never saved in a
project or editor preferences. Duplicate/conflicting or invalid values reject.
The browser compiler separately requires the exact explicit opt-in; an editor
flag never silently installs toolchains or launches a build.

## Current developer workflow

The initial compiler host is Linux. Install the official Emscripten SDK 6.0.13 in
an isolated folder. Keep the complete Judas source (including pinned dependency
archives), matching native judas_export and judas tools, CMake, Python, Node.js and make.
Native Linux, MSVC and Android workflows do not require Emscripten.

```sh
python3 tools/build_browser.py -ExpBrowserSupport=1 \
  --project /absolute/path/MyGame.judasproj \
  --output /absolute/path/fresh-web-package \
  --emsdk /absolute/path/emsdk \
  --build /absolute/path/browser-build-cache \
  --exporter /absolute/path/native-build/judas_export \
  --desktop-runtime /absolute/path/native-build/judas \
  --jobs 2
python3 tools/serve_browser.py /absolute/path/fresh-web-package --port 8080
```

A matching native ICU 76.1 build may be reused with `--icu-host PATH` if its
config/icucross.mk and data tools are intact; otherwise the browser build prepares
native data tools. This is cross compilation, not linking system ICU into WASM.
The first build is expensive; cache it. Subsequent game packaging reuses the
runtime and produces separate game.data/JS files rather than embedding game
rules in C++. Build/output ownership locks prevent concurrent cache mutation.
Existing output directories are never silently replaced. Failed build stages
remain under the selected build cache for diagnosis.

The tool requires matching source/version/channel identities from native tools
and browser runtime. Do not relabel an older binary or disable identity checks.
Copy the whole output directory, including readable engine/web runtime licence
notices, to a compatible host; do not copy just index.html.

## Hosting and persistence

Open the served HTTP localhost URL or deploy on HTTPS with COOP same-origin and
COEP require-corp. This threaded build requires cross-origin isolation,
SharedArrayBuffer, WebGL2, IndexedDB and Web Locks. Opening index.html through
file://, or using a host/iframe without the headers, is unsupported. No public
hosting service or mobile-browser compatibility is assumed.

The page hydrates a per-game IndexedDB mount before enabling Play. An exclusive
browser lock prevents two tabs from writing the same game's saves. Each game's
save identity owns a different mount/database; unrelated games do not share a
whole-filesystem snapshot. Storage errors reject rather than pretending progress
was saved. The native save service waits for completed IndexedDB synchronization
before reporting successful publication. A timeout leaves storage faulted until
reload rather than racing a later write against an uncertain old transaction.

Browser saves are local to that origin/profile. Clearing site data removes them;
this is not cloud synchronization or automatic browser-to-desktop save transfer.
Never close the tab while a save is still pending. Existing content-identity and
migration contracts still apply.

## Initial scope and limitations

- Same C++ engine, QuickJS gameplay and MK2 solver; same sprite/tilemap renderer
  through the existing GLES3 facilities. No second JS game implementation.
- Script-owned projects only. Advanced Fluid Dynamics and legacy hardcoded
  gameplay are rejected at runtime, including named-format project files.
- Only the ordinary save-service publication path has browser durability proof;
  regional streaming archives, mods, user filesystem pickers and other direct
  file-writing workflows are not independently qualified.
- Native TCP hosting/joining is explicitly unavailable. A future WebSocket
  transport/server endpoint is separate work.
- Fixed 1024×768 canvas with proportional CSS display is the initial page layout;
  harness RESIZE rejects explicitly. Full responsive surfaces, mobile touch/controller behaviour and every 3D effect
  are not promised by this prototype.
- Loading/errors, keyboard focus and cold saves have actual Chromium tests.
  Browser audio mixing and Firefox offline-game checks are covered by the next-stage qualification below; physical audio hardware, controllers, touch and other browser families remain unqualified.
  Reload after graphics-context loss; automatic GPU resource restoration is not
  implemented or qualified by this port.
- A worker owns the WebGL context and yields to requestAnimationFrame through
  Asyncify; SDL still owns input/window events. Memory growth and
  these scheduling facilities have costs; no native-equivalent performance claim.
- No browser-hosted editor, automatic SDK update or unconditional browser feature
  switch. Native games keep their existing launch behaviour.

## Evidence status

The original MK2 headless suite passed as actual compiled WASM under Node. The
launch-option parser passed 28 checks and the exporter boundary suite passed ten.
Chromium 156 with software ANGLE/SwiftShader rendered the actual Traversal game,
accepted keyboard movement and completed two durable saves. A completely new
Chromium process recovered the exact saved bytes and loaded the real player
endpoint. A competing same-game tab, denied IndexedDB and missing isolation
headers all failed closed. The native application still switched scenes, moved,
rendered a capture and saved using its ordinary desktop backend.

This is software-browser functional evidence, not browser hardware performance,
Safari/mobile certification or complete-engine compatibility. The first
uncompressed universal WASM is roughly 51 MiB, plus project data; initial memory
is 256 MiB with growth bounded at 1 GiB. Assets are preloaded as one package, not
HTTP-residency streamed. Compression, download size, responsive surfaces,
more 3D effects and browser-specific qualification remain future work. The current
scope is an early optional foundation, not a production browser distribution.

The failed preload, canvas-ownership and integer-attribute experiments are retained
alongside reproduction scripts and final results under the browser evidence folder.
Native Alpha 1.1.0 packages do not include or certify this browser prototype.

## Backend references

The implementation follows Emscripten's [pthread ownership rules](https://emscripten.org/docs/porting/pthreads.html),
[worker WebGL context API](https://emscripten.org/docs/api_reference/html5.h.html),
and [WebGL guidance](https://emscripten.org/docs/optimizing/Optimizing-WebGL.html).
These describe the platform boundary; they are not evidence that every Judas
subsystem has been browser-qualified.

Browser-owned shortcuts such as Firefox F6 may never reach SDL. Bind ordinary
project controls instead of trapping the browser UI: Traversal uses K to save and
L to load, with F6/F7 retained as desktop alternatives.

## Offline-game stage and audio architecture

The next bounded qualification uses Beacon Grove and Traversal in Chromium and
Firefox on Linux. See [the offline qualification procedure](BROWSER_OFFLINE_QUALIFICATION.md)
and its evidence status; this does not certify the entire engine for the web.
Beacon Grove now offers K/L save/load alongside desktop F6/F7.

The default miniaudio Web Audio backend previously looked for `window` on the game
pthread and returned no-backend (-203). Its DOM/Web Audio operations now use
synchronous Emscripten main-thread proxies. The normal native mixer, decoder,
voices and game-side audio APIs remain in use; there is no second browser game
implementation or JavaScript-only sound generator. Native backends are unchanged.
The vendored adjustment is marked at the Web Audio implementation entry.

This first audio path uses miniaudio's existing ScriptProcessorNode backend. It is
deprecated in browsers and runs mixing callbacks on the UI thread. AudioWorklet,
capture, mobile audio, background-tab policy and latency guarantees remain
unqualified. Click the canvas after Play to unlock audio if the browser blocks
autoplay. Quality/speaker output still needs human listening; measured Web Audio
output is functional evidence, not a hardware listening test.

## Editor export and local preview

Start the Linux source editor with `-ExpBrowserSupport=1`. **File → Export browser
 game (Experimental)…** opens Project Settings → Export. The native Release
export remains separate. This launch option is never saved into the project.

1. Save project, scenes and assets. Choose a **new package folder outside the
   project**. Browse chooses its parent; edit the appended `web-game` name.
2. Open **Toolchain and build options** if needed. Select the installed Emscripten
   6.0.13 root, browser build cache and matching Release exporter/runtime. The
   compiler-script field must point into the Judas source checkout. No SDK or
   toolchain is installed automatically. Installed native SDKs do not magically
   become browser compiler hosts.
3. **Check prerequisites** reports missing tools, unsupported project policy,
   output conflicts and mismatched native identities without compiling, creating
   the build/output folders or downloading anything. The final current-source
   fingerprint check remains CMake's responsibility during the actual build.
4. **Build browser game** uses the ordinary saved-content confirmation when the
   project or scene has unsaved edits. Build output appears in the panel and in
   its retained log. The editor stays responsive. Avoid modifying saved assets
   while they are being packaged. The first engine compile can take minutes.
5. **Preview package locally**, then **Open game in browser**. The preview checks
   the complete inventory/hashes, binds only to loopback and sends the required
   isolation headers. Its default port is 8081; choose another if occupied.
   Keep the same port for the same browser save origin. Port 0 chooses a free,
   potentially different port, so it is unsuitable for stable save origins.
6. **Stop local preview** when finished. Closing the editor stops only its own
   compiler/preview process groups; it does not stop another editor/game/server.

Cancelling a build stops its compiler descendants and preserves logs/staging for
inspection. A cancelled/crashed build is not a successful export. If the entire
editor is forcibly killed before cleanup, a cache lock can remain: inspect its
recorded PID and verify that the compiler is gone before removing that lock.
Do not remove a live job's lock or share a cache between simultaneous builds.
Large compiler temporary files use the selected build filesystem, not a small
RAM-backed `/tmp`. Toolchain/output fields are session choices, not game settings.

### Command-line preflight

Add `--check` to the normal `tools/build_browser.py` invocation. It emits a JSON
`judas-browser-preflight` report and exits 0 when prerequisites pass, 2 otherwise.
It needs the same explicit opt-in as building. Missing tools are reported together;
a successful preflight is not an asset/game/browser certification.

### Relocating and hosting the package

New packages include `serve.py` and `browser_package.py`, in addition to the game,
notices and provenance. Copy the **whole folder** to another location or machine
with Python 3.10+; neither source checkout nor Emscripten is needed to preview it:

```sh
python3 /absolute/copied-package/serve.py --verify-only
python3 /absolute/copied-package/serve.py --port 8081
```

The helper verifies every recorded SHA-256 and rejects missing, modified, unlisted
or symlinked files and unsafe inventory paths before serving. It serves only
inventoried paths, with no directory listing. Python bytecode caches are disabled
so preview does not modify its own inventory. Keep served files immutable; rerun
verification after changing anything. This local receipt detects corruption; it
is not a signed publisher-authenticity manifest.

For internet hosting, deploy the same immutable files on an HTTPS static host
that supplies COOP `same-origin`, COEP `require-corp` and suitable resource policy.
The included server is a localhost development preview, not a production hosting
service. Hosts/iframes without the required isolation remain unsupported. Do not
open index.html with file://. No public upload, installation or release happens
from the editor.

The earlier standalone audio-probe evidence used a deliberately incomplete
receipt with the old diagnostic server. Reproduce that historical probe using
the server from `567f03e7`; the current preview intentionally rejects its dummy
receipt. Ordinary completed stage-1/2 packages still verify normally.
