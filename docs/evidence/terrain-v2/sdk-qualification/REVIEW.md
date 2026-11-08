# Terrain V2 local SDK qualification

The local native Experimental SDK is under
.cache/terrain-v2/sdk-native-preview-2e8bebca-final. It is an unsupported source-
build preview, not a published Alpha or an Ubuntu24.04-compatible distribution.
Its exact binary provenance is in BUILD_INFO.json: engine base2e8bebca, GNU15.2,
Release, Experimental, dirty/untracked flags preserved. No attempt was made to
hide unrelated protected work to manufacture clean release provenance.

All17 public tools passed native identity probes. Staging and verification pass;
checksums remain unchanged after SDK-only native terrain exercises. The host
loader/ABI check passes on this installed Linux userland, requiring GLIBC2.43,
GLIBCXX3.4.32 and CXXABI1.3.15. This does NOT qualify older distributions. A future
published package must use the established Ubuntu24 baseline toolchain.

The standard packaged-SDK workflow passes8operations: verify, host inspect,
editor-created project/export, editor Play/Stop, real CLI authoring operations,
checksum verify-after and2moved exports. The tool-operation log covers model,
volume, V1terrain/nav, scripting positive/negative controls, saves and server.
The SDK's OWN tools/validation/qualify_terrain_v2.py runs with only its packaged
projects/tools, exports/relocates them from a foreign cwd, parks editable copies,
uses normal controls to raise/paint/excavate/traverse and cold-restores in a new
process. Both flat and planetary paths restore8volume chunks,2surface dabs,
empty queue and live collision/navigation. Filesystem namespaces were not used;
this is SDK-contained workload/asset verification, not clean-machine proof.

The inventory now explicitly includes both editable Terrain V2 Workshops and
their reproducible driver. Checking source-archive coverage exposed an older
separate learning-project list that omitted Particles V2/Graffiti, plus the new
Terrain examples. Source archives now derive learning roots from the shared SDK
inventory. For --source ANOTHER_CHECKOUT the COMMITTED inventory in that checkout
is used, rather than this tool's live file. Explicit qualification-only fixtures
remain separate. No unrelated consumer games, save files or protected evidence
are added. A new disposable Git archive test proves that its own future fixture
is included while saves and Lunar Island stay excluded; another tests future
inventory extension and invalid broad/nested roots. Pipe handles from the Git
blob reader are closed in its existing failure/cleanup path, removing observed
ResourceWarnings. No publication code or acceptance gate is bypassed.

Coordinated release-packaging/CI-route/registration tests pass36checks with2existing
environment skips. The retained initial run failed4 project-selection assertions;
it was fixed through the shared inventory, not by weakening assertions.

Windows/Android/browser qualification of this Terrain V2 checkpoint was not run.
Desktop NVIDIA GL4.6/portable3.3 evidence is in ../planetary-integration/.
The installed RPG _SDK/current remains on local9f022cf1. No SDK selection, game
asset, main branch, fluid policy, release channel/tag or publication is changed.
