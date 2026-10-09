# Windows editor and exported games

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
and Visual C++ redistributable components. Install **Python 3 with the Windows
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

Output: `build\windows\windows-sdk\`. It contains the editor, standalone runtime,
exporter, model-import and scene/world-authoring tools, a platform readiness test,
engine fonts, runtime DLLs and license notices. The SDK target rebuilds this
**generated** directory from scratch; keep authored projects elsewhere.

```powershell
$sdk = (Resolve-Path .\build\windows\windows-sdk).Path
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
Historical Linux-only harnesses are not all ported; build the SDK target rather
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
