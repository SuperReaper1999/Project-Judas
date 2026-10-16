# M90 third-person camera follow-up and release authorization

Base: `3c3a1a3353a814357545ac11739c2300d2ec9b78` (M90 Experimental).
On 2026-10-16 the operator reported that leaving the rover placed the camera
inside the visible explorer model, requested third person, and explicitly
instructed promotion to stable once corrected. In this repository stable means
`main`; no retired Stable branch is recreated. This records release authorization,
not an invented additional human retest of the corrected camera.

## Cause and correction

The on-foot script put the eye 0.65 m above the character root while its visible
box extended 0.7 m up. It neither hid the avatar nor used an external view.
This is demonstration camera policy, not a missing engine API or origin defect.

- On foot, follow 4.2 m behind an upper-body pivot using presented poses and
  sampled local gravity-up. Mouse input orbits the same controlled character.
- Cast the smoothed camera path for obstructions in both modes. The avatar is
  ignored by its own camera query; the rover can obstruct the on-foot view.
- Keep cargo interaction limited to 5 m from the character rather than reducing
  usable reach by measuring only from the newly offset camera.
- Boarding, dismount inheritance, CharacterMotor, physics, origin publication,
  drivetrain, authoritative state and save formats remain unchanged. No engine
  implementation or public binding was modified.

## Actual reproduction and checks

The focused application fixture loads an owned copy of the original script:
`before.log` fails all three new camera assertions (initial camera external,
dismounted camera external, correct third-person focus). Its remaining physical/
save assertions pass. `before.png` preserves the obstructed view.

The current project passes **16/16** short camera/application checks, including
actual boarding, physical braking/dismount, visible avatar outside the camera,
view direction toward the presented character, save/load and reload.
The full unchanged 2,080 m travel endpoint then passes **18/18**, with four real
origin transitions, actual supported dismount, third-person focus, streamed
retention and exact cold momentum/mechanical continuation. A native frame was
inspected after dismount; screenshots are included. No full production suite was
rerun for this content/presentation correction.

```sh
cmake --build build --target judas_origin_travel_application_tests -j4
build/judas_origin_travel_application_tests projects/travelling_lab/travelling_lab.judasproj .cache/m90-camera/repro --camera-only
build/judas_origin_travel_application_tests projects/travelling_lab/travelling_lab.judasproj .cache/m90-camera/full-repro
build/judas_export projects/travelling_lab/travelling_lab.judasproj .cache/m90-camera/export-repro build/judas
```

Export succeeds (8 assets / 2 scenes). API declarations/inventory still match:
422 members, 37 exports, 17 callbacks, live enumeration, TypeScript 5.9.3 and
45 checked examples. Only current release labels changed in that tooling.
Original M90 evidence and its fingerprints are preserved as historical candidate
receipts. This directory contains refreshed combined source fingerprints.

## Scope and remaining limits

This is third-person prototype presentation, not a camera framework or character
art upgrade. Existing M90 exclusions, two baseline-stale save-bound assertions,
FLU-001 and Windows/Android validation limits remain unchanged. Protected history,
independent research and unrelated games are unchanged. The user's README archive
remains byte-identical. No new numerical/performance acceptance claim is made.

## Operator follow-up

Leave the rover after braking; orbit/walk/jump and confirm the character and world
are visible. Walk beside the rover/shelter to inspect camera obstruction. Board,
travel, dismount, save/load and reload. The reopened desktop watch tour uses a
private launch preset; committed project gameplay defaults remain unchanged.
