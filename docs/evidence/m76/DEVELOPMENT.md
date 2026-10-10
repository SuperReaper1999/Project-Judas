# M76 development record

Chronological development observations. Early pending statements describe their
observation time, not current results. See REPORT.md for the final candidate.

- Initial package/storage test executable: 64 focused checks passed on Linux.
- Integration compilation caught two incorrect source names (`LoadNavigationAsset`
  instead of `LoadNavigation`, and `SceneObject.character` instead of
  `characterMotor`). Corrected against current headers.
- Editing a function signature while an incremental build had already compiled
  its engine object produced an undefined `ParseManifest(..., bool)` at tool link.
  That binary was not accepted as candidate evidence. Subsequent builds must use
  the settled source. Full local logs are retained in `.cache/m76/development/`.
- Source/type API drift check passed with the new detached `mods` catalog; live
  VM enumeration and actual mod gameplay are still outstanding.

HISTORICAL — at that initial development checkpoint, final fixtures, platform
proof, performance and human handoff were outstanding. See REPORT.md for results.

## Integration failures and corrected follow-ups

- Kit authoring initially chose a same-filename published base navigation asset
  while baking owned geometry. Author catalog now namespaces read-only base
  records under `Base/`; owned records retain normal paths. No opaque IDs or
  geometry fingerprints were rewritten. Independent equal filenames/local IDs
  and the actual dependency-module execution now pass.
- Initial dependency fixture used an incomplete scene text header. Rebuilt it
  from the ordinary serializer-compatible prefab; runtime proof exercises its
  published helper and saved value 7.
- Initial hill walking assertion sampled the character before it had settled
  after traversing the steep center. The bounded actual input route now checks
  supported collision height, and the independently spawned body rests there.
- Fresh save preparation remained pending because the example map contained a
  spatial audio-emitter prefab but no audio listener. Adding an ordinary authored
  listener fixed both missing sound and preparation. No save/audio-engine
  semantics changed. The bounded failed save log is retained separately.
- First editor hook invocation supplied the wrong terrain source filename. Real
  viewport editing worked, but the reopen assertion failed. Correct source path
  passes sculpt/paint/Undo/Redo/cancel/focus-loss/Save/Cook/bake/Play/Stop and
  shared publication; its editor-produced package passes real game execution.
- A negative navigation test flipped a valid trailing numeric field, which did
  not necessarily corrupt the asset. Corrected the test to damage its magic;
  a separate legitimate terrain recook without nav rebake proves stale rejection.
- First native Windows batch compiled M76 runtime/tools but then hit an existing
  Linux-only `sys/file.h` include in SaveStorageTests. The old harness was not
  changed. Linux's 28 save-storage checks stand separately; Windows uses actual
  M76 application save/restore checks. Final affected native targets build.
- Linux GCC emits an inlining-related dangling-pointer warning around the focused
  test initializer-list helper; native builds also retain inherited platform
  warnings. No broad zero-warning claim is made.

Original local logs/copies are retained under `.cache/m76/development/`,
`.cache/m76/results/` and `.cache/windows-vm-transfer/reports/m76-*`.
Failures authorize only affected follow-ups; no full production/research rerun.
- Native transfer hash checks exposed a VM timestamp-skew trap: ZIP-preserved
  source mtimes were older than previously compiled guest objects. An apparently
  successful incremental build still ran the old 80-check test binary. That
  result is NOT final-source evidence. Four changed C++ sources were explicitly
  timestamp-refreshed and recompiled; final native evidence records new binaries.
- A loose Windows build directory lacked the engine UI font supplied by the
  SDK, so its first export proof refused publication. Existing SDK font/notices
  were staged beside the loose test tools. This was validation setup, not an
  exporter permission or mod content change.
- Windows harness setup needed ordinary-user ACLs on individual files as well as
  directories; an initial inheritance flag combination left files unreadable.
  Unicode-safe .NET/SID ACL publication was confined to the disposable moved test
  package. No host/global permission or policy changed.
- Native console stderr is not an exit failure. The PowerShell wrapper initially
  treated ordinary renderer diagnostics (and the expected trust denial) as
  terminating exceptions. Process exit codes and separate capture files are the
  authority for the corrected checks.
- The VM's Mesa DLLs require explicit `GALLIUM_DRIVER=llvmpipe`, as in the existing
  M75 VM setup. The unspecified backend exited with `0x80070057` before readiness;
  the identical binary/payload with explicit software backend passes 19 actual
  application checks. No runtime implementation was changed for this VM setup.
- Final kit review found that explicitly published font/content bytes also need
  their license/notice companions. The lab now declares `NOTICE.txt` and
  `Assets/fonts/LICENSE.txt` in its curated source inputs. No automatic private
  source discovery was added. The affected contract/kit/export and fresh-save
  checks were refreshed; A/B payload bytes and runtime implementation are unchanged.
- The native editor-generated terrain made a thrown prop roll lower on the slope,
  exposing a test assumption that its final center must stay above world height 2.
  That is not a collision invariant. The corrected actual-application assertion
  observes non-sensor authoritative body/terrain contacts across the throw interval;
  motor height and navigation still have independent edited-geometry expectations.
  No physics, impulse or terrain quality/cadence was altered.
