# M80 lab generation: invalid asset identifier length

The first `tools/CreateBendingLab.py` generation completed its four asset bakes,
but the subsequent independent metadata/reference check failed. The handwritten
hexadecimal prefix contained 26 characters; appending an eight-character suffix
produced 34-character identifiers. This was caught before launching the project.
It is a content-generator defect, not evidence of a runtime or solver failure.

The observed assertion named `Assets/worlds/lab.judasworld.judasmeta` and its
identifier `808080808080808080808080800000000a`. A follow-up length inspection
reported 34 characters for all 14 newly generated metadata identifiers.

Correction: generate the prefix as `'80' * 12`, then append the eight-character
suffix; update the two JavaScript prefab references to those same identifiers.
Regenerate the new lab's assets/scenes. No historical project was modified.

The repeated check passed:

```text
14 unique asset IDs; all referenced asset IDs resolve.
Project files: 34
Bytes: 913930
radial.judas 21 authored entities
annex.judas 2 authored entities
flat.judas 21 authored entities
```

The check independently read every `.judasmeta`, required exactly 32 lowercase
hexadecimal characters, required uniqueness and an existing source asset, and
resolved each 32-character asset reference in the project, scenes, prefab,
world, UI and JavaScript files. The lab script also passed ES-module syntax
checking. These checks establish content/reference consistency; they do not
establish physical behaviour or human acceptance.
