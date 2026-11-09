# Outstanding work and support boundaries

Reconciled during the architectural purge against current source. This is not a
claim of new human acceptance or platform certification. Publication is recorded
separately in [release status](RELEASE_STATUS.json).

## Current decisions

- Advanced Fluids V2 passed operator physical and visual review on Experimental.
  The old PBF and microscopic conservative research is historical, not pending
  production implementation. The Alpha compiled lock remains unchanged.
- Terrain V2 passed hands-on review; standalone performance is good. Editor
  performance is an accepted future optimisation opportunity, not a blocker.
- Procedural humanoid recovery remains explicitly parked after visual rejection;
  do not claim natural get-up or balance from the existence of controller APIs.
- This architectural purge awaits its own human review after automated checks.
- Windows/Android hardware acceptance of older builds retains its original scope;
  it cannot automatically certify the changed purge checkpoint.

See [current findings](CURRENT_FINDINGS.md), [purge report](ARCHITECTURAL_PURGE.md),
[water contract](ADVANCED_FLUIDS_V2.md) and [support matrix](ALPHA_SUPPORT_MATRIX.md).

## Deliberate extension areas

These are declared boundaries, rather than claims of newly reproduced defects.

| Area | Remaining boundary | Reference |
| --- | --- | --- |
| Networking | Broader assembly/terrain replication, streamed/modded roots, public-server security and browser transport. | [Networking](NETWORKING.md), [runtime contract](judasjs/network.md) |
| Persistence/suspension | Arbitrary topology migration, merging newly authored worlds, all-assembly suspension and missing historical state. | [Save evolution](SAVE_EVOLUTION.md), [living worlds (historical)](https://github.com/SuperReaper1999/Project-Judas/blob/2ee4203a0053cc545eab4a184c627f4f5507c4bd/docs/M97_LIVING_WORLD.md) |
| Large worlds | Multiple distant active physics charts and excavation across every terrain representation. | [Assembly travel](ASSEMBLY_TRAVEL.md) |
| Rendering/residency | Point shadows, cascaded sunlight, universal instancing, per-mip streaming and hard total-memory guarantees. | [Rendering](judasjs/rendering.md), [asset pipeline](ASSET_PIPELINE.md) |
| Broader API controls | Full wheel geometry/query configuration, pairwise collision/query overrides and runtime render LOD policy require ownership/persistence rules. | [Wheels](judasjs/wheels.md), [physics](judasjs/physics.md), [LOD scope](ASSET_PIPELINE.md) |
| Animation/text/input | Morph/cache deformation, bone-key editing, richer text caret behaviour, raw independent keyboards/mice and OS screen-reader integration. | [Import](MODEL_IMPORT.md), [UI (historical)](https://github.com/SuperReaper1999/Project-Judas/blob/2ee4203a0053cc545eab4a184c627f4f5507c4bd/docs/M96_UI_TOOLKIT.md), [local players](LOCAL_MULTIPLAYER.md) |
| Browser productization | Context restoration, wider browser/storage qualification, production hosting, residency, memory and audio modernization. | [Browser scope](BROWSER_SUPPORT.md), [emulator limits](BROWSER_ANDROID_EMULATOR_QUALIFICATION.md) |
| MK2 bounds | Arrival holds, extreme-spin phase error, residual elastic dissipation and content-dependent dense-contact cost. | [MK2 contract](MK2_PHYSICS_ARCHITECTURE.md), [rotation evidence (historical)](https://github.com/SuperReaper1999/Project-Judas/blob/2ee4203a0053cc545eab4a184c627f4f5507c4bd/docs/PREALPHA_MK2_ROTATION_CORRECTION.md) |

## Closed or superseded reports

The earlier MK1 CCD/event architecture, M78 motor/save lifecycle defects, M65
dependency/inert-world costs, old importer palette limits, browser shadow and
submission-growth bugs, pointer/orientation/resize/weather issues and the API
readback/navigation/tilemap pass have later corrections. Do not reopen them solely
from an old handoff. Scope changes and new reproductions should retain their own
evidence.

## Keeping this register current

When closing or changing an item, update its status and evidence in the same
checkpoint as the correction. Preserve current regression coverage; retired contracts and historical evidence remain in Git history. A source inspection, symbol count or release build does not supply
missing behavioural or human validation. Follow the [maintenance checklist](DOCUMENTATION_MAINTENANCE.md).
