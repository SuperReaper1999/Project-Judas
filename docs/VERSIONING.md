# Versions, maturity and update channels

<!-- judas-release-status:start -->
Source identity is **Judas Engine Alpha v2.0.0** (`2.0.0-alpha.1`).
[Source notes](ALPHA_V2_0_0_NOTES.md) · [Published packages](https://github.com/SuperReaper1999/Project-Judas/releases) ·
[Support boundaries](ALPHA_SUPPORT_MATRIX.md).

Source identity and publication are separate. The
[publication record](RELEASE_STATUS.json) is a dated receipt.
Check the release page for subsequently published packages.
<!-- judas-release-status:end -->

## Version ordering

Judas follows semantic version precedence: `1.0.0-alpha.1` < `1.0.0-alpha.2` <
`1.0.0-beta.1` < `1.0.0-rc.1` < `1.0.0`. Numeric prerelease components sort
numerically. Build metadata after `+` identifies builds but does not imply an
update. Future published Experimental builds must advance their prerelease
version if they are to be offered as newer; differing commits alone are not a
version ordering. No promise of Pre-Alpha project compatibility is made.

| Identity | Purpose |
|---|---|
| `version` | SemVer, used for version comparison and release tags |
| `maturity` | `alpha`, `beta`, `rc`, or eventual `stable`; independent of build configuration |
| `release_name` | Human-readable title from the same authoritative file |
| `channel` | Distribution stream; not the Git branch name or proof of testing |
| `source_revision` / tree / fingerprint | Exact Git or verified source-archive provenance |
| `configuration`, compiler, platform, architecture | Actual build environment |
| `source_dirty`, untracked input observations | Honest development provenance |

## Channels and branches

- **Alpha:** operator-accepted, published Alpha builds. It is not Stable.
- **Beta:** reserved for future published Beta builds.
- **Experimental:** ongoing development and published test versions; explicit
  opt-in for Alpha users.
- **Stable:** reserved for GA. Alpha/Beta maturity cannot be staged as Stable.
- **Local:** unknown/custom development provenance; not release eligible.

The repository still uses `Experimental` for engineering and `main` for
human-accepted source. A main build selects Alpha by default during this release
line; that does not publish anything or certify a fresh package. Explicit CMake
`-DJUDAS_BUILD_CHANNEL=...` overrides let one exact checkpoint produce private
channel-specific artifacts. Never overwrite artifacts with a different identity.
The old Pre-Alpha build-channel name `main` is intentionally retired.

`--version`, `--build-info-json`, SDK `BUILD_INFO.json`, exported package metadata
and `engine.buildInfo` in JudasJS read the same embedded identity. Android keeps
its application's versionCode/versionName distinct and also embeds engine build
provenance; changing application branding does not change the engine's version.

## Updates

Help → Check this channel for updates is manual. Editor preferences → Updates
controls startup checking and channel choice, persisted outside game projects.
The default follows the installed build; Alpha never implicitly opts into
Experimental. Checks are asynchronous, HTTPS-only, bounded in time/size, and
optional when curl is unavailable. Offline, absent initial manifests and malformed
responses produce diagnostics, not application failure.

The authoritative endpoint is the corresponding `Alpha.json`, `Beta.json`,
`Experimental.json` or `Stable.json` asset on this repository's `update-manifests`
GitHub release. Nothing is hosted/published there by merely building this source.
The [Alpha manifest](https://github.com/SuperReaper1999/Project-Judas/releases/download/update-manifests/Alpha.json)
is live for this release; Beta, Experimental and Stable manifests have not been
published. Future publication follows [the release process](RELEASING.md).

Metadata requires its exact channel, valid SemVer, source commit, protocol/major
compatibility, bounded notes, repository-owned versioned URLs, artifact sizes and
SHA-256 values. Wrong channels, incompatible majors, unknown platforms and corrupt
metadata do not become update offers. The notification shows installed versus
available versions, notes and a download link. **Nothing is silently installed.**
Opening a browser download cannot attest its resulting local bytes. Verify them
before extraction with the packaged, standard-library-only tool:

```sh
python3 tools/release_manifest.py check --channel Alpha --installed 1.0.0-alpha.1 --platform Linux
python3 tools/release_manifest.py verify release-manifest.json JudasEngine-VERSION-Alpha-Linux-x86_64.tar.gz
```

Existing Windows packages remain historical artifacts. New official releases
contain a Linux SDK and source archive. Download the immutable
`release-manifest.json` from the same versioned release. The helper refuses
size/hash/name mismatches and performs no installation. Checksums protect content
integrity, not compromise of the repository/account or a maliciously replaced
manifest. HTTPS and operator-controlled GitHub publication are the trust root;
independent cryptographic artifact signing is not claimed.

## Compatibility

Engine version, project content identity, save migrations, network protocol and
mod compatibility are separate contracts. Minor/patch engine changes are not
permission to silently discard unknown save data. Project-controlled migrations
remain explicit. See [support boundaries](ALPHA_SUPPORT_MATRIX.md) and
[save evolution](judasjs/saves.md). Back up Pre-Alpha projects before opening them.
