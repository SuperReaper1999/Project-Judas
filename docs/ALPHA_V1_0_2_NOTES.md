# Judas Engine Alpha v1.0.2

Machine version: **`1.0.2-alpha.1`**. This is an Alpha patch, not Stable/GA.
The operator accepted the rain correction and authorized promotion/publication
on 29 October 2026. Exact source and artifact identities are recorded in the
release manifest, SDK `BUILD_INFO.json` and checksums. Preparation is not
publication: the release and Alpha channel manifest must both be live.

## Corrections

- Rain streaks and splashes remain visible against dim overcast/storm skies.
  Lit highlights add light instead of replacing the sky with similarly coloured
  opaque albedo. There is no emissive floor or whole-scene brightness increase.
- Rain preserves HDR target alpha. Snow and tornado debris retain normal alpha
  coverage. Depth/shelter rejection, bounded particle counts, wind, arbitrary-up
  behaviour, dusk/night sky lighting and lightning remain in place.
- Native SDKs include the optional physical-constraint participant interface
  developed on Experimental for future material coupling. It validates proposed
  impulses against native constraints before publication; it is not an enabled
  fluid backend, new gameplay controller or JudasJS binding. Existing projects
  have no participant unless explicitly supplied by native code. Rebuild native
  integrations against matching headers/libraries; Alpha binary ABI compatibility
  across SDKs is not promised.

## Evidence and support boundaries

The rain correction passed **280 focused Linux checks** across lighting,
rendering, weather, world ownership and scripting/persistence. The original
six-check visibility failure and a three-check intermediate HDR-alpha failure
are retained. No original threshold was relaxed.

The supplied original and v1.0.1 Skyhaven Flight projects run without game edits.
Tests use real hangar input for squalls/storm/night, actual editor Play/Stop,
and an exported package moved to an unrelated `/tmp` working directory. Its
captures match the development build exactly. Rendering evidence used Mesa
llvmpipe software OpenGL; this is not new physical-GPU/controller/audio or
Windows hardware certification. Exact-source release preparation records its
additional Linux SDK/workflow and hosted Windows MSVC checks separately.

See [the correction and evidence](ISSUE_20_RAIN_CONTRAST.txt),
[platform/support matrix](ALPHA_SUPPORT_MATRIX.md),
[release procedure](RELEASING.md) and [version policy](VERSIONING.md).

**Advanced Fluid Dynamics remains compiled-locked and unavailable in Alpha.**
Basic Water and swimming remain independent. Private numerical fluid research
and its archived experiments are not a production backend or a fluid enable
switch. Moving geometry, free surfaces, wet/dry evolution, practical conditioning
and end-to-end qualification remain outstanding. This patch does not unlock it.
Previous published Alpha tags and artifacts are unchanged.

## Downloads and updates

The publication delivers Linux x86-64 SDK, Windows x86-64 SDK, tracked engine
source, release manifest and SHA-256 checksums under
[`v1.0.2-alpha.1`](https://github.com/SuperReaper1999/Project-Judas/releases/tag/v1.0.2-alpha.1).
Verify downloads before extraction. Alpha-channel editors can notify about the
newer version; nothing installs automatically or overwrites a project. Android
retains its documented qualification; no new Android package trial is claimed.
