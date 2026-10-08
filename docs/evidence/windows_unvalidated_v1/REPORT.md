# Windows preparation — unvalidated, not final v1

Starting source: `eadc7ca3f6ea521285bcb3d86161feaec7e13c35`.
Prepared 2026-10-08 on Linux. Native Windows results: **NOT RUN**.
This checkpoint authorizes operator testing, not a Windows support/release claim.

## Scope

One shared runtime/editor: native Visual Studio 2022 x64 build, pinned vcpkg
SDL2/GLM static-md manifest, unchanged pinned FreeType/HarfBuzz archives and
ICU 76.1 upstream Visual Studio Unicode/data build. SDK contains tools, fonts,
notices, ICU and VC runtime DLLs. Native export requires an explicit generated
DLL list and license files, uses exclusive staging, and enters normal runtime.
Executable-root lookup, user-data directory, quoted process launch/build-info
capture, import stamps/replacement and bounded save-storage operations now have
Windows implementations. Process-memory sampling uses Windows working set/private
commit counters; its legacy JSON field names are documented.

Snapshot encoding, authored identity, physics, game behaviour, public JudasJS API
and existing project assets remain unchanged. Linux platform services preserve
its original implementations. No controller, installer, signing or cross-build
framework was added. Native compiler/linker, ICU data generation, graphics/audio,
Unicode paths and storage durability remain operator-validation requirements.

## Focused Linux evidence

- Release runtime/editor/exporter and platform fixture build passed; final build
  log has no compiler warnings/errors. Earlier configure retains the existing
  HarfBuzz CMake deprecation notice. This was an affected-target build, not a
  clean full-engine/production-suite campaign.
- Platform fixture: **14 checks, 0 failures**. Its four additional Windows-only
  checks (save lock, native build-info and process/argument delivery) are unrun.
- Existing save storage: **28 checks, 0 failures**.
- Existing export: **18 checks, 0 failures** (transitive assets, stable paths,
  startup, identity, stale replacement and missing-asset rejection).
- Existing model reimport: **37 checks, 0 failures** (race/identity/publication).
- Spring Range standalone startup: 12 deterministic harness frames, exit 0.
- Editor Play/Stop: exit 0; authored scene after Play/Stop reported IDENTICAL.
- Spring Range export: 35 assets, 1 scene, 57,486,515 bytes, 0.121 seconds.
- Copied full package to `/tmp/judas-windows-port-moved-game`, launched executable
  from `/tmp`: 12 harness frames, exit 0.

Graphics smoke used SDL offscreen + software GL; **no human visual/audio/input
acceptance is claimed**. Offscreen SDL reports no relative-mouse implementation;
Linux audio probing reports unavailable device/JACK diagnostics. Those limitations
are retained in logs. No production/research suite was rerun.

## Review corrections and remaining acceptance

Before checkpointing, review corrected leftover POSIX export staging, SDK treatment
of a world-authoring library as an executable, exact manifest-listed DLL/license
copying, stdout draining across process exit, save temporary-file ownership, and
Windows case-insensitive package reservations. These were preparation review
corrections; they are not fabricated native-Windows test failures.

Native build and acceptance instructions: [WINDOWS.md](../../WINDOWS.md).
The SDK build script runs the focused native fixture after a successful Release
build. If MSVC or packaging fails on the operator's machine, retain the actual
logs for correction. Explorer executable-resource icons and porting every
historical Linux-specific harness are outside this candidate; window icons remain
embedded. No unsupported Windows result is labelled passed.

## Preservation

All existing projects, protected FTFT/P1 evidence/prototypes and historical
milestone records are untouched. No unrelated game/physics/fluid/API work is
included. ROADMAP remains absent. No milestone number or tag is invented.

`final-fingerprints.json` records changed implementation/tooling/document files;
this report and test logs are evidence, not runtime content. Generated SDKs,
executables, build trees and export packages are excluded from the commit.
