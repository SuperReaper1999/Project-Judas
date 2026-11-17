# Experimental browser game builds

Optional game export uses the same engine through WebAssembly/WebGL2. It requires
explicit opt-in and a separate Emscripten/source toolchain; no browser editor or
current-snapshot device certification is implied. See [the qualification matrix](DEVELOPMENT_READINESS.md).

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

## Runtime scope and limitations

- Same C++ engine, QuickJS gameplay and MK2 solver; same sprite/tilemap renderer
  through the existing GLES3 facilities. No second JS game implementation.
- Script-owned projects only. Advanced Fluid Dynamics must be disabled: both the browser compiler preflight
  and runtime reject enabled projects. Retired native gameplay is absent.
- Only the ordinary save-service publication path has browser durability proof;
  regional streaming archives, mods, user filesystem pickers and other direct
  file-writing workflows are not independently qualified.
- Native TCP hosting/joining is explicitly unavailable. A future WebSocket
  transport/server endpoint is separate work.
- Responsive browser-owned canvas, optional fullscreen and bounded high-DPI rendering.
  Harness RESIZE still rejects: resize the page viewport instead. Touch has bounded
  Android phone/emulator qualification; physical controllers and every 3D effect
  remain unqualified. See the later qualification records.
- Loading/errors, keyboard focus and cold saves have actual Chromium tests.
  Browser audio mixing and Firefox offline-game checks are covered by the next-stage qualification below; physical audio hardware, controllers and other browser families remain unqualified;
  later records cover bounded Android touch tests.
  Reload after graphics-context loss; automatic GPU resource restoration is not
  implemented or qualified by this port.
- A worker owns the WebGL context and yields to requestAnimationFrame through
  Asyncify; SDL still owns input/window events. Memory growth and
  these scheduling facilities have costs; no native-equivalent performance claim.
- No browser-hosted editor, automatic SDK update or unconditional browser feature
  switch. Native games keep their existing launch behaviour.

## Backend references

The implementation follows Emscripten's [pthread ownership rules](https://emscripten.org/docs/porting/pthreads.html),
[worker WebGL context API](https://emscripten.org/docs/api_reference/html5.h.html),
and [WebGL guidance](https://emscripten.org/docs/optimizing/Optimizing-WebGL.html).
These describe the platform boundary; they are not evidence that every Judas
subsystem has been browser-qualified.

Browser-owned shortcuts such as Firefox F6 may never reach SDL. Bind ordinary
project controls instead of trapping the browser UI: Traversal uses K to save and
L to load, with F6/F7 retained as desktop alternatives.

## Browser audio and durable storage

The next bounded qualification uses Beacon Grove and Traversal in Chromium and
Firefox on Linux. See [the offline qualification procedure](BROWSER_SUPPORT.md)
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
capture, audible physical-device output and latency guarantees remain
unqualified. Bounded background/lock and mobile audio-graph qualification is
recorded below; it is not a speaker-quality certificate. Click the canvas after Play to unlock audio if the browser blocks
autoplay. Quality/speaker output still needs human listening; measured Web Audio
output is functional evidence, not a hardware listening test.

## Editor export and local preview

Start the Linux source editor with `-ExpBrowserSupport=1`. Use **File → Build &
Export… → Build for browser…** or **File → Export browser game (Experimental)…**.
With this launch flag the focused
export window also opens at startup. It handles the workflow; there is
no need to navigate project settings or understand compiler paths.

1. **Choose where to save it.** The default location is beside the project.
   Use **Change folder…** to choose another parent folder if you want.
2. **Build & play.** Setup is checked automatically. Save any edits when asked.
   The editor prepares matching game tools, builds the web files and opens your
   browser. The first build can take several minutes. Nothing is uploaded.
3. Press **Play** on the browser page. Use **Show exported files** to find the
   complete package, or **Export files only** if you do not want to play it.

Each build gets a fresh folder (`Game-web`, `Game-web-2`, …), so earlier exports
are preserved. When rebuilding with an existing local preview, the same address
is reused: refresh that game tab to load the new build. This avoids opening a
second tab that competes for the same saved-game lock. **Stop local preview**
stops only this editor's local server. Closing the editor stops it too.

**Setup & troubleshooting** contains installation paths, build workers, local
port, prerequisite results and logs. They are hidden during the normal workflow.
Tools are detected from the source checkout, the editor's build folder and an
installed EMSDK where present. A missing SDK still requires installation; the
editor never silently downloads or installs one.

Automatic tool preparation updates only `judas` and `judas_export` in their
existing native Release cache, after verifying it belongs to this exact source
folder and has no cross-platform toolchain. It does not rebuild the editor,
change the cache's channel, configure Debug/cross builds, or touch another
checkout. Disable this option in troubleshooting if you manage the tools yourself.
The CLI equivalent is `--prepare-native`. Combining it with `--check` remains
read-only: it validates the cache without configuring or compiling anything.

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



## Canvas, focus and lifecycle

The shared shadow filter now compares each PCF texel with the receiver plane at
that texel's center. This corrects the repeating flat-face shadow pattern without
disabling shadows or increasing bias/resolution. The follow-up includes native,
Chromium and Firefox checks, plus ten minutes of active installed-Firefox play
and three close/reopen cycles. See [the correction and memory evidence](BROWSER_SUPPORT.md).
It remains bounded Experimental qualification, not general browser certification.

The shell offers Fullscreen only after Play, with a separate exit strip that does
not cover the game HUD. CSS owns the available rectangle;
SDL resizes the worker-owned WebGL surface. After queued SDL events are consumed,
the existing renderer receives the actual backing dimensions, rather than stale
CSS/logical resize events. Rendering is limited to device pixel ratio 2,
4096 pixels per axis and 2,097,152 total pixels. This is a presentation budget,
not a physics quality/cadence change. Fractional sizes round down. Relative mouse
look is normalized to CSS pixels so high-DPI rendering does not accelerate it.

Canvas focus owns gameplay input. Clicking a page control releases gameplay
input; held keyboard state is rebuilt through the existing SDL path on return.
Mouse capture still requires an explicit browser gesture and can be refused by
browser policy. Escape and browser controls retain their normal authority.

A hidden tab suspends the existing application loop and audio. A bounded lifecycle
wake handles stopped animation callbacks, and a UI-side visibility epoch retains
transitions even if the browser freezes the worker. These changes reset the clock,
avoiding a catch-up burst on return. Already-dispatched durable
save work retains its existing completion contract. Context loss still requires
reload. No mobile/browser-family or autoplay guarantee follows from these paths.


## Download delivery and repeat loads

New exports retain the original runtime/data files and add deterministic `.gz`
representations where compression saves at least 5%. `BROWSER_BUILD.json` records
both the SHA-256 inventory and optional `encodings` mapping. Older packages without
this mapping remain valid. Verification checks that each sidecar decompresses to
the exact original, with decompression bounded by its verified size.

The included HTTP/1.1 preview negotiates gzip with `Accept-Encoding`, preserves the original
MIME type (including `application/wasm`), and supplies `Content-Encoding: gzip` and
`Vary: Accept-Encoding`. It falls back to the originals for clients that decline
gzip. Representation-specific ETags and `Cache-Control: no-cache` allow the browser
to retain bytes and revalidate with a body-free 304 response on repeat visits.
This is cache validation, not guaranteed offline play or a service worker.
Package changes after startup fail closed; restart the preview after replacing a
package. No update installer or project mutation is involved.

On another static host, configure those same headers for compressed representations;
simply uploading `.gz` files does not make a host negotiate them automatically.
Keep the required isolation headers on cached responses too. The localhost preview
remains a development helper, not an internet-facing production server.

Loading uses an indeterminate indicator until the runtime and persistent-save
storage are ready. Compressed Content-Length and decoded data progress are not
presented as a misleading percentage. Errors stop the indicator and keep Play
disabled. Compression changes transferred bytes, not gameplay cadence, rendering
quality, Unicode coverage or the in-memory engine representation.


The preview explicitly uses HTTP/1.1: Chromium does not use ETag validation from
HTTP/1.0 responses in its HTTP cache. Sending ETag headers alone was insufficient;
real repeat-fetch tests caught this after manual conditional requests had passed.
See the [Chromium cache implementation](https://chromium.googlesource.com/chromium/src/net/+/master/http/http_cache_transaction.cc#3299).


## Touch and mobile browser limits

A bounded Android 16 / Chrome 133 emulator pass verifies rotation, real physical
paddle restoration after fresh-process save loading, genuine Home/lock suspension
without hidden-time catch-up, and nonzero native PCM plus Web Audio suspend/resume.
The emulator host audio driver did not initialize, so audible speaker output remains
unqualified. The original large emulator exceeded its contained memory budget;
a smaller read-only session passed. See [exact scope, failures and evidence](BROWSER_SUPPORT.md).
