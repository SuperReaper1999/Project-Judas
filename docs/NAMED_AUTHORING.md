# Named content and authoring tools

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
quaternions. World-region rotations preserve the region **XYZW** record convention.
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
and other gameplay content do. Save/content compatibility remains explicit.

## Complete current-field coverage

| Family | Representation and normal authority | Fields covered / proof |
|---|---|---|
| Project | Named setting groups → `Project::ParseFromString` / serializer | Name, independent water policy, save/icon identity, directories/start/export/exclusion scenes, composition manifest, all input bindings, tag/collision/render registries, navigation profiles/areas, localization/fallback/fonts, audio groups |
| Scene / prefab | Ordered objects with `id`, `name`, canonical `fields`; settings block → `SceneSerialization` and `Prefab` | All current writer fields: transforms/parent, prefab source/maps/overrides, tags/layers; render/material/mesh/subassets, lights/cameras, physical materials/body/compound/cavity, gravity/regions, motor, joints, skeleton/pose/layers/limb targets/socket/ragdoll, particles, audio, UI, scripts/properties, navigation, Basic Water/fluid-volume/deformable/fracture, terrain/atmosphere/combustion; current scene environment/fluid/fidelity settings |
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

Runtime rendering uses this same scene/prefab field path for authored and durable
render state. Optional fields include object `render-visible`; `render.visible`,
`render.instance-overrides-v1`, `render.part-materials-v1`,
`render.runtime-materials-v1`, `render.material-overrides-v1`; and settings
`sun-enabled`, `sun-intensity`, `appearance-reset-v1`. Versioned values are bounded
encoded strings validated by the ordinary material/appearance codecs, not a new
document format. Default omissions preserve historical schema-5 fingerprints.
Use normal editor/converter output for these encodings. Runtime calls remain
isolated from authored files; [runtime-rendering](RUNTIME_RENDERING.md) describes reset/save/retention ownership.

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
Recipes have no legacy format. Compound geometry needs the ordinary collision-authoring closed
physical source validation; use a lower-resolution collision proxy deliberately.
Physical sweep/arc frames require unit entity scale; edit profile/path/radius dimensions instead. Import recipes use the established model-import format/services, not this JSON envelope.
`--dependencies` is typed direct asset discovery without constructing a JS VM;
asset browser traversal covers normal prefab/UI/material/model/collision sources.
Dynamic JS IDs are not statically linked: export still packages all registered project
runtime assets and validates supported transitive dependencies.

## Editor ↔ source

Open **View → Terrain and world tools → World building / named source**. Scene source and file-document
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

Point lights may omit `light.cone`; the ordinary 15°/25° component defaults are
retained for canonical round-trips, and the cone does not affect point lighting.
Spot lights require `light.cone` (inner/outer degrees). Explicit cone values on
either kind must remain finite and satisfy `0 <= inner <= outer < 90`, with a
positive outer angle. The same rule applies to named and line-oriented scenes.

## Structured JSON ingest

The preferred editable envelope is the named document above. `--structured` is
an additional bounded ingest form calling the same serializers/validators; it is
not a second runtime format. For example, convert this UI JSON to a `.judasui`:

Build `judas_scene_author`. Use:

```sh
build/judas_scene_author --structured input.json output.judasui
```

This developer tool parses named JSON into the existing engine structs, then calls
**the same scene/UI/input serializers and validators used by the editor**. Defaults
come from those structs. It is not a new runtime format. Unknown fields/types fail.
Input is bounded to 8 MiB and arrays to 4096 entries. It supplies no game runtime.

```json
{"kind":"ui","reference":[1280,720],"elements":[
 {"id":"root","kind":"canvas","relativeSize":[1,1]},
 {"id":"title","parent":"root","kind":"text","text":"My game",
  "offset":[30,30],"size":[600,60],"fontSize":32},
 {"id":"start","parent":"root","kind":"button","text":"Start",
  "offset":[30,100],"size":[240,50],"background":[0.1,0.3,0.5,1]}
]}
```

Optional element names correspond to [UI layout](judasjs/ui.md): visible, enabled,
clip, wrap, fit, font/texture stable asset IDs, textKey, anchorMin/anchorMax, offset,
size, relativeSize, align/textAlign, margin/padding (left/top/right/bottom), background,
color, spacing, fontSize, value/minimum/maximum. `flow`: free/horizontal/vertical.
`direction`: auto/ltr/rtl; `textLogicalAlign`: -1 physical, 0 start, 1 centre, 2 end.
Document visible/enabled/modal are optional. Parent IDs must exist, cycles fail.
Vectors are arrays with exactly two/four finite numbers. On disk: `JudasUI 1`;
direction/localized text features use the compatible version-2 serializer as needed.

```json
{"kind":"input","entries":[
 {"name":"move_x","axis":true,"bindings":[
  {"control":"key:A","scale":-1},{"control":"key:D"},
  {"control":"stick:LeftX","deadzone":0.15}]},
 {"name":"confirm","bindings":[{"control":"key:Return"},{"control":"pad:South"}]}
]}
```

Writes InputMap version 1's existing serialized record, ready for the project's
`input-map` quoted field. Names must be unique; normal control/deadzone validation
applies. This does not overwrite a project's other settings.

```json
{"kind":"scene","name":"My scene","objects":[
 {"id":"1","name":"Floor","components":["render","body"],"fields":{
   "position":"0 -0.5 0","render.half-extents":"5 0.5 5",
   "body.half-extents":"5 0.5 5"}},
 {"id":"2","name":"Visitor","components":["motor"],"fields":{
   "position":"0 1 0","motor.gravityScale":"1"}}
]}
```

Writes `JudasScene 3` with engine settings/component defaults. Stable id/parent are
**decimal strings**. Optional parent defaults to 0. Available default components:
render/body/motor/gravity/animation/socket/joint/ragdoll/ui/particle/audio. Required
references for sockets/UI/skeleton mappings must then be supplied. `fields` maps
named **existing scene record keys** to their literal values (strings); quoted asset
strings use `"\"asset-id\""`. Component header changes such as `body:"dynamic box"`
are possible. Missing fields use actual engine defaults; no counting trailing tokens.

These fields are also the editor/prefab property contract (ObjectProperties /
ApplyObjectProperties). Complex region world/manifests and asset cooking use their
existing tools rather than a parallel schema. Runtime handles never belong in files.
