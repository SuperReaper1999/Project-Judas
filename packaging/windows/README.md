# Native Windows candidate v1

Entry point: [docs/WINDOWS.md](../../docs/WINDOWS.md).
Run Build-Windows.ps1 from Developer PowerShell for Visual Studio 2022.
The candidate is Windows-unvalidated and not final; it is not a cross-build.

vcpkg.json and the build script pin the same official 2025.02.14 commit.
The static-md triplet means static SDL2 with the dynamic Visual C++ runtime.
The existing immutable text-library archives retain their original hashes.
ICU is built using its upstream native solution, including the Unicode data DLL.

JudasWindowsSdk.cmake collects ICU and Visual C++ redistributable DLLs, licenses,
fonts and tools. required-dlls.txt is generated from that exact CMake list, not
filesystem guessing. Export rejects missing files and copies only listed DLLs.
judas.manifest supplies asInvoker, Unicode narrow-path and long-path settings.
No installer, signing, launcher or compatibility promise is introduced.
