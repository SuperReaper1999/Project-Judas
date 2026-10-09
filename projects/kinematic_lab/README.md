# Kinematic Lab

An ordinary M71 project containing authored box/sphere colliders, public JavaScript and independent CharacterMotor riders. The orange pusher acts on the blue dynamic crate through contact. Its grey stationary control lane uses the same crate geometry. Green, purple and cyan supports carry yellow motor riders and red dynamic crates. The cyan lane rotates gravity and the entire support frame by 90 degrees.

Open `kinematic_lab.judasproj` and Play. The fixed camera shows the lab; the scene editor uses its existing camera controls. Select a rider with F1 (translating/lifting), F2 (rotating), or F3 (rotated gravity), then use WASD and Space to walk and jump. A released rider keeps its inherited material-point velocity through normal motor movement.

| Control | Action |
| --- | --- |
| T | Stop/resume prescribed motion |
| V | Reverse pusher and rotating support |
| O | Toggle pusher angular velocity |
| R, then J | Reload original crates, then command a fast crossing |
| G / K | Change lifting platform to dynamic / kinematic authority |
| X | Remove the pusher and retire its commands |
| F6 / F7 | Save/load through normal modern save slots |
| R | Reload the authored lab |
| Escape | Pause/resume |

The fixed callback in `Assets/scripts/lab.js` chooses the motion policy. `moveKinematic` accepts a complete authored-pivot position and quaternion, using the next fixed interval by default. `setKinematicVelocity` accepts simulation-space COM velocity in metres/second and angular velocity in radians/second. Values use the established `{x,y,z}` vectors and `{w,x,y,z}` quaternions. It never sets a crate transform, attaches a rider to a support, or drives motion from presentation time.

`Assets/scripts/contact.js` records the existing body-pair callbacks on the moving colliders. The HUD reports commanded/actual motion and rider/support velocities. The native focused checks provide numerical drift, impulse and release comparisons; desktop appearance and handling require a human playtest.

The support script replaces 0.35-second targets each fixed callback to smooth catch-up when dynamic authority returns to kinematic. Stopping freezes its motion clock, so resuming keeps the same trajectory phase. The J crossing deliberately remains one fixed interval.

To recreate the authored project with the candidate author tool:

```sh
python3 scripts/create_m71_lab.py --tool build/judas_scene_author
```

Use a fresh `--project` directory for another copy, or explicit `--overwrite` to recreate this lab. The project includes an ordinary prescribed platform prefab. The original scripts and layout are CC0; the bundled font retains its own licence.
