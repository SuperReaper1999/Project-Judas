# Windows builds and SDK use

<!-- judas-release-status:start -->
Source identity is **Judas Engine Alpha v2.0.0** (`2.0.0-alpha.1`).
[Source notes](ALPHA_V2_0_0_NOTES.md) · [Published packages](https://github.com/SuperReaper1999/Project-Judas/releases) ·
[Support boundaries](ALPHA_SUPPORT_MATRIX.md).

Source identity and publication are separate. The
[publication record](RELEASE_STATUS.json) is a dated receipt.
Check the release page for subsequently published packages.
<!-- judas-release-status:end -->Current native Windows SDK build/tool/contracts and loader inspection passed at
the [development baseline](DEVELOPMENT_READINESS.md). This is separate from a new
Windows editor/GL/export, hardware audio/controller or clean-machine trial.
Older operator acceptance does not certify this changed snapshot.

## Build natively

Install Git, Python 3.11+, CMake and Visual Studio's Desktop development with C++.
Use Developer PowerShell. The build script accepts Visual Studio 2022/2026 or
Ninja, pins its dedicated vcpkg checkout, and rejects an unexpected existing
checkout rather than modifying it. Keep toolchain/cache paths caller-owned.

```powershell
./packaging/windows/Build-Windows.ps1 -Channel Experimental -Jobs 2
# With a prepared VS 2026 toolchain:
./packaging/windows/Build-Windows.ps1 -Channel Experimental -Generator 'Visual Studio 18 2026' -Jobs 2
```

The result is `build/windows-Experimental/windows-sdk`. `-ReleasePackage` requires
clean Release provenance, not merely successful compilation. `-BuildDirectory`,
`-VcpkgRoot`, `-ToolCache`, `-PkgConfig` and `-CompilerLauncher` are explicit supported choices;
see [the script](../packaging/windows/Build-Windows.ps1) and
[packaging prerequisites](../packaging/windows/README.md).
Do not run competing memory-intensive builds/VMs on a constrained host.

```powershell
python tools/package_sdk.py verify build/windows-Experimental/windows-sdk
python tools/inspect_sdk.py build/windows-Experimental/windows-sdk --host-check --system-directory C:/Windows/System32 --output build/windows-Experimental/sdk-check.json
```

The SDK has the common tool inventory plus its Windows readiness executable.
Bundled DLL/runtime notices are part of its checked closure. Import/loader
inspection is not minimum-Windows-version, driver or editor behaviour certification.
[MinGW cross-building from Linux](WINDOWS_CROSS_BUILD.md) is a distinct optional
workflow; a cross-build is not native execution.

## Author, export and relocate

Use `judas_editor.exe PROJECT.judasproj` with this SDK's docs/declarations.
Create a project, import content, edit/undo/save/reopen, then Play/Stop repeatedly.
Inspect runtime errors and preserved authored state. Export with the matching
`judas_export.exe`; move the entire package to a new path and execute its project.
Test cold saves in a fresh process using the same project save identity.

Keep source project, SDK and packaged-game folders distinct. A moved game must not
need the source checkout, the original assets folder or its exporter. Unicode and
space-containing paths must stay quoted. Do not mix old runtime/new exporter
identities or overlay an SDK without strict complete-inventory verification.

## Native application qualification

Use [the harness](TEST_HARNESS.md) and [SDK qualification](DISTRIBUTION.md), then
check actual hardware input, audio, graphics, layouts and repeated lifecycle/load
behaviour. Hosted native physics, material, networking, UI-model and AI contracts
are valuable but do not replace this desktop route.

Windows material instances whose state move cannot guarantee no-throw transfer
run the same ordered kernels on the owner. This preserves physical scheduling/
results and avoids an allocation-prone worker lease; it is not a second solver
or measured Windows frame-rate qualification.

## Dependency bootstrap and security policy

The build entry point prepares native x64 **pkgconf 3.0.8** automatically before
vcpkg runs. It downloads the versioned upstream MSI, checks its pinned SHA-256,
extracts it locally using `msiexec /a` (no installation or elevation), and checks
the executable's pinned SHA-256 and `--version`. Repeated builds reuse the same
verified tool under `%LOCALAPPDATA%/JudasBuildTools/pkgconf`; `-ToolCache` selects
a different caller-owned cache. It does not install tools globally or change PATH.

The pinned vcpkg `2025.02.14` ports still provide SDL2 2.30.12 and GLM 1.0.1#3.
SDL2's `vcpkg_fixup_pkgconfig()` needs pkg-config to validate installed metadata;
without `PKG_CONFIG`, that revision's Windows tool helper acquires pkgconf through
MSYS2 and adds the now-unavailable `msys2-runtime-3.5.4-2` archive. The native tool
satisfies this actual requirement and prevents that acquisition path. Judas does
not patch the vcpkg checkout, change port versions, skip metadata validation or
relax any dependency hash.

The pinned upstream [SDL2 port](https://github.com/microsoft/vcpkg/blob/d5ec528843d29e3a52d745a64b469f810b2cedbf/ports/sdl2/portfile.cmake),
[pkg-config tool helper](https://github.com/microsoft/vcpkg/blob/d5ec528843d29e3a52d745a64b469f810b2cedbf/scripts/cmake/vcpkg_find_acquire_program%28PKGCONFIG%29.cmake)
and [MSYS2 acquisition helper](https://github.com/microsoft/vcpkg/blob/d5ec528843d29e3a52d745a64b469f810b2cedbf/scripts/cmake/vcpkg_acquire_msys.cmake)
define that chain. Native tool provenance is the upstream
[pkgconf 3.0.8 release](https://github.com/pkgconf/pkgconf/releases/tag/pkgconf-3.0.8).

The build script passes the prepared executable through `PKG_CONFIG` and
`VCPKG_KEEP_ENV_VARS` and restores the caller's environment on success or failure.
An ambient PATH/`PKG_CONFIG` executable is not selected implicitly. For an
organisation-approved alternative, pass its full path using `-PkgConfig`; it is
checked before dependency installation and never silently replaced after failure.
An explicit override remains caller-owned and must support the pkg-config
operations used by the pinned port.

Windows Application Control can reject any executable, including the prepared
tool or the MSI extraction. The build then stops with the failing step/path; it
does not rename binaries to evade policy, unblock files, change policy, elevate,
or fall back to an unapproved tool. Ask the administrator for an approved tool
and select it explicitly. A hash mismatch also stops before execution; the
suspect cache is retained for inspection rather than silently trusted.

To reproduce only dependency installation and CMake generation:

```powershell
./packaging/windows/Build-Windows.ps1 -Channel Experimental -DependenciesOnly -Jobs 2
```

This does not compile Judas or qualify its applications. The dedicated Windows
bootstrap CI uses a fresh vcpkg root, disabled binary caches, a space-containing
tool cache, and an unusable ambient tool; it tests actual SDL2/GLM installation,
cache reuse, corrupt-download/executable rejection and explicit-tool failure.
It does not claim to reproduce Application Control policy enforcement itself.

The pinned dependency manifests and [runtime notices](../third_party/RUNTIME_NOTICES.txt)
own third-party terms. Keep Windows runtime DLLs/notices in the exported package.
Missing imports, wrong architecture, stale source/tool identities and unsupported
required graphics features must fail meaningfully. Do not add software-driver
DLLs to stock SDKs or claim that a VM software-GL test is hardware qualification.
