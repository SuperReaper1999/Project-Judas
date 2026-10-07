# World Workshop — M67 candidate

Ordinary project: `world_workshop.judasproj`; startup `Scenes/workshop.judas`.
364 authored objects, editable scene-owned recipes, a six-part non-humanoid rig,
localized UI, an 800 m marker and two independently streamed regions.

## Play

WASD / left stick: move. Mouse / right stick: look. Space: script-driven launch.
V: first/third-person on the same motor. Z: spawn independently initialized debris.
G: ragdoll/return on the displayed rig. L: English/Arabic. Escape: pause/resume;
the menu also offers resume/reload. R: reload. F6/F7: modern slot save/load.
Walk toward negative Z to visit east (−45 m), west (−95 m), then return. The script
adopts the first region traveller into the root and retains a normal save reference.

The quarter ramp is right of the start, stairs left, ledge/corridor farther ahead;
rails and repeated columns show independent recipe parameters. Look upward/toward
negative Z to see the distant turquoise marker; the HUD projects it when visible.
Near/far are .15/2000 m. Resize and switch cameras to exercise the same projection.

## Author

Open in `judas_editor`; **View → World building / named source**. Choose a scene
recipe, change spacing/profile/path/segments, Preview/cook, then Apply (one undo).
Review explicit removal IDs. Undo/redo preserves surviving IDs and overrides.
Detach keeps ordinary content. Select multiple entities with Ctrl/Shift; inspect
mixed fields, alignment/snaps, folders, explicit reparenting and references.

Select `Assets/ui/workshop.judasui` in the asset browser. Open Edit UI document,
enable Runtime canvas preview, choose size/locale, change layout/text/style, undo,
then edit the Named source and apply it. The same file/IDs are used in Play/export.
Edit project input under the world-building file source (`project` kind) or CLI.
Saving project settings requires explicit project reopen; it is not gameplay hot reload.

The six-part figure is original redistributable source art, not Claude's rig.
Select it → Skeleton selection/mapping tools: pick joints, preview fitted boxes,
accept, batch only explicitly enabled limits/resistance. Socket/IK inspectors use
existing stable joint keys. Rest-pose fitting is an authoring estimate, not humanoid
physics or an automatic resistance tuner. Existing physical intent is preserved.

## Reproduction

`tools/m67_fixture.py` creates glTF source art only. The orchestration in
`scripts/m67_create_workshop.py` creates documents/templates, applies JSON patches,
registers assets, imports, fits and generates recipes by invoking the shipped
`judas_scene_author`. It does not emit positional scene/physics records. Its
`--project` destination is a fresh working project; do not reconstruct an edited
project over your changes. See [supported commands](../../docs/NAMED_AUTHORING.md).
Scene-owned recipes are authoritative after placement; files in `Authoring/` are
reusable input presets and cannot silently overwrite live IDs/overrides.

## Rights

Original fixture geometry/animation, recipes and project scripts/content authored
for this milestone are dedicated under CC0-1.0 (see LICENSE.txt). Font files are
copies of the accepted text fixture: DejaVu and Noto Sans Arabic retain the bundled
font-license notices. No restricted original Skate/M66 source art is distributed
by this project. Imported/cooked derivatives of this original fixture are included.
