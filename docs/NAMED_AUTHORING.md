# Supported content authoring — M67 candidate

[World-building workflow](M67_WORLD_BUILDING.md) · [JudasJS](JUDASJS.md)

## One editable document

Schema **1** uses UTF-8 JSON: `{"kind":"scene","schema":1,"data":{...}}`.
Kinds are `project`, `scene`, `prefab`, `ui`, `input`, `world` and `recipe`.
The ordinary file extensions remain `.judasproj`, `.judas`, `.judasprefab`,
`.judasui`, `.judasworld`, `.judasrecipe`; standalone input may use `.json`.
The normal loaders detect named content in these same files. Editor Save preserves
an existing named file; conversion is explicit. There is no second editable
positional counterpart. Existing positional readers remain available.

`--create` supplies complete valid defaults. `--inspect` prints normalized named
content for discovery. Scene component names are the canonical property names
already used by the serializer. Unknown fields fail instead of disappearing.
Optional components are absent until added; existing component validators govern
required fields, references, finite values, shapes and ranges.

```json
{"kind":"scene","schema":1,"data":{"settings":{},"objects":[
 {"id":1,"name":"Floor","fields":{
  "position":[0,-0.5,0],
  "render":{"symbol":"box"},"render.half-extents":[10,0.5,10],
  "body":{"motion":{"symbol":"static"},"shape":{"symbol":"box"}},
  "body.half-extents":[10,0.5,10]
 }}
]}}
```

Positions/dimensions are metres; colors are linear component values where the
existing component says so. Transform rotations and recipe frames use **WXYZ**
quaternions. World-region rotations preserve the M59 **XYZW** record convention.
Vectors are fixed-length numeric arrays. Enums written as bare legacy tokens use
`{"symbol":"box"}`; quoted strings remain strings. `body`, `gravity`,
`gravity.region`, `fidelity-policy`, `light.cone`, compound children and cavities
have named members rather than unlabeled mixed tuples. Compatibility tuple input
is also accepted for those compound records. Mask/ID numbers retain existing
integer width rules; references never follow display names.

Complex pre-existing serialized JSON fields can remain exact strings. This keeps
old script-property/pose-layer bytes and fingerprints intact. To author their
contents directly use `{"json":{...}}`; that explicitly changes their serialized
payload. Whitespace/property order in the surrounding named document does not
change normalized meaning. Whitespace **inside an opaque payload string** is
part of that existing field's data, not formatting of the surrounding document.

Objects, script slots, tracks, layers, UI elements and bindings preserve ordered
arrays. Category IDs, prefab maps, asset IDs and qualified world references keep
existing identities. Deterministic writers normalize defaults/order; comments
and arbitrary original whitespace are not round-tripped. Scene fingerprint
schema remains **5**. Editorial folders/recipe metadata participate in undo/dirty
state but do not alter that gameplay fingerprint. Generated geometry, transforms
and other gameplay content do. M61 compatibility is still enforced.

## Complete current-field coverage

| Family | Representation and normal authority | Fields covered / proof |
|---|---|---|
| Project | Named setting groups → `Project::ParseFromString` / serializer | Name, legacy policy, save/icon identity, directories/start/export/exclusion scenes, composition manifest, all input bindings, tag/collision/render registries, navigation profiles/areas, localization/fallback/fonts, audio groups |
| Scene / prefab | Ordered objects with `id`, `name`, canonical `fields`; settings block → `SceneSerialization` and `Prefab` | All current writer fields: transforms/parent, prefab source/maps/overrides, tags/layers; render/material/mesh/subassets, lights/cameras, physical materials/body/compound/cavity, gravity/regions, motor, joints, skeleton/pose/layers/limb targets/socket/ragdoll, particles, audio, UI, scripts/properties, navigation, liquid/deformable/fracture, terrain/atmosphere/combustion and retained legacy components; all scene environment/fluid/fidelity settings |
| UI | Ordered, named element objects → `UIDocument` | Document reference/visible/enabled/modal; ID/parent/kind/flow/direction; anchors/offset/size/relative size/alignment/margins/padding/spacing/clip; visible/enabled/wrap/image fit; text/font/font size/localization/logical alignment/row mirroring; texture/colors/background; slider/toggle values/ranges |
| Input | Ordered named entries/bindings → `InputMap` | Action/axis/vector name/kind, controls, scale/scaleY, deadzone and circular response; no raw device-policy replacement |
| World composition | Named budgets/regions/references → `WorldManifest` | Every current budget, region ID/scene/origin/rotation/extents/priority/policy/estimated bytes/dependency, qualified endpoints/field/hard-soft policy; soft references remain limited to texture cameras |
| Recipe | Named parameters + stable element history → `WorldRecipe` | Frame, source entity/prefab, operation/count/spacing/radius/arcs/path/profile/twist/smooth/closure/segments/proxy segments, stable keys/IDs/prior generated values and geometry revisions |

Scene/prefab coverage tests round-trip **every loadable ordinary project scene and
prefab** and require identical normalized legacy writer bytes; family tests also
cover Unicode, locale, ordering, defaults, independent reference expectations,
invalid IDs/keys/types/versions/cycles and schema-5 identity. This is a current-field
coverage contract, not a new replacement component schema. Unknown future fields
block conversion. A restricted/missing external asset can still prevent project
build/export even when its document parses; complete validation happens at build.

## Actual CLI

The M72 candidate uses this same scene/prefab field path for authored and durable
render state. Optional fields include object `render-visible`; `render.visible`,
`render.instance-overrides-v1`, `render.part-materials-v1`,
`render.runtime-materials-v1`, `render.material-overrides-v1`; and settings
`sun-enabled`, `sun-intensity`, `appearance-reset-v1`. Versioned values are bounded
encoded strings validated by the ordinary material/appearance codecs, not a new
document format. Default omissions preserve historical schema-5 fingerprints.
Use normal editor/converter output for these encodings. Runtime calls remain
isolated from authored files; [M72](M72.md) describes reset/save/retention ownership.

Build the existing `judas_scene_author` target. Paths below are examples; keep
experiments in a copied project. Commands reject invalid data with nonzero status.

```sh
build/judas_scene_author --create scene /tmp/my-scene.judas
build/judas_scene_author --inspect scene /tmp/my-scene.judas
build/judas_scene_author --validate scene /tmp/my-scene.judas
build/judas_scene_author --convert scene old.judas copy.judas named --dry-run
build/judas_scene_author --convert scene old.judas copy.judas named
build/judas_scene_author --convert scene copy.judas legacy-copy.judas legacy
build/judas_scene_author --edit scene copy.judas edited.judas /data/objects/0/name '"New name"'
build/judas_scene_author --patch scene copy.judas edited.judas edits.json --overwrite
build/judas_scene_author --diff scene copy.judas edited.judas
build/judas_scene_author --references scene edited.judas
build/judas_scene_author --dependencies scene edited.judas
build/judas_scene_author --object-template render,body Floor /tmp/floor.judas
build/judas_scene_author --generate recipe.judasrecipe scene.judas project.judasproj generated.judas --dry-run
build/judas_scene_author --generate recipe.judasrecipe scene.judas project.judasproj generated.judas --overwrite --accept-removal
build/judas_scene_author --create-import project-root source.gltf Assets/models/model.judasmodel
build/judas_scene_author --import-model project-root/Imports/model.judasimport
build/judas_scene_author --track-asset project.judasproj project-root/Assets/file.judasui
build/judas_scene_author --cook-collision model.judasmodel stable-mesh-id collision.judascollision --overwrite
build/judas_scene_author --fit-skeleton scene.judas project.judasproj 6 Spindle,Fork,Branch-A .18 fitted.judas
build/judas_scene_author --build-project project.judasproj /tmp/package build/judas .
```

Patch files are JSON Patch arrays, up to 256 operations: `add`, `replace`, `remove`,
`test`. `--edit` requires an existing JSON pointer; use `add` for an absent field.
`--overwrite` is explicit; `--dry-run` performs document validation without writing.
Recipes have no legacy format. Compound geometry needs the ordinary M64 closed
physical source validation; use a lower-resolution collision proxy deliberately.
Physical sweep/arc frames require unit entity scale; edit profile/path/radius dimensions instead. Import recipes use the established M66 format/services, not this JSON envelope.
`--dependencies` is typed direct asset discovery without constructing a JS VM;
asset browser traversal covers normal prefab/UI/material/model/collision sources.
Dynamic JS IDs are not statically linked: M38 still packages all registered project
runtime assets and validates supported transitive dependencies.

## Editor ↔ source

Open **View → World building / named source**. Scene source and file-document
source retain separate dirty drafts; Apply validates a candidate before one undo
transaction. Scene generation divergence requires explicit apply/discard, and
external disk edits block Save. Scene **Review external semantic diff** and reload
retain the pre-reload draft for recovery. Other documents explicitly require
save/discard and reload; there is no implicit live project/VM hot reload.
UI has the same source/visual document, undo/redo and disk-divergence policy in
**Assets → select UI → Edit UI document**. Reload labels explicitly say discard.
Invalid drafts stay visible while the validated document stays intact.

Syntax diagnostics include JSON parser byte/line context where available; semantic
errors carry the field/component/entity context supplied by normal validators.
Some semantic line numbers refer to the derived canonical record, not the original
JSON line. Use the reported property/ID and source search; there is no full JSON
source-map debugger. The editor source buffers are 4 MiB; named parsing is bounded
to 32 MiB/depth 64, with tighter normal-family limits (world 1 MiB, UI 2048 elements,
recipes 4 MiB/depth 32, profile 64/path 256/segments 512/elements 2048).

## Publication and recovery

Jobs hold immutable input, exact parameters and project/document generation.
Recipe acceptance rejects stale generations/parameters/source-prefab hashes.
Existing generated revisions must match expected bytes; changed geometry gets new
asset IDs so undo cannot point to an overwritten collider. Old revisions can be
retained for undo; remove unused registered products deliberately after review.
For new geometry revisions, IDs derive from recipe identity, product role and
geometry digest; matching accepted revisions keep their IDs. Repeated generation
from the same source in independent project roots produces byte-identical scene
and cooked products. This does not promise byte identity for unrelated import
backends or arbitrary future tool versions.

Cross-file publication stages `.new`/`.old` bytes and a journal under
`Authoring/recovery` or `.authoring-recovery`, checks expected destination bytes,
then replaces each file atomically. Failure rolls back outputs still owned by the
transaction; outside edits are retained. Failure messages give the recovery journal
location and destination/hash list. Keep that directory and restore reviewed
`.old` bytes manually if needed. This is **recoverable per-file publication**, not
filesystem-wide or crash atomicity, fsync durability or an automatic crash-recovery
service. Normal single-file named saves use a unique adjacent stage and baseline
check too. Worker code does no raw GL or live RuntimeWorld mutation.
