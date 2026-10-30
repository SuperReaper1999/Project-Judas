# Judas Engine Alpha v1.0.3

Machine version: **`1.0.3-alpha.1`**. Alpha, not Stable/GA.
The operator authorized this patch on 30 October 2026. Exact-source Linux and Windows release preparation, protected publication
and package integrity verification have succeeded. Source and artifact identities belong in the release manifest and checksums.

## Corrections

- Point lights can omit the irrelevant `light.cone` authoring field. Ordinary
  component defaults remain available for canonical round-trips. Spot lights still
  require their meaningful cone; explicitly supplied values on either kind retain
  finite/range validation. Named and line-oriented scenes use the same loader.
- Android build identity errors now name mismatched fields and their values.
  Channel-only differences suggest the matching flag, such as `--channel Alpha`,
  rather than only recommending a rebuild that would reproduce the same mismatch.
  The default remains deliberately Experimental; no channel is silently changed.
  Exact source/version/provenance validation is unchanged. Android documentation
  lists the current channels and explains Alpha-tool use.

The original point-light loader fails the new regression, the correction passes
both formats, and the full Linux scene suite passes 89 assertions. Seven Python
checks cover Android identities, including an actual builder entry path with
mocked SDK tools that rejects before native compilation or APK publication.
See [the detailed evidence](ISSUES_21_22_FOLLOWUP.txt). Hosted release preparation
qualifies the actual desktop distributions separately; the mocked test is not
new Android hardware or native-device qualification.

**Advanced Fluid Dynamics remains compiled-locked and unavailable in Alpha.**
Basic Water and swimming remain independent and available. Private pressure,
geometry and transport research is not a released backend or an enable switch.
Thin-cell conditioning, complete momentum evolution and moving/free-surface
integration remain unqualified. No fluid implementation is unlocked by this patch.

Existing platform/support limitations remain in the
[Alpha support matrix](ALPHA_SUPPORT_MATRIX.md). No new controller, physical GPU,
clean-machine or Android-device claim is made. Previous published tags and
packages remain unchanged. Updates notify only; nothing installs automatically.

## Release record

Published on 30 October 2026 after explicit operator acceptance of exact candidate
`f2813324155166c6942fd68dde843d9775eb532d`, identified by immutable release tag
[`v1.0.3-alpha.1`](https://github.com/SuperReaper1999/Project-Judas/releases/tag/v1.0.3-alpha.1).

- [Preparation](https://github.com/SuperReaper1999/Project-Judas/actions/runs/40977408836):
  Ubuntu 24.04 qualification, actual Linux SDK/editor/export workflows, native
  Windows MSVC SDK/tools/selected contracts. Both jobs passed.
- [Publication](https://github.com/SuperReaper1999/Project-Judas/actions/runs/40981128911):
  protected approval, exact prepared artifacts, payload downloads/checksum
  verification and Alpha feed update passed.
- Live Linux/Windows/source asset digests match the release manifest; the Alpha
  feed is byte-identical to that manifest. Source commit, channel and version match.
- [Immutable publication receipts](evidence/alpha-v1.0.3-20261030/). No new hardware
  acceptance is claimed. Earlier tags and packages remain unchanged.
