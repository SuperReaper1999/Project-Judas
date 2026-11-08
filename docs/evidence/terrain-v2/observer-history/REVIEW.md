# Isolate observer detail from game transition history

Found during final SDK review at parent
`4838c503a753f6b68a46204f6166060fa587944e`. `RenderWorldFrame` passes zero elapsed
time for the editor's observer view. The first LOD implementation treated that
as a reset of the shared game transition. With Scene and Game views rendered in
turn, the game could repeatedly restart rather than complete its fade.

The added regression fails on the preserved old binary after six checks. Its
expected/actual exit code is 1. The correction is local to TerrainLodTransition:
an immediate observer list overrides the current draw without advancing or
resetting runtime history. The next positive game frame resumes the old fraction.
An observer rendered before the first game view cannot seed its starting detail.
Publication/reset clears both. Storage is bounded to two runtime lists plus one
observer list; there are no new textures, physics state or saved fields.

Both native NVIDIA graphics paths pass 98,822 assertions without optional image
capture, including the new observer controls and exact intermediate colour/shadow
depths. Existing editor edit/undo 15 and planetary authoring/Play 23 controls pass
on each path. World 93 passes, with 300 supported seam steps and zero observed
trajectory difference. The rendering benchmark now initializes runtime history
with a positive timestep, so its active-fade cases remain genuine. Its final
results and original failure are retained separately from the earlier checkpoint.

No tolerance, shader pin or original negative control changed. No main/release,
fluid-lock or installed SDK change. These are automated checks, not human visual
acceptance. Qualify the new matching SDK after this checkpoint rather than
shipping the previously built parent as if it contained the correction.
