# Private four-vehicle crash yard

The second collision-search optimisation remains an engine-only change; see
`MK2_COLLISION_SEARCH_OPTIMISATION.md` for its matched benchmarks and exact-state
qualification. This follow-up repairs the playable review project's controls and
adds the four distinct privately supplied vehicles. It is not a new physics
optimisation benchmark or a revision of the accepted material calibration.

## Project-owned controls

`tools/crash_proof/DrivingProject.js` is the ordinary JudasJS control template used
by the private project generator. It provides actual-count selection, labelled
vehicles, speed telemetry, mouse orbit, zoom, a horizon-level damped camera,
behind-car reset, and explicit selectable-speed physical launch. Launch applies
mass-scaled impulses to the selected connected structure and its attached panels;
it does not reattach debris, script fracture, or change material strengths.

Native fracture part `entity` values are Entity references, not stable scene IDs.
The new controls access their transforms/velocities directly. Wheel and motor
loops use the authored layout, including six wheels and multiple sources.
The generator discovers attached panels and creates matching input bindings.

Controls: **1–4** Challenger / Shelby patrol / Blazer K5 / Ural 6x6;
**W/S** drive/reverse; **A/D** steer; **Space** brake; hold **RMB** and move mouse
to orbit; wheel zoom; **C** behind car; **Enter** launch; **X** cycle 2, 4, 8,
15, 25, 35 m/s; **R** reset; **K/L** modern save/load; **Esc** pause.
The HUD shows the selected vehicle, actual speed and configured launch speed.

## Private project

Editable source:
`/home/conner/Projects/Judas_Deformables_V2_Crash_Proof/Crash-Yard-Repair/Four-Vehicle-Yard/Crash_Yard.judasproj`.
The yard contains a full-width barrier, offset pole, thin wall, inside corner,
angled barrier and perimeter walls. Existing accepted projects and benchmark
fixtures are preserved independently.

The Ural's previous example was a static panel gallery. Its new local six-wheel
rig uses existing finite-material structural sections, four existing physical
panels, fitted/recentred wheel meshes and a six-output powertrain. It has 16
structural sections and total authored mass 6500 kg including panels. The local
source faces +Z; the wheel visual basis preserves the fitted axle while the rig's
authored forward axis and world orientation face the driving lane. No special
engine simulation or hard-coded truck behaviour was added.

Two failed local authoring attempts are retained: omitted finite-material enable
and neighbour filtering broke the frame before collision; missing part groups
prevented drivetrain connectivity. Corrected authoring drives with intact frame
and mounts before impact. These were private fixture errors, not solver changes.
All original downloads, converted assets, local authoring and exports remain
outside Git and public SDK packages. Existing accepted private model limitations
remain; this is not a claim of human acceptance of the new Ural driving setup.

## Low-speed damage report

The reported minor-bump bonnet detachment remains unconfirmed. Original-strength
controlled wall tests did not release the bonnet at 4 or 8 m/s; 15 and 30 m/s
released all three mounts. A wheel-driven wall encounter sampled 4.15 m/s before
impact, retained all mounts and produced 0.402 rad permanent bonnet bending.
The 2 m/s test did not establish wall contact and is not counted as a collision
pass. These results do not disprove the human report. No global physics or
attachment tuning has been applied without a reproducible load case. The new
speedometer, launch settings and obstacles make follow-up reproduction explicit.

## Qualification

- `cmake --build build --target judas_crash_proof -j2`: passed.
- `node tests/CrashDrivingProjectTests.mjs`: passed selection, actual-count HUD,
  camera roll/bob isolation, mouse orbit/capture cleanup, connected-only impulse,
  detached-panel exclusion, six-wheel steering and multiple motor sources.
  This original no-assets test is registered in the focused CI workflow.
- Native desktop NVIDIA run: all four selected, accelerated and steered over
  1150 synthetic application frames without script diagnostics. Sampled driving
  peaks were 4.56 / 4.79 / 4.00 m/s for the three cars; the truck's separate
  intact driving run reached 2.69 m/s, with zero broken connections or released
  panel mounts. A 25 m/s truck launch involved all 16 sections (6456.57 kg),
  not just one orphan section; corner impact produced real structural damage.
- Separate-process cold save/reload: all four structures' broken-state arrays
  and all physical panels' permanent bend/released-attachment states matched
  exactly. The damaged truck had 11 broken connections and detached panels.
  A diagnostic copy serialized its expected values; the delivered ordinary
  gameplay script has no test oracle or synthetic damage logic.
- Clean private export: 55 assets, one scene, 650781428 packaged bytes; moved
  and executed with the complete engine checkout and private source collection
  hidden. All four selections and truck driving/orbit ran without diagnostics.
  This uses the already-qualified local engine/SDK at `94907829`; the follow-up
  changes project controls and authoring, not runtime physics or public SDK APIs.
- These harness runs are functional checks, **not interactive FPS benchmarks**
  or human acceptance. No new Windows, Android or browser execution is claimed.

Raw logs, screenshots, local rig preparation source and cold comparison results
are under the private `Crash-Yard-Repair` folder. Original accepted fixtures and
matched performance results have not been replaced by the new yard.

Launch the ready standalone with:
`/home/conner/Projects/Judas_Deformables_V2_Crash_Proof/Crash-Yard-Repair/Play-Crash-Yard.sh`.
Use the adjacent `Edit-Crash-Yard.sh` for the editable project. The prior
`Performance2-d07e2236-Acceptance/Play-Challenger.sh` now opens this repaired yard;
its original single-car standalone remains preserved in that folder.

Human review: switch all four; drive/reverse/brake/steer each; orbit while parked
and moving; try progressively stronger launches against the different obstacles;
check that detached debris stays independent; save damaged state, exit and load.
The Ural driving rig and controls await this review. The reported minor-impact
bonnet behaviour remains a follow-up reproduction item, not a claimed fix.


## Corrected asset selection and removal of superseded projects

The first yard at `a094db0b` incorrectly assembled the pre-fidelity sibling
projects, rather than the accepted `Stage234-Acceptance/Fidelity-Projects` sources.
Its visual checks were insufficient: the Shelby's white tyre/specular diffuse
input and the Blazer's transparent interior materials reappeared. This was a
private project assembly regression, not a new renderer or importer defect.
The earlier statement that accepted model presentation was preserved is withdrawn.

The yard now copies the corrected Challenger, Shelby and Blazer inputs directly
from `Fidelity-Projects`; 75 copied asset/metadata files are hash-checked against
those sources in the private `ASSET_PROVENANCE.json`. The Shelby includes its
corrected direct-source geometry, calipers and interior; corrected opaque
interior/rubber, glass and normal/material maps are retained. Close-up native
1600x900 captures verify the restored wheels and visible interiors on both cars.
The supplied model downloads have not been modified or redistributed.

The assembler now has one explicit corrected source set, records provenance,
fails on conflicting asset identities and refuses to overwrite an editable yard.
It no longer imports the source projects' obsolete test-wall objects.

Per operator instruction, the three pre-fidelity sibling project directories and
the superseded yard/prototype/export/test copies were deleted after replacement
qualification. Eight generated directories were removed (1,823,948,800 allocated
bytes); the corrected yard/export occupy the existing canonical launcher paths.
The private removal receipt lists every path and measured size. Accepted source
projects, original downloads, performance fixtures, logs and current user data
remain. No compatibility paths or substitute broken copies were retained.

Cold-process damage-state comparison and relocated standalone qualification were
repeated against the corrected content. Earlier visual captures of the defective
yard are superseded by `corrected-shelby-close.png` and
`corrected-blazer-close.png` in the private review folder. Automated checks do
not constitute human acceptance of the corrected yard.
