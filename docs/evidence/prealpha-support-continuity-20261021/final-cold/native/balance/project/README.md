# Balance Lab — M77 review candidate

Open `balance_lab.judasproj` in the ordinary editor, then Play. No task starts
automatically. This is original CC0 mechanical character content, not a claim
that arbitrary imported humanoids can recover with these gains.

## Controls

- F1 flat / F2 moving supports / F3 fallen / F4 radial sphere.
- Tab selects one of four independent instances. Initial selection is the
  slender **Stride** rig. Anchor and Receiver have weighted cubic feet.
- P releases balance effort and explicitly enters passive physics if needed.
- B requests physical standing; G requests physical get-up.
- U applies a weak 2 N s push. J applies a real 29 N s root impulse; K applies 100 N s (expected failure).
- Q rotates the impulse by 45 degrees; E reverses it.
- T toggles stepping permission; Y toggles effort. These affect the next B/G.
- N separately requests collision-checked motor return after measured success.
- O toggles ordinary M70 partial-leg physics after return; L toggles a masked
  Rest layer. A gold visual socket marker follows the resolved Stride skeleton.
- WASD controls the motor after that explicit return.
- Z spawns the normal Anchor prefab; Delete removes the actual supporting body.
- F5 diagnostic colliders/COM/contacts/plans; F6/F7 save/load; R reload; Esc pause.
- Mouse orbits the separate presentation camera.

## Declared examples and limits

**Corrective-step comparison:** F1, Stride selected, P, wait two seconds, B, wait
for succeeded and settled, then J. It lifts a foot and reaches real contact,
then settles. For the denied comparison reload, T, repeat the same sequence.
The same impulse then exceeds this rig's standing envelope. Initial animated
rest straight to B has a different transient history and is not the declared
comparison. Nearby larger impulses/directions are not universally recoverable.

Fourth flat actor is an authored frictionless negative control. F2 also includes
2 cm uneven geometry, a 3-degree slope and a separately authored oblique uniform
gravity patch on a rotated finite platform. F3 fourth actor is under a low ceiling.
These variants use normal shapes, materials and ordered gravity zones.

**Get-up:** F3, Tab twice selects face-up Anchor; P, wait two seconds, G.
Tab once selects distinct face-down Receiver; repeat P/wait/G. These two
weighted-foot robots recover under finite paired joint effort. Stride is the
step fixture, not a demonstrated get-up fixture.

**Planet:** F4, Tab twice selects Anchor; P/wait/B. The real radial field is
sampled at each physical body. This scene explicitly authors planted orientation
weight 1; flat/weighted-foot side-contact get-up uses 0. The slender Stride rig
has not established the same planetary standing envelope.

Every success is based on actual supports, height, uprightness, speed and dwell.
Plans, ray hits, reference trajectories and debug markers are not support. No
root/foot teleport, hidden anchor, special gravity or decorative recovery exists.
Full physical tasks disable the motor explicitly; N is a separate authorization.

Source profiles and importer provenance live in `Sources` / `Authoring`. Asset
metadata and cooked models use normal Judas registration. The generator refuses
to overwrite an edited project unless explicitly requested. See `docs/M77.md`
and the current JudasJS reference for configuration, lifetime and numerical limits.
