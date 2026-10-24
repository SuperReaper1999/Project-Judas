# Final Pre-Alpha corrections and editor workflow

Base: human-accepted MK2 `c859901cc9a52cf5b7104887f56b5e7ad4269b1d`.
Development is on Experimental; main is unchanged. No M100, release/channel change,
new Alpha claim or independent-fluid research modification is part of this work.

## Changes

1. Disabled liquid persistence rejects any owner, parcel, expected-volume/density
   ledger or connection state before publishing it. Exact empty participants remain
   compatible. No fluid numerical method or MK2 solver is changed.
2. Disabled advanced authoring remains in scenes/prefabs but does not demand liquid
   bakes/geometry or create conserved-liquid suspension pins. Export closure follows
   that same project policy. All-assets exports still copy registered raw content.
   Enabling advanced fluids restores normal dependency/freshness checks.
3. Project settings have their own saved baseline, dirty indicator, discard and
   editor undo/redo. Scene state is not falsely marked dirty. Project replacement
   and close protect unsaved configuration. External changes still cause a save
   conflict; failed writes retain the unsaved baseline.
4. Run/Export read saved content. When scene/project edits exist, choose **Save all
   and continue**, **Use saved version**, or **Cancel**. Project settings are no
   longer silently saved while an edited scene is omitted. Unsaved scenes first
   use Save As; a failed save does not continue the queued action.
5. Validation finalizes after all test declarations and schedules MK2, balance,
   project/fluid policy, actual settings controls, basic-water rendering and a
   genuine writer/reader cold-process cycle. Old physical mesh contracts stay;
   removed MK1 scheduling internals do not return.
6. The current MK2 contract is separated from its byte-preserved development
   archive. Current docs no longer validate removed-MK1 source links as if they
   were current implementation. Current status describes the accepted baseline.

## Editor usability

**New project** defaults to **Visible script starter**, with **Empty scene** as an
explicit alternative. The visible starter copies the documented public-API
example into `Assets/game.js`, tracks it normally and attaches it to a render-only
box. Its camera/rotation are ordinary editable project JS, with no hidden native
controller. Edit the script, check/reload through Script Tools and export normally.

**Browse…** is available for new projects, scene/project paths, scene Save As,
asset import source and export destinations. The integrated picker lists/filter
files, navigates folders, accepts a new output name and returns to the original
operation. Choosing a path does not create/overwrite anything. Directory listings
are cached until navigation/Refresh and bounded to 1024 entries. You can still
enter an absolute path; native OS file dialogs are not a new dependency.

Project Settings has **Undo settings / Redo settings** independently of scene
undo. Save project persists its current settings. Both Play policy and the existing
visual-budget reopen rule remain explicit.

## Verification and remaining limits

Fluid checkpoint `e820d50f`: 26 new policy checks, 55 basic policy checks, 61
surface checks, 35 streaming checks and MK2 foundation pass. Original failures and
stale runtime/exporter rejection are preserved under
`evidence/prealpha-final-corrections/`. Later combined editor/CI qualification is
recorded there with exact source and binary hashes; no automated result implies
human acceptance. Windows/Android are not newly hardware-qualified by Linux tests.

The basic water approximations, bounded CCD holds, local terrain editing, migration
limits and supported network/suspension subsets are unchanged. See the current
references rather than assuming universal composition. No historical save data is
invented and deferred fluid failures are not retuned.

## Combined Linux qualification — 2026-10-24

Release, GCC 15.2, native X11/OpenGL on Ryzen 5 5600 / GTX 970. The final
incremental build completed without C++ warnings. This was focused qualification,
not a new full production-suite run. No numerical physics or fluid algorithm was
changed; no performance improvement is claimed from these editor/policy changes.

| Actual check | Result |
| --- | --- |
| MK2 foundations | PASS, zero failures |
| Physical balance | 127 checks, zero failures |
| CharacterMotor | 32 checks, zero failures |
| Living-world streaming | 35 checks, zero failures |
| Save evolution | 40 checks, zero failures |
| Visible starter/public JS and failed/successful reload | 24 checks, zero failures |
| Default water/project policy | 55 checks, zero failures |
| Disabled/enabled fluid persistence, streaming and export | 26 checks, zero failures |
| Actual Project Settings controls and independent document history | 29 checks, zero failures |
| Basic-water application, independent swimming/visual policy and pause | 48 checks, zero failures |
| Separate-process water writer/reader | 18 + 16 checks and unchanged source-save assertion |
| File-picker filesystem negatives and actual ImGui selection | 12 checks, zero failures |
| API/type/reference/examples | 473 symbols, 40 exports, 51 examples; PASS; live enumeration not rerun |
| Gate negative controls / registration / build organization / docs negatives | 10 / 4 / 12 / 10 tests; PASS |
| Current-reference links | 92 documents, 1,935 links; PASS, including staged destinations |

The 12 selected manifest cases all passed. CTest now registers 55 focused entries:
52 scheduled native cases plus three tooling cases. The explicit historical
known-failure case is retained outside the default schedule. Registration is not
a claim that all 55 entries were rerun in this checkpoint.

Three disposable Stillwater desktop-editor sessions exercised project-only close
and cancel, both actual export-choice buttons, exported project **and scene**
contents, dirty-state preservation/reset, Play/Stop, viewport resize and picking.
Both exported versions then launched for 90 fixed steps, with a rendered capture,
from relocated `/tmp` packages and an unrelated working directory. Save-all and
Use-saved packages contained their respective intended names/content. The original
authored Stillwater project was never edited by these sessions.

Reproduction (fresh output paths required):

```sh
cmake --build build --parallel 1 --target judas_mk2_physics_tests judas_physical_balance_controller_tests judas_project_water_tests judas_project_fluid_policy_tests judas_project_settings_editor_tests judas_basic_water_application_tests judas_editor_file_browser_tests judas_first_project_tests judas_character_motor_tests judas_save_evolution_tests judas_living_world_streaming_tests judas_editor
SDL_VIDEODRIVER=x11 python3 scripts/prealpha_validate.py --build-dir build --group water --test mk2-physics --test balance-controller --test editor-file-browser --test first-project --test character-motor --test save-evolution --test terrain-streaming --output .cache/my-fresh-correction-check
node scripts/check_judasjs_api.mjs
python3 scripts/check_current_docs.py --require-tracked
```

For desktop export automation, use a disposable project copy and
`JUDAS_EDITOR_AUTOTEST=<new-prefix>`, `JUDAS_EDITOR_AUTOTEST_POLISH=1`,
`JUDAS_EDITOR_AUTOTEST_EXPORT=<new-package-directory>`. Add
`JUDAS_EDITOR_AUTOTEST_EXPORT_SAVED=1` to choose saved content. The hook edits the
disposable project/scene, operates the real confirmation button, checks the
exported files and exits. `JUDAS_EDITOR_AUTOTEST_PROJECT_UNSAVED=1` separately
exercises project-only close/cancel. These are automated checks, not human tests.

[Raw receipts and fingerprints](evidence/prealpha-final-corrections/combined/QUALIFICATION.json)
record source inputs, native binaries and staged inventory. The binary build
identity is a dirty-development provenance receipt, not a clean release claim;
final prose/evidence were recorded afterward. `SOURCE_FINGERPRINTS.json` identifies
the exact final task source/configuration/docs bytes. Compressed profiles preserve
exact original JSON after decompression. Two development interruptions remain in
`before/`: an environment process-limit build failure, followed by a successful
single-job retry, and the correct rejection of an editor/runtime provenance
mismatch, followed by a rebuilt matching pair and successful exports.

Windows/Android were not rebuilt or hardware-tested in this correction. No fresh
native-platform claim follows from Linux results. Native OS dialogs, automatic
script execution on disk edits and universal cross-system compatibility are not
introduced. The independent-fluid research, existing games and MK2/balance
implementations remain untouched. Human review is still pending.

## Human checklist

1. Create a Visible script starter in a new folder using Browse; Play and see it.
2. Open `Assets/game.js`, change rotationSpeed, check/reload and see the difference.
3. Import a model with Browse, place it, then save and reopen the scene.
4. Change a project setting; see its dirty indicator, Undo/Redo, and close/cancel.
5. Confirm Save and Discard affect the project separately from the scene.
6. Edit a scene without saving; Export, cancel, then compare Save all versus Use
   saved version. Run the moved package and verify the chosen version.
7. Try basic water with advanced fluids off; opt in/out through the warning.
8. Exercise viewport focus, wheel zoom and nested selection during normal editing.
