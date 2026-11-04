# Outstanding work and support boundaries

Reviewed on 4 November 2026 against accepted runtime
`a1f363632582da2855b84f9adc79cfc6cf0521fa` and its source/evidence.
The published identity is recorded in [release status](RELEASE_STATUS.json).
This review searched engine/project documents, evidence reports and local
handoffs, reconciled the original 54 audit findings, and checked live issues.
It is a source/document review. Earlier measurements retain their original scope.

## Repairs and unfinished workflows

| Work | Status and next useful boundary | Evidence |
| --- | --- | --- |
| Production Advanced Fluids | Locked in Alpha. Quiet-water, pressure/support and occupied-volume diagnostics remain. Preserve dry-gap, buoyancy, player and rotated controls. | [Settling trials](FLUID_SETTLING_TRIALS_20261103.md), [volume diagnosis](FLUID_PRESSURE_OCCUPANCY_20261103.md) |
| Conservative cut-cell replacement | Prototype passes bounded controls; both original basins reject before a complete first step. Shared interfaces, phase/donor limits, changing apertures and native coupling remain. | [Latest pressure/transport results](evidence/fluid-moving-grid-20261103/hydroacoustic-coupling/README.md) |
| Advanced-water visuals/cost | No qualified replacement integrated into the pool game. Simulation, coupling, queries, surface extraction and rendering need matched measurements. | [Representation review](FLUID_TRANSIENT_AND_REPRESENTATION_REVIEW.txt) |
| Humanoid recovery | Explicitly parked after visual rejection. No reliable procedural get-up/balance controller shipped. Rig-scale validation is fixed. | [Recovery handoff](HUMANOID_RECOVERY_FOLLOWUP.md) |
| Original pool desktop/GPU reset | Unresolved incident. Logging and later clean runs do not establish the original cause. | [Diagnostics](GPU_DIAGNOSTICS.txt), [retained incident](FLUID_CLEARANCE_REPAIR_AND_GRID_PROTOTYPE.txt) |
| Exceptional resource cleanup | Four allocation/retirement paths need failure-controlled investigation. No ordinary-gameplay leak is established by the watchlist. | [GitHub #29](https://github.com/SuperReaper1999/Project-Judas/issues/29) |
| Virtual-list item transfer | Rows cannot act as item-drag sources. Stable row identity and scroll/drag ownership need extending. | [UI API](judasjs/ui.md#item-transfers-and-live-3d-images-alpha-122) |
| Paused character preview animation | Preview shares the world's animation clock and lighting. No independent paused animation clock or studio scene. | [Menu boundaries](UI_MENU_PASS_REPORT.md#bounds-and-human-checklist) |
| Navigation tile residency/portability | Surfaces can follow regions; individual tile residency is absent. Detour layer payloads retain a native layout. | [Navigation assets](NAVIGATION.md#assets-persistence-and-export) |
| Large-project save preparation | Full identity hashing and durable writes retain cost. Duplicate encoding is corrected; general safe caching needs an integrity contract. | [Save latency](SAVE_LATENCY_FOLLOWUP.md) |
| Integration bookkeeping | Component participation still crosses manual lifecycle/authoring paths. Large scripting/editor modules remain. | [Original findings](PREALPHA_FINDINGS.md) |
| Consumer SDK installation | The separately owned RPG still documented SDK v1.2.4 at review. Installing v1.2.5 and verifying that game is a separate task, before removing older SDKs. | [Developer workflow](DEVELOPER_WORKFLOWS.md) |
| Import-lab clean-checkout example | Local-only character assets leave this particular source example incomplete. Provide a redistributable fixture or explicit preparation path. | [Import-lab boundary](MODEL_IMPORT.md#import-lab-and-original-assets) |
| Game animation/content handoffs | Lunar Island wolf/human/weapon animation, articulated deaths and broader content remain game work. Lastlight has its own animation list. | [Lunar handover](../projects/lunar_island/HANDOVER.md), [artist handoff](../projects/lunar_island/ArtSource/STAGE3_ANIMATION_HANDOFF.md), [Lastlight](../projects/lastlight/ANIMATION_GUIDE.md) |
| Platform/accessibility coverage | Existing Windows hardware and Android phone acceptance remain valid. Physical controllers, broad clean-machine coverage, human IME and some newer combinations remain unqualified. | [Support matrix](ALPHA_SUPPORT_MATRIX.md), [UI scope](M96_UI_TOOLKIT.md) |

## Deliberate extension areas

These are declared boundaries, rather than claims of newly reproduced defects.

| Area | Remaining boundary | Reference |
| --- | --- | --- |
| Networking | Broader assembly/terrain replication, streamed/modded roots, public-server security and browser transport. | [Networking](M99_NETWORK.md), [runtime contract](judasjs/network.md) |
| Persistence/suspension | Arbitrary topology migration, merging newly authored worlds, all-assembly suspension and missing historical state. | [Save evolution](M98_SAVE_EVOLUTION.md), [living worlds](M97_LIVING_WORLD.md) |
| Large worlds | Multiple distant active physics charts and excavation across every terrain representation. | [Assembly travel](M90_ASSEMBLY_TRAVEL.md) |
| Rendering/residency | Point shadows, cascaded sunlight, universal instancing, per-mip streaming and hard total-memory guarantees. | [Rendering](judasjs/rendering.md), [asset pipeline](M93_ASSET_PIPELINE.md) |
| Broader API controls | Full wheel geometry/query configuration, pairwise collision/query overrides and runtime render LOD policy require ownership/persistence rules. | [Wheels](judasjs/wheels.md), [physics](judasjs/physics.md), [LOD scope](M93_ASSET_PIPELINE.md) |
| Animation/text/input | Morph/cache deformation, bone-key editing, richer text caret behaviour, raw independent keyboards/mice and OS screen-reader integration. | [Import](MODEL_IMPORT.md), [UI](M96_UI_TOOLKIT.md), [local players](M89_LOCAL_MULTIPLAYER.md) |
| Browser productization | Context restoration, wider browser/storage qualification, production hosting, residency, memory and audio modernization. | [Browser scope](BROWSER_SUPPORT.md), [emulator limits](BROWSER_ANDROID_EMULATOR_QUALIFICATION.md) |
| MK2 bounds | Arrival holds, extreme-spin phase error, residual elastic dissipation and content-dependent dense-contact cost. | [MK2 contract](MK2_PHYSICS_ARCHITECTURE.md), [rotation evidence](PREALPHA_MK2_ROTATION_CORRECTION.md) |

## Closed or superseded reports

The earlier MK1 CCD/event architecture, M78 motor/save lifecycle defects, M65
dependency/inert-world costs, old importer palette limits, browser shadow and
submission-growth bugs, pointer/orientation/resize/weather issues and the API
readback/navigation/tilemap pass have later corrections. Do not reopen them solely
from an old handoff. Scope changes and new reproductions should retain their own
evidence.

The current release summary and finding register were reconciled in this docs
pass. [Issue #17](https://github.com/SuperReaper1999/Project-Judas/issues/17) now records
the later pressure/volume and prototype findings and remains open. #29 remains a cleanup watchlist. No open
PR was found at review. These live tracker facts can change.

## Keeping this register current

When closing or changing an item, update its status and evidence in the same
checkpoint as the correction. Preserve historical tests, reports and rejected
experiments. A source inspection, symbol count or release build does not supply
missing behavioural or human validation. Follow the [maintenance checklist](DOCUMENTATION_MAINTENANCE.md).
