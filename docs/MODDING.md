# Local mods and curated game kits

Modding is optional and off by default. Ordinary projects do not discover or
execute mods. Game authors opt in through their project-owned `mod-contract`
(quoted JSON in text projects, `modContract` in JSON projects).
See the [JudasJS catalog](judasjs/mods.md) and [authoring reference](SDK_AUTHORING.md).

## Game-owned contract

A contract declares the game identity, exact game/contract versions, allowed
content kinds, explicitly published references, replacements and extension
points. Mods cannot enlarge it. Get the current registry fingerprint with:

```sh
judas_mod registries /path/game.judasproj
```

Use that output in `registries`; changing input, classification, navigation,
localization or audio registries requires a newly matching contract. A minimal
contract is:

```json
{
  "format": 1, "enabled": true, "executable": false,
  "game": "my-game", "gameVersion": "1.0.0", "version": "1.0.0",
  "registries": "REPLACE_WITH_THE_64_HEX_DIGIT_REGISTRY_FINGERPRINT",
  "additions": ["mesh", "scene"],
  "published": {}, "behaviors": {}, "replacements": {},
  "extensions": {}, "editableSources": []
}
```

This permits additions but publishes none of the game's private content. Publish
all dependencies of any shared resource explicitly. Include source licences and
notices in `editableSources` when distributing them. A contract is not a security
sandbox: executable mods are trusted project JavaScript with ordinary world APIs.
Enabling them requires game permission and user consent to the exact payload hash.

## Author and package

Use a matching SDK. Mod authors need no C++ compiler. The editor's local-mods
panel and `judas_mod` use the same kit, validation and packaging services.

```sh
judas_mod kit /game/game.judasproj /new/CuratedKit /sdk/docs
judas_mod new /new/CuratedKit /new/MyMod my-mod
judas_editor /new/MyMod/my_mod.judasproj
judas_mod validate /new/MyMod/my_mod.judasproj
judas_mod package /new/MyMod/my_mod.judasproj /new/my-mod.judasmod
judas_mod inspect /new/my-mod.judasmod
```

Use the project filename actually printed by `new`. Its `mod.json` is an author
draft: change the name, licence, notices and declarations before packaging.
The [minimal manifest](judasmod.example.json) has no historical lab dependencies.
The packer fills and verifies owned asset/file inventories. Base kit resources
are read-only; copy an explicitly published source before editing it.

Kits use `Judas-ModKit-1`. Previous milestone-labelled kits are intentionally
retired. The kit copies only declared base resources and matching offline docs;
it never copies the private game wholesale. Terrain V2 authoring uses the normal
native terrain pipeline; retired M75 terrain fork/cook commands no longer exist.
The current mod format does not admit native `.judasterrain` assets. Terrain
remains available to the base game; mod-owned terrain assets and terrain-state
merging are not claimed. Ordinary cooked mesh/scene additions remain supported.

## Install, select and launch

```sh
judas_mod install /game/game.judasproj /new/my-mod.judasmod
judas_mod list /game/game.judasproj
judas_mod enable /game/game.judasproj my-mod 1.0.0 EXACT_SHA256
judas /game/game.judasproj
judas /game/game.judasproj --no-mods
judas_mod disable /game/game.judasproj my-mod
judas_mod remove /game/game.judasproj my-mod 1.0.0 EXACT_SHA256
```

Add `--trust-executable` to enable only when intentionally trusting executable
content. Installation does not enable it. Changes apply to a new session, not a
live hot reload. Bind authoring dependencies with `judas_mod bind-dependency
PROJECT PACKAGE`, declare them in the manifest, and use published imports/exports
rather than private filesystem paths. Scripts see detached `mods.active`,
`mods.contributions(point)` and `mods.resolve(owner,name)` descriptors.

## Validation and limits

Package format 1 inventories every regular file with path, size and SHA-256.
Versions are exact `x.y.z` or `>=x.y.z <a.b.c`, numeric components 0..65535;
no caret, wildcard, prerelease range or automatic version selection. Paths are
portable UTF-8 compared after NFC/case folding. Traversal, absolute/drive paths,
backslashes, reserved names, aliases, links and undeclared references reject.

Limits: metadata 1 MiB, individual file 8 MiB, package 64 MiB, 4096 files,
16 selected mods, 128 dependency edges, 512-byte paths; installed payload and
cache budgets are separately 512 MiB. A kit copy permits 8192 files/512 MiB.
Content must fit those limits. Mods cannot add numeric registries. Owned world
manifests, deformables, environments, audio effects, localization catalogs and
sprite-sheet assets remain unsupported by the mod package validator; ordinary
base-game capabilities are unaffected. There is no online mod distribution,
hostile-code isolation, registry merging or arbitrary JSON patching.

Missing, changed, disabled or conflicting saved mod context refuses load before
replacing the live world. Saves retain exact versions, hashes and resolution and
contract identities. No silent enabling, downloading or migration occurs.
Inspection and packaging compile scripts where needed but do not evaluate them.
Fix the named phase/reference and republish instead of disabling validation.
