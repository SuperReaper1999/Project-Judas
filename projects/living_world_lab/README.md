# Living World Lab

Ordinary JudasJS project for M97. Open `living_world_lab.judasproj` in the editor.
[Controls, authoring, ownership limits and human checklist](../../docs/M97_LIVING_WORLD.md).

Gameplay is `Assets/scripts/controller.js`; generic volume initialization is
`volume.js` using packaged `source.js`. `Imports/quarry.judasvolume` remains the
author-owned initial source. Edit/cook and run `tools/EmbedVolumeSource.py` as
explained in the guide. Three regions share immutable initial assets and acquire
independent density during Play. Save slots preserve gameplay edits; Stop resets.
