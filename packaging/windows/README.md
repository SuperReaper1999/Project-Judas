# Native Windows build dependencies

Entry point: [Windows build and SDK guide](../../docs/WINDOWS.md).
Run `Build-Windows.ps1` from Developer PowerShell for Visual Studio 2022/2026,
or select Ninja with a prepared native compiler environment.

The manifest and build script pin official vcpkg `2025.02.14` at
`d5ec528843d29e3a52d745a64b469f810b2cedbf`: SDL2 2.30.12 and GLM 1.0.1#3.
The static-md triplet means static SDL2 with the dynamic Visual C++ runtime.
The existing immutable text-library archives retain their original hashes.
ICU is built using its upstream native solution, including the Unicode data DLL.

`PkgConfig.ps1` owns the hash-pinned native pkgconf preparation used by both
local builds and release CI. This is a build-only tool, not an SDK runtime DLL.
It satisfies SDL2 metadata validation without the pinned vcpkg helper's obsolete
MSYS2 runtime acquisition. See the guide for tool-cache/explicit override options,
security-policy failures and the dependency-only diagnostic mode.
`Test-Bootstrap.ps1` tests a fresh dependency installation and failure controls on
Windows CI; no binary dependency cache or manually installed pkg-config is needed.

JudasWindowsSdk.cmake collects ICU and Visual C++ redistributable DLLs, licenses,
fonts and tools. required-dlls.txt is generated from that exact CMake list, not
filesystem guessing. Export rejects missing files and copies only listed DLLs.
judas.manifest supplies asInvoker, Unicode narrow-path and long-path settings.
Native build/tool validation is separate from Windows editor/graphics/hardware
qualification and operator acceptance.
