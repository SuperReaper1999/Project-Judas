# M70 captured floor-contact freeze: numerical replay

This compact fixture comes from the actual human-reproduced wall-scene freeze after **G / full active**. The application was force-closed externally; it did not throw an exception. Its first articulation processed 6,638 CCD events while advancing only from 9.147 to 11.500 microseconds of one 16.667 ms fixed step. The repeated pair was body slots 8/12. Quaternion normalization alternated two binary32 representations while the angular increment could not change the represented orientation.

`fixture.json` preserves the exact native post-velocity-solve anchors, inverse mass/inertia, ordinary shapes, filters, 30 joints and adjacent suppression. The original start-contact rows restore the initial support classification. No new gravity or drive force is added: that work is already represented in the captured velocities.

`probe.cpp` is a **test-only numerical diagnostic**, using private access only in its own translation unit to invoke authoritative `AdvanceImpacts`. Repeating the ordinary velocity solve through public `Step` changes the captured state and masks this exact case. It does not add private hooks or bindings to shipping Judas. The event joint solver resets its impulses, so its prior warm cache is unnecessary here.

## Recorded result

- Original sampler: bounded 35-second run **timed out**, still processing pair 8/12; last progress row had 5,376 events / 473,097 queries. This is a lower-bound runtime, not a completed timing.
- Corrected sampler: **29.343 ms**, 18 events / 1,684 queries / 320 motion segments; zero sampling fallbacks or event caps.
- Geometry, CCD budgets, cadence and physical material/filter policy were unchanged between these two runs. The header correction preserves an anchor quaternion when its binary32 angular update is unchanged and uses logarithmic ledger lookup with the original earliest-segment boundary choice.

## Reproduction

This Linux Release diagnostic reuses the existing `judas_character_lab_performance` compiler/link configuration and library cohort. It compiles a temporary copy of `PhysicsWorld.cpp` with progress logging, shadows `RigidMotion.h`, and leaves source/build targets untouched. Use fresh output directories:

```sh
python3 docs/evidence/m70/development/passive-crash-follow-up/replay/reproduce.py --output .cache/m70-review/replay-corrected
# Optional original-failure reproduction; deliberately bounded to 35 seconds:
python3 docs/evidence/m70/development/passive-crash-follow-up/replay/reproduce.py baseline --output .cache/m70-review/replay-original
```

The baseline uses the original header from checkpoint `934c5d3f0556c920cc7cae8b80dc4677d8cbf87b`; its expected bounded timeout exits 124. The default corrected variant uses the current header. The preserved recipe itself was verified separately: exit 0, 29.937 ms, the same 18 events / 1,684 queries / 320 motion segments and zero caps/fallbacks. `receipt.json` records hashes and exit/timeout state; `run.log` retains the last progress row. The core dump, generated full source copies, objects and binaries remain outside tracked evidence. Normal public motion/impact/contact/joint and M70 lifecycle tests cover shipping behavior separately.
