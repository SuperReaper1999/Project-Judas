# Planar Playground — Experimental 2D Stage 2

Open `Planar_Playground.judasproj` in the current Experimental editor, or run:

```sh
build/judas projects/planar_playground/Planar_Playground.judasproj
```

WASD/arrows move; Space jumps in the platformer. **1** platformer, **2** top-down,
**3** the same physical lab in a tilted motion plane. **R** restarts the scene.
**F6/F7** use modern save/load slots. No legacy world-delta save is used.

Walk and jump onto platforms; push crates; ride the translating and rocking
supports; test the sloped ramp and pin-connected rotor; reach the green sensor.
The platforms command native kinematics; the player applies forces/impulses.
Nothing teleports the player to a scripted support or clamps its height.

In Edit mode inspect Body → Native 2D, preview its contour, change shape/material/
rotation locking, or create another 2D body/player from the hierarchy context menu.
Each body is optional; adding a sprite alone still creates no physical object.
The ramp outline is visual; its filled convex collider is the physical boundary.

`Assets/game.js` is the reusable controller. `platform.js` commands native
kinematic supports; `playground.js` owns scene switching, HUD, sensor visits and
save requests. `Scenes/*.judas` are ordinary editable scenes, not engine modes.
Original explorer art is from the Stage 1 starter; remaining visuals are authored
sprites. Generator: `tools/demos/PlanarPlayground.cpp` (requires a fresh output
folder). Tilemaps, frame animation and later 2D stages are not implemented here.

Physics is planar in a fixed local XY frame with spin about Z. Mixed spatial/
planar physical producers, spatial CharacterMotor/ragdoll/wheel/fluid coupling,
dynamic chains and legacy delta saves are explicitly unsupported. 3D visuals and
cameras remain usable. Windows/Android and human gameplay are not certified by
the Stage 2 Linux tests; no v1.1.0 release is published.
