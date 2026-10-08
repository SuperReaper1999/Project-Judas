# Character Lab asset provenance

The two complete figures are original M70 content, dedicated under CC0-1.0 in
`Sources/LICENSE.txt` and `Assets/LICENSE.txt`. They are reproducibly generated
by `scripts/create_m70_lab.py`, then cooked by Judas's ordinary named import
service. No original/restricted Skate character, artist rig or cooked derivative
is included. Existing local consumer workspaces are untouched.

| Fixture | Source structure | Explicit variation |
|---|---|---|
| `Sources/figure.gltf` | 65 joints, fingers/helper branches, six meshes, two complete skin palettes with different order, four clips | Original names and dimensions |
| `Sources/alternate.gltf` | Same full structure and independently bound parts | Every joint renamed; width ×1.18 and limb scale ×0.92 |

The source has 71 hierarchy nodes (65 joints plus six mesh nodes). Normal import
retains them and appends the standard `__JudasMotionRoot`, producing 72 runtime
nodes, six parts and 130 draw-palette entries. Both fixtures use immutable shared assets
and independent instance state. `Authoring/joint-names.json` is fixture content,
not an anatomical naming convention in Judas. Physical mapping is an explicit
subset: the independently hidden visual head follows the mapped neck without
requiring a zero-scale collider. Fingers/helpers remain in the skeleton.

Original animation sources are `Idle`, `Carry`, `Wave`, and `HeadHidden`.
Imported/cooked assets and recipes remain project-owned. The font is the existing
redistributable DejaVuSans, whose separate notice is kept in `Assets/fonts/`.

Fixture import reports, final-source hashes and executed results are recorded
under `docs/evidence/m70` after candidate validation; human review remains pending.
