# Browser export usability correction

Operator feedback: the previous panel was too complicated. This is an
Experimental correction, not human acceptance or a release/main promotion.
Parent: `9049c3a0c206b9395a15f65006401d7dea4c414e`.

The browser opt-in now opens a focused **Export your game for the web** window.
Its normal path is an optional folder change, **Build & play**, then Play on the
browser page. Setup is checked automatically; tool paths, ports, detailed results
and logs are collapsed under troubleshooting. Native project settings/export stay
separate. The window can be reopened from File. Default-off gating is unchanged.

Each build chooses a fresh sibling folder, preserving earlier packages. A trailing
slash cannot nest the new build inside an older package. Completed builds start
an owned preview automatically when requested. Rebuilds reuse the preview address
and ask the user to refresh the existing tab instead of opening a competing save
owner. Export without playing remains available. Closing the browser tab stops
the game; Stop local preview/editor close stops the editor-owned hosting process.

Matching game tools are refreshed as part of Build & play. `--prepare-native`
validates an existing Release cache for this exact source and rejects another
checkout, Debug, toolchain/cross-platform targets and unexpected tool paths. It
builds only judas/judas_export, preserves channel configuration, uses an owned
lock and never rebuilds/replaces the editor. `--prepare-native --check` stays
read-only. No software/SDK is installed automatically.

## Results

- Browser tooling: **26 tests passed**, including read-only preparation checks,
  wrong-source/Debug/cross-target rejection and cleanup on preparation failure.
- Editor widgets: **36 checks passed**. Dedicated window, automatic readiness,
  one Build & play action, existing settings/history and Play-mode rejection.
  The UI-only readiness fixture is explicitly synthetic and needs no SDK in CI.
- Process/folder workflow: **21 checks passed**, including prior cancellation,
  independence and bounds plus fresh/repeated/trailing-slash output preservation.
- Gate registration: **9 tests, 7 passed / 2 skipped** because Ninja multi-config
  is unavailable. No historical numerical regression expectations were changed.
- Real installed-tool integration passed through the same controller/arguments:
  read-only check, automatic native preparation, actual compiler cancellation,
  lock cleanup, successful retry/build, verified preview HTTP/headers, shutdown.
  Receipt verifies all **21 package files**. No browser was auto-opened by the
  automation: its explicit `playAfterBuild=false` prevents disturbing user tabs.
- Real rendered editor UI was inspected, and its copied scene remained
  **IDENTICAL** across the native Play/Stop/save smoke. Human usability acceptance
  of this revised window is pending.

The early widget test caught action emission during programmatic activation in
Play; an explicit Edit guard fixes it, with the application guard also retained.
The first integration attempt correctly refused mixed source after a concurrent
UI correction; the frozen-source rerun passed. These logs are preserved.
Final window startup/layout and additional native cross-target validation were
checked separately after the first successful integration. The final integration
was then repeated with the corrected product source and passed.

Artifacts: `.cache/browser-simple/beacon-web-final`; explicit native job command
is the existing `judas_browser_export_smoke` documented in the browser guide.
The receipt retains its real parent-plus-dirty identity, never a relabelled later
commit. SOURCE_SHA256 pins current tested files; large binaries/caches/profiles
are not committed. Independent fluid research and main are untouched.

Linux source-host browser export remains Experimental. The SDK must already be
installed; arbitrary imported native build configurations are not automatically
rewritten. Same save-origin and offline/hosting constraints remain documented in
[BROWSER_SUPPORT.md](../../BROWSER_SUPPORT.md).
