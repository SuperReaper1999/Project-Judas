# Clean checkout and distribution qualification

20 October 2026. Scoped Pre-Alpha qualification before an operator-authored game.
No Alpha declaration or human acceptance is implied.

## Candidate and corrections

The accepted starting implementation was `3ecf26cdf248e271c96d17928bd33634e37f30ce`.
The qualified final source is `39d0234ada05ec554738e9e93166c55928f21f6b` on
Experimental. Main remains at the starting commit. Later evidence-only commits do
not change the source identity embedded in these binaries.

- `9dd8a222f2303a53399a10f889fc6cbd1c32ea52`: clarify SDK-root versus source-build
  launch/check/export paths; explain that engine archives and SDKs omit repository
  games; replace stale hardcoded branch status in README with provenance guidance.
- `39d0234ada05ec554738e9e93166c55928f21f6b`: stop Android CLI imports creating
  untracked `tools/__pycache__` inputs before CMake records source identity. A
  subprocess regression ignores inherited Python cache flags; it fails on the old
  entry point and passes on the correction. Identity checks remain strict.

No C++, gameplay, API, physics, protected evidence, independent research, existing
game content or licensing terms changed. The compiled Linux engine static library
remained byte-identical through both updates: SHA-256
`521f0b3ebd75478c84940622109ce6263a4375302742f67405da2170d6fe9d7e`.

## Linux checkout and SDK

A shallow GitHub clone, not a local linked checkout, supplied 55,672 tracked files.
It had no existing engine build or dependency output. Installed system tools and
libraries were reused deliberately: this is a clean source/build qualification,
not a freshly installed operating system. The host is Ubuntu 26.04.1 x86-64,
i7-4790, 16 GB RAM and GTX 970; CMake selected GNU 15.2.0.

Cold Release configuration took 5.72 s and building the strict release SDK plus
three focused test programs took 555.30 s with three jobs. Documentation and Python
tool changes subsequently required metadata/relink/staging updates, not an engine
recompile. These are build timings, not engine performance benchmarks.

The final 17-tool, 224-file SDK was copied outside the repository into a path with
spaces and Unicode, made read-only and run from an unrelated directory with no
source/cache/compiler overrides. Its exact payload hashes were unchanged afterward.

| Check | Result |
| --- | --- |
| Inventory, hashes and all 17 native loader dependency checks | Passed |
| Remove real `judas_server`, verify rejection, restore and verify | Passed |
| Build identity | 55 assertions passed |
| Native save storage | 14 assertions passed |
| Ordinary first-project source, properties, reload, state and camera | 20 assertions passed |
| SDK script checker and standalone exporter | Passed |
| Relocated read-only standalone, real OpenGL rendering and screenshot | Passed, 120 frames |
| New project in the actual SDK editor | 49 assertions, 36 physical steps passed |
| Editor duplicate/undo, Play/Stop and save | Authored scene remained identical |
| Public model import recipe, cook, inspection and reimport | Passed; reimport receipt reused |

The standalone was given a deliberately invalid engine-source hint and still
loaded its packaged resources. Actual application runs used the installed Linux
GPU driver. The old diagnostic HUD remains visible by default in UI-less scenes;
[the first-project guide](FIRST_PROJECT.md) explains how to hide it. The starter
script deliberately supplies a camera and rotating visual object, not a modern
player controller.

## Source archives and negative controls

The final engine-profile archive contains 1,478 manifest entries, all independently
SHA-256 verified. It excludes Git history, repository game projects, protected
research and historical evidence. Repeated packaging from the same final commit
produced byte-identical tarballs:
`a1f43aeaf008c47a9ce83f5663990758d517340ab4e7bd8ca5777d237d2f42eb`.

The original source archive configured with Git explicitly unavailable and its
identity-only executable passed 50 assertions. Corrupting its source manifest was
rejected; exact original bytes were restored. Full engine compilation from the
archive was separately exercised by the Windows build below. The old README's
missing repository demo example was reproduced and its instructions corrected.

Independent focused controls passed: 24 provenance checks, 28 packaging tests and
17 distribution tests. The new seventeenth test demonstrably fails against the
old Android entry point; it is not merely a source-location assertion. A complete
production/physics suite was not rerun for this documentation/Python-only change.

## Linux distribution compatibility

This locally compiled SDK requires GLIBC 2.43, GLIBCXX 3.4.32 and CXXABI 1.3.15.
It passes this host's loader checks and correctly fails the Ubuntu 24.04 / GLIBC
2.39 baseline gate. It must not be advertised as a universal Linux binary.

The successful [final-commit GitHub CI run](https://github.com/SuperReaper1999/Project-Judas/actions/runs/39435308378)
separately qualifies all 17 tools at `39d0234a` on Ubuntu 24.04.5 with GNU 13.3.0,
GLIBC 2.38, GLIBCXX 3.4.32 and CXXABI 1.3.13 requirements. The downloaded receipt
identifies that exact final source. Its artifact contains qualification evidence,
not a complete ready-to-download SDK. Publishing baseline-built binaries remains
a distribution task; neither host's receipt certifies other distributions or
graphics drivers.

## Windows native qualification

The Windows 10 22H2 VM built the original verified engine archive using its
installed Visual Studio 2022/MSVC 19.44 toolchain. No old Judas build was reused;
vcpkg had a new checkout/download/installation directory and its binary cache was
disabled. The documented build command passed in 1,888.92 s. Strict release
reconfiguration and SDK staging also passed. These VM timings overlap some later
host work and are diagnostic, not a hardware performance qualification.

The final archive was separately extracted and all 1,478 source hashes verified.
Updating provenance/staging left `judas_engine.lib` byte-identical:
`f66a535ff6bb0eaf2ce171c931a4fe0f2760ffd797e17a5b317cf1aed30aae21`.

The resulting native 18-tool, 248-file SDK was moved into a Unicode/spaced path and exercised
as the ordinary Conner user from an unrelated working directory, with PATH limited
to Windows system directories for native programs:

- Inventory and regular/delay-import dependency resolution against actual System32
  passed. Native storage/process checks passed 21 assertions.
- Script checking, SDK export and the genuinely relocated standalone passed. The
  standalone rendered 120 frames using its own resources despite an invalid source
  hint.
- New-project editor checks passed 49 assertions and 36 physical steps. The normal
  editor duplicate/undo/Play/Stop/save path preserved the authored scene exactly.
- Final verification confirmed the pristine SDK remained unchanged.

Graphics ran through Mesa llvmpipe added only to separate harness copies, not the
pristine SDK or ordinary game distribution. QXL does not supply the required native
OpenGL driver. Physical Windows GPU, clean physical machine and audible device
output acceptance remain separate. MSVC emitted existing warnings; this is not a
warning-free certification.

## Android package qualification

The complete Breakwater ARM64 APK was built from the final clean Git checkout,
using Android SDK 36, build tools 36.0.0 and NDK 28.2.13676358. Target libraries were
cold-built; the matching ICU host generators came from this task's fresh Linux
build. Two native compiler jobs took 689.22 s including packaging/signing, with
some Windows work running concurrently.

The host initially had only a JRE, and the builder correctly rejected the missing
compiler. Matching official Ubuntu JDK/JRE packages were privately extracted into
the task cache; no system package was installed. The completed build used javac
25.0.4.1. Source Git status remained fully clean after packaging.

The APK is 53,584,343 bytes, contains 41 inventoried content files and has SHA-256
`f3b9c7caa4c4b0056d06006963ce0052e837d5d154bfa911ee0be4694166b449`.
Independent verification using the unpacked SDK's stdlib-only verifier passed:
content and native hashes, expected imports, source identities, ELF architecture
and 16 KiB alignment. Android's own signature and ZIP alignment tools passed, as
did the published APK/receipt checksum pair.

This is a development-signed ARM64 package, target API 36 with minimum API 26.
No phone was installed or modified, no emulator runtime was run in this check and
no new physical-device acceptance is claimed. Existing accepted Android gameplay
is not being recertified by a packaging pass.

## Preserved failed attempts

The original missing-demo README command, JRE-only preflight failure and Android
cache-creation failure are retained alongside corrected results. Deliberate
missing-tool, incompatible-host and corrupt-manifest controls are classified as
expected rejection, not production failures.

Two Windows audit-wrapper mistakes are also retained: reading UTF-8 paths with
PowerShell's default encoding, and treating a native CMake warning as a terminating
PowerShell error. Corrected wrappers used explicit UTF-8 and native exit codes;
the production engine was not changed to accommodate either mistake. The limited
user launcher reported a denied traversal of an unused vcpkg SDL source junction;
actual SDK access and application checks passed independently. Some captured
Windows log paths use the console code page and display mojibake; verified file
access, metadata hashes and rendering from the actual Unicode directories passed.

## Reproduction and artifacts

See [distribution instructions](DISTRIBUTION.md), [Windows prerequisites](WINDOWS.md),
[Android setup](ANDROID.md) and [first project](FIRST_PROJECT.md). Machine-readable
commands, timings, expected negative exits, original failures and hashes are in
[evidence](evidence/prealpha-clean-distribution-20261020/).

Local untracked artifacts are under:
`/home/conner/Documents/GitHub/Project-Judas/.cache/prealpha-clean-distribution-20261020/`.

- `Qualified-Linux-SDK/`: final verified editor/runtime/tool payload, kept together.
- `Judas-Experimental-source-final.tar.gz`, `.build.json` and `.sha256`: reproducible
  engine archive and matching receipts.
- `Breakwater-Clean.apk`, `.build.json` and `.sha256`: development Android artifact,
  subject to the qualification scope above.

Historical commands retain their exact capture locations. For a repeat, choose
fresh output/qualification directories rather than overwriting preserved receipts;
the durable SDK hash record maps the former outside-repository copy to its retained
local payload.

Binaries, SDK trees, private development keys, temporary packages and build caches
are not committed. Evidence identifies exact file bytes and commands; SHA-256
inventories detect omissions/corruption, not publisher authenticity.

## Operator checklist

1. Open `Qualified-Linux-SDK/judas_editor`, then File → New project in your own
   writable folder. Keep the SDK read-only and the project elsewhere.
2. Follow [First project](FIRST_PROJECT.md): rotating box, explicit camera, save,
   Play/Stop, deliberate script edit/check/reload and export.
3. Run the exported directory from another location; confirm it does not depend
   on the repository or editor installation.
4. Copy `docs/judas.d.ts` and the documented `jsconfig.json` into the project for
   IDE completion; the editor's new-project operation does not install an IDE.
5. Add your own movement/UI/content, then report the first workflow that becomes
   confusing or cannot be expressed using the documented public API.

This qualification makes the next operator-authored game a useful developer
experience test. It does not close deferred physics/fluid limitations or authorize
M100, an Alpha release, tags or promotion to main.
