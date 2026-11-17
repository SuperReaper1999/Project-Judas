# Verified desktop testing route

The operator clarified that real Kubuntu desktop testing is mandatory. A failed
restricted display query is not evidence that desktop testing is unavailable.
No display, KDE, NVIDIA, permission or system configuration was changed.

## Existing supported workflow

- `docs/TEST_HARNESS.md` documents the `JUDAS_TEST_SCRIPT` application driver.
  `src/TestHarness.cpp` implements it; `src/Application.cpp` invokes it through
  the normal host, simulation, rendering and application-service boundaries.
- This driver is built into `judas`, not a separate shell launch script.
  Existing launch syntax is `JUDAS_TEST_SCRIPT=SCRIPT build/judas PROJECT`.
- `docs/evidence/terrain-v2/stage1/desktop-script.txt` is a retained example
  using `REALTIME`, logical actions, look and screenshots.
- Editor automation uses `JUDAS_EDITOR_AUTOTEST` through
  `src/editor/EditorAutomation*.cpp`. `scripts/qualify_sdk.py` orchestrates
  create/author/Play/Stop/export/reopen and relocated exports; native Linux
  uses the desktop directly unless an explicit baseline rootfs is supplied.
- `docs/WHOLE_ENGINE_PERFORMANCE.md` documents serial real-desktop captures.
  `tools/benchmarks/run_whole_engine.py` wraps the ordinary Application loop;
  `observe_editor.py` observes editor Edit/Play via the existing SDL interposer.
- CI's Xvfb/Mesa route is a separate supported environment, not a replacement
  for testing the actual desktop here.

## Current environment verification

The restricted shell cannot reach the display/local sockets. The execution
tool's reviewed `require_escalated` route successfully runs the existing harness
on the unchanged desktop. `glxinfo -B` reports direct rendering, NVIDIA GeForce
GTX 970, GL 4.6, driver 580.178.04; nvidia-smi reports 4096 MiB.
This is observed behavior, not an inference from sandbox configuration.

## Small non-destructive graphical probe

Existing binary: `build/judas`; exact SHA256 and its clean Alpha identity at
ca0d437 are in `evidence/existing-executable-identities.json`. This is a harness
availability check, not qualification of current dirty source.

Disposable copy: `.cache/platform-audit-20261116/desktop-probe/project`.
Script: `.cache/platform-audit-20261116/desktop-probe/smoke.test`.
Save routing uses that probe's `user-data`; sound uses SDL's dummy backend.
Eight deterministic frames, actual OpenGL screenshots at frames 2 and 5,
viewport assertions at 800x450, 450x800 and back to 800x450. Exit 0.
Landscape image was visually inspected: populated Beacon Grove scene, player,
lantern cells and HUD. This is synthetic input and real rendering, not human
acceptance, physical-device testing or FPS evidence.

`evidence/desktop-harness-probe-corrected.log` retains the successful run. The
first command used the wrong case for `Beacon_Grove.judasproj`; its explicit
file-open failure is retained in `desktop-harness-probe.log`. Correcting the
filename was the only retry change.

Screenshots remain in the audit-owned ignored probe directory. Current-source
tests and full SDK/editor workflows follow the isolated audit build.


Source-hidden reconciliation verified the same desktop with the rebuilt production
SDK. In the read-only-host outer namespace, create a caller-owned writable temp
folder and set `TMPDIR` to it (for example `/tmp/judas-platform-audit-20261116/runtime-temp`).
The terrain runtime uses standard temporary storage for page preparation; leaving
`/tmp` read-only correctly fails with an explicit filesystem error during world
instantiation. Setting the process's temp folder resolves that exact failed step;
no Linux/KDE/NVIDIA/display permissions are changed. Flat and planet author/cook/
export/relocate/write/cold-settle/traverse/read then pass against the source-hidden
production SDK with normal-detail dependencies and restored state.

When refreshing a disposable relocated SDK, an overlay copy can leave files that
were retired from the new inventory. The strict checksum verifier caught an old
prototype document in this audit copy. Compare complete inventories before use;
never interpret a successful overlay copy as a verified SDK. The document's source
was preserved on its experiment branch before removing that one stale disposable
copy. The packaged SDK itself was valid and needed no implementation repair.
