# Lunar Island Stage 5 — Reconnect the Coast

Stage 5 is implemented on Experimental, pending operator review. The operator
approved progression from Stage 4 based on its screenshots/tests; that is not a
claim of a separate hands-on vehicle test. Stage 6 has not begun. Main, engine
implementation, protected history, independent research and other projects remain
unchanged. There are no new engine bindings or game-specific C++ systems.

## Playable world

Seven new destination districts: Whisperwood Forestry, Moonbridge Town, Tidemark
Harbour, Blackwater Works, Lunar City's civic quarter, Eastwatch Coast and Lantern
Islet. Seventeen additional roadside wilderness patches connect the expedition
space. The earlier garage remains another region: 25 optional streamed regions,
26 scene files including the root. Five physically collidable roads total about
17.9 km, 6.8 m wide, fitted to the actual native terrain samples and settlement
approaches. The island shell remains the existing resident 11.2 × 6.8 km bounding
terrain; no verified claim of 64 km² of populated land is made.

Stage 5 adds 21 furnished enterable buildings (23 including the homestead houses),
11 NPCs, 5 delivery tasks, 2 ocean skiffs and 730 harvestable resources. The generated
skyline/shop façades and lighthouse tower are sealed landmarks; interiors use
editable modular house/workshop assemblies with real opening frames, hinged doors,
furniture, beds, storage and workbenches. Industrial art currently reuses cottage/
depot/shop templates; specialized factory interiors remain unfinished.

M now provides a real location list, persistent tracking and distance/compass guide,
plus a toggle retaining the original reference map. Discovery requires physical
proximity; no destination teleport is implemented. Existing cars, pickup, motorcycle,
boat, helicopter and plane support travel. The actual road test constructs one
ordinary car prefab on a road and drives over 850 m using real logical input.
No vehicle position or velocity is injected after construction.

Regional survivors have finite trade stock, dialogue and delivery tasks connecting
wood, medical dressings, scrap, radio components and iron ore to provisions, money,
fuel and repairs. Hostiles use existing CharacterMotor/navigation/combat and leave
physical lootable remains. Mara's patrol now specifically counts the original two
homestead scavengers: unrelated island kills cannot satisfy that task.

## Persistence and physical ownership

Each district's ordinary `region-content.js` instance owns its own plain JSON
housing and harvest state. The player retains personal inventory, progression and
root/homestead state. `regional-housing.js` is a transient interaction view over
live district records, never a serialized Proxy or duplicate world-wide player
ledger. Access follows the owning instance's current state rather than caching
objects that become stale after deliberate state replacement/restoration.

The connected world test loots a town house, consumes real planks/nails to create
an actual attached window barricade, opens a real hinged door, gathers/depletes
forest salvage, shoots an industrial enemy and creates native physical remains.
All three modified districts genuinely unload. A new process loads the save,
returns and verifies exact storage/depletion/death/loot plus actual restored door
angle, barricade collision/support joint and corpse body. The player record stays
about 7 KB rather than growing with every settlement's furniture.

New fixed construction belongs to its support region, with rollback if attachment
creation fails. A regional bed explicitly pins the one claimed home for recovery;
claiming another bed or removing it releases the old pin. The home test proves both
residency and release. Distant regional NPCs clear navigation intent before
suspension. Driver-controlled mobile assemblies adopt to root as in Stage 4,
retaining stable regional references; driven vehicles remain resident afterward.

Runtime regional adoption deliberately supports root rigid/render/script/joint
members, not newly spawned prefab hierarchies. Regional human remains therefore
use an equivalent single-entity body/render prefab: the old visual child's −0.8 m
offset is baked into a normal glTF scene-parent node, preserving the mesh/rig data.
No engine guard was bypassed or widened, and the physical mass/shape is unchanged.

The world retains a bounded 64 MiB snapshot budget. The original 16 MiB budget
correctly refused to discard additional districts once exhausted; it was too small
for this authored content. The higher project-owned budget preserves all visited
regions while staying bounded. Physics/rendering costs do not run for unloaded
content. The geometry cache budget is 1 GiB; pinned root terrain/assets remain
resident, so this is not a claim of a hard total-process/GPU-memory ceiling.

## Assets, authoring and takeover

The verified installed Prompt3D parser/procedural builders generated corner shop,
pharmacy, supermarket, stepped tower and lighthouse sources in the realistic preset
(seeds 91557–91561). Original receipts/GLBs/settings remain in
`ArtSource/stage5-prompt3d`; catalogues retain source/output hashes. Existing original
house/furniture/tree/rock/vehicle assets and CC0 finishing are reused. No downloaded
asset, new third-party licence, voice or external sound was introduced. The external
generator and independent research stayed read-only. Runtime and export require no
ML tool, Blender or Python environment.

- `Authoring/regions.json`: destination configuration, chest stocks, house count,
  optional vegetation density/height and narrative content.
- `Authoring/routes.json`: editable road graph/waypoints.
- `tools/author_stage5.py`: original road mesh/collision cooking, local-frame scene
  templates, NPC defaults, deterministic scattering, piers and native prefabs.
- `region-content.js`, `region-session.js`, `regional-housing.js`: durable regional
  state and transient live registries, with lifecycle cleanup.
- `exploration.js`: project-owned discovery and guidance.
- Existing `housing.js`, `harvesting.js`, `person.js`, `transport.js`, player/HUD
  integrate ordinary interactions; inventory transactions still use one Bag module.

README explains rebuilding and manual variants. The project author owns generated
regional files and reserved road IDs; preserve manual edits before rerunning it.
The first seven districts use 10k–79k local ranges, roadside patches 80k onward;
additional major districts are allocated above one million. Extending a template
still requires sensible navigation bounds, scene ownership and content budgets.
It is an inspectable project author, not a general city-generation engine framework.

## Corrections established during development

Original failed logs/screenshots are preserved under `Review/stage5`.

1. Incomplete native box-render fields and an incorrectly encoded world-manifest
   frame were authoring errors. Corrected required fields and genuine region-local
   transforms with absolute manifest origins.
2. A copied ocean-skiff powertrain referenced the prefab's original bearing ID.
   The author now remaps its actual graph references.
3. A global player-owned furniture ledger exceeded the engine's unchanged bounded
   plain-JSON contract after several visits. Partitioned state by district rather
   than increasing serialization limits. Live views follow current state instances,
   avoiding stale aliases exposed by the observer's deliberate state replacement.
4. A captured observer-local JSON object outlived its scope, causing a test-process
   crash after the tour. Captured it by value. Early fixture placement also occurred
   before the street was published; fixtures now place the player after real region
   and script readiness. Neither issue was an engine crash in ordinary gameplay.
5. Cold-load completion can precede asynchronous player script instantiation.
   The observer waits for the player record to exist, then compares exact contents;
   it never waits for values to become the expected result. Physical checks remain.
6. The old hierarchical corpse prefab was rejected by the documented runtime
   regional-adoption guard. Authored the equivalent single-entity representation
   described above and proved actual regional corpse persistence.
7. Finite snapshot budget exhaustion was preserved as evidence; adjusted the normal
   project budget to fit this content rather than dropping inactive state.
8. Road-driving screenshots exposed doubled local offsets in the earlier game-side
   vehicle author: body/wheel/rotor children used world positions, and prefab
   normalization subtracted origins from children again. Corrected parent-local
   authoring. Native physical wheel/power/mass settings remain unchanged. One matched
   850+ m driven route has an exactly identical physical endpoint and cross-track
   result before/after; visual tyres no longer orbit away from the chassis.
9. Removed the islet's coplanar street/terrain flicker with 0.12 m surface clearance.
   Boat exit now checks solid dock/shore candidates before the open-water fallback;
   skiffs are moored within a usable pier-exit distance.
10. Profiling separated initial navigation validation from active driving. The
    homestead's bake now selects its actual local terrain/buildings rather than
    also consuming two irrelevant far-coast meshes. Its resulting fingerprint and
    105-layer/796-polygon data match the earlier Stage 4 homestead bake. All physical
    collision geometry remains present. Timing phase data and original spikes are
    retained; startup work is not presented as a driving-physics stall.

No genuine new engine defect required a native correction in Stage 5. These were
consumer authoring, state ownership, workflow or fixture issues; existing engine
safety boundaries remained intact.

## Verification scope

Linux, existing Release native engine/tools. The native game observer was compiled
and linked against that exact build. No engine implementation changed, so no clean
engine rebuild or unrelated full production-suite run was performed. Focused native
application tests use ordinary project input, UI, queries, physics and save service.
Player placement/aim shortens distant content checks; items, NPC health, cargo,
vehicle motion and physics endpoints are not injected. The actual car road test
uses only normal prefab construction followed by physical control.

Authoritative per-scenario logs and final checkpoint results are in
`stage5/VALIDATION.json`; raw logs, saves, expected records, images, CSVs and M56
profiles are retained. Repeated development attempts are not counted as additional
coverage. The late dock/islet/local-navigation changes receive affected reruns;
earlier regressions are identified by their actual candidate/log rather than
silently relabelled. All public APIs are existing JudasJS bindings; no inventory,
type declaration or native-dispatch change was needed.

## Remaining scope and acceptance

Human traversal/visual/audio judgement remains authoritative. Windows, Android,
physical controllers and listening tests are not qualified by these Linux runs.
The terrain shell is resident/coarse outside detailed authored pockets. District
NPC navigation is bounded to static streets/courtyards; new runtime obstacles use
physical blocking/combat rather than automatically rebaking island-wide detours.
There is no offline raid/economy simulation. Authored building shells and landmark
façades are static; player structures/barricades and harvested trees retain the
existing physical behaviour. Driven vehicles and ordinary player drops remain
root-resident. Flight/bike balance are assisted; stored cargo does not change chassis
inertia. Existing animation/art limitations remain in the Stage 3 handoff. Final
polish, richer industrial/civic interiors, save-upgrade policy and broader game
content belong to separately reviewed follow-up work, not a claimed finished
commercial RPG or automatic Stage 6 authorization.

Main remains untouched. The exact changed files and final source/binary/evidence
fingerprints accompany this report. Use README and `stage5/HUMAN_CHECKLIST.md` to
review this candidate before approving the next stage.

## Final measured results and qualification

All 18 final checkpoint application scenarios passed. Counts are per meaningful
scenario, not a sum of repeated development runs:

| Final checkpoint scenario | Checks / failures |
| --- | ---: |
| Modified town/forest/works gameplay and save | 38 / 0 |
| Fresh-process inactive-world restoration and physical continuation | 25 / 0 |
| Regional tree felling/ore depletion, unload and cold restoration | 37 / 0; 17 / 0 |
| Actual 850+ m road drive and forest discovery | 18 / 0 |
| Seven-destination tour and all regional unloads | 77 / 0 |
| Ocean skiff, basic-water flotation, ownership and dry pier exit | 17 / 0 |
| Interactive map/reference toggle/tracking/no teleport | 20 / 0 |
| Claimed-home pin and release through actual furniture UI | 18 / 0 |
| Existing combat/trade/quests/death; fresh-process read | 80 / 0; 33 / 0 |
| Existing harvesting/housing/construction/swimming; cold read | 133 / 0; 42 / 0 |
| Truck cargo/services/build loop; cold physical driving | 55 / 0; 44 / 0 |
| Garage teardown/adoption/driving; cold continuation | 34 / 0; 48 / 0 |
| Representative root gameplay performance sample | 10 / 0 |

Pure inventory/crafting checks: 83. Encounter rules: 37, including the new
unrelated-kill negative control. Vehicle service/cargo rules: 20. Native script
preflight: 30 modules, zero compile/link failures. Earlier combined-candidate six-
vehicle checks (18 each for car/truck/bike, 19 each for boat/heli/plane) passed after
the visual-parent correction. The final boat-exit change separately passed both
17-check aircraft landing/exit cases; subsequent homestead navigation filtering
does not change vehicle physical settings or collision. Their actual logs remain
identified rather than presented as extra independent coverage.

The final homestead navigation file is **byte-identical** to the accepted Stage 4
file (SHA-256 in `NAVIGATION_SCOPE.json`). The final native runtime/editor/tool
binaries also match Stage 4's bytes; the new observer is test-only. Historical
Stage 1–4 evidence is unchanged. Root/regional scene validation and navigation
bakes are retained, together with native prefab validation through real construction,
corpse spawning and vehicle creation in the application scenarios.

### Representative performance

Ryzen 5 5600, GTX 970 4 GB, 8 GB RAM; Linux, existing Release binaries, 1024×768,
hidden rendered window, vsync disabled, dummy audio and an explicitly supplied
60 Hz simulation clock. This is reproducible workload throughput, not human
monitor FPS or clean-machine/platform certification.

| Workload | Mean frame | p95 frame | Worst frame | Mean physics | Worst physics |
| --- | ---: | ---: | ---: | ---: | ---: |
| 600 root gameplay frames, 323 bodies | 8.451 ms | 9.946 ms | 16.058 ms | 0.146 ms | 0.261 ms |
| 3,919 active road-driving frames | 7.466 ms | 10.919 ms | 19.369 ms | 0.164 ms | 0.837 ms |

The active-driving CSV is phase-tagged by actual vehicle control; whole-run CSVs
also include initial loading and must not be mistaken for driving-only timings.
The earlier 848 ms whole-run outlier was dominated by 840 ms of initial navigation
source validation before driving, not a collision-solver stall. Selecting the actual
homestead source mesh instead of three terrain meshes reduced that recorded loading
scope to about 544–551 ms. These are observed runs, not a rigorous overall loading-
time speedup benchmark; the remaining synchronous initial validation is preserved
as a limitation. Driving starts at frame 826 in the final trace, after that work.

The last 120 root profiler frames show JS fixedUpdate mean 1.235 ms, update 0.048 ms
and presentation 0.405 ms. Inclusive scopes overlap and are not added as independent
costs. Resident process memory was about 533 MiB, including about 67.5 MiB reserved
by the enabled profiler. Raw M56 data is losslessly compressed beside the summaries.
Resource estimates and delayed GPU samples remain in those files; no isolated
physical VRAM or cross-platform performance certification is claimed.

### Editor and package

The final editor test used an isolated copy and isolated preferences: duplicate/undo
and Play/Stop preserved the authored scene **IDENTICALLY**, save succeeded, resources
reported zero failures and jobs zero failures. Automated debug rendering is not a
normal gameplay timing sample.

Final export: **176 assets, 26 scenes, 241,603,576 bytes in 7.438 seconds**. The complete
package was moved to `/tmp/Lunar Island Stage5 Checkpoint Relocated` and launched
from unrelated `/tmp` working directory. The shipping runtime loaded the actual
modified-world slot using the ordinary save service with no script/service/harness
failure. Logs and screenshot are retained. Package source/tool provenance is
preserved honestly, including the existing native build's embedded identity; no
new clean engine-build provenance was invented.

Project: `projects/lunar_island/lunar_island.judasproj`.
Reproducible untracked standalone: `.cache/lunar-island-stage5-checkpoint/Lunar Island/`.
Use `stage5/REPRODUCE.sh`, `stage5/HUMAN_CHECKLIST.md`, `stage5/VALIDATION.json` and the
source/binary/evidence/package fingerprint manifests. The checkpoint is intended
for Experimental only. Stage 6 remains awaiting authorization.
