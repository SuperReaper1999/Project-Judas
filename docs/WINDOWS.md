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
`-VcpkgRoot`, `-PkgConfig` and `-CompilerLauncher` are explicit supported choices;
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

## Dependencies and failure reporting

The pinned dependency manifests and [runtime notices](../third_party/RUNTIME_NOTICES.txt)
own third-party terms. Keep Windows runtime DLLs/notices in the exported package.
Missing imports, wrong architecture, stale source/tool identities and unsupported
required graphics features must fail meaningfully. Do not add software-driver
DLLs to stock SDKs or claim that a VM software-GL test is hardware qualification.
