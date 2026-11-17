# Native MSVC dependency sources

`cmake/JudasWindowsDependencies.cmake` compiles unmodified upstream archives with
CMake. No vcpkg, MSYS2 or pkgconf executable is used. Other platform dependency
selections remain independent.

| Source | Upstream | SHA-256 | Licence |
| --- | --- | --- | --- |
| SDL2-2.30.12.tar.gz | https://github.com/libsdl-org/SDL/archive/refs/tags/release-2.30.12.tar.gz | 560da2e54dd8af933e35bd08fb1b6cf80d4f6938c67710fecf13b7e9bdd6c47e | zlib; archive LICENSE.txt |
| ../android/glm-1.0.1.tar.gz | https://github.com/g-truc/glm/archive/refs/tags/1.0.1.tar.gz | 9f3174561fd26904b23f0db5e560971cbf9b3cbda0b280f04d5c379d03bf234c | MIT; archive copying.txt |

Both SHA-512 hashes match the source archives used by the former vcpkg baseline
`d5ec528843d29e3a52d745a64b469f810b2cedbf`. The CMake module checks those full
hashes during extraction. SDL2's port-only Linux/metadata patches and GLM's
Clang-only patch are unnecessary for native MSVC. No upstream source is patched.
SDL2 and GLM licences are copied from these exact extracted archives into the
Windows SDK/runtime closure. GLM reuses the existing archive rather than adding
a second source copy. The Windows SDL2 archive intentionally retains 2.30.12;
Android/browser/MinGW continue using their existing 2.32.10 archive.
