# Build identity and release channels

This is the current **Pre-Alpha** build contract. It does not publish an Alpha
release, change save compatibility, or certify human validation.

## Explicit version and channel

[`engine-version.json`](../engine-version.json) owns the engine version and
maturity. The initial identity is `0.1.0-prealpha` / `pre-alpha`. CMake's numeric
project version is derived from the same file. Milestone numbers and commit
subjects are no longer used to guess a build version.

Configure separate directories for independent channels:

```sh
cmake -S . -B build/Experimental -DCMAKE_BUILD_TYPE=Release -DJUDAS_BUILD_CHANNEL=Experimental
cmake -S . -B build/main -DCMAKE_BUILD_TYPE=Release -DJUDAS_BUILD_CHANNEL=main
```

`main` is the operator-accepted release lineage. `Experimental` contains automated
development checkpoints. `Local` is available for local work. Setting a channel
does not check out a branch, promote a candidate, or prove that it was tested.
An empty `JUDAS_BUILD_CHANNEL` selects the current branch only when its name is
exactly `main` or `Experimental`; otherwise it selects `Local`.

`JUDAS_ENGINE_VERSION` is an optional explicit override. Leave it empty to follow
the checked-in version file, including in an existing build directory. Clear an
old override with `-DJUDAS_ENGINE_VERSION=`. This override is tooling metadata,
not a request to migrate a save or negotiate a network protocol.

## Inspect an executable

The runtime, editor and public SDK tools support these commands before opening
windows, loading projects or initializing application services:

```sh
./judas --version
./judas --build-info-json
./judas_editor --build-info-json
./judas_save --build-info-json
```

`--version` is readable text; `--build-info-json` is schema-1 machine-readable
provenance. The runtime's old `--build-info` line is retained for compatibility.
New tooling should use JSON. Android embeds per-ABI identity in its APK assets
and external build receipt; it does not require a desktop command line.

The same identity is generated as `judas-build-info-<configuration>.json` in the
build directory and `generated/JudasBuildIdentity.h` for native code. It records:

| Field | Meaning |
|---|---|
| `version`, `maturity`, `channel` | Explicit engine identity and selected channel. |
| `build_id` | Version/channel plus abbreviated revision and source fingerprint, with clean/dirty status. Git builds with observed untracked inputs also include their fingerprint suffix, even when tracked files are clean. Use the complete fields for comparisons. |
| `source_revision`, `source_tree` | Full Git commit and tree identities, or their frozen source-archive counterparts. |
| `source_dirty` | Whether tracked source differs from the recorded checkpoint; edited source archives are also marked dirty. |
| `source_fingerprint` | SHA-256 of the recorded tree plus canonical tracked changes. For a clean tree it is SHA-256 of the tree ID followed by a newline. |
| `source_origin` | `git`, verified `archive`, or explicitly `unknown`. |
| `source_untracked_count`, `source_untracked_fingerprint` | Separate conservative observations of untracked files under build-input directories in a Git checkout. These are not a claim that every observed file is compiled. |
| `platform`, `architecture`, `configuration` | Build target and configuration; APKs preserve separate ABI records. |
| `compiler`, `compiler_version` | Compiler provenance. |

Untracked observations do not change the tracked-only `source_dirty` field, and
no unrelated research/assets are deleted. A clean tracked tree can still compile
an untracked input through a source glob. Therefore release packaging rejects
**any observed untracked build inputs**. Checkpoint intended inputs; when
unrelated files must remain, build an isolated, verified source archive using
explicit `--allow-untracked` exclusion. Development packages may retain matching
observations. This is conservative: it does not claim every observed file is compiled.

Build-input changes, Git index/branch changes and version metadata trigger CMake
reconfiguration. Run configuration explicitly when recording final evidence after
documentation-only changes. No current wall-clock timestamp or absolute build
path is inserted into the identity. This makes metadata repeatable; it is **not**
a promise of byte-identical binaries across compilers, paths or platforms.

## Source archives without Git

Use the tracked-source packager after checkpointing intended inputs:

```sh
python3 tools/package_source.py --output /tmp/judas-source.tar.gz --channel Experimental
```

The default `engine` selection contains build sources, engine assets, tools,
tests and current documentation. It omits consumer games, historical evidence
and research; tests requiring those fixtures need the matching checkout or an
explicit `--profile full` archive. Full archives may be several GiB.

The packager refuses tracked changes and, by default, untracked files. Explicit
`--allow-untracked` excludes those files and records the exclusion count. It
never deletes or modifies them. It reads exact Git blobs, preserving tracked
bytes without `export-subst`/`export-ignore` transformations. Archive timestamps
are fixed by `SOURCE_DATE_EPOCH` (default zero), and receipts/checksums accompany
the archive.

An archive includes two required root files:

- `JUDAS_SOURCE_INFO.json`: frozen revision/tree/channel and source fingerprint,
  plus the SHA-256 of the file inventory.
- `JUDAS_SOURCE_FILES.json`: a sorted, hashed inventory of selected source files,
  with one complete record per line.

CMake verifies the inventory and each recorded file. Edited/missing source files
and added build inputs produce dirty archive provenance. A damaged inventory,
malformed identity, duplicate/unsafe path or inventory-hash mismatch is refused.
Ordinary Python bytecode/cache files are not added source inputs.

The generated `JUDAS_SOURCE_INFO.json` in a build directory is intermediate
metadata, not a complete archive by itself: the packager must bind it to the
actual file inventory. Copying source without Git or valid archive metadata
still permits a local build, explicitly marked `unknown`; release packaging must
refuse that identity rather than guessing a milestone or commit.

Checksums detect accidental corruption relative to the expected receipt. They
are not publisher signatures and cannot authenticate a maliciously replaced
archive and its replacement metadata.

## Export/runtime matching

The exporter queries the selected runtime itself through a shell-free probe,
bounded to five seconds and 16 KiB. A missing, stalled, unsuccessful or malformed
runtime fails with a diagnostic. The response must identify a Release runtime
matching the exporter version, channel, source revision/tree/fingerprint, platform and
architecture. An old runtime with only the legacy text line must be rebuilt.
All runtime/exporter pairs also require matching untracked-input counts and
fingerprints, including clean tracked trees, so a new uncommitted compiled file
cannot be overlooked. Android compares the same observations across Git native
ABIs and desktop tools.

Development exports may carry matching dirty provenance. A distributable release
requires known, clean `main` or `Experimental` provenance without observed
untracked build inputs. Channel identity does
not replace human acceptance.

Every exported desktop game includes `BUILD_PROVENANCE.json` containing exporter
and runtime identities and the SHA-256 of the copied runtime. Android replaces
desktop-runtime provenance with its own native ABI identities. See
[distribution manifests and verification](DISTRIBUTION.md) and
[Android build settings](ANDROID.md#build-identity-and-reproducibility).

## Update notification

The editor title identifies its version, channel and build. **View → Check this
channel for updates** checks GitHub's ancestry comparison for that exact source
revision and channel, including unnumbered Pre-Alpha commits. An older remote or
a diverged history is not called a newer build.

This is optional, read-only notification. It never installs, downloads or applies
an engine update and never writes the repository. No GitHub CLI or login is
required. The HTTPS request uses optional platform `curl` with certificate
validation, bounded time/output and no project code. Windows resolves only the
OS system-directory `curl.exe`, never a project-local executable. Missing utility, offline
operation, rate limits, unavailable commit history and oversized responses leave
the editor usable and report an unavailable check. Local/unknown builds cannot
claim a channel update result.

## Focused verification

```sh
python3 scripts/prealpha_build_identity_checks.py --output /tmp/build-identity-controls.json
cmake -S . -B /tmp/judas-identity -DCMAKE_BUILD_TYPE=Release -DJUDAS_BUILD_IDENTITY_ONLY=ON
cmake --build /tmp/judas-identity --target judas_build_identity_tests -j2
/tmp/judas-identity/judas_build_identity_tests
```

The dependency-light configuration requires a C++17 compiler, CMake and threads;
it does not configure SDL, ICU or a graphics context. Configuration controls use
Python and Git for independent clean/dirty/archive fixtures. Native checks cover
runtime mismatch, malformed data, update ancestry, missing/failed tools, excessive
output, timeouts and real child-process argument round trips (including Unicode,
quotes and trailing backslashes). Live HTTP service availability is not a release-test
requirement.

## Release policy

Keep artifact manifests/checksums tied to the validated candidate. Promote only
the exact operator-accepted Experimental checkpoint to `main`. Preserve any
outstanding platform, performance and human-validation status. If an artifact is
rebuilt for different branding/toolchains/channels, identify and validate that
new artifact rather than claiming byte identity with an earlier one.

Save format/content migration policy and network protocol compatibility remain
their existing independent contracts. A new engine build is not automatically
incompatible, and a matching version is not permission to ignore those contracts.
