# Judas V2 architectural purge

This is an Experimental engineering checkpoint awaiting a separate human review.
It does not promote main, publish V2, unlock Advanced Fluids or change the RPG SDK.
The operator separately accepted Terrain V2 and Advanced Fluids V2 gameplay;
that acceptance is not acceptance of this repository consolidation.

## Baseline and preservation

- Starting engine: `2ee4203a0053cc545eab4a184c627f4f5507c4bd`.
- Protected main: `1094d64515604fe9d03fdab479efc7f880b112a8`.
- Historical implementations remain in Git. No history was rewritten or pruned.
- Existing HDD archives and independent research were not modified.

Six consumer games were extracted before their engine copies were removed:

| Editable project | Files preserved | Apparent bytes |
| --- | ---: | ---: |
| Hearthworld | 140 | 16,685,465 |
| Lastlight | 416 | 50,638,001 |
| Lunar Island | 1,833 | 435,627,107 |
| Planetfall | 470 | 152,798,577 |
| Shooter Game | 77 | 6,094,351 |
| Tuppence | 143 | 5,986,110 |

Destination: `/home/conner/Projects/Judas-Extracted-Games/2026-11-08-2ee4203a/`.
Each project has a sibling `NAME.preservation.json` with source revision, last
project commit, file sizes and SHA-256 hashes. Verification compared copied bytes
and permission modes; a second complete content-hash pass also succeeded. These
are independent historical editable projects, not promises of current compatibility.

The two locally modified water screenshots were preserved in
`preserved-local-water-screenshots/` there. Fourteen unique ignored import inputs
(393,684,966 bytes) were preserved in `historical-inputs/projects/import_lab/`,
including their manifest. The RPG project and its selected SDK were untouched.

## Completed dependency bundles

| Bundle | Removed | Current ownership retained |
| --- | --- | --- |
| A: repository content | Six extracted games, 55 originally identified historical projects, then the redundant V1-terrain Save Evolution application; root technology-demo scenes/prefabs; dedicated generators, runners and assets; obsolete reports and prototype trees | 22 distinct examples; small independently licensed regression fixtures; current V2 evidence and reference docs |
| B: native gameplay/saves | GameSession, native player/pilot/spacecraft/pickup/door/light-switch rules, native HUD/menu/reset, legacyGameplay, pose-delta and `.judasstate` paths | RuntimeWorld, InteractivePlay, SceneSession, project JS, CharacterMotor, input, gravity, joints, runtime UI, SaveService and modern world snapshots |
| C: liquids | PBF FluidWorld, ProductionFluidCoupling, exclusive contact/reconstruction/diagnostic machinery; M54/M55 LiquidSystem, assets, bindings and liquid baker | Basic Water/swimming; accepted game-water grid, rendering and MK2 coupling; unchanged Alpha lock |
| D: terrain | Separate bounded V1 terrain and volume runtime/editor/persistence, historical catalogues, terrain.create, volume author tool, v2-import and full-mesh v2-cook bootstrap | Native Terrain V2 sources, heightfields, sparse excavation, planetary foundations, precision, paging, collision/navigation publication and persistence |
| E: infrastructure | Fixed-panel editor fallback, empty compatibility sources, obsolete ASCII atlas/rasterizer, unused MK1 solver instances, forwarding-only UI/joint bindings, milestone runners | Docked Editor V2, Unicode text, authoritative MK2, subsystem-owned scripting modules and real portability fallbacks |

The accepted GameWater algorithms were not redesigned. Its runtime admission
predicates simply no longer select between retired PBF and grid backends.
[Intentional breaking changes](V2_BREAKING_CHANGES.md) are deliberate removals,
not deprecations with shims or migrations.

The cheap non-collision CPU particle kernel remains distinct from collision-aware
Particles V2: it avoids staging/collision costs and supports portable visual
weather. Modern GL and portable GL/GLES/WebGL paths remain distinct supported
capabilities. Generic radial terrain collision, aerodynamic forces, physical
constraint participants and flat/radial gravity are not removed merely for age.

## Final example inventory

All 22 editable projects remain under `projects/`:

| Project | Distinct purpose |
| --- | --- |
| 2d_traversal_workshop | One-way platforms and grid movement |
| ai_workshop | Utility AI authoring and inspection |
| animation_studio | Animation/retarget authoring |
| audio_lab | Audio buses, emitters and spatial authoring |
| basic_water_lab | Default inexpensive water/swimming |
| beacon_grove | Complete small 2D game workflow |
| browser_player_lab | Experimental browser player/touch workflow |
| connected_workshop | Multi-process authoritative networking |
| core_scramble | Input/controller gameplay workflow |
| developer_lab | General project/script workflow |
| graffiti_workshop | Collision droplets and persistent deposition |
| menu_workshop | Runtime UI, drag/drop and model preview |
| particle_v2_lab | CPU/GPU world-aware particles |
| physics_stress_lab | Current MK2 workloads |
| renderer_v2_lab | Rendering and camera workloads |
| sprite_animation_workshop | Sprite frame animation/events |
| terrain_v2_planet_workshop | Planetary travel and precision |
| terrain_v2_workshop | Sculpting, materials, cross-chunk tunnels and paging |
| tilemap_workshop | Tile authoring/collision/runtime edits |
| ui_showcase | Authored runtime UI controls |
| water_v2_workshop | Locked-release Advanced Fluids V2 source workflow |
| willow_town | Utility AI jobs, needs, decisions and persistent progression |

The SDK inventory explicitly distinguishes 14 shipped example directories from
8 source-only projects. It has 15 ordinary tools; retired liquid/volume tools
are absent. Windows additionally has its platform-readiness diagnostic.

## Size and build inventory

Measurements exclude `.git`, active builds and ignored caches. They describe
tracked/current checkout material, not reclaimed Git history or SSD free space.
The exact post-commit byte receipt accompanies the handover.

| Measure | Before | Consolidated working snapshot |
| --- | ---: | ---: |
| Tracked files | 74,367 | approximately 5,580 |
| Tracked apparent material | 4,130,630,845 bytes | approximately 264.2 MB |
| Files under src | 821 | 735 |
| C/C++ source/header lines under src | 120,051 | approximately 104,200 |
| Editable projects | 84 | 22 |
| Generated CMake target directories | 569 | 427 |
| Judas target registrations | 540 | 398 |
| Test executable registrations | 451 | 331 |
| Ordinary SDK tools | 17 | 15 |

Source lines count physical lines including comments/blanks in `.c/.cpp/.h/.hpp`,
with the same method at both revisions. Generated target directories include two
dependency targets and 27 CMake helper targets. Test registrations are not the
same as selected release gates. The previous 560 was an older inventory; the
41 storage-purge qualification targets were a selected subset, never the full
engine build graph. Current focused qualification registers 174 CTest cases:
168 manifest entries over 161 native targets plus six build/registration controls.

## Coverage preserved, retired and repaired

Tests of retired contracts were removed with their implementations. Tests of
surviving capabilities were ported to current ownership or small fixtures:

- Native player swimming became current CharacterMotor/Basic Water coverage;
  reusable shape queries, gravity, support/contact, joints and MK2 trajectory
  checks remain. No MK2 numerical algorithm was changed by the purge.
- Modern snapshots replaced legacy pose overlays in camera, prefab, ragdoll,
  joint and script fixtures. Terrain V2 and modern save-evolution coverage remain
  without the historical five-version game application.
- Independent animation/model/material/audio/navigation fixtures replace large
  historical projects. Model rendering retains actual GPU high-index skinning,
  eight influences, per-instance animation, materials and teardown checks.
- Navigation dependency tests preserve real geometry/cooked-collision freshness,
  cancellation and atomic multi-scene publication. SDK navigation qualification
  now bakes a real independent tiled surface, not an empty-scene no-op.
- Cookbook execution now owns six small current-API VM examples. Subsystem
  scripting suites retain their unique contracts; a historical game matrix is
  not kept as an API compatibility requirement.
- Shared UI/joint module extension tests remain, as do panel extension, workspace,
  hierarchy creation, script debugging, Play/Stop and export workflows.

Qualification found and corrected real current defects:

1. Removing historical editor automation also removed the shared frame counter.
   The common automation owner now advances it; creation, Play/Stop and exports
   complete. Driver waits in the failed run were not proof of a GPU deadlock.
2. A live consumer may retain a destroyed camera identity for its white fallback.
   Modern snapshots lacked the definition needed to construct that reference.
   A bounded optional retirement participant preserves identity without reviving
   the camera/body/scripts/target; a fresh-process checksummed cold-save regression
   now verifies the roundtrip and incompatible/missing participant rejection.
3. Mod-kit export still required a deleted milestone guide. Kits now copy current
   documentation, use `Judas-ModKit-1`, and have self-contained qualification.
   `judas_mod registries PROJECT` exposes the existing registry identity needed
   to author the explicit contract; ordinary projects remain opted out.
4. Several fixtures depended on the process working directory. They now locate
   their committed inputs explicitly. No private user data path is required.

The single changed UI vertex-shader golden is backed by a byte comparison:
only its removed-font explanatory comment changed; executable GLSL is identical.
Other shader goldens and negative controls were preserved. Failed experiments,
old full-run failures and rejected GL-flush hypotheses remain in the local
qualification record; no renderer workaround was introduced for that timeout.

## Qualification receipts and scope

Local evidence is under `.cache/architectural-purge/`:

- `default-all-build-8.log`: all registered native build targets, not just gates.
- `ctest-current-2.log`: original full run, 165/173 passed; retained failures.
- `fixed-gates.log`: all eight failures plus the new cold-camera case, 9/9 pass.
- `persistence-after-camera/RESULTS.json`: modern persistence and save exclusion.
- `sdk-workflow-4/COMPLETE.json`: packaged project creation, Play/Stop, 39 tool
  operations, unchanged SDK checksums and two genuinely relocated rendered games.
- `docs-current-qualification.json`, `api-qualification.log`: current reference
  links, declaration/binding consistency, 279 callables, 25 structured results and
  eight negative drift controls. Live VM enumeration is a separate retained run.
- `extraction-reverified.json`: all six consumer file hashes reverified.

The final local 174-case run passed 172 cases. Two unchanged NVIDIA GPU tests
returned invalid first-capture/first-dispatch data (terrain transition depth and
one-slot particle state). The unchanged terrain executable passed three direct
reruns; an instrumented copy also passed with no GL errors, and the unchanged
particle executable passed its rerun. No shader, particle algorithm, tolerance
or assertion was changed. The cause is not established; these failures are
retained and must not be represented as a proven driver defect or a repair.
Final exact-checkpoint CI results and this local limitation accompany the handover.

The final coordinated run, exact Experimental commit, final matching SDK and
completed CI receipts are supplied with the handover. This document is not an
advance assertion that a pending CI job passed. Clean Ubuntu baseline and native
Windows execution are separate from local host/NVIDIA qualification. Current
Windows/Android controller, audio hardware and every-device behaviour cannot be
inferred from a hosted runner or earlier accepted build.

## Remaining boundaries and human checklist

Terrain editor performance remains an accepted future optimisation opportunity.
Natural humanoid get-up is still parked. Content-dependent MK2/CCD bounds and
[other current limitations](CURRENT_FINDINGS.md) remain explicit. Experimental
browser support retains its launch opt-in. No fluid unlock or release is implied.

After receiving the exact build, test in this order:

1. Create a fresh project; create/parent/select objects, undo/redo and save/reopen.
2. Edit and attach a script; Play, pause/step/debug, Stop and verify authored state.
3. Test the Terrain V2 workshop: sculpt/paint, materials, tunnel across a chunk
   boundary, traverse away/return, save and restart; check collision support.
4. Test planetary terrain/origin travel and return to edited regions.
5. Test Basic Water/swimming independently; confirm Alpha still rejects advanced
   fluid opt-in. The accepted source workshop remains a separate explicit opt-in.
6. Test Particles V2/Graffiti persistent paint, ordinary physics and a connected
   session; check script-owned controls/UI rather than retired native gameplay.
7. Export a new game, move the entire package and launch/save/reopen it there.

Use `build/judas_editor projects/terrain_v2_workshop/terrain_v2_workshop.judasproj`
for the source build, or the matching SDK's editor with its bundled project.
Use the actual `.judasproj` filename in each project directory. No historical
consumer project or the RPG SDK is automatically upgraded.
