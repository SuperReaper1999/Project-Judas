# Experimental browser export workflow — 31 October 2026

Parent: `567f03e72ec052e0659d131f6186b5c5f0c8b090`.
Main/published Alpha remain at the accepted `49c4f3d9` lineage. No publication,
release/channel changes, new platform certification or human acceptance.

## Implemented

- Opt-in File menu entry opens Project Settings → Export. Browser output,
  prerequisite check, background build, bounded logs, cancellation, preview port,
  Open in browser and Stop live in the existing editor UI. Advanced toolchain
  paths are collapsed. Default-off UI and native export remain independent.
- Browser build uses the existing unsaved-scene/project shipping confirmation,
  terrain/sprite draft guards and Edit-mode boundary. Checks read saved files.
- Editor-owned Linux process groups use literal argv, retain logs, reap children
  and cancel compiler descendants without adopting or killing unrelated jobs.
  Builds have time/output bounds; preview ends on Stop/editor close. Tool fields
  are session choices and no toolchain is installed automatically.
- CLI `--check` emits a read-only JSON prerequisite report, collecting missing
  toolchain/tools, incompatible project policy, unsafe output/cache overlap and
  native-tool identity mismatches. CMake still owns exact source verification.
- Compiler temporaries use the selected build filesystem, avoiding the small
  RAM-backed /tmp on this 8 GiB Linux host. Interrupted builds retain diagnostic
  staging. Graceful cancellation runs the existing cache-lock cleanup.
- New packages include their own Python preview launcher/verifier. It validates
  the complete SHA-256 inventory before serving, rejects modified/missing/extra
  files, symlinks and unsafe paths, and serves only recorded paths over loopback
  with COOP/COEP/CORP. It does not create bytecode caches inside the package.
  This is corruption detection, not a signed publisher-authenticity guarantee.

## Verification

- Python browser tooling: **23/23**, including real relocated-helper HTTP/headers,
  corruption, missing/extra files, parent/absolute paths, symlinks and preflight
  negative controls. Discovery now includes both browser test modules.
- Native editor settings/widgets: **34/34**. Default-off hiding, explicit opt-in,
  separate browser-build action, Play rejection and previous settings/history
  behaviour are exercised.
- Native process workflow: **17/17**. Literal metacharacters, missing executable,
  no log overwrite, descendant cancellation, independent-process preservation,
  timeout, output bound and bounded display tail.
- Explicit real integration through the editor's actual job runner: prerequisite
  check → cancel a real compiler after lock acquisition → lock removed/no output
  → build successfully → verified localhost preview/header fetch → Stop closes
  the server. It did not remove a stale lock manually to manufacture a pass.
- Relocated package, **Chromium 156.0.8078.4**: complete Beacon gameplay, real
  AudioSystem output, pause, two-cell cold save, exact saved bytes after browser
  process restart, actual restored endpoint, negative beacon controls, physical
  gate removal, victory, completed save/load, defeat/retry and Quit teardown.
- Relocated package, **Firefox 157.0**: actual one-way underside/jump/landing,
  blocked/free grid cells, scene changes, durable save and actual pose/load
  completion. Both use the package's own launcher from a non-source working
  directory. Each final package's **21 file hashes** still match after use.
- Actual editor application, copied project and isolated preferences: duplicate /
  undo, rendered Edit/Play, Stop and Save; authored scene **IDENTICAL** afterward.
  This smoke is separate from the widget-level browser-panel checks.
- Native folder browser **15/15**, explicit browser option parser **28/28**,
  build organization **18/18**, gate-registration **9 tests: 7 passed, 2 skipped**
  (Ninja/multi-config unavailable). Registration manifest validates. The removed
  workflow-discovery negative control proves the new Python coverage is required.

## Failures preserved and classified

`settings-ui.log`: early programmatic activation could emit a browser-build
request during Play despite ImGui's inherited disabled state. An explicit Edit
predicate now prevents request emission; the actual application's Edit guard
also remains. Final widget checks pass, without deleting the assertion.

`workflow-tests.log`: the restricted sandbox refused loopback binding, not a
package/HTTP defect. The same test ran with loopback permitted and passed.

`registration-final.log`: the old registration test required the literal single
file pattern, while the gate now discovers both browser modules. The corrected
contract checks that the pattern includes both files and adds a negative control
which removes workflow discovery. Original known-failure/channel/manual checks
remain intact. A mistaken discovery filename produced a local zero-test run;
that is not counted as validation.

## Provenance and reproduction

SOURCE_SHA256.json pins tested implementation, UI, tools and fixtures. Package
receipts retain their actual parent-plus-dirty source identity and native/WASM
provenance; no binary was relabelled with a later checkpoint. The Python qualification tests were finalized after package compilation: gate
discovery was strengthened, Linux compiler contracts explicitly mock their host,
and preview readiness uses a portable reader thread instead of POSIX pipe select.
They were rerun; these test-only changes do not alter compiled product/package code. All tested product hashes match this checkpoint.
The raw integration log is losslessly compressed to preserve CMake whitespace
without introducing whitespace-only diff warnings. Its decompressed SHA-256 is
`1ed31decb85e5af12aa083a889fe18510877bb9a29b26bf216817b8b745f8358`. Logs/receipts and their SHA256SUMS are retained; large SDKs, binaries, profiles and
unrelated research are not committed.

[Workflow and CLI instructions](../../BROWSER_SUPPORT.md#editor-export-and-local-preview)
include editor launch, fresh output, fixed save-origin ports and relocated hosting.
[Integration command](../../BROWSER_OFFLINE_QUALIFICATION.md#export-workflow-integration)
uses `judas_browser_export_smoke` and the existing installed SDK. No automatic SDK
download is required by the regression gates.

Local artifacts: `.cache/browser-stage3/beacon-web`, `traversal-web`, and copies
under `.cache/browser-stage3/relocated/`. Build with existing matching native
Release tools and the pinned Emscripten 6.0.13. Run Python tooling via:

```sh
python3 -m unittest discover -s tests -p 'test_browser*.py'
python3 -m unittest discover -s tests -p test_validation_registration.py
build/judas_browser_workflow_tests
build/judas_project_settings_editor_tests
```

## Limits and human checklist

Linux source-host workflow only. Windows/Android browser hosts, Safari, mobile
input, AudioWorklet/background-tab policy, networking, responsive canvas, download
compression and general 3D qualification remain outside this stage. The large
preloaded WASM/runtime cost is unchanged. This is not an automatic public uploader
or a browser-hosted editor. Keep packages immutable while served.

Source provenance still covers the whole checkout: changing tracked demos/docs
inside it can require refreshing matching native/WASM metadata. Normal consumer
projects outside the engine checkout avoid that development-tree coupling.
Interrupted copy/publication may leave an incomplete fresh output directory;
verification rejects it. Do not treat a failed build or receipt's mere presence
as a completed package. A forcibly killed editor can leave a cache lock; inspect
its recorded PID before removing it. Graceful cancellation is tested.

Human review pending:

1. Launch editor with `-ExpBrowserSupport=1`, open a disposable game, use File →
   Export browser game; inspect layout, advanced options and prerequisite errors.
2. Choose a fresh destination; test the ordinary unsaved-content choice, build
   and cancellation. Check the retained log and retry into a fresh folder.
3. Start preview, Open game in browser, use K/L saves; Stop and restart on the
   same port and confirm progress. Try an occupied port and select another.
4. Copy the entire export elsewhere, run its `serve.py --verify-only`, then serve
   and play without a source-directory working path.
5. Confirm native export and ordinary editor shortcuts remain comfortable.
