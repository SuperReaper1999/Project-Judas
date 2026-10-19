# Pre-Alpha: Linux → Windows game build/export

Date: 2026-10-19. Development branch: **Experimental**. Main remains at accepted
`f6892caac5f912241ad600cec9ebeba99043e14d`. This is automated qualification, not
human Windows hardware acceptance or M100/Alpha publication.

## Outcome

A native Windows x64 Release Hearthworld game was **built and exported on Linux,
then actually executed in Windows 10**, including rendering, game interactions,
physical objects, input/UI routing, audio output callbacks, saves and fresh-process
restoration. No VM or Wine is used by the build/export pipeline itself.

The package is `.cache/windows-cross/Hearthworld-Windows.zip` in the development
checkout. Unzip the whole directory on Windows and run `judas.exe`. The archive
is **37,174,800 bytes**; exact sizes/hashes, all 124 package files,
DLL closure and embedded identity are in [ARTIFACTS.json](ARTIFACTS.json).
The unpacked package is **102,861,403 bytes**; runtime bundle 87,662,789 bytes.
Executables/build directories are deliberately not committed.

Start with [Windows cross-build instructions](../../WINDOWS_CROSS_BUILD.md).

## Findings and engineering decisions

| Actual blocker | Implemented correction |
|---|---|
| CMake rejected every Windows compiler except MSVC and unconditionally required the vcpkg MSVC triplet | Separate opt-in MinGW x64 path; native MSVC requirements remain in their existing branch |
| SDL/GLM discovery expected native-installed packages | Reuse the already-vendored, checksum-pinned SDL 2.32.10 / GLM 1.0.1 archives; static target SDL |
| Unicode stack had only native Linux, MSVC and Android branches | MinGW target FreeType/HarfBuzz + ICU 76.1, with separate native Linux ICU generators |
| Relocated windres selected Linux gcc to preprocess Windows resources | Explicitly select the target C preprocessor |
| ICU tried linking unnecessary Windows build tools against incompatible library names | Disable target tools; run native generators, not Windows executables, during cross build |
| ICU's generated COFF data archive and installed stub had different paths | Install the actual 31 MB generated `sicudt.a` at the explicit static target link path, not the 736-byte stub; verify real CLDR/shaping on Windows |
| Existing exporter is compiled for its host and validates a same-platform runtime | Reuse native asset validation/cooking, then assemble a checked Windows target payload in an owned staging directory |
| Target executables cannot be queried by running them on Linux | Embed target identity in PE RCDATA 101 and parse it with bounded, non-executing metadata inspection |
| Resource compiler did not discover JSON identity changes | Declare an explicit resource dependency on configured identity and application manifest; preserve original failing drift checks |
| Runtime DLL/licensing closure differs from MSVC | Discover selected compiler runtimes, recursively inspect PE imports, stage only required non-system DLLs and notices |
| Linux permits names Windows cannot safely consume | Reject Windows-invalid device/path names, trailing dots/spaces, case aliases, symlinks and runtime payload collisions before publication |

The selected route is **MinGW-w64 GCC POSIX x64**. GCC and Clang both produced
real Windows thread/filesystem probe executables; both ran successfully on Windows
([receipt](windows-compiler-probes.json)). Clang's bare target/sysroot invocation
failed to find C++ headers. Supplying MinGW headers, C++ libraries and linker made
the probe work. **No full Clang engine qualification is claimed.** LLVM-MinGW is
a plausible alternative distribution, not a requirement or delivered second build.

Primary references: [MinGW toolchains](https://www.mingw-w64.org/downloads/),
[Debian/Ubuntu setup](https://www.mingw-w64.org/getting-started/debian/),
[Clang cross-compilation requirements](https://clang.llvm.org/docs/CrossCompilation.html),
[ICU build/cross-build instructions](https://github.com/unicode-org/icu/blob/main/docs/userguide/icu4c/build.md).
Actual pinned source/build results, rather than these pages, determine this qualification.

## Implementation and ownership

- `cmake/toolchains/WindowsMinGW64.cmake` and three small cross-specific CMake
  modules separate compiler selection, dependencies, ICU generation and staging.
- `tools/build_windows_cross.py` builds target runtime/tools and, when exporting,
  matching native Linux runtime/exporter. Its evidence contains commands/timings.
- `tools/windows_runtime.py` prepares/verifies the bounded PE identity, hashes,
  machine type, notices and recursive DLL closure without executing Windows code.
- `tools/export_windows.py` reuses the native exporter, rejects source/channel
  mismatches, replaces only the runtime payload and atomically publishes a **new**
  output directory. Existing destinations are never overwritten.
- `scripts/check_windows_cross.py` uses the actual generated engine executable
  for metadata/packaging controls. It is not native Windows execution evidence.
- `tests/WindowsCrossAudioTests.cpp` exercises a real output-device callback,
  pause/resume cursor and stop. It fails rather than silently using a dummy mixer.

The **only** existing C++ change is an accurate Windows requirements-file sentence
in `ProjectExporter.cpp`. No gameplay, physics, JudasJS binding, save-format,
renderer, audio implementation or platform-lifecycle semantics were changed.
MSVC dependency/staging modules were not modified. No new public scripting API.

The qualified compiler prefix is the Ubuntu MinGW POSIX distribution recorded in
[HOST.json](HOST.json) and [mingw-version.txt](mingw-version.txt). Tools were
extracted into an ignored task-local prefix because system sudo was unavailable;
no host package installation was required for this investigation. Ordinary users
can install the documented distribution packages instead.

## Build measurements

Host: i7-4790, 8 logical CPUs, 16 GB RAM, Linux x64; Release, four build jobs.
See [host inventory](HOST.json), [successful pipeline](pipeline-02/RESULTS.json),
[failed first pipeline](pipeline-01/RESULTS.json), and preserved build logs.

| Measured phase | Result / elapsed |
|---|---|
| Full target engine + Windows runtime/export/script tools | Successfully compiled from previously absent target engine objects; original cold-build wall time was not instrumented, so no invented clean-build timing |
| Final matched cross configure / incremental build | 3.01 s / 10.31 s |
| Final matched native configure / incremental build | 2.91 s / 9.16 s |
| Fresh automatic native ICU generator bootstrap (no reused tools) | 109.40 s, success |
| Native Linux export | 52 assets, one scene, 70,183,926 bytes; exporter measured 0.978 s |
| Windows game package | 124 files plus its manifest; 102,861,403 bytes unpacked |

The final game used the existing ICU 76.1 native tools. The separate fresh bootstrap
also built and ran its own native generator tools successfully. This checks both
paths; it is not a second full cold engine build. Compiler/dependency warnings are
preserved. This is **not** a warning-free, byte-reproducible-across-hosts or speedup claim.

## Genuine Windows results

Existing Windows 10 Home 22H2 build 19045, interactive **limited user Conner**,
6 GB VM. No VM configuration, system toolchain or disk-layout changes.
Runtime search path was only `C:\Windows\System32;C:\Windows`, with unrelated
working directories. All transferred binaries and all 124 pristine package files
were hash-verified. All 13 actual system imports resolved in System32.

| Native Windows invocation | Checks / observed result |
|---|---|
| Actual runtime `--build-info-json` | Exit 0; exactly equals the PE resource and Linux bundle identity |
| Build identity / child-process argument handling | 55 / 0 failures |
| Windows readiness, locale, storage, backup, locks, child launch | 21 / 0 failures |
| CharacterMotor | 32 / 0 failures |
| Real audio output, callback consumption, pause/resume, stop | 9 / 0 failures; cursor 10080 → held 10080 → resumed 15840 frames |
| ICU/localization/text shaping | 49 / 0 failures, including actual CLDR 46 data, Arabic/Indic/bidi, fallback and locale formatting |
| Hearthworld application gameplay writer | 66 / 0 failures, 139.30 s |
| Separate fresh-process save reader | 41 / 0 failures, 6.33 s |
| Actual game bindings, inventory/pause/UI save-and-quit | 20 / 0 failures, 15.04 s |
| Cross-built script checker | 11 registered modules; 0 compile/link failures |
| Relocated exported game in `Moved Package résumé` | Exit 0, 8.59 s; movement/look, camera toggle, inventory/pause, async save and reload, scene assertions and rendered screenshots |
| Missing compiler-runtime DLLs | Windows loader rejects executable: `0xc0000135`; pristine positive loads successfully |
| Stock QXL without software graphics | Clean exit 1: required OpenGL 3.3 Core entry points unavailable |

**293 native engine/game assertions passed**, plus the actual runtime, script-tool,
compiler-probe, relocated-package and negative-control invocations. This does not
mean every Judas subsystem or physical input device was exhaustively tested.
Input was synthesized through the existing game/application harness; physical
Windows controllers and human keyboard/mouse acceptance remain outstanding.

Rendering used isolated app-local llvmpipe copies: Mesa OpenGL 4.6, 1024×768.
**The distributed package contains neither Mesa nor a VM-specific graphics DLL.**
The VM audio device consumed real callback frames; audible quality on physical
Windows hardware was not human assessed. VM timings are functional diagnostics,
not Windows GPU/game-performance acceptance. Linux's relocated native game smoke
also passed on the actual GTX 970/OpenGL 3.3.

The writer exercised actual tree felling/log processing, correct/wrong tools,
mining/resource accounting, gravity-gun grab/launch, radial traversal, construction,
real excavation/collision loss and physical support failure. The cold reader
compared restored state at the capture boundary and after real simulation, including
terrain bytes, inventory, harvested objects, constructed objects and physical state.
These are the existing unchanged Hearthworld acceptance fixtures, not a synthetic
replacement game or manually altered score/state.

Native evidence: [engine/game results](windows-native/RUNS.json),
[writer](windows-native/game-write.stdout.txt),
[cold reader](windows-native/game-cold-read.stdout.txt),
[controls](windows-native/game-controls.stdout.txt),
[relocated run](windows-moved/RUNS.json), [negative controls](windows-native/NEGATIVE.json).

## Other qualification and preserved failures

- Linux: matching Release runtime/exporter build; build identity **55**, readiness
  **14**, real native export and relocated 110-frame gameplay/UI smoke pass.
- Cross packaging: **26 checks, zero failures**, with real PE metadata, changed
  executable/sidecar, missing/unexpected DLLs, source/channel mismatch, bad binary,
  case aliases, reserved names and existing-directory rejection controls.
- JudasJS declarations/API checker: 473 public symbols, 40 exports, TypeScript
  5.9.3, negative drift controls pass. No new binding was added.
- Current documentation checker and implementation whitespace checks pass. Raw
  diagnostic evidence retains its original whitespace and line endings.
- Android ARM64/API26/NDK28.2 full **configuration** passes without MinGW dependencies.
  No fresh Android engine/APK/device execution or fresh native MSVC build is claimed.
  Shared runtime implementations are unchanged and existing platform branches remain.

Original failures are retained: target resource preprocessor, ICU target tools/data
installation, stale embedded identity, and the rejected mismatched host/target
export. `contracts-01.json` and `contracts-02.json` each preserve the stale-identity
failure; `contracts-03.json` passes after the dependency correction. Some detailed
ExternalProject ICU stamp logs were overwritten during its retry; the original
wrapper build logs and mechanisms are retained, not recreated as ceremonial failures.

The first Windows runner rejected an empty `Start-Process -ArgumentList`; native
process handling corrected that. Its next relocated launch failed because Windows
PowerShell 5 decoded a UTF-8 script without BOM as the legacy codepage. The corrected
BOM script launched the actual accented path. Both are **test-runner defects**, not
engine failures. Original errors and separate successful follow-up remain distinct.
Some captured native console UTF-8 text is mojibake in the old .NET default-codepage
log reader; actual path lookup, rendered Unicode and ICU assertions passed.

## Provenance, remaining boundaries and human test

[TESTED_SOURCE.json](TESTED_SOURCE.json) hashes all 15 implementation/current-doc
inputs. [TESTED.patch](TESTED.patch) records their exact delta from the accepted
baseline. `SHA256(source_tree + newline + TESTED.patch)` equals the embedded
`ce88da00f2d5def2dc4244f4e50ed369f62781cfb56cf7f272d35e605a79d9b1`.
The binaries honestly identify this **dirty development candidate**, including
16 observed pre-existing untracked inputs. The scoped `.gitattributes` addition preserves these new evidence bytes verbatim.
Later evidence/report files are not
pretended to have been present when the binaries were compiled. No expected hash
was rewritten to hide a source mismatch. Final per-input hashes remain identical.

Initial scope: Linux host, MinGW GCC POSIX, x64 Release game exports. Clang engine,
32-bit/ARM Windows, native MinGW Windows builds, cross-built full editor/SDK,
signing/installers, universal Windows hardware and cross-MSVC-save interoperability
are not qualified. The editor retains native Export; cross export is the documented
CLI. A functioning Windows OpenGL 3.3/audio stack is required. Compiler-runtime
notices currently follow the documented Debian/Ubuntu toolchain layout.

### Human acceptance checklist

1. Build a fresh bundle/game with the documented Linux helper; verify the package.
2. Move/unzip the complete archive on Windows; run `judas.exe` without Visual Studio.
3. Move/look/jump, switch cameras, collect tools and fell/process a tree.
4. Mine, grab/launch a prop, construct something and excavate its support.
5. Check HUD, pause/inventory, mouse capture and audible game effects.
6. Save, quit fully, reopen and load; verify inventory, buildings and excavation.
7. Repeat from a path with spaces/accented characters and an unrelated working directory.
8. Check physical Windows GPU/audio/controllers separately; VM validation does not replace this.

Protected historical evidence, independent fluid research, existing game content,
main and unrelated local work are unchanged. No M100, release/tag or human-acceptance
claim. Linux acceptance and native Windows functional evidence support an
**Experimental-only** checkpoint.
