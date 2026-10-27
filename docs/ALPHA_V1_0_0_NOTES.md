# Judas Engine Alpha v1.0.0

Machine identity: **1.0.0-alpha.1**. Release candidate; not yet published or finally
approved. Copyright (c) 2026 Conner / SuperReaper1999. MIT licence unchanged.

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
these notes alone are not a test result. Final publication remains the operator's
explicit decision.
