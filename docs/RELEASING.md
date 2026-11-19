# Prepare, qualify and publish Judas

<!-- judas-release-status:start -->
Source identity is **Judas Engine Alpha v2.0.0** (`2.0.0-alpha.1`).
[Source notes](ALPHA_V2_0_0_NOTES.md) · [Published packages](https://github.com/SuperReaper1999/Project-Judas/releases) ·
[Support boundaries](ALPHA_SUPPORT_MATRIX.md).

Verified publication on **2026-11-04** is
[`v1.2.5-alpha.1`](https://github.com/SuperReaper1999/Project-Judas/releases/tag/v1.2.5-alpha.1) from
`a1f363632582da2855b84f9adc79cfc6cf0521fa`.
[Linux/Windows qualification](https://github.com/SuperReaper1999/Project-Judas/actions/runs/41753879925)
and [protected publication](https://github.com/SuperReaper1999/Project-Judas/actions/runs/41757770903) passed.
The published packages and Alpha feed were verified. Native Windows CI
is functional evidence. Earlier Windows hardware and Android phone
acceptance keep their recorded scopes.

Source identity and publication are separate. The
[publication record](RELEASE_STATUS.json) is a dated receipt.
Check the release page for subsequently published packages.
<!-- judas-release-status:end -->

## 1. Freeze a source candidate

Update `engine-version.json` once, then update curated release notes and support
boundaries. Commit intended source to Experimental; exclude research, games not
needed in an SDK, build outputs and unrelated local experiments. Generate a
factual changelog with `tools/generate_changelog.py --base ACCEPTED_BASE --head
CANDIDATE --output PATH`. Review notes rather than treating commit subjects as
validation claims. Retain required current qualification/provenance receipts; superseded documentation belongs in Git history.

Before freezing the candidate, refresh the maintained summaries and verify them:

```sh
python3 tools/release_docs.py --write
python3 scripts/check_current_docs.py --require-tracked
```

Curated source notes must exist for the new engine version. The dated
`docs/RELEASE_STATUS.json` publication receipt may still name the previous
published version. Record a new receipt only after successful publication and
verification, then refresh again on Experimental. See
[documentation maintenance](DOCUMENTATION_MAINTENANCE.md#source-publication-and-contributor-authority).

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

Run `scripts/qualify_sdk.py --sdk SDK --work FRESH_WORK --rootfs
PREPARED_UBUNTU24_ROOT`. It mounts only the SDK for project/editor/tool work and
only each moved game for standalone smoke. Frozen test fixtures make the tool
workflows reproducible from engine-only source archives. No deleted /tmp script
is required. All 15 common Linux tools have actual representative operations, not merely
version probes. Positive/negative script and collision controls are included.

The **Prepare Linux release candidate** workflow has read-only repository
permissions. It builds the SDK and source archive for the exact Experimental
commit. Windows is unsupported and excluded from official preparation and
publication. Use the same exact artifacts for human review and publication.

## 3. Prepare local candidate artifacts

`tools/prepare_release.py` requires a verified Linux x86-64 SDK, matching exact source /
version/channel, a matching tracked source archive, notes and a successful
exact-source qualification receipt. Example:

```sh
python3 tools/prepare_release.py --linux-sdk LINUX_SDK --source-archive source.tar.gz --notes PATH_TO_CURRENT_VERSION_NOTES --qualification QUALIFICATION.json --epoch SOURCE_COMMIT_EPOCH --output NEW_CANDIDATE_DIRECTORY
```

This creates versioned Linux SDK and source `.tar.gz`,
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
5. Only when authorized, add `--publish --human-approval 'PUBLISH EXACT_TAG'`,
   replacing `EXACT_TAG` with the tag from the verified candidate manifest.
   This checks remote main/commit and required reviewer protection,
   refuses an existing final tag, creates a draft prerelease, uploads the exact
   files, downloads and verifies every artifact, then publishes. It never
   overwrites final release artifacts. It updates Alpha.json **after** verification.
6. Alternatively, use **Publish explicitly accepted Linux release** for the exact
   successfully prepared CI run, then approve its protected environment. Dispatch
   the publication workflow from accepted `main` (`gh workflow run ... --ref main`)
   when the environment permits protected branches only; an Experimental dispatch
   is rejected before the publication job starts. Do not relax that protection.
   The workflow validates run origin, exact SHA and preparation-workflow identity.
   Its artifacts must themselves have received final approval; do not substitute
   them for different locally approved binaries.
7. Recheck immutable downloads and the live trusted channel manifest, then test a
   manual editor notification. Record the actual tag/release/manifest and source
   identities. Interrupted draft/upload failures remain diagnosable; do not
   bypass guards or clobber the existing release to force a retry.

Before explicit publication approval, none of steps 1–7 is authorized merely by a passing build.
The publication scripts contain no installer, project updater or version-control
synchronization for developers. Offline users continue using their installed SDK.

## Compatibility and future updates

Use new SemVer prerelease versions for Alpha/Beta updates. Reserve plain versions
and Stable manifests for GA. Save/content/mod/network compatibility still uses
its explicit project/engine contracts, with backups and deliberate migrations.
Update the platform matrix from actual evidence each time. Historical receipts
are never rewritten as current validation.
