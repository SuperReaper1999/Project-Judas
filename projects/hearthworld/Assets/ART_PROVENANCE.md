# Hearthworld art

These are self-contained GLB models in metres, with local **+Y up** and **+Z
forward**. Local authoring axes do not prescribe the world's gravity. The home
has an actual open entrance and side windows; they are absent wall geometry,
rather than textures. Its front faces +Z. The scene author owns collision;
importing a visual model alone does not create a physical body.

## Verified Claude tool

The available tool is the local `ClaudesNewML-AI-Thing` project, whose command is
`python -m prompt3d`. Its trained `.npz` classifier/tagger select supported
procedural geometry and parameters. It is **not** a diffusion model inventing
arbitrary meshes. The trained files and implementation were inspected before
use. No model was downloaded, trained or served for Hearthworld.

The actual prompts used were:

| Model | Prompt | Seed |
| --- | --- | --- |
| Pine | `a low poly pine tree 7 metres tall` | 7421 |
| Birch | `a low poly birch tree 6 metres tall` | 7422 |

Both went through the trained parser, which returned category `tree`, the
requested species, height and `lowpoly` style. The resulting trunk/branches and
foliage were exported separately with matching root pivots, alongside whole
models. Their geometry is unchanged by the split. Dimensions vary around the
prompt's nominal height because the tool deliberately introduces seeded size
variation.

The cottage, axe, shovel, pickaxe, hammer, gravity gun, backyard sign, fence, wall, floor, pillar,
stone foundation, stone wall and log are original Hearthworld geometry built
with the verified tool's mesh primitives/exporter. They are procedural
authoring, **not claimed as ML-generated geometry**. This provides intentional
gameplay dimensions and collision specifications that a free-form model would
not guarantee. No image generation was needed.

## Reused original Judas assets

`rock_cluster`, `wood_crate`, `barrel`, `bed`, `bench`, `tool_rack`, `well`,
`campfire`, `chopping_stump`, `hanging_lantern` and `character_adventurer` are copied byte-for-byte from
Conner / SuperReaper1999's original Judas Low Poly Starter pack. Those models
were originally authored in Blender for this repository; no third-party model
pack was downloaded. The original local pack and its editable masters remain
unchanged.

Campfire flames and lantern details in these models are geometry. They do not
imply a combustion, light or cloth simulation. Tree foliage is opaque faceted
geometry. Small branches/leaves use visual geometry, while the game explicitly
authors bounded trunk collision for felling.

## Rebuild and inspect

From the Judas repository root, with the two existing external authoring
folders available:

```sh
PYTHONDONTWRITEBYTECODE=1 /path/to/ClaudesNewML-AI-Thing/.venv/bin/python \
  projects/hearthworld/tools/generate_art.py \
  --prompt3d /path/to/ClaudesNewML-AI-Thing \
  --starter-pack /path/to/low_poly_starter/Exports
```

The tool reads the external directories and writes only this project's art.
The output does not require Python, Prompt3D, an ML service or Blender at
runtime. The game package contains the GLBs it uses.

`art_sources/asset_catalog.json` records paths, hashes, dimensions, geometry
counts, pivots and the cottage collision specification. The corresponding
`generator_receipts.json` preserves parsed prompts and SHA-256 hashes of the
actual generator source and trained models. Those hashes identify the tool
used; they are not a claim that its external repository is bundled here.

For an optional visual overview:

```sh
blender --background --factory-startup -noaudio --threads 2 \
  --python projects/hearthworld/tools/preview_art.py
```

The generated preview is an authoring overview, not a screenshot of gameplay.
GLBs can also be imported directly into Blender for manual editing.

`art_sources/native_import_report.json` records a native Judas importer pass over
the exported model files and their exact hashes. The reused adventurer includes
the original Idle, Walk, Run and Jump clips, which are presentation animations;
they do not provide a movement controller or extracted root motion.

## HUD and audio

`tools/generate_ui_audio.py` rebuilds the ordinary `.judasui` document and seven
original short sound effects. The HUD uses project-owned native text, panels and
buttons, with the separately licensed DejaVu font already included under
`Assets/fonts/`. Menu and inventory root panels start hidden; project scripts
control visibility, focus, pause and button meaning.

Sounds are deterministic synthesized PCM, with no borrowed recordings or
external samples. `audio/PROVENANCE.json` records the seven registered asset IDs,
sample counts and exact output hashes. The generator is an authoring tool; the
standalone only needs the resulting UI, font and WAV assets.
