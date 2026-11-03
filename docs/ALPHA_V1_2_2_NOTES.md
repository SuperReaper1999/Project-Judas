# Judas Engine Alpha v1.2.2

Machine version **1.2.2-alpha.1**. Alpha, not Stable/GA. This patch packages the
operator-accepted UI checkpoint `49d03f863da54e415eaf07f5597342e100d5ca3c`, with
release metadata, documentation and SDK integration. Previous releases are immutable.
The publication manifest identifies the exact shipped source and artifact hashes.

## Native UI transfers and 3D previews

- Opt-in typed item dragging with pointer and keyboard/Activate input, cancellation,
  clipping, player/modal ownership and cross-document pointer routing. Drop events
  carry identifiers; inventory rules and item mutations remain project-owned JS.
- Image elements can display a live existing render camera without a second world,
  renderer or CPU readback. Invalid/stale sources are rejected; disabling the
  camera avoids stale presentation and unnecessary camera rendering.
- Editor category authoring, declarations, positive/negative type checks and
  [practical documentation](UI_TRANSFERS_AND_PREVIEWS.md) accompany the APIs.
- The SDK includes **Satchel Menu Workshop**, an editable inventory/pause menu with
  nearby-container transfers, bag/equipment slots, merge/split, capacity checks,
  consumables, character/settings tabs, text-size preferences and real Save/Load.
  Original PNG/OBJ assets and readable game scripts are included.

The existing camera owns its target; menu scripts must disable it when unused.
Bindings and active drags are transient and must be reconstructed after restore.
The demo uses render-only equipment proxies, not a new animated wardrobe system.

## Compatibility and qualification

New category-bearing UI documents use JudasUI format 4 and require this SDK or a
newer compatible engine. Documents without categories retain their previous
encoding. No Pre-Alpha compatibility promise is introduced. The example targets
desktop/landscape; virtual-list row dragging, portrait reflow, isolated studio
lighting and a new paused skeletal-animation clock are outside this pass.

Focused native Linux qualification passed 280 checks, plus API/type controls,
editor dark/light Play/Stop and genuinely relocated standalone tests. The cold
reader checks actual physical endpoints and inventory progress; preview controls
check orientation, changed geometry pixels, disabled sources and target cleanup.
[Implementation evidence and limits](UI_MENU_PASS_REPORT.md).

Exact-release preparation repeats the authoritative Linux gates, Ubuntu 24.04
SDK/tool workflows, cold saves and moved exports. Hosted Windows builds the MSVC
SDK and runs native contracts, including the new transfer tests. Successful hosted
checks are not a fresh Windows graphics/audio/controller hardware trial. These
new menu paths are not newly device-qualified on Android or in browsers. Local
render checks used Mesa llvmpipe because the NVIDIA driver was unavailable; no
physical-GPU speedup or performance qualification is claimed.

**Advanced Fluid Dynamics remains locked end-to-end in Alpha.** Basic Water and
swimming remain available. This patch changes no fluid solver or qualification
threshold, and does not close the outstanding fluid limitations. Existing driver,
dense-physics and platform limitations remain documented in the
[Alpha support matrix](ALPHA_SUPPORT_MATRIX.md).
