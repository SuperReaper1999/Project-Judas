# Pre-Alpha distributions and package verification

The two engine channels are **main** (operator-accepted release lineage) and
**Experimental** (automated development checkpoints). `Local` identifies explicitly
local builds. Selecting a channel is build metadata; it does not promote a branch
or certify human acceptance. No Alpha release is implied.

## Native SDK

Configure separate build directories, then build the shared inventory target:

```sh
cmake -S . -B build/prealpha-Experimental -DCMAKE_BUILD_TYPE=Release -DJUDAS_BUILD_CHANNEL=Experimental
cmake --build build/prealpha-Experimental --target judas_linux_sdk -j4
python3 build/prealpha-Experimental/linux-sdk/tools/package_sdk.py verify build/prealpha-Experimental/linux-sdk
```

On Windows use [Build-Windows.ps1](../packaging/windows/Build-Windows.ps1) and the
`judas_windows_sdk` target; see [Windows prerequisites](WINDOWS.md). Staging and
verification use only Python 3.11+ standard library. Packaged games do not require
Python. Linux SDK executables require compatible glibc/libstdc++, SDL2 and OpenGL
3.3 drivers; platform libraries/drivers are not bundled. Windows SDKs include the
existing ICU/Visual C++ runtime dependency closure and redistribution notices.

### Binary compatibility qualification

The Linux distribution baseline is **Ubuntu 24.04 x86-64**, built and qualified
on that host in CI. The SDK gate reads ELF **required** symbol versions (not
exported symbol definitions), rejects glibc requirements above **2.39**, records
GLIBCXX/CXXABI requirements and checks all 17 tools with the actual host loader.
It also requires the qualification host to identify as Ubuntu 24.04. This is a
declared baseline with a mechanical gate; only a completed CI receipt establishes
that a particular checkpoint passed. GPU/application checks remain separate.

SDKs built on newer systems retain those systems' actual ABI requirements. The
pass-1 Ubuntu 26.04 build requires glibc 2.43 and is **not** an Ubuntu 24.04 binary.
Nothing rewrites that requirement or claims universal Linux/AppImage portability.
Build from source on a compatible host if no qualified binary is available.

```sh
python3 tools/inspect_sdk.py build/linux-sdk --host-check --max-glibc 2.39 --require-os ubuntu:24.04 --output /outside/sdk-qualification.json
# Native Windows: inspect regular and delay imports, including each bundled DLL.
python tools/inspect_sdk.py build/windows-sdk --host-check --system-directory C:/Windows/System32 --output C:/Temp/sdk-qualification.json
```

The inspector first verifies the SDK inventory/checksums. ELF/PE parsing does
not run binaries. `--host-check` uses `ldd` on Linux and is appropriate only for
an SDK you trust; do not use it on arbitrary downloaded executables. Windows
qualification resolves every non-API-set import to an SDK DLL or the specified
real system directory. API-set contracts are recorded explicitly; import closure
does not prove minimum-Windows-version or GPU compatibility. Malformed metadata,
unknown architecture and unresolved imports fail rather than silently passing.
The inspector and its stdlib-only binary reader ship in the SDK.
The independent `tools/verify_android.py` APK verifier ships alongside them;
APK content/native-byte verification does not need the source checkout or JDK.

[sdk-inventory.json](../packaging/sdk-inventory.json) is the single list used by
CMake and the packager. It includes the runtime/editor/exporter; model, scene,
navigation, liquid, environment, deformable, collision and terrain authoring;
mods; asset cooking; script checks; save evolution; and dedicated hosting.
Historical harnesses and lab builders are not public SDK tools.

SDKs include engine fonts, licenses, top-level developer guides and small historical
design references, the full JudasJS reference/examples and `judas.d.ts`. They exclude
projects, research/prototypes, the old README archive, historical evidence and build
caches. Links into those excluded materials refer
to the matching source checkout; an SDK is not a complete repository archive.

## Identity and checksums

- `BUILD_INFO.json`: configured engine/source/platform/toolchain identity.
- `SDK_MANIFEST.json`: required tools, exact file sizes and SHA-256 hashes.
- `SHA256SUMS`: every payload hash plus the SDK manifest hash.
- Every public tool's `--build-info-json` is checked against the configured build
  before publication; mixing stale executables with a new manifest is refused.

The preparation directory is verified before replacing a generated SDK. A failed
preparation preserves the previous SDK. Existing nonempty directories without an
SDK manifest are refused even with `--replace`; choose a new destination. Publication
uses same-filesystem directory renames with rollback on an ordinary rename failure;
it is not a cross-process transaction or a power-loss durability guarantee. Do not
package concurrently into one SDK destination or replace one while a tool runs.

SDK staging defaults to a clearly recorded `development` preset so an uncommitted
candidate can be tested. `-DJUDAS_SDK_RELEASE=ON` (or the packager's `--release`)
requires clean known Git/archive provenance, no observed untracked build inputs,
Release configuration and a main or Experimental channel. Dirty, unknown, Local
and untracked-input identities are refused. An isolated verified source archive
can explicitly exclude unrelated untracked files without deleting them. This does
not replace main's separate human-acceptance requirement. Identity probes are
bounded to 10 seconds and 16 KiB output per tool.

Verification works after relocation from an unrelated working directory. It detects
missing, changed and unexpected files, including stale DLLs. Checksums do not prove
authenticity against malicious replacement; distribute the expected manifest/hash
through a trusted channel. The manifest does not hash itself recursively.

```sh
python3 /moved/sdk/tools/package_sdk.py verify /moved/sdk
```

## Android

See [Android presets, credentials and provenance](ANDROID.md#build-identity-and-reproducibility).
Application versions and save/content compatibility remain project-owned, separate
from the engine version and channel. A release-signed APK is not automatically a
human-accepted engine release. No device installation is performed by packaging.

## Source archives without Git history

After checkpointing the exact intended source, create an engine-source archive:

```sh
python3 tools/package_source.py --channel Experimental --output /outside/repository/Judas-Experimental-source.tar.gz
cd /outside/repository
sha256sum -c Judas-Experimental-source.tar.gz.sha256
```

Run checksum verification from the archive's parent directory. The command refuses
tracked edits and untracked inputs by default. Commit intended source first.
`--allow-untracked` explicitly excludes all untracked files and records their count;
it never publishes or deletes them. `--replace` is required to overwrite an archive.
Archive outputs must be outside the checkout. Existing archive/receipt/checksum
files require `--replace`; symlink outputs are refused.

The default **engine** profile includes native source/build dependencies, engine
assets, authoring tools, test sources, scripts and current API/reference docs.
It excludes games, prototypes and historical evidence/research, preserving those
in the original checkout. Tests that depend on excluded game/evidence fixtures
require the full checkout or explicit `--profile full` (several GiB). This selection
is recorded, not represented as a complete repository mirror.

The archive uses exact Git blob bytes, including files Git export attributes might
otherwise omit or rewrite. Paths, modes and tar/gzip timestamps are deterministic;
`SOURCE_DATE_EPOCH` overrides the default Unix epoch. Symlinks, submodules and names
that the provenance verifier cannot safely represent are refused explicitly.
`JUDAS_SOURCE_INFO.json` freezes the source commit/tree/channel and binds the
`JUDAS_SOURCE_FILES.json` SHA-256 file inventory. CMake validates this inventory
without Git; edited build inputs no longer masquerade as the clean frozen source.
The external `.build.json` and `.sha256` identify the exact archive. As with SDK
checksums, this is provenance/accidental-drift detection, not publisher signing.

## Focused checks

```sh
python3 scripts/prealpha_packaging_checks.py --output .cache/prealpha/packaging/checks.json
python3 scripts/prealpha_distribution_checks.py --output .cache/prealpha/packaging/distribution.json
```

These fixture tests exercise omissions, mixed identities, corruption, failed
replacement, Windows DLL closure, Android presets and exact/deterministic source
archives. They need Python and Git, but no compiler, SDK or device. They do not
substitute for native Windows, Android or relocated executable smoke tests.
Native platform results belong in the current evidence.
