> Stage A historical evidence used the original dirty physics experiment.
> The approved production reconciliation and current qualification are in
> [RECONCILIATION.md](RECONCILIATION.md) and [SOURCE_SELECTION.md](SOURCE_SELECTION.md).

# Independent platform audit — 2026-11-16

Stage A findings and the Stage B reconciliation plan are ready for review.
This is a current-source evidence record, not a release certification. The
overall platform objective remains unfinished; read MISSION.md for continuation.
No architectural replacement, merge or selected-SDK change was performed.
Storage cleanup and guard changes were subsequently authorized explicitly.

**Subsequent operator storage decision:** complete `.cache` and `build` removal
was explicitly authorized and performed after the source-safety check. The
initial inventory below is historical measurement, not current disk usage.
All four worktrees had no uncommitted source; all commits remained reachable.
Git removed their registrations. Both directories were verified empty; 119.53
GiB of directory allocation was removed and filesystem available space grew
about 119.43 GiB. `cleanup-safety.json` and `cleanup-result.json` are the compact
transaction receipts. No historical output was backed up for this cleanup.
The primary dirty source and `codex/physics-core-v2` branch remain intact.
One fresh bounded `build/` is now being reused for qualification; this is new
output after the verified reset, not retained historical artifacts.

## Repository and provenance

The primary worktree starts on Experimental at
`ca0d437135ac5f5dd7adf9314b56526099d1c313`. Local main is `1094d645`;
`git rev-list --left-right --count main...Experimental` returns 0/172.
These are local refs; remote-tracking names are cached observations, not a live
GitHub branch-protection or publication audit. Other local branches and cached
remote branches are recorded in `evidence/initial-state.json`.

There are five registered worktrees:

| Worktree relative to repository | Initial revision | State / purpose evidence |
|---|---|---|
| `.` | ca0d4371 | Dirty tracked physics work and untracked reduced-hull implementation; no staged changes |
| `.cache/physics-core-replacement-20261116/source` | d12490a9 | Clean branch codex/physics-core-v2, four commits ahead of Experimental |
| `.cache/vehicles-v2/baseline-source` | cd4cb87e | Clean detached baseline snapshot |
| `.cache/vehicles-v2/committed-source` | b6a4fedc | Clean detached vehicle implementation snapshot |
| `.cache/whole-engine-performance/sdk-source` | c2a4e408 | Clean detached performance/SDK snapshot |

Tracked primary dirt spans CMakeLists, PhysicsWorld and private physics body,
arrival/continuous/solver modules plus the event debugger. It adds a contact
material interface, interval/trial energy accounting, changed geometry
publication and opt-in reduced vehicle hull proof/tests/tools. The complete
tracked diff is retained in `evidence/initial-state.json`; untracked implementation
inputs are hashed in `evidence/input-sha256.json`.

`docs/REDUCED_VEHICLE_HULL_PROOF.md` and ignored task/stop records identify this as
a stopped, unqualified same-time material/contact experiment. That is a provenance
clue, not independent reproduction of its numerical claims. Source confirms that
ordinary engine MK2 was modified to accept the opt-in interface, while the reduced
hull library and proof are EXCLUDE_FROM_ALL. Its author/session cannot be proved
from Git diff or timestamps. Preserve it and the negative experiment records.

The physics-core branch adds `src/physics_v2`, standalone tests and a crush proof.
Its CMake declares a separate library/proof, whereas the shared engine source list
still owns MK2. It is an isolated alternative, not an engine/editor/SDK replacement.
No branch merge or selection change is justified merely by its existence.

## Environment and build selection

Host: Linux x86-64, Ryzen 5 5600 (6 cores/12 logical processors), approximately
15 GiB physical RAM, GCC 15.2, CMake/CTest 4.2.3, Python 3.14.4, Node 20.20.2.
Real desktop: direct-rendered GTX 970, 4096 MiB, NVIDIA 580.178.04, OpenGL 4.6.
The supported graphical harness is verified in `DESKTOP_WORKFLOW.md`.

The initial scan finds 34 CMake caches including dependency subbuilds and twelve
top-level source/toolchain configurations. Complete options and source roots are
in `evidence/build-configurations.json`; associated identities are in
`evidence/build-identities.json`. Existing build output includes historical native,
MinGW, Android and Emscripten configurations; their presence does not qualify
their binaries against today's dirty source or actual target hardware.

Primary `build/` is Release, channel Alpha, SDK release gate ON, validation ON.
Its runtime/editor/server report clean ca0d437 source and fingerprint c520564a6959.
They cannot establish behavior of the current dirty source. The isolated audit
build uses Release/Experimental, validation ON, known-failure inclusion ON and
development SDK staging. Its identity explicitly records dirty source.

Production source selection is defined by actual CMake targets:

- `judas_engine`: common native world, simulation, renderer, resources, scripts,
  physics, terrain, water, particles and other runtime modules.
- `judas` and Android's `main`: Application/RuntimeOptions/diagnostics/harness
  linked to that engine; browser uses the same engine with its platform wrapper.
- `judas_editor_core`: project/document/workspace/panels/Play/preview/authoring
  services, linked to `judas_engine` plus editor-only ImGui and importer/baker.
- Desktop SDK: fifteen tools from `packaging/sdk-inventory.json`, staged with
  common embedded identity probing and complete file checksums. This is a binary
  tool/asset/JudasJS authoring SDK, not evidence of a separately versioned native
  C++ library ABI. C++ headers exported by build include paths are not proof of
  a supported packaged C++ SDK contract.
- Native/browser/Android each select platform dependency and context wrappers;
  desktop editor/import/bake programs are not Android/browser runtime components.

## Ownership and subsystem map

The table records source-traced ownership. Runtime correctness is established
only by named runs in the execution results, not by this table or a V2 label.
Coverage below identifies relevant existing test families, not full qualification.

| Capability | Authoritative owner / integration boundary | Lifecycle / persistence / relevant checks |
|---|---|---|
| Projects, scenes, prefabs, entities | Project, SceneSerialization, Prefab, RuntimeWorld | Stable authored IDs vs runtime handles; Scene/Project/Prefab/lifecycle tests |
| Host, resources, jobs, async I/O | EngineHost, ResourceManager, AssetDatabase, JobSystem | Resources release while GL context is current, workers joined before renderer/window; generation rejects stale completion; resource-handoff/residency/job tests |
| Edit/Play/Stop, undo, workspace | EditorApplication coordinates services; EditorPlaySession owns a separate RuntimeWorld | Document is copied into Play; End precedes world reset; preferences and runtime previews have separate lifetimes; actual editor lifecycle and workflow tests |
| Simulation scheduling | Simulation.cpp; InteractivePlay owns frame accumulator | One authoritative fixed-step entry for runtime/editor/server; replica path bypasses local dynamics; catch-up truncation must be measured |
| Rigid dynamics, contacts, CCD, joints, sleeping | PhysicsWorld plus cmake/PhysicsWorldSources.cmake private modules; MK2 solver | Native body generation/ownership; persistence and contact lifecycle; direct fixtures share world source list |
| Character motor, stepping, ragdoll, IK and balance | WorldCharacter, CharacterMotor, WorldRagdoll, WorldAnimation, physical reference/balance phases | Pre/post physics phases are distinct responsibilities; not automatically duplicate whole-body integration; character/pose/IK/ragdoll checks |
| Wheels, mechanical sources, damage, towing | vehicles/VehicleRuntime, WorldWheels, WorldPowertrains, Simulation coupling | Four contact sweeps per relevant interval with separate time owner; connected reactions/persistence require application tests |
| Deformable materials, fracture, render skins | WorldDeformables, deformables/*, WorldFracture | Material substeps couple into native chronology; immutable cooks vs per-instance state; material/contact/publication/cold-save tests |
| Reduced hull prototype | Untracked vehicle_damage plus modified native contact interface | Opt-in standalone proof, not SDK/editor-authorable production feature; unresolved closure claims require independent reproduction |
| Terrain editing, mesh/collision/navigation pages | TerrainV2 sources, RuntimeTerrain, editor TerrainV2Editor, author/cook tools | ValidateOwner rejects competing motion/geometry owners; worker preparation and owner publication; terrain lifecycle/render/save/SDK tests |
| Planets, origins, streaming, coarse fidelity | WorldPlanets/PlanetTiles, WorldOrigin/WorldRegions, WorldStreaming, CoarseSimulation | Full/coarse ownership distinct; reference shift, regional admission and persistence tested separately |
| Basic Water / advanced game water | WorldWater, BasicWater vs fluid/GameWaterRuntime | Basic optical/swim volumes and conservative grid are different supported jobs; explicit project settings, source-owned persistence; water application tests |
| Visual particles | particles/WorldParticles, CPU/GPU backends, shared collision snapshots | Distinct from rigid bodies/fluids; compatible snapshots reused; authoring/collision/publication/persistence tests |
| Weather, gravity, atmosphere, combustion | WorldWeather, gravity resolver, atmosphere/aerodynamic fields, CombustionWorld | Field selection vs integration distinct; thermal/accounting/world tests; full cross-system effects need workload evidence |
| Rendering, materials, lights, shadows, cameras | Renderer plus render/* and lighting/* | Shared world presentation for runtime/Play; editor previews are separate authored views; modern optional GPU features vs portable fallback; framebuffer tests |
| Animation/sprite/tilemap/2D | WorldAnimation, PoseComposition, WorldSpriteAnimation, WorldTilemap, PlanarAuthoring | Authored model/clip/retarget cooks vs live animation; mixed planar/spatial physical motion rejected; editor and exported 2D tests |
| JudasJS, typed tools, events, AI | QuickJS; Modules.json-generated binding catalogue; AI sensing/reservation/utility owners | Native dispatch enforces phase and handle boundaries; scripts stop before world retirement; type, binding, workflow tests |
| Input, UI, localization, local players | InputSystem/DeviceHub, RuntimeUI, TextLayout, LocalMultiplayer | Logical edges, per-player ownership, UI pause routing; synthetic harness does not validate hardware/IME |
| Audio | AudioSystem/WorldAudio; miniaudio backend and shared jobs | Voices end before resource/worker retirement; dummy backend tests do not validate speakers/real devices |
| Networking and server | NetworkSession/NetworkTransport, same authoritative Simulation step | Content identity, client replica gate, session lifecycle; loopback application tests do not qualify WAN/security/load |
| Saves and scene transitions | SceneSession, SaveService, WorldPersistence, SaveStorage | End old world before publishing replacement; file locking, previous-slot recovery, bounded slots; cold-process and failure-path tests |
| Models/import/reimport/cooking/export | ModelImport/ModelArchive/AssetCook, ProjectExporter/GamePackage | Asset IDs and recipes must agree across tool/editor/runtime; SDK tools and relocation workflow |
| Mods/content storage | modding/*, authoring tools and project opt-in contract | Session content view has separate lifetime; explicit loadout/registry checks; SDK workflow exercises install/enable/remove |
| Diagnostics/profiling/update/provenance | PerformanceProfiler, PhysicsEventDebugger, BuildIdentity/BuildUpdate | Debug manual clock differs from throughput; bounded profiler buffers do not bound accumulated on-disk captures |

No duplicate authoritative engine was established from similar names alone.
The independent physics-core proof is inactive in the primary checkout.
Renderer, terrain, fluid and particle source lists are shared with platform
wrappers. A renderer fallback or separate authoring preview is not intrinsically
a competing authoritative simulation. Exact source selection and execution
coverage still need to be joined to fresh run receipts.

## Verified limitations and decisions so far

**Reproduced defect SCENE-DIAGNOSTIC-001 (P2):** the full existing
`judas_scene_tests` fails one assertion outside the selected manifest. Building
an authored TriangleMesh with collision asset `no-such-surface` and no resource
service correctly fails and leaves the world unbuilt, but returns
`object 5 "Bad terrain": cooked collision requires project ResourceManager`.
The expected asset ID is missing. ResolveBodyCollision's no-resource branch
discards the ID it already has. A derived diagnostic executable added only a
printf after the original unchanged assertion and reproduced the exact message;
setup/acceptance conditions were not altered. Evidence: scene-tests-failure.log,
scene-diagnostic.log/.json. Recommended bounded repair: include the asset ID in
the resource-unavailable diagnostic and register the full original SceneTests
contract in the focused suite. No engine repair was made during Stage A.

1. **Current qualification cannot be inherited from old binaries.** Old build
   identities are demonstrably clean Alpha; current source contains opt-in and
   shared physics changes. Use the isolated build and identify its inputs.
2. **A selected regression suite is not all tests.** The repository has 347
   `*Tests.cpp` translation units; the manifest selects 192 entries. There are
   164 existing `*tests` binaries not named by that manifest (counts are not
   one-to-one with source filenames). Audit this coverage explicitly rather than
   treating CTest success as universal coverage. See coverage-inventory.json.
3. **Real-time scheduling discards backlog by policy.** InteractivePlay clips
   incoming frame time and zeros the remaining accumulator at its step cap,
   reporting counters. This is not hidden in source, but no performance pass
   may equate it with complete simulation-time preservation. Keep behavior
   unchanged during audit; record actual loss in workload captures.
4. **Architecture prose has drift.** docs/ARCHITECTURE.md's phase map still says
   to begin liquid work before fixed scripts and advance legacy fluid coupling.
   Current Simulation.cpp calls fixed scripts before UpdateGridWater and has no
   corresponding legacy-fluid step in that post-physics phase. The source is
   authoritative; reconcile prose after final ownership verification.
   The Water V2 Workshop README also claims an Alpha fluid lock, and its script's
   disabled-state text advises an Experimental source build. Current ConfigureProject
   copies the explicit opt-in without a channel gate, and ProjectFluidPolicyTests
   verifies opt-in export. Those source-preview messages are stale guidance.
5. **Prototype status remains unqualified.** The reduced-hull records disclose
   multi-second fixed-step tails and incomplete atomic publication. These are
   historical experiment claims until rerun; do not merge the independent core
   or declare the existing proof complete on bookkeeping/unit controls.
6. **Platform coverage is not inferred from artifacts.** Native Linux desktop
   access is verified. Existing Windows/Android/browser files are inventories,
   not fresh native target tests, hardware acceptance or portability evidence.
   The actual fifteen-tool SDK inspector reports GLIBC 2.43, GLIBCXX 3.4.32 and
   CXXABI 1.3.15 requirements on Ubuntu 26.04.1. It passes this host's loader check;
   it does not establish Ubuntu 24.04 compatibility. Use the existing complete
   Ubuntu-baseline build workflow before making that distribution claim.

## Storage inventory and retention proposal

Initial allocated storage (`du -x -B1 --max-depth=2 .`): 112.55 GiB total,
91.21 GiB .cache, 18.63 GiB build, 2.29 GiB .git. The audit itself creates a
separate identified build/evidence group, so later totals will differ.

Largest cache groups: whole-engine-performance 19.40 GiB,
mk2-collision-architecture 18.30 GiB, physics-v2-audit-20261113 6.22 GiB,
vehicles-v2 4.94 GiB, deformables-v2-stages234-windows 3.52 GiB,
android 3.35 GiB, architectural-purge 3.19 GiB, prealpha 3.04 GiB.
Full allocated directory inventory is storage-du.tsv. storage-content.json adds
apparent byte totals, extension composition, largest files and ownership markers.

Major producers are copied consumer/model projects, complete SDK and export
copies, multiple native/cross/dependency builds, and JSONL frame/OS captures.
For example whole-engine-performance includes 7.34 GB of JSONL and 6.06 GB of
native models; mk2-collision-architecture includes 15.44 GB of native models.
These are apparent sizes, not independent allocated-space measurements.

SHA256 verification of candidate equal-size files >=50 MiB found four duplicate
groups representing 23.61 GiB of repeated apparent bytes. Hardlink verification
matters: the 165 copies of one 145,345,674-byte model occupy 160 distinct inodes;
the other groups include existing hardlinks. These numbers are not approved
cleanup scope or guaranteed reclaimable space (reflinks/filesystem sharing and
unique project/evidence context still matter). Exact paths/hashes/inodes are in
large-duplicates.json. The subsequent authorized reset removed the cache/build
copies; these inventory figures describe the pre-cleanup state.

Inspected workflows create unique per-run validation directories without an
automatic age/byte retention bound. run_whole_engine.py streams OS and profiler
records; run_matrix.py requires a fresh campaign directory and freezes binaries.
These choices preserve reproducibility but accumulate with repeated campaigns.
SDK staging already uses an owned TemporaryDirectory and atomic prepared payload
replacement; that bounded temporary lifecycle should be retained. Native ICU is
an ExternalProject under each build's unicode directory with hardcoded make -j4;
independent builds recreate its preparation rather than sharing a verified host
toolchain/source-keyed cache. Do not casually share native/cross builds.

The operator rejected indefinite preservation of historical artifacts and
explicitly authorized the complete storage reset after the worktree source check.
That cleanup is complete. The sustainable policy now implemented is
[WORKSPACE_STORAGE.md](../../WORKSPACE_STORAGE.md):

- 8 GiB .cache / 14 GiB build / 16 GiB combined; 256 MiB per capture.
- One reusable active build per toolchain/configuration and one current campaign.
- Source belongs on explicit Git branches outside generated roots. Keep concise
  verified findings/tests/docs; do not archive temporary binaries or every raw run.
- tools/workspace_storage.py checks before/after guarded commands and every five
  seconds during them; exhaustion terminates the command's Linux process group.
  No automatic deletion. Five controls pass: a real producer is stopped, content
  is preserved, path escapes/root symlinks rejected, nested links not traversed,
  and hardlinked bytes not double-counted.
- Focused validation preflights storage and checks between cases. Whole-engine
  captures reject reused output paths and monitor their own byte ceiling every
  two seconds. Use the wrapper for aggregate campaign limits as well.
- Sampling permits overshoot; this is not a filesystem quota. Manually bypassing
  the wrapper bypasses ongoing enforcement. External output is not covered by the
  two repository ceilings; moving unbounded output elsewhere is not the policy.
- Future cleanup needs its own reviewed exact scope and source/worktree check.
  No deletion command is built into the guard.

## Current execution and qualification results

Exact rebuilt identity: `2.0.0-alpha.1-Experimental-ca0d437-dirty-0e93fdf8cbbf-ud1f3a815687b`.
The final-input-sha256.json inventory covers 1678 selected input files; the Git
source tree, canonical tracked diff and untracked fingerprint complete the build
identity. Native engine inputs remain unchanged from the initial inventory.
All changes made by this audit concern storage tooling, its CI/CTest checks and
source-checkout documentation. Nothing has been committed, pushed or promoted.

| Evidence | Actual outcome | Qualification limit |
|---|---|---|
| Fresh Release/Experimental build | Native runtime/editor, fifteen-tool SDK, all manifest binaries, extra lifecycle targets and benchmark/observer built | One GCC/Linux host; current dirty snapshot |
| Standard CTest | 199/199 pass, 370.18 seconds | Explicit manifest plus tooling gates, not every repository test |
| Additional native invocations | 16/17 pass | Full SceneTests exposes SCENE-DIAGNOSTIC-001; original assertion retained |
| Portable-policy graphics | Eight selected cases pass, including particle-world execution | Same physical GPU with portable feature policy, not all GL drivers |
| Python tooling | 208 run: 205 pass, three skipped | Two Java controls lack JDK; multi-config discovery lacks Ninja |
| JudasJS declaration checker | 58 examples, 8 negative drift controls, 26 result shapes, 281 callable signatures pass | Not exhaustive runtime API enumeration |
| Source reference checker | 177 documents / 2255 links pass | Link/managed-summary checks do not establish semantic accuracy |
| Final source-hidden desktop SDK | Eight outer operations and 39 tool operations pass; SDK checksums unchanged; both moved exports run | Native-host package, not Ubuntu 24.04 or Windows qualification |
| Flat terrain cold workflow | Edits, cooking, moved export, save, cold settlement, traversal and cold read pass | No page-eviction or universal terrain-budget claim |
| Planet terrain cold workflow | Same applicable sequence passes with restored origin/navigation/edit state | Geographic workshop fixture only |
| Whole Application baseline | 19 traced runs across 13 workloads plus 5 profiler-disabled controls | Short initial baselines; details in PERFORMANCE.md |
| Real editor observer | Edit and Play timings/mode checks pass | Smaller viewport; X11 screenshot readback fails with error 8 |
| Native editor visual verification | Existing screenshot harness works and shows real content | Exposes the Willow Town layout defect below; no human acceptance |

The supported SDK workflow creates an editable starter, authors/undoes/saves,
reopens and Plays/Stops with identical authored scene afterward, imports/reimports
and cooks a model, bakes terrain/navigation/environment/collision content, checks
invalid scripts and other expected failures, exercises mod lifecycle/server,
exports through editor/CLI and runs both moved exports. An outer read-only-host
bwrap namespace hides the entire checkout while retaining the actual NVIDIA
desktop. The helper's `packageOnlyNamespaces: false` flag does not describe that
outer namespace. Authoritative final receipts are evidence/sdk-final/.

Flat terrain restores eight volume chunks, two surface dabs, no pending brush
queue, one persisted particle contact and player z=-10.6087 after real tunnel
traversal. Planet terrain restores eight volume chunks, navigation, two surface
dabs, no queue and the geographic origin; player z=-10.3806. These use the existing
qualifier, unchanged engine implementation and independent cold processes.
Receipts and harness inputs are evidence/terrain/ and evidence/planet/.

**Reproduced application limitation TOWN-UI-001 (P2):** the curated Willow Town
UI has overlapping/clipped header/sidebar text in the default small editor Game
view. A standalone 681x386 run reproduces it, so the editor and runtime agree;
the application layout itself is unsuitable for that viewport. The 1280x720
native screenshot also has overlap near the lower scores/experiment labels.
The project clamps its reference to at least 640x480, reduces row heights and
retains font sizes. Fix the project layout/reflow and add a real text/layout oracle;
do not infer a generic renderer replacement is necessary. Evidence: native
editor-reference log, town-small.log and the inspected current-campaign images.

The performance programme is driven by actual results, not a guessed physics
bottleneck. Profiler-off animation has p50 10.539 ms and IK 13.638 ms, exceeding
the 8.33 ms target. The combined control has p99 6.847 ms. Traced timings are
slower, and one IK trace truncates detailed records. The enclosing shadow-planning
scope includes actual rendering; its full duration cannot be blamed on planning
alone. Five profiler-off controls have no observed >16.67 ms frame, but loss
counters are unavailable there and the runs do not certify sustained 60 Hz.
See PERFORMANCE.md for all percentiles, hitches, memory, content verification,
CPU/GPU overlap, profiler limitations and editor overhead.

## Operational failures and corrections

- Restricted socket/display access failed. The reviewed execution route successfully
  ran the repository's existing desktop harness without any desktop/driver changes.
- The small desktop probe initially used the wrong project filename case; the
  corrected case passed actual rendering/resize checks.
- The initial 12 GiB build allowance stopped atomic SDK preparation at about 12.09
  GiB. The previous SDK stayed intact. The failed preparation's 172 files matched
  current source/build products or interrupted prefixes, including one empty
  metadata file. Only that audit-owned transaction was removed. The measured
  staging peak justifies the 14 GiB per-build limit; aggregate remains 16 GiB.
- One SDK attempt started before copying finished and correctly failed manifest/ID
  agreement. Waiting for copy completion resolved it; the final workflow passes.
- Adding the storage test required updating the exact CTest registration contract.
  A missing-gate negative control was added; no acceptance fixture was weakened.

## Remaining qualification gaps and next decision

There is no current native Windows/MSVC, Android-device, browser-runtime or
Ubuntu 24.04 baseline execution in this audit. Old toolchains/builds were removed
as authorized and are not evidence of current behavior. Physical audio output,
controllers/IME, long-duration leak/race/stability testing, detailed vehicle damage
and same-time closure, multi-car deformable workloads, hostile network/load cases
and feature-by-feature complete editor/SDK workflows remain unqualified. Tests of
those individual systems do not silently fill these gaps.

Keep the shared production runtime and retain the separate core proof as
unselected research. Fix the reproduced diagnostic and project UI defects, close
specific coverage gaps, correct guidance, and investigate the measured rendering/
shadow/instrumentation path before proposing substantial replacement. The concrete
programme is RECONCILIATION_PLAN.md. Stage C/D/E and a development-ready baseline
have **not** been declared complete; main and selected external SDKs are unchanged.

Final verification: all 1678 frozen selected inputs still match their recorded
SHA256 values; all nineteen profiled runs match the exact build identity and
report zero clipping/discard/cap events. Vehicle telemetry verifies both velocity
signs and all four fleet trajectories. Git still lists only the primary worktree.
Current new outputs after cleanup occupy 0.98 GiB in .cache and 11.29
GiB in build (12.27 GiB combined), below the 16 GiB total ceiling.
`git diff --check` and the final current-document reference check pass.
