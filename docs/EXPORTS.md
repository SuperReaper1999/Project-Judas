# Standalone project exports

Linux desktop packages include the standalone Release `judas` runtime. The
editor exports the startup scene and settings saved on disk.

1. In Edit mode, open **File → Build & Export…**. Check the **Startup scene**
   shown there. To choose one, use **Choose startup scene…** when none is set,
   or select it in **Project Settings**. Save the scene and project settings.
2. Enter the destination in **Package directory** or use **Browse package
   folder…**. Choose a fresh folder outside the source project that does not
   contain the source project.
3. Select **Export desktop game**. If the editor reports unsaved scene or
   project changes, choose **Save all and continue** to include those changes,
   **Use saved version** to export the disk version, or **Cancel**. The
   Build & Export panel reports the package path or the error.

Use the editor, exporter and runnable Release runtime from the same build and
channel. The editor and CLI default to a sibling `judas`. An optional third CLI
argument selects another matching Release runtime. The exporter checks its
build identity before packaging and rejects mismatched provenance. See
[First project](FIRST_PROJECT.md#4-save-and-export) for the end-to-end workflow.

Command line from an existing Release CMake build:

```sh
./build/judas_export /path/to/Project/MyGame.judasproj /path/to/MyGame ./build/judas
/path/to/MyGame/judas
```

The [asset inclusion policy](SCALABILITY.md#export-policy-and-inclusion-report)
describes the default conservative packaging, opt-in closure, explicit runtime
roots and `DEPENDENCIES.json`.

## Package and resource ownership

```
MyGame/
  judas                         # standalone Release executable
  game.judasproj                # normal project data, including input map
  judas-package.txt             # version 1 launch/save namespace record
  Assets/...                    # project's actual relative assets-dir
  Scenes/...                    # project's actual relative scenes-dir
  engine/assets/fonts/DejaVuSans.ttf
  engine/licenses/...
  engine/third_party/RUNTIME_NOTICES.txt
  RUNTIME_REQUIREMENTS.txt
```

The default asset policy is deliberately **all registered project assets**, plus every
`.judas` scene in the scenes directory and the explicit startup scene. This
supports dynamic scene selection and runtime `SpawnPrefab(assetId)` without
predicting gameplay code. Registered but currently unused runtime assets are
included intentionally. Unregistered files, repository sources, editor data,
evidence, research, caches and saves are excluded. Co-located LICENSE/COPYING
notices on registered assets' ancestor directories are preserved.

Existing loaders validate every asset; scenes and prefab sources resolve via
AssetDatabase, including source-derived/overridden texture, mesh, audio and
particle references. Missing/wrong-type dependencies or invalid prefabs fail the
export. OBJ material files are not a competing runtime resource path: Judas's
current rendering consumes explicit scene texture IDs. Cameras generate runtime
textures normally; there is no special export camera implementation.

Asset IDs and scene bytes are unchanged. Sidecars retain IDs/types and replace
import provenance with project-relative paths. Project format remains version 1;
scene/fingerprint/save schemas do not change. The launcher record names the
project explicitly. No-argument packaged startup resolves it from `/proc/self/exe`,
not the current directory or demo environment switches. Engine data comes only
from the package's `engine/` directory, ignoring development font overrides.
The existing standalone application loads the normal RuntimeWorld once and uses
the existing async ResourceManager, renderer, input, audio and particle paths.

## Replacement and saves

Export validates first, builds a unique sibling staging directory, then promotes
it. Re-export replaces the complete previous package, so stale files cannot linger.
It refuses to overwrite an arbitrary directory or export into/over its source
project. Failed validation leaves the old package intact. Source scene symlink
aliases/escaping dependencies are rejected rather than silently changing paths.
This is ordinary filesystem replacement, not a crash-recovery protocol.

Packaged saves live in `$XDG_DATA_HOME/judas/games/<save-id>/Saves`, otherwise
`$HOME/.local/share/judas/games/<save-id>/Saves`. An absolute user-data root is
required. Installation directories need not be writable. Save identity is SHA-256
of project name plus original project filename: moving/re-exporting retains it;
renaming either intentionally selects a new namespace. Equally named projects
with the same project filename share a namespace, but the existing strict
content-based baseline fingerprint rejects incompatible saves. Explicit developer
`JUDAS_WORLD_STATE` overrides still work. No authored project path is rewritten
into an absolute installation/save path. Existing FTFT1 failure atomicity remains.

## Platform and limitations

Current support: Linux, same CPU architecture/compatible distro ABI as the Release
build. SDL2 (including its system dependencies), compatible glibc/libstdc++ and
OpenGL 3.3 drivers must be installed. No arbitrary system libraries are bundled.
Judas's small implementation dependencies are statically linked; their notices,
Judas MIT license and the HUD font license ship alongside the runtime. Additional
asset redistribution permissions remain the project's responsibility.

No cross-platform installers, archives, shipping editor, streaming/cooking or
incremental patches. The accepted prefab runtime-spawn component subset is unchanged.
Human visual/listening acceptance is pending; automated image/voice state checks
are not human perception.

## Optional local mods

A developer must opt in with a saved game-owned `modContract`. The ordinary exporter
includes its explicitly published runtime references/replacement scenes in the same
checked closure. Curated authoring kits are separate, explicit `judas_mod kit`
exports; game packages do not automatically include private source or the offline
documentation tree. Installed selections and exact-byte trust live in per-game
user data, never the package. See [local mods/kits](MODDING.md).
