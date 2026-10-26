# Lunar Island asset provenance

- Island reference image: supplied by Conner, copied unchanged into ArtSource.
  Used as geographical inspiration and the in-game field map; labels deliberately
  distinguish planned later regions from Stage 1 gameplay.
- coastal_pine, silver_birch, island_oak, fern: generated locally through Claude's
  Prompt3D trained parser and procedural generators. Exact prompts/seeds/parameters
  and generated model hashes are retained in generator_receipts.json and prompt3d/.
  No external generation service, downloaded model or retraining was used.
- woodsign: original Lunar Island timber/board geometry and raised HOMESTEAD /
  FOREST TRAIL mesh lettering using Prompt3D primitives; source in generate_assets.py.
- axe, pickaxe, rock, campfire, chopping_stump, barrel, character_adventurer, log,
  well, wood_crate: existing operator-authorised original Judas Hearthworld art,
  copied from projects/hearthworld/Assets/models (rock source: rock_cluster.glb).
  Those original source generators/settings remain in the preserved Hearthworld
  project. No commercial/ripped asset library was introduced.
- Terrain geometry, ground textures, scene composition and short PCM sound effects:
  original Lunar Island procedural authoring, exact source in author_project.py.
- DejaVuSans.ttf: redistributed with its upstream licence alongside the font in
  Assets/fonts/LICENSE.txt. Font licence remains distinct from project MIT terms.

The realistic-art pass uses installed ambientCG material scans through Prompt3D.
Source asset URLs, creator, CC0-1.0 status and exact scan hashes are recorded in
`materials/PROVENANCE.json`; the source arrays and original notice are preserved
alongside it. The official licence was checked at https://docs.ambientcg.com/license/.
Commercial use and redistribution are allowed without mandatory attribution.
No material downloads were needed: these were already installed in Claude's tool.
Local generated texture overrides were explicitly excluded. Foliage/ceramic maps
use the tool's original procedural implementation.

Reused static models received UVs and a PBR finish while retaining their geometry
and baked node transforms. The animated player rig remains unchanged. Tree/plant
geometry uses the realistic preset with embedded colour/normal/roughness maps;
canopies remain simplified procedural forms. Raw generated originals, including
the pot removed from the fern, remain under `prompt3d/`.

 The optional Unity SFX Designer
was not installed or used; the operator will arrange its Linux port separately.
External Prompt3D and independent research directories were kept read-only.

## Stage 2

House modules, pitched roofs/gables, steps, opening frames, barricades and building
panels are original author_stage2.py geometry. Furniture reuses Conner's original
LowPolyAssets starter pack exports (treasure chest, bed, chair, table, sofa,
cabinet, bookshelf, smith workbench and desk lamp), with source model paths and
hashes in asset_catalog.json. Geometry is flattened, UV-mapped and normalized by
the retained project tools, with PBR surfaces from the installed Prompt3D material
library. Source fabric/leather CC0 arrays and receipts were copied alongside the
other scans. The generator and original starter pack were not modified.
These are offline authoring inputs; the game package is self-contained.
