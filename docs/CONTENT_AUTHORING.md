# Content authoring

Current M67 candidate: [supported named documents and CLI](NAMED_AUTHORING.md)
are editable in place through ordinary loaders/editor/export.

## Historical M65 structured converter (retained compatibility workflow)

Build `judas_scene_author`. Use:

```sh
build/judas_scene_author --structured input.json output.judasui
```

This developer tool parses named JSON into the existing engine structs, then calls
**the same scene/UI/input serializers and validators used by the editor**. Defaults
come from those structs. It is not a new runtime format. Unknown fields/types fail.
Input is bounded to 8 MiB and arrays to 4096 entries. It supplies no game runtime.

## Menu without positional records

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

## Input

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

## Scene

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
ApplyObjectProperties). Complex M59 world/manifests and asset cooking use their
existing tools rather than a parallel schema. Runtime handles never belong in files.

A complete copyable menu source is
`projects/m65_integration/tools/integration_ui.json`. M65 checks valid and invalid
scene/input/menu generation and round-trips through normal loaders. Existing raw
records remain accepted unchanged. No scene/UI/fingerprint schema bump was required.

## Project export scene selection (M65)

Project version 1 accepts `export-scenes` and `exclude-scenes`. Each value is an
outer quoted string containing a list of quoted, normalized project-relative paths:

```text
export-scenes "\"Scenes/main.judas\" \"Scenes/annex.judas\" "
exclude-scenes "\"Scenes/unused.judas\" "
```

An empty include list means all registered scenes. Exclusions then apply. Startup
and world-manifest region scenes must remain included; excluding either fails export.
Registered runtime assets remain available for dynamic loading. This is an explicit
scene policy, not JavaScript dependency analysis. The editor Project panel authors
these settings through the normal project serializer.
