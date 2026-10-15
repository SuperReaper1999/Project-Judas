# M87 final Linux cold-save continuation

These are native Linux functional results against the final engine archive
`1cb3e2a74bfb2d2e2d5fa9b14c79158eadd4b974b8036ae758f59a4e4c8a5b15`.
No historical source, original controller gain, timestep, physical threshold or
test fixture was edited. The two checks run writers and readers as separate
processes. Windows is not inferred from these results.

| Unchanged fixture | Writer | Fresh-process reader |
|---|---:|---:|
| M70 `CharacterLabTests.cpp` | 10 / 0 failures | 14 / 0 failures |
| M77 preserved current-physical-endpoint observer | 24 / 0 failures | 13 / 0 failures |

The M77 observer additionally continues the exact captured live world for its
original 720-step interval. Uninterrupted and cold-restored **current physical
endpoints are exactly equal** within this paired Linux run. Both require current
Succeeded progress, two actually supported feet, current rigid-body pose/velocity
and the original physical standing limits. The verdict does not merely inspect
the legacy cached Standing observation.

Current metrics in both endpoints: upright **0.998586118**, root height above
current support **1.361597657 m**, relative COM speed **0 m/s**, two supported feet,
and `currentPhysicalStanding=true`. Both saved snapshot files retain identical
SHA-256 bytes before and after the reader. These new-format results do not claim
retroactive reconstruction of the historical missing-state `a106…` snapshot.

## Reproduction

Build the current normal Release `judas_character_lab_tests` and
`judas_balance_lab_tests` targets first. On Linux, from the repository root:

```sh
# Preparation only: validates and copies the exact historical observer source,
# then derives commands from the current normal CMake compiler/link flags.
python3 scripts/m87_cold_validation.py --output /tmp/m87-cold-fresh

# Run only when other timed benchmarks/builds are idle. Requires desktop OpenGL.
python3 scripts/m87_cold_validation.py --run --output /tmp/m87-cold-fresh
```

The output directory must not already contain writer fixture projects. The runner
clears diagnostic replay/boundary environment variables, enables only the unchanged
observer's uninterrupted continuation recording, and links its explicit object
against the current archive. It does not change engine objects or replay commands.

Preserved observer source remains at
`docs/evidence/m78-cold-boundary/final-current-standing/FinalCurrentStandingObserver.cpp`,
SHA-256 `715dd4ae56bc5a8b9556379d95bb5b6e8f4402fd4a60515b69ba039f0b918e13`.
The runner verifies those exact bytes before compiling. Source/archive/executable
hashes, actual commands, timings and snapshot hashes are in `RECEIPT.json`.

Evidence here includes essential logs, the two captured snapshot byte streams and
actual endpoint JSON. Executables, objects, copied project assets and large
per-step diagnostic trace trees are intentionally kept out of the repository.
This is focused continuation coverage, not human acceptance or a full physics audit.
