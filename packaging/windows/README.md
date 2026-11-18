# Native Windows build dependencies

Windows is unsupported. These instructions are retained for independent use.
Official Windows qualification and SDK distribution have ended.

Entry point: [Windows build and SDK guide](../../docs/WINDOWS.md).
Run `Build-Windows.ps1` from Developer PowerShell for Visual Studio 2022/2026,
or select Ninja with a prepared native compiler environment.

Native MSVC uses [verified local upstream sources](../../third_party/windows/README.md)
for SDL2 2.30.12 and GLM 1.0.1 through `JudasWindowsDependencies.cmake`.
There is no vcpkg, MSYS2, pkgconf download or pkg-config execution. SDL2/GLM are
static libraries using `/MD` in Release and `/MDd` in Debug. The existing
immutable text-library archives retain their original hashes; ICU still uses
its upstream native solution and Unicode data DLL.

`Test-Bootstrap.ps1` builds both dependency configurations without binary caches,
installs and relocates their CMake packages, and compiles/runs separate console
and SDL2main consumers. It also checks CRT directives, actual source-hash failure,
legacy-toolchain rejection, and that a deliberately unusable pkg-config was never
executed. This is dependency qualification, not Windows 11 Smart App Control or
full Debug SDK acceptance.

`JudasWindowsSdk.cmake` collects ICU and Visual C++ redistributable DLLs, exact
upstream SDL2/GLM licences, fonts and tools. `required-dlls.txt` is generated from
that exact CMake list. Export rejects missing files and copies only listed DLLs.
The ordinary SDK remains a JudasJS authoring/runtime/tool distribution; it does
not become a public C++ engine library or ship pkg-config contracts.
`judas.manifest` supplies asInvoker, Unicode narrow-path and long-path settings.
No installer, signing or security-policy change is introduced.
