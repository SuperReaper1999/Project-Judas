# Bounded UI menu pass — Experimental

Base: `954a23dd30d4cc88aae2e6bc1062a239630c6243` (accepted Alpha 1.2.1 closeout).
This checkpoint extends the engine and adds an editable consumer example. It is
not a release, main promotion or claim of human acceptance. The installed RPG SDK,
RPG project, character controller and all fluid implementations are unchanged.

## Inspection and implementation

Existing native UI already provided clipping, focus, editable controls, virtual
lists, modal pause, project-owned callbacks, state snapshots and images. Existing
world presentation already owned offscreen render cameras, masks and their targets.
The missing reusable mechanisms were item-transfer gestures and borrowing one of
those camera targets in an ordinary UI Image. No second renderer/world/inventory
framework was needed.

- RuntimeUIDrag.cpp owns bounded per-player transient gestures, typed transfer
  requests, cancellation and ghost drawing. RuntimeUI coordinates existing input,
  focus, layouts and rendering. Project code owns stack/slot/capacity policy.
- RuntimeUIControls.cpp cancels a transfer after a successful state restore;
  failed restoration remains atomic. UI asset categories persist through both
  legacy-format v4 and named authoring; default documents retain old encoding.
- ScriptBindingsUI.cpp validates live camera ownership, element type and atomic
  replacement. ScriptSystem.cpp exposes the narrow API/events. Non-Entity JS
  arguments are rejected rather than accidentally clearing an image binding.
- InteractivePlay passes its existing world to UI drawing. A bound Image borrows
  the current target; disabled/unpublished cameras do not expose a stale image.
  Stop destroys resources through existing world ownership.
- The UI inspector exposes category fields in a collapsed group and points to the
  camera-binding workflow. Declarations, positive/negative type contracts, API
  inventory and current documentation are updated.
- Satchel Menu Workshop demonstrates actual project JS, authored UI and original
  low-poly/icon assets: nearby container, sixteen bag slots, three equipment
  slots, merge/split, weight rejection, consumables, character/settings tabs,
  text-size preference, physical pause and real asynchronous Save/Load.

Exact changed-file and binary SHA-256 inventories, commands and raw results are
under [evidence](evidence/ui-menu-pass-20261102/results.json). The source inventory
is scoped to this pass and deliberately excludes unrelated historical experiments.

## Qualification

Linux GNU 15.2 Release; Mesa 26.0.8 llvmpipe / LLVM 21.1.8, OpenGL 4.5. The NVIDIA
driver was unavailable during this pass. These are native functional/rendering
checks, not physical-GPU performance measurements or a speedup claim.

| Check | Result |
| --- | --- |
| Native transfer/format/lifecycle/viewport negatives | 41 checks, 0 failures |
| Menu writer / independent cold reader / actual application Save/Load | 53 / 38 / 10 checks, 0 failures |
| Existing runtime UI | 22 checks, 0 failures |
| Existing production UI/input/HUD | 88 checks, 0 failures |
| Existing UI Showcase application | 28 checks, 0 failures |
| Validation registration contracts | 10 tests, 2 existing Ninja-unavailable skips |
| Gate-runner contracts | 15 tests passed |
| Build-organization contracts | 22 tests passed |
| Documentation-checker controls | 10 tests passed |
| API/declaration/type contracts | Passed, including rejected new invalid calls |
| Project script module check | 2 modules, 0 compile/link failures |
| Current documentation links | 107 documents, 2217 local links, no errors |
| Editor dark 1.0 / light 1.25 | Play/Stop, batch undo, duplicate undo and save passed; authored scene identical |
| Relocated standalone | 17 assets, 1 scene; launched from /tmp with no engine-root override; complete menu capture and pause assertion passed |

The cold reader checks actual body position/velocity as well as progress. Preview
checks compare changed pixels after geometry rotation, use independent red/blue
orientation controls, reject stale disabled images and invalid bindings, exercise
eight close/reopen cycles without new targets, and verify zero live targets after
world destruction. No test solely accepts a controller status or public symbol.

A first sample used a nonexistent Entity.setTransform method; the project was
corrected to the existing transform setter. A first application test waited for
SaveService status `complete` instead of its documented `completed`; the save had
actually committed. Its failing log is retained and the oracle corrected. Neither
was an engine physics/save defect, and no persistence gate was weakened.

Existing opt-in GPU diagnostics report shader compiler warnings about
shadowWorldDx/Dy in unchanged shader code. No new GL error/reset was observed;
this pass does not qualify those warnings or an unavailable NVIDIA driver.

## Bounds and human checklist

The camera shares scene lighting/weather; no isolated studio environment, separate
physics scene or new paused skeletal-animation clock was added. The original
mannequin demonstrates render-only geometry/equipment rotation, not your eventual
animated character. Virtual-list row dragging is not supported. Cross-document
pointer transfer is tested; keyboard transfer remains in its source document.
The example targets desktop/landscape. Windows, Android, browser and physical
controller/touch checks of these additions remain pending.

1. Open the editable project or relocated standalone; drag between chest/bag and
   weapon/offhand/pack slots, including an invalid slot and overweight pickup.
2. Split/merge stacks and consume food; confirm counts and health make sense.
3. Rotate/equip the mannequin, change tabs/text size, save, mutate and reload.
4. Resume with Escape, observe the falling sphere, reopen; try keyboard pickup,
   navigation, place and Back cancellation. Judge readability yourself.

See [the practical guide](UI_TRANSFERS_AND_PREVIEWS.md) for APIs and reproduction.
Main and published packages require separate operator acceptance.
