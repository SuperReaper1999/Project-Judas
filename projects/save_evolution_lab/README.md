# Save Evolution Lab

Three genuine project/content versions sharing one explicit save namespace:

| Project | Game state | F6 destination |
|---|---|---|
| `v1/save_evolution.judasproj` | Legacy `salvage` count | `old-progress` |
| `v2/save_evolution.judasproj` | `inventory.ore` | `v2-progress` |
| `v3/save_evolution.judasproj` | Inventory + chapter, updated appearance | `upgraded-progress` |
| `failure/save_evolution.judasproj` | Injected second-step test failure | No load publication |
| `incompatible/save_evolution.judasproj` | No approved old sources | Explicit refusal |

Open v1 in the editor, Play, press **C** to collect salvage, **F6** to save.
Stop and open v3, Play, press **F8** to see exact-only refusal, then **F7** to
migrate `old-progress`. It performs both ordered steps and preserves your count.
**F9** instead upgrades a v2 slot. **F6** writes a separate upgraded destination;
the original is protected. Editor and standalone saves use separate directories.

**WASD/mouse/Space** move/look/jump; **Click/Q** edit/change terrain tool; **B** build
physical blocks; **Delete** remove a prop or release a beam joint; **F3** travel to
the next quarry; **F1/F2** flat/rotated quarry; **E** throw a ball; **F4** shift origin;
**Escape** pause; **R** reload the authored world (does not delete disk saves).

The lab deliberately reuses M97's actual runtime terrain and regional suspension.
Leave a quarry and return after migration: the excavation is a persisted density
field and collider, not an inventory value pretending to be a world edit.
Regional scripts/geometry remain the same between these game versions.

The policy lives in Project settings → Save compatibility and migrations. Its
source hashes are recorded in `SOURCE_IDENTITIES.json`. Do not edit old project
versions after recording real save fixtures. To deliberately reauthor this lab's
allowlist after content changes, run `python3 tools/ConfigureSaveEvolutionLab.py`.
Validation never regenerates expected content identities.

[Contracts, tools, restrictions and human checklist](../../docs/M98_SAVE_EVOLUTION.md)
· [JudasJS saves](../../docs/judasjs/saves.md).
