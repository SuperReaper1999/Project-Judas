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

## Platform support and executable trust

Linux is the primary, officially supported development and distribution platform.
Windows is experimental and best-effort. Windows builds and SDKs may be provided
without a compatibility guarantee. Existing Windows workflows remain available for
developers who want to use them experimentally.

Windows 11 with Smart App Control (SAC) enabled is unqualified for official Judas
SDKs and exported games. Windows may reject an executable or a DLL at launch or
load time. The tested Windows 11 Home configuration confirmed failures in both
[native source builds](https://github.com/SuperReaper1999/Project-Judas/issues/34)
and [unsigned exported games](https://github.com/SuperReaper1999/Project-Judas/issues/36).
The project is deferring SAC compatibility work. Successful Windows CI or
testing with SAC disabled does not establish compatibility with SAC enabled.

Keep your Windows security settings in place. Changing them is outside the
documented installation and build procedures.

## Build natively

Install Git, Python 3.11+, CMake and Visual Studio's Desktop development with C++.
Use Developer PowerShell. The build script accepts Visual Studio 2022/2026 or
Ninja and builds verified local SDL2/GLM sources with CMake. No dependency
package manager or downloaded metadata-validation executable is needed.

```powershell
./packaging/windows/Build-Windows.ps1 -Channel Experimental -Jobs 2
# With a prepared VS 2026 toolchain:
./packaging/windows/Build-Windows.ps1 -Channel Experimental -Generator 'Visual Studio 18 2026' -Jobs 2
```

The result is `build/windows-native-Experimental/windows-sdk`. `-ReleasePackage` requires
clean Release provenance, not merely successful compilation. `-BuildDirectory`,
`-CompilerLauncher` remains an explicit supported choice;
see [the script](../packaging/windows/Build-Windows.ps1) and
[packaging prerequisites](../packaging/windows/README.md).
Do not run competing memory-intensive builds/VMs on a constrained host.

```powershell
python tools/package_sdk.py verify build/windows-native-Experimental/windows-sdk
python tools/inspect_sdk.py build/windows-native-Experimental/windows-sdk --host-check --system-directory C:/Windows/System32 --output build/windows-native-Experimental/sdk-check.json
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

## Native dependencies and security policy

Native MSVC compiles **SDL2 2.30.12 and GLM 1.0.1** from the hash-pinned local
archives listed in [dependency provenance](../third_party/windows/README.md).
These are byte-identical to the former vcpkg source inputs; CMake verifies their
SHA-512 hashes. No SDL/GLM source fork is maintained. The existing FreeType,
HarfBuzz and ICU archives/owners are unchanged. Configure needs no dependency
downloads, package-manager binary cache, MSYS2, pkgconf or pkg-config executable.

The former SDL2 port's unconditional pkg-config metadata validation caused the
obsolete MSYS2 download and then an unsigned-tool Smart App Control rejection.
That entire native acquisition path is replaced. SDL's optional Unix backend
PkgConfig discovery is disabled only in the native MSVC dependency scope; no
Windows backend or compiler/link check is suppressed. Independent build/link/
runtime consumers validate the actual CMake contract instead of pretending the
retired port's pkg-config check ran.

SDL2 and GLM remain static libraries. `CMAKE_MSVC_RUNTIME_LIBRARY` is explicitly
`MultiThreaded$<$<CONFIG:Debug>:Debug>DLL`: `/MD` for Release and `/MDd` for Debug.
Other CRT selection is rejected. SDL's generated headers and transitive Windows
system libraries come from upstream CMake targets. Judas keeps `SDL_MAIN_HANDLED`
on its own targets; upstream `SDL2::SDL2main` stays available and is tested through
a separate Windows-subsystem executable. `SDL2::SDL2`, `SDL2::SDL2-static` and
`glm::glm` remain usable. Debug GLM is named `glmd.lib` to prevent overwriting the
Release library when both configurations are installed.

The ordinary SDK is a JudasJS application/tools distribution, not a public C++
engine library. Native dependency tests separately install upstream CMake packages
and verify `find_package(SDL2 CONFIG)` / `find_package(glm CONFIG)` after moving
the install prefix and removing the original build location. Upstream's generated
`sdl2.pc` may exist in that temporary dependency prefix; it is not a qualified or
distributed native pkg-config contract. SDK/export licensing comes from the exact
extracted sources. No pkgconf or software graphics DLL is shipped in stock SDKs.

The normal build script produces the Release SDK. Dependency Release/Debug
compilation and independent consumers are qualified separately; a complete Debug
engine/editor/SDK (including Debug ICU and nonredistributable Debug CRT delivery)
is not implied. `-ReleasePackage` still requires clean Release provenance.

Use the new default `build/windows-native-Experimental` directory. Old vcpkg
build trees, tool caches and selected SDKs are left untouched. An explicitly
selected legacy toolchain/build tree is rejected; choose a new `-BuildDirectory`.
The retired `-VcpkgRoot`, `-ToolCache` and `-PkgConfig` build parameters are removed.
An existing `PKG_CONFIG` environment variable is irrelevant to native MSVC and is
never executed or changed by the build. Windows security remains enabled. Native
build CI does not certify Windows 11 Smart App Control acceptance of Judas or
any other compiler-generated program; that requires the real Windows 11 trial.

```powershell
# Build the native SDL2/GLM dependencies without compiling Judas:
./packaging/windows/Build-Windows.ps1 -Channel Experimental -DependenciesOnly
# Fresh Release/Debug dependency and relocated consumer contract (new work dir):
./packaging/windows/Test-Bootstrap.ps1 -WorkDirectory "$env:TEMP/JudasDependencyTrial"
```

Hosted bootstrap CI disables binary caches, uses an unusable pkg-config trap,
and checks real hash/configuration rejections. Application qualification is
separate: stock SDK integrity/loader inspection, editor authoring/Play/Stop,
exports, relocation and cold process behaviour. Hosted test-only software OpenGL
runs are functional evidence, not hardware graphics/performance or human acceptance.
The previous local VM is not part of this checkpoint; VM restoration/provisioning
is deferred to the operator.

The pinned dependency sources and [runtime notices](../third_party/RUNTIME_NOTICES.txt)
own third-party terms. Keep Windows runtime DLLs/notices in the exported package.
Missing imports, wrong architecture, stale source/tool identities and unsupported
required graphics features must fail meaningfully. Do not add software-driver
DLLs to stock SDKs or claim that a VM software-GL test is hardware qualification.
