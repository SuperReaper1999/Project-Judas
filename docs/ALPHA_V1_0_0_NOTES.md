# Judas Engine Alpha v1.0.0

Machine identity: **1.0.0-alpha.1**. [Published release](https://github.com/SuperReaper1999/Project-Judas/releases/tag/v1.0.0-alpha.1),
finally approved on 27 October 2026 at `636eec475b9955dcfd7d6d943d00c05d911135ad`. Copyright (c) 2026 Conner / SuperReaper1999. MIT licence unchanged.

This is the first versioned Alpha release line, following the 99-milestone engine
and accepted Pre-Alpha stabilization. It combines optional MK2 physics, arbitrary
gravity, characters, joints, wheels/powertrains, physical structural/deformation
mechanisms, planetary/local terrain, streaming, persistence/migrations, rendering,
weather, animation, UI, audio, JudasJS and scoped local/online multiplayer.
Capabilities retain their documented support boundaries; a common version number
does not certify every possible combination.

## New release/developer infrastructure

- One authoritative version/maturity/title, embedded build/channel/source
  provenance across editor, CLI, SDK, exports, Android metadata and JudasJS.
- Published-version update notifications with explicit channel selection, optional
  startup checks, release notes and download locations. No automatic installation.
- Strict release manifest validation and local download size/SHA-256 verification.
- Deterministic archive preparation, checksums, source-archive provenance and
  separate qualification/publication workflows with final human approval barriers.
- Portable actual SDK tool/project/export qualification rather than disposable
  scripts left in /tmp. Existing public-tool inventories remain authoritative.
- Includes the newer Runtime Inspector, ownership/readiness teaching project and
  preserved Lunar Island generated-audio work already on Experimental. Their
  individual human review status is not inferred from release automation.

## Installation and scope

Linux x86-64 uses the Ubuntu 24.04-compatible SDK build/userland. Windows x64 uses
native MSVC with its required runtime DLLs and redistribution files. Extract into
a writable development location, verify checksums and open `judas_editor` (or
`judas_editor.exe`). Python 3.11+ is required for SDK verification/development
helpers, not to run an exported native game. See [installation](DISTRIBUTION.md),
[first project](FIRST_PROJECT.md), [Windows](WINDOWS.md), [channels](VERSIONING.md)
and the [support matrix](ALPHA_SUPPORT_MATRIX.md).

Android is an existing, separately qualified runtime/export path with earlier
phone acceptance, not a fresh M100 hardware/emulator certification or desktop
editor distribution. macOS, iOS, consoles and a browser runtime are unsupported.

## Known Alpha limitations

- **Advanced Fluid Dynamics is disabled in Alpha due to contact stability issues.**
  Alpha blocks activation, Play and export of projects requesting it. Basic Water
  and swimming remain available independently. The implementation is retained in
  Experimental/local builds for a future validated patch; this is not a numerical
  repair. FLU-001 and half-turn sampling remain deferred. See the
  [investigation and restriction](M100_FLUID_CONTACT_STABILITY.md).
- Physics has bounded CCD/fallback behaviour and content-dependent performance.
  No universal dense-pile frame-budget or industrial deformation guarantee.
- Streaming, origin shifts, regional suspension, saves/mod migrations and online
  replication support their declared participant subsets and reject unsupported
  combinations. Networking is not a secure managed public-Internet service.
- Historical saves lacking necessary physical continuation data cannot be exactly
  reconstructed. Pre-Alpha project compatibility is not guaranteed.
- Current Windows VM tests are functional/software-graphics evidence, distinct
  from previous accepted Windows 11 hardware evidence and clean-machine testing.
  Controller/audio hardware coverage remains exactly as documented.
- No universal clean-machine/GPU/driver certification, automatic updater, binary
  signature chain or full per-member API certification is claimed.

Actual exact-candidate qualification receipts accompany the prepared artifacts;
these notes alone are not a test result. Final publication followed explicit
operator approval. Sealed package notes/receipts retain their preparation-time
status; see [the release record](ALPHA_V1_0_0_NOTES.md#release-record) for the closeout and
preserved Windows helper caveat.

## Additional Android human verification

The operator confirms making and playing multiple real Judas Android games.
Exact revisions/devices were not supplied for that additional report; no fresh
APK was built or tested during the desktop Alpha package refresh. This does not
infer controller support or clean-machine certification.

## Release record

**Judas Engine Alpha v1.0.0** (`1.0.0-alpha.1`) received explicit final operator
approval and was published on **27 October 2026**.

- [Release, packages and checksums](https://github.com/SuperReaper1999/Project-Judas/releases/tag/v1.0.0-alpha.1)
- Source/tag target: `636eec475b9955dcfd7d6d943d00c05d911135ad`.
- Tag: `v1.0.0-alpha.1`. GitHub marks it as a prerelease; it is not Stable/GA.
- Main was fast-forwarded to that exact accepted source. Later documentation-only
  commits do not change the published source or relabel its binaries.
- [Alpha update manifest](https://github.com/SuperReaper1999/Project-Judas/releases/download/update-manifests/Alpha.json)
  is live. Beta, Experimental and Stable manifests were not published.

## Exact published packages

The publisher downloaded all three archives back and verified their size/SHA-256.
All eight uploaded files additionally matched GitHub's asset digests and the
approved local files. Public Alpha metadata matched the approved manifest through
both the Python checker and the editor's system-HTTPS transport. The current
version receives no update offer; an older compatible Alpha receives an offer.

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `JudasEngine-1.0.0-alpha.1-Alpha-Linux-x86_64.tar.gz` | 385767572 | `765996fe1d87bf57b6eaec8b7018de903d347904429285372cb11516be842b87` |
| `JudasEngine-1.0.0-alpha.1-Alpha-Windows-x86_64.zip` | 103819445 | `558dafacccea98ccc91c0121227c672cd7dfa6c477e2bae6e8f53218264db1ae` |
| `JudasEngine-1.0.0-alpha.1-Source.tar.gz` | 70577084 | `952e28f8df2287a289758b6324be00c99806a72c47c0420ca1ce41d48d8f77b5` |

## Qualification and retained limits

Fresh Linux SDKs used the Ubuntu 24.04 reference userland; native Windows SDKs
used MSVC in the Windows 10 VM. SDK tooling, project creation/reopening,
Play/Stop, cold saves, relocated exports, Basic Water/swimming, the editor water
preview and runtime rendering received focused checks at the exact release
source. Broader original-RC engine evidence retains its original source identity;
it is not presented as a full rerun at the final source.

One Windows readiness-helper invocation crashed with `0xC0000409`. The unchanged
helper subsequently passed 21 checks in both stock/disposable test copies and ten
consecutive stock-SDK controls. The failure is retained with cause unconfirmed;
this is not a flawless-Windows or clean-machine certification claim.

The operator human-tested the corrected Stillwater editor preview and finally
approved this release. The operator separately confirms making and playing
multiple Android games; exact revisions/devices were not specified for that
additional report. No fresh Android APK qualification accompanied the desktop
refresh, and no untested controller capabilities are inferred.

**Advanced Fluid Dynamics is compiled unavailable in Alpha** because of contact
stability issues. Basic Water/swimming remain independent. FLU-001, half-turn
sampling, historical missing-save continuation and bounded physics/performance
limits retain their documented status. See [the support matrix](ALPHA_SUPPORT_MATRIX.md).

The sealed `CANDIDATE.json`, qualification and original package notes intentionally
retain their preparation-time pending-approval state and original checksums. This
release record and the public release description record the subsequent operator
approval. Earlier `0dab8e5d` and `31205c6a` packages are superseded; do not distribute
them as the final Alpha.

## Publication versus CI

Publication used the explicitly approved local artifacts and the guarded
publisher, not a substitute CI rebuild. The Experimental engine and documentation
workflows passed for the exact release commit. Main's repeat runs have their own
[GitHub Actions results](https://github.com/SuperReaper1999/Project-Judas/actions).
The documentation gate verifies links/anchors, not the truth of every prose claim;
a green result does not replace operator acceptance or platform qualification.
