# Alpha scope and qualification matrix

This describes the intended initial Alpha support boundary during **Pre-Alpha**.
It is not an Alpha announcement, release certification or promise of universal
component compatibility. Accepted engine source: `c76e92dc20b5765f61d6077bf618a8b94ac22462`.
A bounded Experimental correction restores free-body external-kick timing; it
awaits human acceptance. The [final qualification report](PREALPHA_FINAL_QUALIFICATION.md) records the exact
SDK identity, selected checks and outstanding acceptance work.

**Supported** means an intended developer workflow within the stated boundary.
**Experimental** means explicit opt-in with unresolved limitations.
**Unsupported/deferred** means no current qualification promise. Existing source
or an older successful platform run is not proof of current coverage.

## Platforms and distribution

| Configuration | Scope | Qualification boundary |
|---|---|---|
| Linux x86-64, Ubuntu 24.04 reference userland | Supported reference build | Complete older-userland SDK, ABI/loader checks, editor/project workflows and moved games; see final receipts. System graphics/audio libraries and drivers remain prerequisites. |
| Newer Linux host-native builds | Supported build workflow | Binaries inherit the build host's ABI. They are not automatically compatible with Ubuntu 24.04. |
| Other Linux distributions / Ubuntu 22.04 / non-x86-64 desktop | Unqualified | No inference from a glibc symbol check alone. |
| Windows MSVC and Linux-to-Windows cross-build | Existing implementation; current qualification deferred | Historical tests remain valid only for their recorded versions. No new Windows result in this pass. |
| Android runtime, touch, APK workflow | Existing implementation; current qualification deferred | Earlier phone acceptance is retained; no current-device or emulator certification in this pass. |
| macOS, iOS, consoles, browser runtime | Unsupported | No supported distribution pipeline promised. |
| Docker/Podman Ubuntu baseline builder | Available, execution-unqualified here | Rootless bubblewrap is the exercised baseline backend. |

No release channel, engine version, signing preset or publication policy changes
are made by this qualification. Main remains operator controlled. Use
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
| Conserved reservoirs / dynamic surfaces / legacy particle fluids | Experimental, explicit project opt-in | FLU-001 and half-turn disturbed-surface sampling remain deferred failures. Not advertised as universally accurate flow. Independent research is not production code. |
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

Still separate: the operator's own game-building trial and current Windows/Android
qualification. A usability blocker found in that trial deserves correction even
if automated tests pass.
