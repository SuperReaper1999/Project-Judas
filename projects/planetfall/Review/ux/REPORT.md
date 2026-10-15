# Planetfall UX revision

Starting Experimental checkpoint: `29ea41a22c3bdac73e9851a2c38ebeb8ac25fb1b`.
Human acceptance of this revision is pending. No runtime engine/API changes.

## Changes

- One contextual E action, with the same selection function used for its visible prompt and fixed-step intent. Each request is consumed once by its owning subsystem.
- Equipment, journal, flight chart and controls use a paused, mouse-operated expedition menu opened with Tab. Escape provides visible save/load controls.
- Vendor and vehicle interaction use E. Travel, ration use, side contracts, upgrades, carried-prop rotation, gravity release and final decisions have labelled buttons.
- Right mouse aims; Space attempts low-ledge mantle before ordinary jump. Combat shortcuts remain explicit in Controls; 1/2 weapon selection is optional because Equipment provides it.
- Short feedback notifications and a compact current objective replace the old key-heavy HUD text. Detailed dialogue remains in Journal.
- UI commands queue plain intent for the resumed authoritative fixed update. Existing fire-blocking prevents menu clicks leaking into weapon fire.
- Old logical action names remain unbound for rebinding compatibility; no old hidden modifier combination is required.
- Authoring regeneration reads Tools/input-map.txt so it cannot reintroduce the rejected bindings. The native harness now uses E and actual travel/save/load UI clicks.

## Validation actually run

Release-configured focused application target built without warnings. JavaScript syntax checks and 96 campaign-rule checks passed.

Final scoped native cases: menu 25, avatar 9, Earth 12, damaged-drive refusal 16, repaired-drive travel 19, save 15, fresh-process load 13, exported-package menu 25: **134 checks, zero failures**. These are fixtures, not a complete human campaign playthrough.

Menu checks exercise real pointer/button input, paused state, equipment/journal/map/controls navigation, E conversation, save completion and unchanged ammunition after clicking/resuming. Save/load checks verify actual player endpoint, resource and camera restoration. Travel checks verify real scene publication, fuel consumption and pilot ownership. Package checks ran from unrelated /tmp working directory against the exported project using the native application harness.

Export: 151 assets, eight scenes, ~181 MB. No full production-suite or Windows rerun was appropriate for this game-only pass.

Representative synthetic 60 Hz fixture results: menu fixed-step p95 8.03 ms; Earth 6.24 ms; travel 5.85 ms with ~504 ms region-publication maximum. Exported menu fixed p95 6.51 ms. These are diagnostic fixture costs, not certified interactive FPS.

## Preserved failures

first-missing-import.log: new module metadata initially omitted the mandatory source field; corrected metadata registers the module. No engine change.
first-save-observation.log: the initial new test inspected a transient HUD string at a fixed frame and failed. The corrected test observes actual asynchronous save request completion, rather than assuming completion/display timing. Existing physical restoration checks remain intact.

## Human checklist

1. Follow the marker to Mara; verify the E prompt explains and performs the action.
2. Open Tab; click Equipment, Journal, Map / Travel and Controls. Close with Tab/Escape.
3. Aim with right mouse, jump/mantle with Space, and verify menu clicks never fire a shot.
4. Collect supplies/mine/grab/trade with the displayed E action.
5. Board/leave the rover; enter Wayfarer and select travel in its menu.
6. Save, move, load; verify position, equipment and campaign state.

## Limits

This is the first usability revision, not a polished tutorial or full visual cartographic map. Combat shortcuts remain R/F/Q/G and optional 1/2. Context uses existing authored interaction ranges and sight queries. Original campaign, physics, prototype-animation and regional-loading limitations remain. Existing saves may be invalidated by strict project/content compatibility. The already-open old package is not hot-reloaded.

Protected history, other projects, independent fluid research and runtime source are unchanged. main is not promoted.
