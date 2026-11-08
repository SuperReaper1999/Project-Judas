# Integrated Terrain V2 local SDK qualification

Engine: `28984e7a04db2ae1bcb07dbca2f25b598263c850`, Experimental.
Final local artifact:
`.cache/terrain-v2/sdk-native-preview-28984e7a-qualified`.

All 17 tools rebuilt from this checkpoint. Staging probes and all 842 payload
checksums pass. Standard packaged consumer qualification passes eight operations,
including project creation, Play/Stop, tool workflows and relocated exports.
The SDK-contained terrain driver also passes both native games: flat and
planetary cook/export/relocation/excavation/traversal and genuine cold-process
reload. Both restore eight excavated chunks, two surface dabs, ready navigation
and an empty edit queue. The flat script additionally restores its one actual
reliable particle contact and configured local coverage. These use the SDK's
own tools, learning projects and driver, not development-tree tool paths.

The first verified package exposed five terrain-document links into deliberately
omitted historical evidence. They now point to files verified in the pinned
engine commit on GitHub. The final package differs from the first only in
`docs/TERRAIN_V2.md`; every native binary and learning-project file is identical.
The restaged final directory verifies and receives its own standard consumer
qualification. This documentation correction does not require a second physics
or terrain-game performance run. Raw manifests and file-difference proof remain.

This is an unsupported host-native source-build preview, not a published SDK or
the Ubuntu 24.04 distribution. GNU 15.2 / GLIBC 2.43 dependencies are explicit.
SourceDirty and 18 preexisting untracked build inputs are recorded, not erased.
Native desktop NVIDIA execution does not certify a clean machine or new
Windows/Android/browser functionality. The consumer does not use filesystem
namespace isolation. The RPG's `_SDK/current` remains local `9f022cf1`.
No main/release/channel/tag/fluid-lock changes or human acceptance are claimed.

Reproduce with fresh output directories:

```sh
python3 tools/package_sdk.py verify .cache/terrain-v2/sdk-native-preview-28984e7a-qualified
python3 scripts/qualify_sdk.py --sdk .cache/terrain-v2/sdk-native-preview-28984e7a-qualified --work /tmp/FRESH-integrated-sdk
python3 .cache/terrain-v2/sdk-native-preview-28984e7a-qualified/tools/validation/qualify_terrain_v2.py --build .cache/terrain-v2/sdk-native-preview-28984e7a-qualified --output /tmp/FRESH-integrated-flat --particle-region
python3 .cache/terrain-v2/sdk-native-preview-28984e7a-qualified/tools/validation/qualify_terrain_v2.py --build .cache/terrain-v2/sdk-native-preview-28984e7a-qualified --output /tmp/FRESH-integrated-planet --planet
```

The integrated checkpoint is suitable for reviewing the present flat/planetary
workflows. It does not finish all approved Terrain V2 work. Physical residency
is still source/region-granular with all up-to-1024 collision chunks resident;
runtime arrays load complete native sources. Fine physical interest and demand
paging remain open. Mapped planetary patches stay within one cube face, reject
overlap/moving placement and render full patch detail. Four texture/tint layers
do not constitute a larger virtual-texturing or layered-normal-map system.
Runtime volumetric smoothing remains unsupported; ordinary surface smoothing
and add/remove excavation work. Do not call these limits resolved by the SDK.
