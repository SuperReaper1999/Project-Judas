# Playable combined acceptance copy

The frozen combined performance fixture uses an elevated overview to expose the
whole crowd. It is not an appropriate first-person terrain-editing setup.

Create a separate playable copy with the packaged SDK's existing APIs:

```sh
python3 tools/benchmarks/prepare_playable_combined.py --source FROZEN_COMBINED --out NEW_PROJECT
node tests/PlayableCombinedProjectTests.mjs
```

The helper preserves frozen inputs and authors a normal eye-level spawn on the
actual courtyard. The script obtains its edit ray from the actual camera, rather
than duplicating a ray from a different physical position. Its character-capsule
placement guard uses the fixture's authored radius, half-height, offset and skin,
plus the sampled terrain cell diagonal. It rechecks queued additions immediately
before submitting them. Rejected placement displays an explanation; no motor is
teleported, frozen or given a velocity reset. Building away from characters,
painting and excavation remain available. Copies receive a separate save identity.

This is project-owned placement policy, not a general engine solution for a
character already enclosed inside newly created solid terrain. Removing the ground
under a character can still legitimately make it fall. The helper rejects parented
or scaled character fixtures until their placement representation is reviewed.
Changing authored motor dimensions later requires updating the generated capsule
configuration. This does not alter the elevated, frozen timing inputs or qualify
new performance results against the old input hashes.

Desktop packaged-SDK controls reproduced the original downward-build fall, then
verified rejected close placement with sustained support, valid distant addition,
painting, digging and genuine process-exit save/load. Native projection of the
edit points was within 0.000001 of normalized screen centre on both axes.
