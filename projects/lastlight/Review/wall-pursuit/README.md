# Wall-pursuit application preview

10 October 2026; Lastlight base checkpoint
`3b5e5b5d7b62e097208727ee7cdd10526be04c27`. Uncommitted gameplay follow-up to
the independently recorded `../ui-style/` revision.

## Scope

New `zombie_framewalk.js`, current-frame pursuit/attack handling in
`zombie_ai.js`, and motor-relative facing/lifetime integration in `enemy.js`.
The normal script resource metadata/IDs and current project guides are updated.
No scene, prefab, navigation bake, animation asset, input map, physics engine or
gravity binding was changed. The UI revision's imported-image UV correction
remains the only native change in this combined working tree.

## Actual application observations

`preview-fixture.py.txt` documents disposable project copies outside source.
They retain the real town wall929, real zombie prefab, script sensing, motor,
gravity API, physical geometry and eleven-body ragdoll. The setup puts the player
on a nearby wall with a normal public gravity selection, gives 1000 health for
observation, spawns one runner and suppresses additional director spawns.
It is not a normal collection/equipment proof or shipped game logic.

The existing Release application/input/capture runner advances ordinary fixed
updates at 60 Hz; screenshots are real rendered frames. No new test suite or
native harness was added, no full production validation was repeated.

| Record | Observation |
|---|---|
| `wall-final.log` | One finite jump from street to wall. Supported with up approximately `(-1,0,0)` at x31.843 m, then pursues and punches; player health1000 →978. |
| `wall-final.log`, `wall-after.png` | Ordinary lethal hit enters eleven-body passive articulation from the wall pose. Owner gravity returns to spatial; first observed pelvis y2.781 m, final y0.091 m. Score is awarded once. |
| `return-final.log` | Player removes its override and falls naturally. Pursuer makes its second jump to the reachable floor and returns to spatial gravity/support at y0.017 m, continuing attacks. Maximum **15-frame sampled** speed5.900 m/s; this is not a per-step maximum claim. |
| `ground-final.log` | Ordinary grounded chase/punch continues, zero surface jumps; health1000 →934. |
| `package.log` | Standalone moved outside the repository, launched from `/tmp` without a development-root override. Inventory artwork renders and the real director starts wave1 with eight normal zombies. No callback/resource fault. |

`observations.json` holds the exact selected values. `source-sha256.json` records
the wall-follow-up inputs; `../content-sha256.json` is the combined current
source-content manifest. The UI-specific inputs and original physical revision
manifest remain separate. Final export: **86 assets, one scene**; see export log.
The last destination-selection guard only rejects returning to a lower floor
when the remembered target is still higher on a roof. Its affected wall/return
previews were repeated; the ordinary-ground package preview precedes that guard.
The final exported asset bytes include it.

## Genuine failures preserved

- `wall-initial.log`: the disposable fixture initially edited a noncanonical
  field order and failed to relocate its player. It tested no wall approach;
  the fixture now edits the whole object block. This was setup, not an engine
  or shipped-content defect. `wall-followup.log` preserves the first valid chase.
- `return-initial.log`: floor selection succeeded, but an attack captured its
  direction while the up axis was tilted. The script subsequently added its
  full world-space lunge vector to retained up-axis velocity every fixed step.
  Its newly normal component accumulated; measured +Y velocity16.77 →50.02 m/s
  launched the creature to about134 m. Project intent now projects onto the
  **current** tangent plane, with the one-off jump separate; attack starts wait
  for frame alignment. Corrected return reaches ordinary floor support without
  that amplification. No native limit/clamp, teleport or solver change hides it.

## Human review

Equip boots, let nearby zombies observe you, switch onto a building wall, move
upward, and watch their jump/orientation/punch. Kill one on the wall; drop back to
the street and observe return pursuit. Inspect the refreshed field-kit HUD and
inventory too. Human gameplay/visual acceptance is pending. Windows/controller
hardware review and arbitrary three-dimensional maze routing are not claimed.

No commit/push/tag. Protected historical evidence/research, other projects and
the original low-poly backup remain unchanged.
