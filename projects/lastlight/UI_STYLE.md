# Lastlight field kit

Current presentation revision, 10 October 2026. The ordinary game uses a worn
olive canvas, warm cream lettering, restrained amber selection marks and teal
equipment status. Painted item illustrations replace the text-only preview;
compact corner cards leave the town and crosshair clear.

## Content and ownership

- `Assets/ui/lastlight.judasui`: authored HUD, field kit, pause and supply stall.
- `Assets/textures/ui/`: seven original generated PNGs. Two canvas backings and
  five item illustrations: rifle, launcher, boots, rocket and grenade.
- `Tools/ui-art-prompts.json`: exact prompts, generation tool and source paths.
  Art was made with the built-in image generator for a hand-painted **look**;
  it is not represented as human-painted work.
- The existing DejaVu font family supplies live body text, condensed bold
  headings and monospaced quantities. Its license remains in `Assets/fonts/`.
- `Assets/scripts/hud.js` and `inventory_ui.js` display the existing game data.
  Images contain no text, prices, stock, health, input or equipment logic.

Inventory keeps its eight stored stack slots and separate capped ammunition
stock. Selection highlights and the preview follow the selected item. Disabled
actions explain kept starting gear or empty stock. Equipment, collection, drop,
vendor prices, health, weapons, gravity, waves and pause ownership retain their
existing game rules. Full controls are in the pause menu; the live HUD gives
short context hints for carrying and the nearby stall.

## Authoring and export

Run `python3 projects/lastlight/Tools/build_ui.py` from the repository to rebuild
only the UI document, metadata and IDs. This does not regenerate the scene,
prefabs, animations or navigation bake. The town builder calls the same authoring
function, so a future content rebuild does not restore the former demo layout.
All images and fonts use the normal AssetDatabase/ResourceManager/export path.
No new widget, atlas format, style API or game-specific native component exists.

The document uses a 1280×720 reference canvas with uniform scaling. Anchor-based
corner cards and centred menus share the existing rectangular UI layout. The
smallest inventory text is intentionally secondary; the main labels and actions
have larger type. A compact window scales down this reference UI; there is no
separate mobile interface or arbitrary aspect-specific reflow.

## Narrow renderer correction

The first actual game preview exposed upside-down imported UI images. Judas's
texture decoder stores the authored bottom row first, while the UI quad starts
at its top-left. `Renderer::DrawUIImage` now compensates imported image UVs.
Font atlas drawing, world materials, render-target conventions and physics stay
on their existing paths. The asymmetric rifle artwork provides a visible
before/after record in `Review/ui-style/`.

## Review

Screenshots and capture notes are in `Review/ui-style/`. Preview the HUD, press
**I** to inspect/select pack items, **Escape** for pause/controls, and **N** near
the blue stall for supplies. Check item art, long boots text, kept/empty action
labels and readable menus. Human visual acceptance remains outstanding for this
new presentation revision.

The updated standalone is `.cache/Lastlight-FieldKit-Package/`. Build products
and disposable capture helpers stay outside the project source.
