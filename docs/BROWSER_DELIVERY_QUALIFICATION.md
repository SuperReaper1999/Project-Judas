# Experimental browser delivery and phone qualification

31 October 2026. Development follows `Experimental`; this is not a release or
main promotion. Browser export remains hidden unless explicitly enabled with
`-ExpBrowserSupport=1`. No engine C++, physics cadence, rendering quality, Unicode
data, game rules or native export platforms were changed.

## Changes

- Export optional deterministic gzip representations of large JS, WASM and game
  data, retaining the original files. The schema-1 receipt's optional `encodings`
  map identifies the representations; old receipts remain supported.
- Verify original and sidecar hashes, then stream-decompress each sidecar against
  its verified original's size/hash. Reject malformed, truncated, mismatched or
  oversized decoded representations before serving.
- Negotiate gzip/identity, keep the original MIME type, and send `Vary`, distinct
  representation ETags and cache-revalidation headers. Refuse changed package
  files instead of answering a stale cache hit. The preview is localhost-only.
- Use HTTP/1.1 with correctly framed responses. Actual browser testing found that
  ETag headers on the inherited HTTP/1.0 server were insufficient: Chromium's
  [cache implementation](https://chromium.googlesource.com/chromium/src/net/+/master/http/http_cache_transaction.cc#3299)
  only reads ETags for HTTP/1.1 responses. The same `game.data` experiment changed
  from repeated 200 bodies to 304 responses after this correction.
- Show an indeterminate loading indicator until runtime/storage readiness, hiding
  it on success or failure. Do not compare decoded download progress against a
  compressed Content-Length to present a false percentage.
- Make the canvas own touch gestures, preserving touch delivery while preventing
  page panning underneath a windowed game. Ordinary page scrolling outside the
  canvas remains available. No global `preventDefault` or global zoom/scroll ban.

Compression fixes the gzip timestamp and filename; byte-identical reproduction
assumes the same Python/zlib compressor version.

The update uses Python's standard library; it adds no runtime library dependency,
installer, service worker or automatic update mechanism. It does not reduce decoded
WASM size or runtime RAM. Keeping both originals and gzip sidecars increases the
package's on-disk size in exchange for lower transfer costs.

## Matched desktop measurements

Chromium 156.0.8078.4, fresh persistent profile for each cold/warm pair, three pairs
per variant in alternating order. Both variants contain identical original engine,
game, shell and helper bytes; the identity-only variant omits sidecars and their
receipt mapping. CDP shaping: 4 MiB/s download, 1 MiB/s upload, 40 ms latency.
A warm visit navigates away and returns using the same profile. Headless SwiftShader
is used: these are network/functional measurements, not hardware GPU benchmarks.

| Variant | Cold ready median (range), n=3 | Return ready median (range), n=3 |
| --- | --- | --- |
| Identity only | 15.386 s (15.371–15.575) | 0.565 s (0.553–0.567) |
| Gzip | 6.056 s (5.995–6.122) | 0.556 s (0.553–0.567) |

The measured WASM is 53,296,079 bytes; its gzip representation is 19,601,895 bytes.
Browser Resource Timing reports approximately 55.08 MB transferred for the cold
identity page versus 20.65 MB compressed. Warm visits report 3,900 resource-transfer
bytes and the actual HTTP logs show body-free 304 responses for WASM and game data.
Resource Timing sizes are browser-reported accounting, not a TCP packet capture.
Three samples do not establish a robust p95 or general connection-speed guarantee.

Firefox 157.0 also passes real preload, correct representation validators, normal
return-visit 304s, physical character movement and camera switching. Its single
unshaped relocated-package sample reports 721 ms cold and 548 ms warm; that is
functional evidence and is not comparable to Chromium's shaped network timings.
Firefox's Resource Timing still reported encoded body sizes after 304 responses;
wire logs and ETags establish cache reuse independently.

These measurements use the fingerprinted package before the subsequent canvas-only
touch change. The final touch follow-up is independently built and exercised below;
no new timing claim is inferred for that package.

## Correctness and integration

- **46 Python tests pass:** existing export/preflight/relocation boundaries plus
  deterministic compression, malformed metadata, wrong decoded content, bounded
  decompression, truncation, real HTTP negotiation/406, MIME types, variant-specific
  validators, HEAD/304/error connection framing, and mutation rejection on cache hits.
- **8 actual-shell callback checks** run within that suite. Readiness cannot overwrite
  an error, late progress cannot overwrite readiness, and loading does not show a
  misleading compressed/decoded ratio. The existing 25 frame-backpressure checks run
  unchanged within the suite.
- **Six matched Chromium samples pass:** actual WASM becomes ready, cached payload
  bodies are not retransmitted, keyboard moves the supported MK2 player and the
  camera toggle reaches gameplay. Firefox passes the corresponding single pair.
- A complete package copied outside the repository verifies in a clean working
  directory/PYTHONPATH and runs in Firefox. No SDK or source checkout is needed by
  its included preview helper.
- **29 existing player checks pass on the final touch-follow-up package:** mouse
  capture and DPI, drawable resize/pixel markers, UI focus and clicking, fullscreen,
  and a real fresh-browser-process save/load. Saved and restored observed position
  is `(1.5975871086120605, 0.917275071144104, 6.829291820526123)` in this fixture;
  UI progress is retained too. This observes gameplay transforms, not cached status.
- A real Chromium touch test against actual shell CSS receives 12 canvas touch
  moves with zero page-scroll delta; scrolling outside moves the page 463 px.
  The original shell fails the unchanged gate, scrolling 489 px during a game drag.
- Matching native Release runtime/exporter and WASM identities are checked by the
  exporter. Both Browser Player Lab and ordinary Breakwater export successfully.

Browser automation uses a 2300 MiB cgroup limit with zero swap. This is containment,
not a newly claimed memory-endurance benchmark; the prior endurance evidence remains
separate. No host-wide OOM is needed or used as a test method.

## Genuine phone observations

Actual **SM-S948B, Android 17/API 37, Chrome 156.0.8078.75**, over temporary USB
localhost forwarding. No APK, phone settings change or separate gameplay code.

The operator confirms the 3D player runs/renders, then confirms Breakwater plays
with touch. He reports page scrolling while dragging outside fullscreen. After the
canvas fix, he is asked to check windowed paddle dragging and ordinary page scrolling
outside the game and replies **“perfect”**. An exit through the game menu is explicitly
operator initiated and is not classified as a crash.

Independent synthetic touch events on the actual phone receive all 12 canvas moves
with **0 px** page-scroll delta. Scrolling outside remains functional (−30.22 px in
that viewport). The test records CSS width 384, height 721 and `touch-action: none`.
These synthetic gestures are not claimed as physical digitizer/controller tests;
the operator's own play is separately recorded in `HUMAN_PHONE_OBSERVATIONS.json`.

The first phone automation waited only for pre-launch `Play`, although the operator
had already pressed it. Its retained timeout contains a supported player and more
than 250 seconds of advancing gameplay, with no observed page exceptions. A later
debugging timeout after the user's exit was resolved by reopening Chrome. Neither is
reported as an engine startup failure.

## Retained unsuccessful experiments

Evidence retains the first incognito/input run, Reload measurements, fixed-delay
camera-observation failure, HTTP/1.0 request trace, Firefox Resource Timing and cached
header assumptions, the original touch-scroll negative control, and the phone
startup-state timeout. The final driver uses fresh ordinary profiles, observes real
gameplay transitions, records wire responses and compares representation ETags.
Cache reuse is not inferred from headers alone. A stale-source export refusal was
resolved by refreshing matching native tools; provenance gates were not bypassed.

## Boundaries and next checks

- Still **Experimental**. Static internet hosts/iframes must supply correct gzip,
  MIME, cache and isolation headers; no production host was certified here.
- No service-worker/offline-first promise. Clearing browser data removes browser
  saves, which remain scoped to origin/profile. Changing a preview port changes
  the origin; native APK saves are not shared with a browser.
- The 3D player lab is keyboard/mouse based. A project must author suitable touch
  controls; there is no automatic virtual joystick. Breakwater already has them.
- Mobile browser audio, physical gamepads, Safari/iOS, Samsung Internet, other Android
  devices, online transports and every renderer feature are not certified by this pass.
- No Windows/Android native build requalification or public Alpha support/channel
  changes. Advanced Fluids remain locked; independent research is untouched.

Human checklist: try a cold load and a return visit; play the 3D lab on desktop;
verify windowed/fullscreen touch dragging in Breakwater; save/reopen/load; exit and
restart deliberately. Main promotion/publication require separate authorization.

## Changed files

- `tools/browser_package.py`: sidecars and strict representation verification.
- `tools/serve_browser.py`: HTTP/1.1, encoding/cache negotiation and immutable-file checks.
- `tools/build_browser.py`: packaging/inventory and exported hosting instructions.
- `platform/browser/shell.html`: honest loading state and canvas touch ownership.
- `tests/test_browser_delivery.py`, `tests/browser/loading.cjs`,
  `tests/browser/delivery.cjs`, `tests/browser/touch_scroll.cjs`: regression and real-browser drivers.
- `docs/BROWSER_SUPPORT.md`, this report and its evidence directory.

## Artifacts and reproduction

Local playable packages: `.cache/browser-delivery/ready-player-web` and
`.cache/browser-delivery/breakwater-touch-web`. They retain parent/dirty-source build
identity, not a fabricated clean post-commit identity. Exact receipts, original gates,
source snapshots, raw results, failures, source copies and hashes are preserved in
[`evidence/browser-delivery-20261031`](evidence/browser-delivery-20261031).
The benchmark package and identity comparison remain under `.cache/browser-delivery`.
Raw log files are losslessly gzip-compressed; `RAW_LOG_HASHES.json` records the exact
decompressed hashes and sizes. No whitespace or failure output was stripped.

```sh
python3 -m unittest discover -s tests -p 'test_browser*.py' -v
PLAYWRIGHT_MODULE=/absolute/node_modules/playwright \
  node tests/browser/delivery.cjs COMPRESSED_PACKAGE IDENTITY_PACKAGE FRESH_RESULTS chromium
# Repeat with firefox. Use a fresh output path; keep localhost socket access enabled.
PLAYWRIGHT_MODULE=/absolute/node_modules/playwright node tests/browser/touch_scroll.cjs
python3 /absolute/web-package/serve.py --verify-only
python3 /absolute/web-package/serve.py --port 8080
```

Use the documented memory containment for browser runs on this 8 GB host. Keep SDK
compilation and CPU-intensive browser qualification sequential. Failed logs are
retained; disposable isolated browser profiles can be regenerated from these commands.
