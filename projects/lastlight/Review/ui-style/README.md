# Field-kit presentation review

10 October 2026; based on Lastlight checkpoint
`3b5e5b5d7b62e097208727ee7cdd10526be04c27`. Uncommitted visual revision.

## Scope

Seven generated PNGs, two additional licensed DejaVu faces, a 96-element normal
JudasUI document and project presentation scripts. Compact HUD cards, selected
item art/status, legible field notes, consistent pause/supply menus, and clearer
disabled-action labels. The scene, models, navigation, combat, inventory storage,
equipment, gravity, physics and wave rules were not changed.

The sole native correction is `Renderer::DrawUIImage` UV orientation for imported
textures. `orientation-before.png` shows the actual upside-down rifle before the
correction; current inventory captures show the unedited original PNG upright.
The texture decoder, world material paths and font atlas drawing remain unchanged.
Render-target images retain their existing UV convention. No public API changed.

## Actual application previews

`capture.log` records the ordinary asynchronous application with all seven
textures and two additional fonts ready. A disposable `/tmp` capture driver
uses normal logical I/Escape and ordinary mouse input at the calculated row
centres. It changes the real SDL window size and captures the rendered frame.

- HUD, inventory and pause at **1280×720** and **1680×900**.
- Mouse selection switches the live preview to launcher, rockets and grenades.
- Modal menus pause fixed updates; closing them resumes ordinary play.
- Script diagnostics remain empty throughout the captured sessions.
- `shop.log`: a disposable project copy changes only the authored player start
  position to the stall. Normal N opens the real supply UI at both resolutions.
- `boots.log`: another disposable copy gives the pack one boots item through
  the existing project `Inventory.add` helper and selects it for visual review.
  `inventory-boots.png` checks the long title, row, status, art and notes. This
  fixture does not prove normal collection and is not shipped as gameplay.

The initial attempt at a 1920×1080 desktop window was constrained by the
compositor; that capture helper timed out after the three reference images.
The final review uses actual 1280×720/1680×900 sizes and completed normally.
This was a capture setup limit, not a game or UI solve failure.

## Build/package

The existing Release build incrementally rebuilt Renderer and relinked runtime
and editor successfully, with no warnings. `build.log` records that work; no
unrelated production suites were run for this presentation task.

Normal export includes **85 assets / one scene**. `export.log` has exact bytes.
The package was copied to `/tmp/judas-lastlight-fieldkit-20261010`, launched from
`/tmp` with no development-root override, and displayed its HUD, pack and pause
at the runtime's default **1024×768** window. See `package.log` and package images.
No missing-resource or script-callback error was reported.

`../content-sha256.json` is the current project source-content manifest; the prior
physical revision's manifest is preserved as `../physical-sha256.json`. The
separate `source-sha256.json` here records the narrow Renderer change and exact
UI presentation inputs. Generated source art/prompts remain in the project.

## Review status

Actual rendered previews were inspected for upright images, clipping, contrast,
live text and selection. **Operator acceptance of this new style is pending.**
Windows/controller hardware review and a full gameplay retest are not claimed.
Tiny windows scale down the reference UI; no mobile reflow was introduced.

No commit, push or tag. Protected engine evidence, other projects and the original
low-poly backup were left unchanged.

## Subsequent gameplay request

After this presentation review, the operator additionally requested zombie wall
pursuit. Its independent source/evidence is in `../wall-pursuit/`; that follow-up
does not turn the UI captures above into gameplay proof. The newest combined
standalone contains 86 assets, adding the normal wall-pursuit script resource.
