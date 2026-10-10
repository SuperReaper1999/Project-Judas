# Optional local mods

[Index](../JUDASJS.md) · [Modules/lifecycle](lifecycle.md) · [Scene/session](scenes-state.md)

M76 provisional checkpoint; **NOT HUMAN VALIDATED**, human review pending. These bindings expose detached metadata,
not installation, permission or sandbox controls. See the [modding guide](../MODDING.md).

```js
import {mods, scenes, world} from 'judas';
const maps = mods.contributions('maps');
const entry = maps[0];
if (entry?.reference.kind === 'scene') scenes.load(entry.reference.value);
const prefab = mods.resolve('my_mod', 'decoration');
if (prefab?.kind === 'prefab') world.spawnPrefab(prefab.value);
```

| Member | Result |
| --- | --- |
| `mods.active` | Detached array of `{id,name,version,sha256,executable}` in resolved dependency order. Empty for an ordinary no-mod session. |
| `mods.contributions(point)` | Detached matching `{owner,point,name,reference,data}` descriptors. An unknown point returns an empty array. The game interprets meaning. |
| `mods.resolve(owner,name)` | A published `{kind,value}` reference, or null. Asset values are stable runtime AssetIds; scene values are registered logical scene paths. Never filesystem paths. |

Each access returns fresh bounded plain data. Mutating it does not change the
loadout or contract. Cache references for the current session; reacquire on a new
scene or restored script as appropriate. The loadout is fixed for a session.
Installing/enabling is developer/player tooling, unavailable to gameplay scripts.
Scene changes do not hot-load a newly selected package.

The game must opt in, and executable payload requires game permission and explicit
user trust of its exact content hash. Trusted code has ordinary JudasJS world
capabilities; published references and manifests are **not** a security sandbox.
No Node, native plugin loader, filesystem/network binding or new async lifecycle
is added. Mods use ordinary script slots, modules, callbacks, signals and saves.
