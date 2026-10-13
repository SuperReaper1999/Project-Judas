# M81 human review — accepted 2026-10-13

The operator accepted implementation `7489a902d7e8158dbaed3328ffe96e29fe0936a8`
after the Linux desktop autopilot demonstration and authorized Stable promotion.
See [the acceptance record](HUMAN_VALIDATION.md). The checklist below remains a
reusable review guide; no individual result or Windows acceptance is invented.

Open `projects/wheel_lab/wheel_lab.judasproj` in the editor or launch the ordinary exported package. Automated checks do not constitute human acceptance.

## Controls

W/S torque, A/D steer, Space brake; Tab selects buggy/rover/spawned assemblies. Mouse orbits; C selects close view. V starts/stops the project-owned watch tour. B adds/removes a physical 225 kg payload, H switches tyre grip, P spawns a buggy. F3 requests/releases the garage region. 1/2/3 switch flat/oblique/radial scenes. F5/F8 save/load, R reload, Escape pause.

## One combined checklist

1. Drive the four-wheel buggy; accelerate, steer, coast and brake. Confirm real chassis motion matches visible wheel rolling.
2. Cross the low bumps slowly and observe independent suspension travel without exploding or detached wheels.
3. Add/remove the B payload at rest. Confirm the suspension compresses/unloads visibly.
4. Use H to reduce grip; confirm more wheel slip and less effective acceleration/braking. Restore grip.
5. Select the six-wheel rover with Tab and drive it. It uses the same wheel primitive with different dimensions/strength/layout.
6. Press 2 and drive the oblique fixture; behavior should rotate with its authored gravity and geometry.
7. Press 3 and drive on the curved planetary surface; contact/gravity should follow the real sphere rather than world-Y.
8. Press P and switch to the new prefab; confirm independent controls and wheel state.
9. Request/release/revisit the F3 garage; confirm normal region lifetime and restored assembly.
10. Save while moving/spinning; load and confirm sensible physical continuation and no wheel zero-pose flash.
11. Pause/resume and check pointer/camera/input behavior; reload and confirm authored state returns.
12. In Edit mode inspect a Wheel component, adjust spring/friction/mount settings, save/reopen and Play/Stop.
13. Repeat the important controls in the moved standalone package launched from an unrelated directory.

Model limits: swept sphere contact, scalar axle spin, no separate unsprung body, no automatic two-wheel balancing or full gyroscopic transport. Very thin/high-speed/lateral terrain exceeds the wheel-specific contact approximation. Windows M81 remains untested and nonblocking; do not infer physical hardware acceptance.

Stable promotion was explicitly authorized for this exact Experimental candidate.
