# Local mods and terrain-capable kits — M76 candidate

This describes the M76 provisional checkpoint (**NOT HUMAN VALIDATED**), based on M75 checkpoint
`38d345883a541d4eb2faa43c321e940d223fce09`. Human review is pending.
See [M76 implementation/evidence](M76.md) and [JudasJS catalog](judasjs/mods.md).

## Ownership and trust

Modding is optional and **off by default**. Omit `modContract` for ordinary
projects. No Mods-directory discovery or script auto-enable occurs. A game's
versioned contract is authored in its `.judasproj` and exported unchanged; mods
cannot enlarge it. See the complete working contract in
`projects/modding_lab/modding_lab.judasproj` (JSON member `modContract`).

Judas resolves content. A game publishes references, permitted replacements,
property schemas and extension points. Its JavaScript interprets contributions.
Executable mods are **trusted project code, not sandboxed code**. They have the
ordinary JudasJS world capabilities. File policy and callback budgets are not
hostile-code isolation. Inspection, installation and validation compile scripts
where necessary but never evaluate them. Enabling requires consent to the **exact
payload SHA-256**, not just its name/version. There are no install hooks, native
plugins, downloads, Node or extra filesystem/network script bindings.

## Format and support

`.judasmod` v1 reuses Judas's bounded binary archive primitives without compression.
Manifest v1 inventories every file's path, byte size and SHA-256 and declares
identity/version/game/contract, notices, owned assets/scenes, dependencies,
imports/exports, contributions and whole-resource replacement requests.
Hashes identify bytes, not authorship or trust. Versions are exact `x.y.z` or
`>=x.y.z <a.b.c`; each numeric component is 0..65535 with no leading zeros.
No caret/wildcard/prerelease grammar or automatic installed-version selection.

| Content | v1 policy |
| --- | --- |
| Cooked `.judasmodel`, textures, materials, physical materials | Normal registered dependencies; external raw-model files must be cooked. |
| M75 terrain | Editable source stays in kit/author project; package includes matching cooked mesh/texture/collision and offline navigation. |
| Scripts, scenes, prefabs | Registered normal content; executable permission and exact trust. Data-only may instantiate explicitly published base behavior with validated properties. |
| Audio, fonts, authored UI, collision/navigation assets | Normal asset/resource path and declared closure. |
| Baked animation | Part of normal cooked model; no alternate animation pipeline. |
| New numeric registries | Unsupported: reuse exactly the kit's input/classification/navigation/localization/audio registries. |
| Owned world manifests, liquid/deformable/environment assets, audio effects, localization catalogs | Rejected in v1. A base game's existing world/regions may reference expressly permitted scene replacements. |

Limits: metadata/catalog 1 MiB; file 8 MiB; complete package 64 MiB;
4096 files/assets; 16 selected mods; 128 dependency edges; paths 512 UTF-8 bytes;
128 installed versions; installed payloads and derived cache each 512 MiB.
A kit copy is bounded to 8192 files/512 MiB. Metadata parsing limits nesting to
16 and structural events to 65536. Terrain samples retain M75's own limits;
use product sizes within the per-file cap. There is no promise that an arbitrarily
large M75 patch fits this format unchanged.

Paths are portable UTF-8, compared after NFC/Unicode casefold normalization.
Absolute/drive/UNC/traversal paths, backslashes, alternate streams, control/NUL,
Windows reserved names and trailing-dot/space aliases are rejected. Case/NFC
aliases reject rather than winning by directory order. Ordinary Unicode is valid.
No symlink/hardlink archive payloads or extraction through existing links.
Runtime cache filenames are flat hashes, separate from logical package paths.

A real author draft is available as [judasmod.example.json](judasmod.example.json).
It belongs to the lab contract; other games publish their own names/types. The
packer fills and verifies owned asset/file inventory from the saved author project.

## End-to-end commands

Use a matching Release Judas Editor/tool SDK. Mod authors do **not** compile C++.
The kit includes offline declarations/reference docs, deliberately published
resources/registries/source, an inclusion report and creation instructions;
list required license/notice files explicitly in `editableSources` too, so their
bytes accompany the curated references rather than depending on private source;
it does not copy the entire private game. `new` creates the ordinary template.

```sh
judas_mod kit game/game.judasproj /new/CuratedKit /sdk/docs
judas_mod new /new/CuratedKit /new/MyMod hill-map
judas_editor /new/MyMod/hill_map.judasproj
judas_terrain_author fork /new/MyMod/Kit/Imports/landscape.judasterrain /new/MyMod/Imports/hill.judasterrain
judas_terrain_author cook /new/MyMod/hill_map.judasproj Imports/hill.judasterrain
judas_navigation_bake /new/MyMod/hill_map.judasproj Scenes/main.judas
judas_mod validate /new/MyMod/hill_map.judasproj
judas_mod package /new/MyMod/hill_map.judasproj /new/hill-map.judasmod
judas_mod inspect /new/hill-map.judasmod
judas_mod install /moved/game/game.judasproj /new/hill-map.judasmod
judas_mod list /moved/game/game.judasproj
judas_mod enable /moved/game/game.judasproj hill-map 1.0.0 EXACT_SHA256 --trust-executable
judas /moved/game/game.judasproj
judas /moved/game/game.judasproj --no-mods
judas_mod disable /moved/game/game.judasproj hill-map
judas_mod remove /moved/game/game.judasproj hill-map 1.0.0 EXACT_SHA256
```

Use paths/manifest names actually reported by `new`; quotes are needed for spaces.
Windows uses the matching `.exe` tools and a safe short authoring root (the existing
Windows SDK long-path limitation remains). The editor's **View / Local mods / game
kits** provides kit creation, manifest draft Undo/Redo/cancel/save, packaging,
inspection, installation, exact-payload trust, selection and launch/no-mod launch.
Terrain editing uses the ordinary **View / Terrain sculpt / paint** panel and
viewport brushes, then Save, Cook and offline navigation Bake. Open published
source read-only and Fork into this mod's `Imports/`. Don't edit `Kit/`.

Manifest Save and Package are separate explicit actions. Dirty/external changes
retain normal guarded publication semantics. A dependency is bound into authoring
with `judas_mod bind-dependency PROJECT PACKAGE`, then declared in the manifest.
Only its published exports may be imported. Installation requires declared
required dependencies already installed and explicitly selected; it doesn't guess
which version you intended or grant runtime trust during compile-only validation.

## Identities, references and replacement

A writable source's runtime AssetId is the first 32 hex characters of
SHA-256(`Judas.ModAsset.1:GAME:MOD:LOCAL_ID`). The author ledger records the local
ID; qualification happens **before** cooking/baking. Base IDs stay unchanged.
Independent mods may use the same filename/local ID. Runtime collision checks
still reject actual qualified-ID collisions. Do not manually rewrite opaque cooked
assets or JavaScript strings. `judas_mod identity PROJECT LOCAL_ID` is available
for explicitly registered author assets; normal importer/terrain creation does this.

Use manifest exports/imports and the public catalog for relocatable references.
Registered relative modules stay within the owner; `mod:base/name` and
`mod:dependency-id/name` require declared published script imports. Private
same-named modules are not granted by a dependency. The virtual `judas` module
cannot be shadowed. `base` and `judas` are reserved mod namespaces.

Replacement is an explicit whole-asset/scene provider, with expected original
revision/type/required components/script-property interfaces. Two mods replacing
the same target fail with both owners named. Terrain-bearing scene replacement
must include all its required products and a navigation bake matching actual
composed collision. A new hill over old collision/navigation is rejected.

## Runtime, storage and saves

Selected dependencies resolve deterministically, stable-ID tie-breaking unrelated
owners. Any invalid requested set fails as a whole before scripts/world activation;
use an explicit corrected selection or `--no-mods`, which leaves selection intact.

A frozen effective AssetDatabase supplies the ordinary ResourceManager, one
QuickJS VM, scenes/prefabs, physics/navigation and streaming. Registered scene paths
are logical (`Mods/owner/Scenes/map.judas`); catalog results expose no host cache
paths. Installation bytes remain unchanged. Cache payloads and installed versions
are leased while worlds/resource jobs use them; Stop/project replacement waits
for old resource jobs before releasing the retiring view. No hot mount/unmount.

Storage is below platform user data (`judas/mods/GAME`), distinct from saves.
Immutable installed packages, selection and derived cache have separate ownership.
A failed/cancelled update leaves the previous index/selection. Publication uses
synchronized staged files and atomic per-file replacement, **not** a filesystem-wide
crash transaction. An interruption may leave an unindexed exact immutable payload,
reusable on retry. Selected or leased versions cannot be removed. Cleanup retains
shared cache entries and skips leased preview files; later selected startup can
reclaim unowned cache. Author sources/save slots/base files are never removed.

Nonempty loadouts save exact IDs, versions, payload hashes, resolution signature
and contract hash; modern effective-resource fingerprinting also remains in force.
Missing/changed/disabled/conflicting context refuses before replacement of the live
world. No automatic enable, download or migrations. Empty/unmodded saves retain
v2 encoding/current identities; modded metadata uses checked v3 encoding. Listing
exposes nullable `modContext`. Restore is ordinary state capture/restore: wrapper
reacquisition and subscriptions are transient; pending M73 events aren't replayed.

## Troubleshooting and limitations

Errors identify package/phase/reference. Fix the manifest/contract or authoring
source, recook/rebake, then republish; don't disable freshness validation. Structural,
reference, module compile and navigation checks are eager. Ordinary resource
publication/decoding remains asynchronous and later script exceptions retain normal
slot fault isolation. Uninstalling trusted gameplay code cannot undo arbitrary
side effects already made in the world. No hostile-JS isolation, online distribution,
hot reload, registry merging, JSON patch merge or runtime terrain brushes.

Terrain remains a finite static height patch, four base-color layers, unit collision
scale and arbitrary rigid orientation; gravity is independently authored. No caves,
automatic seams/LOD/spherical sculpting/water coupling added. VM/native platform
results and physical Windows GPU/input/audio acceptance are separate categories.
