# Pre-publication 2D usability review — 31 October 2026

Experimental follow-up to human-tested 70fe8ff4. This is agent-run workflow,
source and screenshot review, **not invented human acceptance**. No publication,
main promotion, fluid change or browser implementation.

## Ordinary tasks checked

| Task | Method and result |
|---|---|
| Choose a movement preset, create a project | Actual editor CreateProject handler, fresh directories, separate dark/1x grid and light/1.5x side-view profiles; passes. It does not type/click every New Project modal control. |
| Find player, transform, script and controls | Source/guide review and actual editor screenshot inspection. Transform is prominent, script attachment and Edit script visible; large-scale inspector scrolls. Names may truncate in narrow rails. |
| Import art, build a small level, add animation | 25-check actual editor-handler/native runtime workflow: PNG import, duplicate rejection, sheet slicing/events, tile stroke/Undo/Redo, script attachment, syntax-error negative control, correction and save/reopen/run. |
| Create/attach scripts and player presets | Existing 65-check editor scripting workflow passes. |
| Themes and saved layouts | 45 checks, actual GPU theme frames, mouse tab selection and ImGui navigation activation of the actual save-layout button. Preference roundtrip/invalid input tests retained. |
| Play/Stop/save | Both actual editor runs preserve identical authored scene bytes, then save successfully. |
| API/documentation consistency | JudasJS inventory/type check passes. No API implementation or signatures changed. |
| Browser feasibility | Source and current official Emscripten documentation review; no web build or browser gameplay claim. |

Dark and light screenshots were visually inspected. At 150% scale fewer inspector
properties fit on screen, but its scrollbar preserves access. These bounded runs
do not certify every screen size, IME, controller, theme or OS. The automation had
injected a profiler overlay into its own screenshots; 2D onboarding automation now
omits that injection so screenshots reflect the ordinary editing workflow. This
does not change the user's profiler preference or remove the profiler.

## Concrete improvements

- README no longer says tilemaps remain pending. It points to a short current
  walkthrough rather than historical stage reports as the first tutorial.
- docs/2D_QUICKSTART.md explains template choice, movement, where visual and
  collision settings differ, scripts/cameras, tiles, animation and export.
- New Project gives explicit Play/Stop/script/collision next steps. Create is
  disabled with an explanation while name/directory fields are missing.
- One-way checkbox now says jump-through first, with blocking-arrow instructions
  and the static straight-edge restriction. No collision response was changed.
- Editor guide and docs index lead to the quick start.

## Failed attempts preserved and classified

The first workflow invocation supplied explorer.png instead of the declared
multi-frame palette. It exposed an unsafe test assumption: frames[1] was read
without validating frame count, resulting in bad_alloc. The test now rejects that
fixture explicitly; the declared palette still passes all 25 checks. This was
not a demonstrated SpriteSheetEditor bug and no acceptance threshold was relaxed.

The preferences test clicked y609 after added panel rows had moved the save button
to y637. It clicked the name input, then claimed layout-save failure. Final test
activates the actual ImGui button by its scoped ID through navigation; the real
mouse test for selecting Layouts is retained. The first navigation experiment
omitted the tab ID scope and also failed; that log is retained. Final 45-check run
passes, including persistent save-request semantics. No production layout-saving
implementation changed to make the test pass.

## Remaining usability limits

- Native 2D and 3D physical bodies cannot coexist in one physical world. The guide
  makes that boundary explicit rather than implying sprites disable it.
- Visual sprite extents/padding and physical footprints remain separate authored
  settings. Grid moves choose whole destinations; they do not creep to every wall.
- One-way surfaces remain static straight segments; tilemap solids are separate.
- Sheet/palette/draft concepts still require learning. The quick start links to
  concrete editable examples rather than claiming a full RPG framework.
- Browser export is unsupported today. See BROWSER_EXPORT_REVIEW.md for actual
  blockers, reuse opportunities and staged acceptance proposals.
- No new Windows/Android qualification, release SDK rebuild or final release gate
  was attempted as part of this bounded UX task.

## Reproduction / evidence

Raw logs and screenshots: [evidence/2d-usability](evidence/2d-usability/).
Use the current native Release build. Fresh workflow directories are required.

```sh
build/judas_2d_game_workflow_tests projects/tilemap_workshop/Assets/tiles.png /tmp/ux-fresh
build/judas_editor_script_workflow_tests
SDL_VIDEODRIVER=x11 build/judas_editor_preferences_tests /tmp/ux-themes
node scripts/check_judasjs_api.mjs
```

For real editor runs use isolated XDG_DATA_HOME with workspace.json appearance,
JUDAS_EDITOR_AUTOTEST=<output-prefix>, JUDAS_EDITOR_AUTOTEST_NEW_PROJECT=<fresh-dir>,
and JUDAS_EDITOR_AUTOTEST_NEW_2D=grid or side-view. Logs and screenshots record
actual runs at 1280x800 using NVIDIA GTX970/X11, Linux/GCC15 Release. Saved profiles
were isolated; the operator's project and preferences were not modified.

## Human checklist / publication boundary

1. Read the quick start from a fresh developer's perspective; choose the right
   template without opening a historical report.
2. Create a grid or side-view project, Play, Stop and edit the player's script.
3. Preview collision on a one-way platform and identify its blocking side.
4. Try your preferred theme/scale and save/reopen a named layout.

The earlier workshop acceptance remains the operator's actual acceptance of that
checkpoint. These new usability edits require their own review before main or
publication. No web implementation is included in the 1.1.0 candidate by this task.
