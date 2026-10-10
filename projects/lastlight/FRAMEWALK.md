# Judas Framewalk Boots

Framewalk is optional Lastlight equipment implemented in project JavaScript.
Judas supplies gravity selection, actual geometry queries and the CharacterMotor;
the boots choose a surface and acceleration. The town starts with ordinary
spatial gravity and no boots equipped.

## Use

**C** collects the boots on the starting plinth. **I** opens the pack; select the
boots, equip them, and close it. Jump with **Space**, release it, then press
**Space a second time while airborne**, aiming at a clear surface within 7 m.
The first jump arms one successful surface selection; merely walking off a ledge
does not arm it. A failed selection can be retried during that jump. **X** releases
the selected frame while leaving the boots equipped. Unequipping or dropping the
boots also restores prior gravity when fixed simulation resumes.

## Configuration and reuse

[framewalk.js](Assets/scripts/framewalk.js) exports a player helper, not a native
boot component. [player.js](Assets/scripts/player.js) constructs it, calls
`tick(dt)` before movement, `launch()` for a supported jump and `airborneJump()`
for an airborne jump press. It checks `inventory.equipped('feet')` for
`framewalk_boots`. Reuse that integration with the same helper interfaces, or
adapt its small player dependencies (`entity`, `camera`, `inventory`, `grab`,
`climb`, plain `state`) to your own character.

Pass options when constructing it: `new Framewalk(this, {reach: 7})`.

| Option | Current default |
|---|---|
| `strength` | 9.81 m/s² |
| `reach` | 7 m aimed selection |
| `recoveryRange` | 24 m nearby search |
| `turnSpeed` | 150 degrees/s nominal angle-to-duration setting |
| `radius`, `halfHeight`, `offset` | 0.3, 0.6, 0.9 m; match the authored motor capsule |

Transition duration is `max(0.1 s, angle in degrees / turnSpeed)` at selection.
Acceleration uses a smoothstep blend from its actual starting vector to
`-targetNormal * strength`; `turnSpeed` is not a constant direction rotation rate.
Magnitude can decrease during the blend; opposite directions briefly pass
through zero. The motor retains up near zero and independently turns at authored
`motor.reorientationDegreesPerSecond` (120 here). Completion waits for the blend
to finish and actual motor up to align. Match capsule dimensions/offset when
reusing the helper. Threshold messages and fall-damage values are ordinary code
settings, not exported Inspector properties.

The generated town has a normal uniform Gravity region. The editor's **Add
Component → Gravity selection** also lets a project author a uniform owner
override or select an existing Gravity source, including a RadicalGravity field.
The boots save `entity.gravity.state` on first activation and retain it through
further switches/recoveries. They use `setUniform()` for their temporary direction.
X, unequip or recovery failure restores the saved uniform acceleration or live
selected field; otherwise `clear()` resumes applicable spatial fields, including
when a saved source has vanished. This fallback is not hardcoded world-Y. Saved
source handles remain private helper fields and are cleared on release.
Stop/reload restores authored data.

## Motion, references and recovery

Selection accepts only real, nonsensor static/kinematic surfaces with a usable
normal. The player, held prop and enemies are excluded. A landing capsule must
fit and the face must be visible. The straight capsule approach rejects blocking
contacts; an initial overlap is accepted only when moving outward from its contact
normal and the final landing capsule is clear.
The selected point/normal are retained in the reference body's local frame and
refreshed from its live transform, including kinematic rotation.

The helper changes gravity intent without assigning the character transform or
resetting its velocity. Normal locomotion, the first jump, contacts and gravity
still change motion. First/third person camera, weapons and the whole rendered
skeleton follow the presented motor orientation. The public motor up comes from
its actual pose; gravity direction and collision support normals remain separate.

Recovery begins when an active reference is invalid/disabled, or after the
transition when an airborne character has no suitable landing along current
gravity. It examines at most eight nearest-body candidates, refining each nearest
point with an ordinary raycast to obtain a real face normal instead of using an
edge separation direction. It then samples fourteen local ray directions within
24 m, checking the same clearance rules. The shortest
accepted sampled ray wins; this does **not** guarantee the globally nearest
usable surface or an available route. Straight approach and final capsule tests
do not sweep every intermediate rotation or predict a curved fall. Cramped
geometry, obstructions, range, fast motion and unsampled directions can defeat
recovery. If no candidate works, prior gravity is restored without a teleport;
a vanished selected source falls back to spatial gravity.

Landing impact subtracts current support velocity from the previous player
velocity, then measures motion into the actual support normal. Above 9 m/s,
damage is `ceil((impact - 9) * 8)`, capped at 100. It applies during preparation
and waves, with or without boots. X, unequip and recovery do not erase momentum.

[grab.js](Assets/scripts/grab.js) explicitly gives the held finite-mass body the
player's effective acceleration while boots are active. Its bounded force/torque
grip and ordinary collisions remain. Release/inactive boots restore the saved
uniform override, live selected source or spatial mode; a vanished source falls
back to spatial gravity. This is project policy, not implicit child inheritance.

## Items and lifetime

[items.js](Assets/scripts/items.js) defines names, stack bounds and equipment
slots. [inventory.js](Assets/scripts/inventory.js) owns eight plain pack stacks,
equipment and deferred drop intent in `player.state.inventory`. Slots are
`weapon`, `feet`, `body`, `hands`, `accessory`; only weapon/feet have content here.
Starting rifle/RPG selection calls the existing weapon helper. Explosive quantities
live exclusively in `game.rockets`/`game.grenades`, capped at 12, so collection,
vendor purchases and firing share one stock.

Reusable inert pickup prefabs are `framewalk_boots_pickup.judasprefab`,
`rockets_pickup.judasprefab`, `grenades_pickup.judasprefab`. Their ordinary dynamic
root has `inventory_item`/`physical` tags and `items.js` in slot 1 with
`{item, quantity, prefab}` properties. That slot publishes pickup metadata for
the nearest eye cast. Player properties `framewalkBoots`, `rocketsPickup` and
`grenadesPickup` hold their registered prefab IDs. A drop queues one item,
closes the modal, checks clear space and spawns its normal body on a fixed step;
contents change only after successful creation. Restart rebuilds authored loot
and starting round state. No cross-scene inventory workflow is implemented.

`player.state.boots` publishes equipment/activity, reference ID/point/normal,
transition mode, switch/recovery/failsafe counts and momentum delta for inspection.
Live reference handles remain helper fields. [build_framewalk_world.py](Tools/build_framewalk_world.py)
authors the three east buildings and roof supplies; [build_town.py](Tools/build_town.py)
registers the assets, input actions and UI. All behaviour uses public APIs; no
Lastlight engine branch was added. Existing `Review/` evidence remains historical;
current application captures are recorded in `Review/BUILD_NOTES.md` and human
gameplay acceptance remains pending.
