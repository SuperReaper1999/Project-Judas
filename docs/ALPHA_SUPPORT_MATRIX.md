# Alpha scope and qualification matrix

This is the support boundary for [published Alpha v1.0.2](https://github.com/SuperReaper1999/Project-Judas/releases/tag/v1.0.2-alpha.1),
published on 29 October 2026 at `24ae686f5918105dd84c2b00f98f5354bad13ef8`.
The original Alpha v1.0.0 was human accepted on 27 October at `636eec47`.
The patch adds fresh Ubuntu 24.04 SDK/editor/export qualification and hosted
native Windows SDK/tool/selected-contract checks. It does not create new Windows
GPU, audio, controller, clean-machine or Android hardware acceptance.
Exact M100 artifact receipts distinguish release qualification from the accepted
Pre-Alpha baseline and historical platform evidence. See
[patch notes](ALPHA_V1_0_2_NOTES.md) and [release process](RELEASING.md).
No claim of universal component composition or clean-machine certification is made.

## Platforms and distribution

| Configuration | Scope | Qualification boundary |
|---|---|---|
| Linux x86-64, Ubuntu 24.04 reference userland | Supported reference build | Complete older-userland SDK, ABI/loader checks, editor/project workflows and moved games; see final receipts. System graphics/audio libraries and drivers remain prerequisites. |
| Newer Linux host-native builds | Supported build workflow | Binaries inherit the build host's ABI. They are not automatically compatible with Ubuntu 24.04. |
| Other Linux distributions / Ubuntu 22.04 / non-x86-64 desktop | Unqualified | No inference from a glibc symbol check alone. |
| Windows x64 MSVC / tested Windows 11 hardware | Human accepted at `6941b381` | Effective 59/59 native gate plus 33/33 extras; operator project creation, save/reopen, small game, audio and editor/gameplay trial. Controller hardware and clean-machine certification remain unqualified. [Evidence](WINDOWS.md). |
| Linux-to-Windows MinGW cross-build | Existing separately qualified workflow | Native MSVC acceptance does not supply a new MinGW qualification; historical cross-build receipts keep their own scope. |
| Android runtime, touch, APK workflow | Human-verified working through real games | Operator confirms making and playing multiple Judas Android games, alongside earlier scoped phone acceptance. Exact revisions/devices were not supplied for the additional report; no fresh APK/device/emulator qualification accompanied the desktop Alpha refresh. |
| macOS, iOS, consoles, browser runtime | Unsupported | No supported distribution pipeline promised. |
| Docker/Podman Ubuntu baseline builder | Available, execution-unqualified here | Rootless bubblewrap is the exercised baseline backend. |

M100 defines Alpha/Experimental and reserves Stable for GA; preparing a candidate
does not publish or promote it. Main remains operator controlled. Use
`--build-info-json`, `BUILD_INFO.json` and package checksums for exact provenance.

## Runtime systems

| Area | Scope | Important boundary / evidence |
|---|---|---|
| MK2 rigid bodies, contacts, CCD, joints | Supported | Authoritative solver. Bounded arrivals can hold/truncate travel; no unrestricted exact dynamics claim. Rotation, independent collision timing, momentum and cold continuation have focused tests. [Contract](MK2_PHYSICS_ARCHITECTURE.md). |
| Dense physics workloads | Supported with content-dependent limits | Benchmark results are fixture/hardware specific. No guarantee that every 1024-body disturbance meets a frame budget. |
| Elastic / very fast spin | Supported bounded approximation | Corrected moderate-rate energy gain; long-run nominal-e=1 dissipation and extreme-spin phase error remain. [Measurements](PREALPHA_MK2_ROTATION_CORRECTION.md). |
| Wheels, powertrains, structural/deformation mechanisms | Supported within documented representations | Vehicle-agnostic authored components, not universal industrial mechanics. Native physical/lifecycle tests cover wheels and powertrains; other accepted mechanisms retain their own evidence. |
| CharacterMotor and script-owned controllers/cameras | Supported preferred path | Engine supplies movement/query primitives, project JS supplies policy. Not a skeleton, camera or AI system. |
| Retained C++ player / spacecraft gameplay adapters | Supported compatibility subset | Optional legacy policy path; distinct from removed MK1 physics. Targeted locomotion, swimming, input and spacecraft/frame tests do not certify every old game. New projects use JS policy. |
| FaithfulGravity / RadicalGravity / script selection | Supported | Resolver owns default selection; per-entity overrides are optional. Support normals remain separate. Arbitrary orientation does not remove local-chart precision limits. |
| Basic water and swimming | Supported; default configuration | Rendering and simple interaction do not require advanced-fluid solvers. Default-off and cold-process controls cover actual runtime policy. |
| Conserved reservoirs / dynamic surfaces / legacy particle fluids | **Disabled in Alpha builds** | Contact-stability issues can launch nearby rigid bodies. Alpha rejects advanced-fluid project requests before Play or export; the editor cannot enable them. Experimental/local builds retain opt-in implementation for repair, not Alpha support. FLU-001 and half-turn sampling remain deferred. [Investigation](M100_FLUID_CONTACT_STABILITY.md). |
| Large-world origins / planetary terrain | Supported bounded chart and terrain workflow | Checked participant admission, tile-local geometry and double absolute positions; not multiple simultaneous independent physics charts. |
| Streaming / regional suspension | Supported declared subsets | Inactive state persists; unsupported active assembly/liquid combinations reject explicitly. No universal suspension claim. |
| Volumetric excavation | Supported local volumes | Real collision and navigation publication for bounded edits. Not arbitrary planet-wide excavation through every terrain representation. |
| Saves and cold restoration | Supported declared current formats | Real physical endpoints tested; old snapshots missing historical state cannot be retroactively reconstructed. |
| Save migration / mods | Supported controlled policies | Explicit identities, source allowlists and ordered migrations. Not arbitrary physical topology rewrites, automatic merges of newly authored worlds, or universal mod compatibility. |
| Local multiplayer | Supported implemented device/view combinations | Device identity, players and controlled entities are separate. Platform/device availability limits practical configuration. |
| Online sessions / replication | Supported scoped foundation | Explicit server authority and supported authored-root representations. Dedicated hosting currently rejects streamed/composed worlds, mod-enabled projects and legacy gameplay. Not universal assembly replication, secure public-Internet deployment, rollback for every game or a managed online service. |
| Resources, rendering, LOD and budgets | Supported documented paths | No universal per-mip residency, every-backend performance or all-camera combination certification. |
| Animation, IK, UI, localization, audio, weather | Supported documented workflows | Their milestone-specific support boundaries remain. No new all-member validation claim from this final focused pass. |

## Developer workflow and coverage

The packaged SDK is intended to create, edit, script, debug, save, import/cook and
export ordinary projects without the development checkout. Script policy remains
project owned. Current reference/declaration consistency is checked separately
from runtime behavioural coverage.

The final pass deliberately selects high-risk contracts and positive/negative
controls; it does not certify every public API member or every permutation of
features. Remaining registry/module debt is maintainability work, not evidence
that tested ownership is incorrect. No speculative rewrite is required merely
to remove all deferred entries from this table.

The operator's Windows 11 project/save/reopen/small-game/audio trial is accepted.
Broader game-building coverage, controller hardware, clean-machine distribution
and fresh qualification of a particular Android build remain separate. A usability blocker still
deserves correction even if automated tests pass.
