# Prepare, qualify and publish Judas

[Identity/channels](VERSIONING.md) · [Alpha notes](ALPHA_V1_0_0_NOTES.md) ·
[Support matrix](ALPHA_SUPPORT_MATRIX.md) · [Linux baseline](LINUX_BASELINE_BUILD.md)

## Current boundary

**[Alpha v1.0.0 is published](https://github.com/SuperReaper1999/Project-Judas/releases/tag/v1.0.0-alpha.1)** after final operator approval on
27 October 2026. Tag `v1.0.0-alpha.1` and accepted main point to exact source
`636eec475b9955dcfd7d6d943d00c05d911135ad`. The verified Linux, Windows and source
packages and Alpha update manifest are live. These publication steps remain the
procedure for future releases; compilation, staging and preparation alone never
grant approval. Do not republish or replace the existing Alpha tag/assets.

The Git branches remain `Experimental` and operator-accepted `main`. Distribution
channels are Alpha, optional future Beta/Experimental, and eventual GA Stable.
Do not create a duplicate Stable branch. Do not publish Alpha as latest Stable.

The `0dab8e5d` candidate predates the advanced-fluid restriction; `31205c6a`
predates the accepted Basic Water editor-preview correction. Both are superseded.
The final published packages use `636eec47`; do not distribute older candidates
as the final Alpha. Sealed preparation receipts retain their original pending
approval wording as historical evidence; the public release records the later
operator approval. See [the release record](ALPHA_V1_0_0_NOTES.md#release-record).

## 1. Freeze a source candidate

Update `engine-version.json` once, then update curated release notes and support
boundaries. Commit intended source to Experimental; exclude research, games not
needed in an SDK, build outputs and unrelated local experiments. Generate a
factual changelog with `tools/generate_changelog.py --base ACCEPTED_BASE --head
CANDIDATE --output PATH`. Review notes rather than treating commit subjects as
validation claims. Keep protected historical evidence unchanged.

Create a tracked engine-source archive with `tools/package_source.py --output
OUTSIDE_SOURCE/source.tar.gz --channel Alpha`. When unrelated untracked
experiments exist, `--allow-untracked` explicitly excludes them and records their
count. Build provenance must retain the exact commit and clean/archive identity.
Never relabel older binaries to match a new candidate.

## 2. Build and qualify

Linux uses the established Ubuntu 24.04 complete build userland:

```sh
python3 tools/build_linux_baseline.py --rootfs PREPARED_UBUNTU24_ROOT --build FRESH_BUILD --channel Alpha --jobs 1
```

Configure `JUDAS_ENABLE_VALIDATION=ON` and `JUDAS_SDK_RELEASE=ON` in the baseline
namespace, then use `scripts/qualify_m100.py` for one coordinated authoritative
pass. This runs the registered native gates plus joints, vehicles, gravity and
swimming. It preserves known-failure classification; it does not silently waive
new failures. Alpha deliberately excludes the advanced-fluid RuntimeWorld tests:
the feature is unavailable, and `unavailableTests` records those cases as
**not-exercised**, not passed. Original tests remain intact for Experimental;
Alpha instead exercises refusal, inactive ownership, Basic Water and swimming.
Direct geometry/storage checks remain in scope. See
[the restriction](M100_FLUID_CONTACT_STABILITY.md). Run relevant API/type and
tooling controls too.

Run `scripts/qualify_sdk.py --sdk SDK --work FRESH_WORK --rootfs
PREPARED_UBUNTU24_ROOT`. It mounts only the SDK for project/editor/tool work and
only each moved game for standalone smoke. Frozen test fixtures make the tool
workflows reproducible from engine-only source archives. No deleted /tmp script
is required. All 17 Linux tools have actual representative operations, not merely
version probes. Positive/negative script and collision controls are included.

Windows uses the established native MSVC 2022 x64 workflow:

```powershell
./packaging/windows/Build-Windows.ps1 -Channel Alpha -ReleasePackage -Jobs 2
```

Use `scripts/qualify_m100.py --build BUILD --configuration Release --output
FRESH_RESULTS` and `scripts/qualify_sdk.py --sdk SDK --work FRESH_WORK` in an
appropriate desktop session. The latter accepts `--mesa TEST_DRIVER_DIRECTORY`
for **disposable VM test copies only**; stock SDK/exports remain free of that
software-driver overlay. VM functionality, hosted-CI checks, previous human
Windows 11 acceptance and clean-machine/hardware qualification are separate.
Do not run memory-intensive local Linux and VM builds simultaneously.

The manual **Prepare release candidate** Action is contents-read-only. It builds
Linux and MSVC packages for an exact commit. Hosted Windows CI records its limited
native/SDK scope and does not claim a desktop/GPU trial. Operator desktop/VM
qualification is still required before final acceptance. Use the same exact
artifacts for human review and publication; a later CI rebuild with different
hashes is a new candidate, not an approved replacement.

## 3. Prepare local candidate artifacts

`tools/prepare_release.py` requires both verified SDKs, matching exact source /
version/channel, a matching tracked source archive, notes and a successful
exact-source qualification receipt. Example:

```sh
python3 tools/prepare_release.py --linux-sdk LINUX_SDK --windows-sdk WINDOWS_SDK --source-archive source.tar.gz --notes docs/ALPHA_V1_0_0_NOTES.md --qualification QUALIFICATION.json --epoch SOURCE_COMMIT_EPOCH --output NEW_CANDIDATE_DIRECTORY
```

This creates versioned Linux `.tar.gz`, Windows `.zip` and source `.tar.gz`,
`release-manifest.json`, `RELEASE_NOTES.md`, `QUALIFICATION.json`,
`CANDIDATE.json` (awaiting approval) and `SHA256SUMS`. Archive ordering, timestamps
and ownership are deterministic for identical inputs and the source epoch. This
is not a claim that arbitrary compilers/linkers produce bit-identical binaries.
No previous output is silently replaced; incomplete preparation remains labelled.

Verify local bytes with `tools/release_manifest.py verify MANIFEST DOWNLOAD`.
Give the operator the commit, channel, artifact hashes, actual platform receipts,
known limits and human checklist. Stop for final approval.

## 4. Only after explicit final approval

1. Fast-forward main to the **exact accepted source candidate**, following
   AGENTS.md. Stop on divergence; never reset or force-push.
2. Configure GitHub environment **judas-publication** with required human reviewers.
   The publisher checks that protection exists; naming an unprotected environment
   is insufficient. GitHub settings are not changed by these source files.
3. Once, explicitly provision the metadata-only `update-manifests` prerelease on
   this repository after approval. Do not upload Stable.json during Alpha.
4. Validate the already-reviewed files with `tools/publish_release.py CANDIDATE
   --accepted-commit EXACT_SHA` (dry-run; no network/publication).
5. Only when authorized, add `--publish --human-approval 'PUBLISH
   v1.0.0-alpha.1'`. This checks remote main/commit and required reviewer protection,
   refuses an existing final tag, creates a draft prerelease, uploads the exact
   files, downloads and verifies every artifact, then publishes. It never
   overwrites final release artifacts. It updates Alpha.json **after** verification.
6. Alternatively, use **Publish explicitly accepted release** for the exact
   successfully prepared CI run, then approve its protected environment. That
   workflow validates run origin, exact SHA and preparation-workflow identity.
   Its artifacts must themselves have received final approval; do not substitute
   them for different locally approved binaries.
7. Recheck immutable downloads and the live trusted channel manifest, then test a
   manual editor notification. Record the actual tag/release/manifest and source
   identities. Interrupted draft/upload failures remain diagnosable; do not
   bypass guards or clobber the existing release to force a retry.

Before approval, none of steps 1–7 is authorized by M100 implementation alone.
The publication scripts contain no installer, project updater or version-control
synchronization for developers. Offline users continue using their installed SDK.

## Compatibility and future updates

Use new SemVer prerelease versions for Alpha/Beta updates. Reserve plain versions
and Stable manifests for GA. Save/content/mod/network compatibility still uses
its explicit project/engine contracts, with backups and deliberate migrations.
Update the platform matrix from actual evidence each time. Historical receipts
are never rewritten as current validation.
