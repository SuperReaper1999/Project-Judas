# Fresh 2D project → complete game qualification

Experimental follow-up to native 2D Stages 0–4, based on
`54db79cdf9b93dade83ed9d4c6d6bdde10f3ad6e`. No main promotion, patch publication,
version change or platform acceptance. Historical evidence and fluid research
are unchanged.

## Result and playable handoff

[Beacon Grove](../projects/beacon_grove/Beacon_Grove.judasproj) is a small complete
one-level top-down game. Recover three lantern cells, restore the east beacon
with E, then cross its physically locked gate to finish. Brambles and a kinematic
wisp hurt the player; three hits cause defeat. R retries. F6/F7 save/load.
Escape opens an authored pause menu with Resume, Retry and Quit.

The source project is independent and editable. Its [README](../projects/beacon_grove/README.md)
explains scripts, tags, palette, animation, gate property and controls. Original
existing Judas pixel art is reused, with generation sources retained; no external
asset/service or new game-specific engine system was introduced. One level and
no audio track are deliberate content limits, not a promised full RPG.

`judas_beacon_grove <repository-root> <fresh-destination>` reproduces content using
`Project::CreateNew`, the same top-down starter used by the editor, ordinary asset
tracking, sprite sheets, tilemap compilation, scene/UI serializers and public
script properties. It refuses existing destinations. It does not copy a previous
game/project. Existing art and the reusable controller are intentional reuse.

## What was actually exercised

- The **actual editor New 2D Project handler**, using isolated preferences and a
  fresh destination, creates, saves and Plays/Stops an unedited top-down starter.
- `TwoDGameWorkflowTests` combines actual editor handlers: fresh project, external
  PNG import, rejecting duplicate destination, Create sprite sheet, slicing,
  authoring a clip/event, saving its draft, viewport painting with native ray/grid
  conversion, Undo/Redo, creating/attaching JavaScript, detecting a syntax error,
  correcting it, saving/reopening and executing the attached source in a native
  world. It checks execution state, not merely file or symbol presence.
- Real SDL/ImGui pointer painting in the complete game, Undo, save and editor
  Play/Stop preserve exact authored source. The painter's optional automation now
  selects a known empty cell instead of assuming the workshop's camera location.
  This changes test setup only, not authoring or physical behaviour.
- `BeaconGroveApplicationTests` runs the real rendered application with keyboard
  events, native collision/trigger delivery, actual animation samples and ordinary
  project scripts. An activation without resources/proximity cannot open the gate.
  Collection, timed opening, crossing the former collider and victory are observed.
- Native hazard overlap produces defeat, then R truly rebuilds a fresh run.
- The project-owned modal pause menu must freeze both native motion and game time,
  including movement input supplied while paused.
- Partial and completed runs are saved through the production save service. An
  independently read snapshot supplies the expected native endpoint, checked at
  restoration before stepping. Script state is checked after the first restored
  script lifecycle boundary. Victory and the removed gate survive restoration.
- Separate fresh processes load the partial and completed slots. The partial run
  can continue collecting/opening/finishing; completed progress and collider
  removal remain intact. No teleporting or mutation of gameplay state is used to
  manufacture objectives or losses.
- A relocated standalone runs away from the development checkout with an invalid
  engine-root override and an isolated writable save directory. Packaged relative
  JavaScript imports, palettes, animation sheets, scene and UI dependencies are
  required to resolve there.

The content assembly and handler-level workflow are programmatic automation;
that is **not** a claim that every import/animation/script UI control was clicked
manually. Actual editor pointer painting and New Project handling are separately
recorded. Human discoverability/gameplay judgment remains pending.

## Corrections and retained failures

No new engine correctness blocker was established. The only editor-source change
makes its optional tilemap autotest independent of a particular demo layout.
Game/test defects found during development are preserved rather than hidden:

- Initial content helper used string tags where the native API requires registered
  category masks, and omitted an explicit startup scene. Both were corrected in
  the consumer helper; they were not engine failures.
- The first collectible script queried a wrapper after destroying it, then queried
  it again from an exit event. Source-located diagnostics identified both cases.
  The final script checks `entity.valid` and returns after consumption.
- A test initially waited for `ApplicationControl::worldReady` on restoration;
  that callback is startup-only. The corrected test observes actual transactional
  world replacement and physical state. It originally inspected script state
  before the restored script lifecycle initialized; assertions now run at the
  correct boundary without relaxing the native physical-endpoint assertion.
- Sandbox protection blocked the default home save directory in an early run.
  Final tests use an isolated writable XDG data directory and project copy.
- A copied Escape hint did not provide a pause implementation. The final game
  explicitly authors its pause UI/script and tests its effect on simulation.

[Raw evidence, commands and source/binary fingerprints](evidence/2d-game-workflow/README.md).

## Scope and remaining decisions

This qualifies a practical bounded top-down workflow on Linux; it is not a claim
that all 2D genres, tile counts or subsystem combinations are production-certified.
Static tilemap, animation and planar-physics boundaries remain as documented in
[Stage 4](2D_STAGE4.md). Existing 3D, skeletal and fluid implementations were not
changed. Windows/Android validation and human acceptance remain separate.

Frame measurements in evidence are whole application update/render durations
before swap/wait, using Mesa llvmpipe on a shared Linux host. They include transition
and resource work; shader/upload outliers are retained. They do not certify GPU
performance or imply a steady-frame FPS guarantee.

## Human checklist

1. Open Beacon Grove, Play, move and push the supply crate.
2. Try E early: the gate must stay closed. Recover all three cells along the path,
   use E at the east beacon, then pass the gate to win.
3. Enter the west brambles, remain until defeat, then retry with R.
4. Pause/resume with Escape; try moving while paused and use the menu buttons.
5. Save after two cells, quit/reopen, load and finish. Save/load the completed run.
6. Stop; paint and Undo a tile, edit animation timing, save/reopen and export.

No human validation, main promotion or v1.1.0 release is inferred from these checks.
