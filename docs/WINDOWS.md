# Windows editor and exported games

## Windows 11 human acceptance — 25 October 2026

The operator human-accepted Windows portability checkpoint
`6941b3815f93d4f716fe64faffff69799c7637b0` on **actual Windows 11 hardware**.
The trial covered creating a project, saving and reopening/loading it, running a
small game, working audio, and functioning editor/gameplay on that machine.
This is genuine operator acceptance, separate from automated or VM results.

The supplied native receipts record **58/59 initially**, followed by an unchanged
passing build-identity retest: **effective 59/59**, plus **33/33 additional checks**.
The original failure remains preserved: the native identity test passed its 55
assertions, but its wrapper compared null and empty tracked-status values.
The corrected rotation executable retained its ordinary **1 MiB stack reserve**.
The clean Git build identity, SDK verification/inspection and logs are archived
with the [acceptance record](evidence/windows11-acceptance-20261025/OPERATOR_ACCEPTANCE.json). These receipts were inspected read-only
from the operator's mounted HDD; no checks were rerun during this closeout.

**Not claimed:** controller/gamepad hardware validation, clean-machine distribution
certification, every GPU/configuration or complete API coverage. The recorded ICU
build path-with-spaces workaround remains a limitation; see the original receipt.
Earlier automated files correctly retain their pre-human-test boundary; this
later operator statement supplies only the explicit hardware scope above.

The [seven-finding correction report](PREALPHA_WINDOWS_PORTABILITY_FOLLOWUP.md)
retains the original Linux/Windows 10 VM failures, fixes and regression evidence.

## Building Windows games from Linux

An optional [MinGW-w64 cross-build/export workflow](WINDOWS_CROSS_BUILD.md) now
builds native Windows x64 games on Linux. Native MSVC remains the editor/SDK
workflow below. These are separate toolchains, not interchangeable C++ libraries.

## Current Pre-Alpha SDK packaging

The checkpoint above has scoped native Windows 11 automation and operator
hardware acceptance. New packaging changes or later source revisions do not
automatically inherit that validation. The shared
[SDK inventory](../packaging/sdk-inventory.json) now includes asset cooking, script
checking, save recovery/migration, the dedicated server and all public authoring
tools. Runtime DLL staging covers executables declared after M76 as well.

### Qualification scope

| Recorded work | Actual scope | What it does not establish |
|---|---|---|
| [Windows 11 acceptance](evidence/windows11-acceptance-20261025/OPERATOR_ACCEPTANCE.json) | Effective 59/59 native gate, 33/33 extras and operator project/save/game/audio trial at `6941b381` | Controller hardware, clean-machine distribution, universal GPU/configuration coverage or later revisions |
| [Pass 4](evidence/prealpha-pass4/windows/REPORT.md) | Complete 18-executable SDK, native physics/persistence, editor and relocated export in Windows 10 VM | Physical GPU/audio/controller acceptance or every later revision |
| [Pass 5](evidence/prealpha-pass5/windows/REPORT.md) | 16 selected native build targets; engine, editor, runtime/export and M70/M77 cold restoration | A rebuilt all-tool SDK, hardware performance or a clean-machine install |
| [Later CCD/dense follow-ups](PREALPHA.md) | See each current report's Linux results | No fresh Windows qualification is inferred from shared source or older VM passes |

The Pass 4/5 reports preserve their four then-failing curved-mesh assertions.
Later Linux corrections do not rewrite those historical Windows results. Linux
is the current acceptance/performance platform; Windows VM timing is diagnostic
and nonblocking. Use [Pre-Alpha status](PREALPHA.md) for current finding dispositions.

`Build-Windows.ps1` defaults to `build\windows-Experimental`; use `-Channel Alpha`,
`Beta`, `Experimental`, `Stable` (GA only) or `Local` and an optional `-BuildDirectory` to keep independent
outputs. Channel selection does not grant human acceptance or promote a branch.
The script preserves Visual Studio 2022 as its default; use
`-Generator 'Visual Studio 18 2026'` with that installed toolchain and a compatible
CMake. Release CI detects the installed supported MSVC generation and supplies a
checksum-pinned native pkgconf, avoiding expired MSYS2 helper downloads in the
pinned vcpkg revision. These are build prerequisites, not changes to game input or rendering.
SDK staging requires Python 3.11+. Existing generated SDKs without the new manifest
are not overwritten: use a fresh build directory or move the old SDK aside.

Every new SDK contains `BUILD_INFO.json`, `SDK_MANIFEST.json` and `SHA256SUMS`.
Verify a moved SDK without Git or the engine source checkout:

```powershell
python "$sdk\tools\package_sdk.py" verify $sdk
python "$sdk\tools\inspect_sdk.py" $sdk --host-check --system-directory C:/Windows/System32 --output C:/Temp/judas-sdk-qualification.json
```

The inspector checks regular and delay imports in every tool and bundled DLL.
It reports missing dependencies instead of treating the DLL filename list as
proof of closure. System32/API-set resolution is not a minimum-OS, graphics or
application test. Write the qualification receipt outside the verified SDK.

The [Pre-Alpha pass-2 check](evidence/prealpha-pass2/packaging/REPORT.md) exercised
15 packaging controls but could not complete native compilation with its then-full
VM disk. That remains the historical result. Pass 4 supplies the newer scoped
native qualification below; physical hardware validation remains outstanding.

See [distribution and package verification](DISTRIBUTION.md). The historical
Windows receipts below describe their original checkpoints, not new validation.

## Pre-Alpha Pass 4 native qualification

The [Pass 4 receipt](evidence/prealpha-pass4/windows/REPORT.md) qualifies native
Windows 10 VM execution of engine checkpoint
`9deb47d1667e5b6bd1abe708ed343c3fbf6fe847`, with the later Python-only SDK fix
`df8929f73505d407697b5d35b115fc71d9100471` identified separately. MSVC 2022 x64
Release compiled the engine, extracted physics modules, editor and complete
**18-executable SDK** from a verified no-Git source archive. Existing compiler
warnings are retained; this is not a warning-free build claim.

- All 18 executable identities and actual selected author/import/cook/script
  operations were exercised. Native SDK relocation/export and missing-DLL/tamper
  negative controls pass.
- The documented **packaged** SDK inspector now passes without creating cache
  files. Its seven positive/negative follow-up cases preserve strict inventories;
  the original self-mutation failure is retained. No `-B` or environment-variable
  prerequisite is needed. Write receipts outside the SDK as shown above.
- Focused physics/origin/CharacterMotor/persistence checks pass **4,494 checks**.
  The ordinary M70 case passes **98**. Eight independent M70/M77 cold-save
  writer/reader processes pass **131**, checking actual restored physical
  Standing endpoints rather than accepting cached controller status.
- Ordinary editor authoring/undo/Play/Stop restores authored scene bytes exactly.
  Runtime save/load, rotated scene transition, pause/reload and the relocated
  Unicode/spaces standalone pass their gameplay harness. A deliberately wrong
  scene oracle fails as expected.
- The known curved-mesh continuation failure remains: the native mesh-cost
  invocation reports **372 checks / four failures**. It is not relabelled as a
  Windows pass or hidden by the passing entry/physics controls.

The original compile failed because a virtual JudasJS raw literal exceeded the
MSVC per-literal limit. Splitting one adjacent C++ literal preserves every
JavaScript byte. The separate inspector fix prevents Python sibling-import caches
from mutating a verified SDK; it does not relax hash or unexpected-file checks.
Both original failures and corrected follow-ups are preserved.

The operator-authorized VM disk expansion is **40→90 GiB**, retaining the original
system/recovery partitions and exposing the new tail as a **50 GiB P: work
volume**. Hash-backed generated-build relocation preserved older evidence and
toolchains. The persistent VM XML is unchanged. Native validation used existing
installed tools, not a freshly installed clean machine.

Stock QXL cannot provide OpenGL 3.3 and reports that limitation cleanly. Actual
editor/game checks use isolated llvmpipe software-rendered copies. Task-owned
SYSTEM-created staging permissions and missing harness fonts/example inputs were
corrected with original failures retained; engine fixtures and thresholds were
unchanged. **Physical GPU, audio, controller and hardware performance acceptance
remain outstanding.** VM timing is informational and does not gate Linux fixes.

## M76 local modding candidate

The provisional [M76 checkpoint](M76.md), **NOT HUMAN VALIDATED**, based on accepted M75 checkpoint
`38d345883a541d4eb2faa43c321e940d223fce09`, was compiled and exercised natively
in the existing Windows 10 VM. **Human M76 review and physical Windows validation
remain pending.** See [M76 evidence](evidence/m76/REPORT.md) for exact scope.

- Native package/policy/storage: 124 checks passed (three POSIX-specific link cases
  are Linux-only). Transferred source hashes and refreshed MSVC binaries were checked.
- Kit export, ordinary Create/Fork/Save/Cook/navigation bake/package worked with
  original game source unavailable. The actual editor viewport sculpt/paint,
  Undo/Redo/cancel, manifest publication and Play/Stop passed; scene bytes remained
  identical after Stop. Its package passed 19 real application checks.
- The **identical Linux-created A payload** ran in the matching native exported
  game with B and declared dependency packages: 20 real application checks passed.
  Fresh save/restore passed 9/11 checks; changed-loadout refusal passed 7 checks.
  Zero-use/empty selection, explicit trust refusal and removal were also exercised.
- The actual shipped executable ran from an unrelated working directory in a moved
  Unicode/spaces path, with all authoring sources hidden and base files read-only.
  Base hashes remained unchanged; saves/install/cache used ordinary writable user
  storage. The pristine SDK/export contained no Mesa DLLs.

The VM requires explicit `GALLIUM_DRIVER=llvmpipe` with app-local Mesa in disposable
test copies. This is software-rendered native execution, not GPU/audio/controller
hardware acceptance. Existing Windows short-root/long-path limits and inherited
warnings remain. A full production suite was not rerun for this focused content
integration; the existing Linux-only save-storage harness was not presented as a
Windows result.

The earlier Windows update retains its historical operator acceptance. On
**2026-10-09**, native Windows 10 VM verification passed for checkpoint
`df0c301f688e62e4baa59a837beb05d42aa75e3c`: accepted [M72](M72.md) plus optional
per-entity gravity selection. The Release SDK, focused checks, editor and moved
game packages worked under the qualifications below. This was native Windows,
not Wine; it does **not** certify hardware graphics/audio/controller support or
power-loss durability. Those checks and operator Windows acceptance remain
outstanding. Earlier evidence retains its original platform scope.

## Native Windows VM results — 2026-10-09

Environment: Windows 10 Home 22H2, build 19045, KVM/QXL, four cores and 6 GiB RAM.
The SDK built with Visual Studio 2022 Build Tools 17.14.41, MSVC 19.44.35229,
Windows SDK 22621 and CMake 3.31. MSVC/upstream warnings were retained; this is
not a zero-warning build claim. No Judas runtime/source changes were needed.

| Focused native check | Passed | Failed |
|---|---:|---:|
| Windows readiness: paths, save recovery, locks and process handling | 21 | 0 |
| CharacterMotor | 32 | 0 |
| Physical animation | 64 | 0 |
| Shared multi-target IK | 38 | 0 |
| Gravity selection | 70 | 0 |
| Actual-GL M72 render controls | 54 | 0 |
| Character lab, including ragdoll owner-contact delivery | 98 | 0 |
| **Total** | **377** | **0** |

Checks ran in the ordinary, non-administrator Windows desktop session. The full
production suite and every historical Linux harness were **not** run on Windows.

- Editor batch authoring/undo, duplicate/undo, Run Project, scene save and
  Play/Stop passed. The authored scene hash was identical before and after.
- Render Control Lab, Gravity Selection Lab and Spring Range launched normally.
  Their harnesses exercised render controls, save/load, gravity selection,
  shooting, camera switching, pause/resume, localization and reload.
- All three projects exported and ran after moving to paths containing spaces,
  `é` and `α`, from an unrelated working directory. Exported saves used
  `%LOCALAPPDATA%\judas\games\...`, including current/previous save generations.
- Re-export removed deliberately obsolete content. Removing a manifest-listed
  ICU DLL from a copied SDK caused export to fail without publishing a package.
- Virtual keyboard/mouse input into an ordinary Spring Range process opened the
  pause menu, resumed, switched cameras and registered a scored mouse-click hit.
  This does not substitute for physical input-device acceptance.

**Graphics qualification:** the stock SDK could not load OpenGL 3.3 Core entry
points on QXL. An app-local Mesa 26.2.4 llvmpipe fallback supplied software OpenGL
for the successful renderer/editor/game checks. Only the VM tooling copy, test
directory and moved VM packages received `opengl32.dll` and
`libgallium_wgl.dll`; the stock SDK and system OpenGL were unchanged. Ordinary
exports do not bundle Mesa. These results are not a hardware-GPU performance
certification, and an unmodified stock package still requires a suitable driver.

Setup failures and corrections were retained: the initial configuration lacked
the VS developer environment, retired MSYS mirrors required the documented
native pkgconf workaround, and ICU data generation failed until Python and its
`py -3` launcher were installed. Individual receipts, screenshots and the report
are retained locally under `.cache/windows-vm-transfer/reports/` (ignored, not
distributed with the repository). Temporary test-runner summary and missing-DLL
probe mistakes were corrected without changing Judas; their original receipts
remain alongside the corrected results.

## Build on your Windows install

Use Windows 10 (1903 or newer) / Windows 11, **x64**, with an OpenGL 3.3-capable
vendor graphics driver. Install Git for Windows and Visual Studio 2022 with
**Desktop development with C++**, the v143 toolset, Windows SDK, CMake (3.21+),
and Visual C++ redistributable components. Install **Python 3.11+ with the Windows
`py` launcher**, available as `py -3` in the build environment. ICU's upstream
Windows data-generation tools require it; Python 3.13.16 was used in the VM
verification. Python is a build prerequisite, not a packaged-game requirement.
Start **Developer PowerShell for VS 2022** and check the launcher:

```powershell
py -3 --version
```

Clone/pull this repository into a normal writable location. Avoid junction/symlink
save directories. The first build needs internet access for pinned SDL2/GLM via
vcpkg; the text/physics/script/import dependencies already reside in the repository.

From the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File .\packaging\windows\Build-Windows.ps1
```

The script creates a dedicated vcpkg checkout under
`%LOCALAPPDATA%\JudasBuildTools\vcpkg`, pinned to
`d5ec528843d29e3a52d745a64b469f810b2cedbf` (2025.02.14), and builds Release
using `x64-windows-static-md`. It refuses to alter an existing checkout at another
revision. An alternative dedicated path can be passed as `-VcpkgRoot`.
ExecutionPolicy Bypass applies to this invocation; it does not alter system policy.
The pinned baseline's MSYS helper downloads may return HTTP 404 as mirrors retire
old packages. In that case install native pkgconf and pass its executable explicitly;
this keeps the pinned SDL2/GLM versions unchanged. For a D-drive tooling checkout:

```powershell
winget install --id pkgconf.pkgconf --exact --source winget --location D:\BuildTools\pkgconf
powershell -ExecutionPolicy Bypass -File .\packaging\windows\Build-Windows.ps1 -VcpkgRoot D:\BuildTools\Judas\vcpkg -PkgConfig D:\BuildTools\pkgconf\pkg-config.exe
```

If a build fails, retain the first error and the build logs; do not treat the
candidate label as a guarantee that Windows compilation has already passed.

Output: `build\windows-Experimental\windows-sdk\` by default. It contains the
editor, runtime, exporter, public authoring/cooking/debug/save/server tools,
platform readiness test, fonts, current reference docs, DLLs and license notices.
The SDK prepares and verifies a replacement before publishing it. Keep authored
projects outside this generated directory.

```powershell
$sdk = (Resolve-Path .\build\windows-Experimental\windows-sdk).Path
$game = (Resolve-Path .\projects\shooter_game\shooter_game.judasproj).Path
& "$sdk\judas_editor.exe" $game
# Or enter standalone directly:
& "$sdk\judas.exe" $game
```

If the project file was renamed, select the actual `.judasproj` in that folder.
Use the SDK executables, not a copied lone executable. Your authored project still
has its normal assets/scripts/scenes; the SDK is engine tooling, not a game package.

## Export and move a game

Use the editor's existing project settings/export action, or:

```powershell
& "$sdk\judas_export.exe" $game "$env:USERPROFILE\Desktop\Judas Game"
# Copy the whole result to a different directory, then deliberately change cwd:
Copy-Item "$env:USERPROFILE\Desktop\Judas Game" "$env:USERPROFILE\Documents\Moved Judas Game" -Recurse
Set-Location $env:TEMP
& "$env:USERPROFILE\Documents\Moved Judas Game\judas.exe"
```

The package contains `judas.exe`, `game.judasproj`, the explicit package startup
record, selected project scenes/assets/scripts, engine fonts/notices, ICU DLLs
and the Visual C++ runtime DLLs listed in the SDK's generated `required-dlls.txt`.
SDL2 is linked statically on this build. The exporter requires all manifest-listed
DLLs plus SDL2/GLM notices and fails if any are missing. It does not bundle system
OpenGL/audio/Windows DLLs. Graphics drivers and Windows itself remain prerequisites.
Export runs the normal standalone entry point; no editor is shipped. Re-export
uses exclusive staging and the existing controlled package replacement policy.
Windows packages must be exported using a native Windows Release SDK; Linux
exports remain Linux. This is not cross-compilation or an installer.

## Persistence, paths and profiling

Executable location uses `GetModuleFileNameW`; game data is rooted at that location,
independent of working directory. The Windows process path uses Unicode arguments
and CRT quoting. Embedded manifests request UTF-8 narrow-path handling and long
paths; still test paths containing spaces/non-ASCII characters on your install.

Exported saves use `%LOCALAPPDATA%\judas\games\<authored-save-identity>\Saves`,
not the installation directory. Authored fingerprints and snapshot encoding stay
unchanged. The native save backend pins ancestor directories, rejects reparse
points, locks writers, flushes temporary file data, replaces with
`MoveFileExW(...WRITE_THROUGH)`, and keeps the previous valid generation.
That Windows publication barrier is not a claim of POSIX directory-fsync semantics
or proven power-loss durability. Native fault/crash/storage testing remains pending.
Existing editor/project save-location policies otherwise remain unchanged.

M56 profiling uses `GetProcessMemoryInfo` on Windows. Resident bytes mean working
set; the existing process-virtual counter means **private committed bytes** there,
versus Linux mapped virtual bytes. For compatibility, snapshot JSON retains the
legacy names `linux_resident_bytes` / `linux_virtual_bytes` even on Windows. Compare CPU/frame costs across platforms with
care; these memory fields are not directly equivalent.

## Native test checklist

The build script first runs `judas_windows_readiness_tests.exe`: save/backup recovery,
publication failure, locks, import replacement/stamps, executable/user-data roots,
export staging and native process argument/build-info handling. Linux passing this
fixture does **not** validate its Windows-only checks.

1. Build Release successfully; retain console output and any MSVC errors/warnings.
2. Open Spring Range in the editor; check icon, font/UI text, viewport and assets.
3. Play/Stop, move/look/shoot, pause/resume, switch cameras; check pointer capture.
4. Check audio, controller if available, save/load and scene reload/transition.
5. Use **Run Project** and verify it launches standalone separately from the editor.
6. Export, move the entire package, launch from an unrelated working directory.
7. Confirm UI/scripts/fonts/audio/physics work and saves go to LocalAppData.
8. Re-export and confirm obsolete package content disappears. Temporarily remove
   a DLL from a **copied** SDK and verify export fails, then restore it.
9. Open a project/path containing spaces and non-ASCII characters. Check import,
   save/load and exported startup there too.

The VM results above establish native compilation and bounded editor/runtime
verification, including screenshots. **Hardware validation remains outstanding:**
vendor-GPU graphics/performance, audible audio and device switching, physical
mouse confinement, controller hardware, real-machine suspend and fault/crash/
power-loss storage durability. Operator Windows visual/editor acceptance also
remains outstanding. Retain the unvalidated candidate label until the appropriate
remaining checks are accepted; VM screenshots and synthetic input do not supply
that acceptance.
The unvalidated build-script label describes a newly produced artifact; earlier
VM passes do not automatically validate that artifact. Historical Linux-only harnesses are not all ported; build the SDK target rather
than every historical test target. No installers, signing, MSIX or automatic
updates are supplied. Explorer executable-resource icons are not added in this
candidate; the existing Judas SDL window/fallback game icon remains embedded.

## Dependency provenance

SDL2 and GLM use the pinned [official vcpkg baseline](https://github.com/microsoft/vcpkg/commit/d5ec528843d29e3a52d745a64b469f810b2cedbf)
and [manifest workflow](https://learn.microsoft.com/en-us/vcpkg/consume/manifest-mode).
FreeType 2.13.3, HarfBuzz 10.4.0 and ICU 76.1 retain existing archive hashes and
notices. Windows builds ICU with its upstream Visual Studio solution, including
real Unicode data; Linux retains its existing static build. ICU DLLs are distributed
beside the executables. Visual C++ redistribution remains subject to Microsoft's
Visual Studio license. SDL2/GLM license files accompany the SDK and exported games;
other existing notices remain in `third_party/RUNTIME_NOTICES.txt`.
