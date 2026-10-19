# Hearthworld consumer-game evidence

Final combined native result: **169 assertions, zero failures**. See
[measured validation](VALIDATION_SUMMARY.md), [cold hitch classification](HITCH_CLASSIFICATION.json),
[delivery receipts](delivery-RESULTS.json), and the
[game report](../../HEARTHWORLD_SURVIVAL_SANDBOX.md).

`attempt-01` through `attempt-03` retain unsuccessful qualification/oracle runs;
`attempt-04` is the passing combined source. Native raw logs, CSV, M56 data,
exact final observer source, save bytes and before/capture/restore dumps remain.
Original failed observer source was not snapshotted; recorded hashes do not
pretend it was. The authoring receipt predates final colour-only scene settings
and is historical, not the final source fingerprint.

The final project README additionally corrects a tooling distribution sentence:
runtime-only exports do not ship the repository declaration tree. No executable
game script, asset, scene, physics setting or test changed after the passing run.
Runtime/exporter provenance records the tested staged development source; it is
not relabelled as a clean post-commit binary. Standalone content and runtime hashes
are independently recorded in `delivery-PACKAGE_FILES.json`.

## Preserved development observations

1. The first high-resolution soil source met the sample budget but exceeded the
   existing runtime triangle budget. Authoring chose a coarser source with room
   for actual excavation; neither runtime limit was increased. The equivalent
   reconstructed over-budget source is labelled as reconstructed, not the lost
   original bytes. Raw cook counts and final source hashes remain distinct.
2. First native gameplay run: 56 checks passed. The cold read rejected changed
   content correctly because an authoring agent had rewritten scene/metadata
   during the run. This is preserved as a qualification failure, not an engine bug.
3. The next native run exposed an invalid observer assumption: a save request can
   wait for content identity/preparation while normal simulation continues.
   A pre-request endpoint is not the eventual captured endpoint. Independent
   dumps established exact agreement between the real capture boundary and
   fresh restoration. Pending script state is inspected only after its normal
   lifecycle synchronization; physical state is still inspected before its first
   restored simulation step. Tolerances and engine behaviour were not weakened.
4. Native image review caught legacy scene rendering hiding imported material
   colours and unsupported planet vertex colours. Existing linear rendering and
   a normal instance grass tint correct content selection. Some shadow banding
   remains an honestly documented presentation limitation.

No protected evidence, engine physics, fluid research or existing game source
was modified for these content and verification corrections.
