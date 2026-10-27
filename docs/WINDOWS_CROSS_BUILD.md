# Build Windows games on Linux

Current optional support: **optional Windows x64 Release games built with
MinGW-w64 GCC on Linux**. The native MSVC editor/SDK workflow remains available.
This is a command-line game export workflow; the editor's Export command still
builds for its native platform. It does not produce an Android package or a
complete cross-built Windows SDK.

## Requirements

Use the ordinary [Linux build prerequisites](../README.md), CMake, GNU make,
Python 3.11+ and a POSIX-thread MinGW-w64 x64 compiler, resource compiler and
Windows development headers. On Debian/Ubuntu:

```sh
sudo apt install g++-mingw-w64-x86-64-posix binutils-mingw-w64-x86-64
```

SDL2, GLM, FreeType, HarfBuzz and ICU use the repository's pinned local archives.
ICU needs **native Linux generator tools as well as target libraries**. The
build creates both automatically; `--icu-host-build` can reuse an existing
**ICU 76.1** native build containing `config/icucross.mk`. No Wine, Windows VM,
Visual Studio, Windows SDK or GitHub login is needed to build/export.

## Build and export

From the source root, use separate native and target build directories:

```sh
python3 tools/build_windows_cross.py \
  --build build/windows-cross-Experimental \
  --host-build build/linux-cross-host-Experimental \
  --jobs 4 \
  --bundle build/windows-runtime-001 \
  --project projects/hearthworld/hearthworld.judasproj \
  --output build/Hearthworld-Windows-001
```

Choose **new** bundle/package destinations. Existing content is never replaced.
Omit `--project` and `--output` to build only the checked target runtime bundle.
Use `--channel Alpha|Beta|Experimental|Stable|Local` for provenance, not branch promotion.
A relocated toolchain prefix can be supplied with `--mingw-root PREFIX`; it
must contain `bin/` and `x86_64-w64-mingw32/`, including redistribution notices.
The helper records commands, elapsed times and outcomes in its evidence directory.
First builds compile the engine and its dependencies for two platforms; allow
substantial time and disk space. Subsequent builds are incremental.

To reuse a bundle explicitly:

```sh
python3 tools/export_windows.py \
  --project projects/hearthworld/hearthworld.judasproj \
  --output build/Hearthworld-Windows-002 \
  --runtime-bundle build/windows-runtime-001 \
  --host-exporter build/linux-cross-host-Experimental/judas_export \
  --host-runtime build/linux-cross-host-Experimental/judas \
  --engine-data .
python3 tools/export_windows.py --verify build/Hearthworld-Windows-002
python3 tools/windows_runtime.py verify build/windows-runtime-001
```

The Linux exporter validates and cooks the ordinary game and assets. The second
phase substitutes the checked Windows runtime and recursive non-system DLL
closure. **Host exporter and target runtime must have identical source/channel
identities.** Changing source, tracked documentation or observed untracked build
inputs requires rebuilding both; do not edit identity JSON to bypass this check.
Windows-invalid filenames, case aliases, symlinks and runtime payload collisions
are rejected before publication. An interrupted export leaves the destination
unpublished. Checksums detect substitution/corruption, not malicious signing.

## Package and provenance

Distribute the **whole output directory**, not just `judas.exe`. It contains the
game project/assets, engine fonts/notices, selected compiler-runtime DLLs,
`BUILD_PROVENANCE.json` and the exhaustive `WINDOWS_PACKAGE.json` checksums.
SDL2 and ICU (including Unicode data) are static. GCC runtime DLLs remain app-local;
players do not need Visual Studio or a compiler. Windows system APIs and working
OpenGL 3.3/audio drivers remain OS/device requirements. No software graphics
replacement is shipped. User saves use the ordinary LocalAppData game identity,
independently of the installation directory.

Target provenance is read from a bounded **PE RCDATA resource**, not inferred from
a renamed executable or a sidecar JSON. Native `--build-info-json` can be compared
with that resource on Windows. Existing source-archive provenance also works;
use the established [distribution instructions](DISTRIBUTION.md) to preserve the
archive identity rather than stripping it.

## Qualification and boundaries

See the [cross-build evidence](evidence/windows-cross/REPORT.md) for exact tested
versions, commands, failures and native Windows results. Compilation and import
closure alone are not gameplay acceptance. VM software rendering is functional
validation, not physical GPU or performance qualification. Test your own project
on its target Windows hardware before distribution.

Qualified scope is MinGW-w64 GCC, x64, Release, Linux host, Windows 10 VM.
Windows 11/hardware/gamepads and other compiler versions require further testing.
Clang's Windows GNU target was investigated with an actual executable probe, but
**the full Clang engine build is not qualified**. A target flag alone does not
supply Windows headers, C++ libraries, runtime DLLs or ICU native tools. MSVC ABI
and MinGW C++ ABI are different: do not mix their prebuilt libraries.
Signing, installers, ARM/32-bit Windows, native Windows editor cross-SDK delivery
and automatic editor integration are outside this first support boundary.
