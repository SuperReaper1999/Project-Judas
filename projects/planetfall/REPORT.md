# Planetfall implementation report

**Experimental consumer game. Human acceptance and a complete twenty-mission playthrough are pending.**

Starting engine checkpoint: `e738f727ab49ad044a0d47015adee5ddda7ce6dc` (M84). Main remains the accepted M83 release. No runtime engine implementation or public API was changed. The only existing tracked source change is a CMake target for focused consumer-game tests.

## UX corrective pass — 15 October 2026

The original keyboard-heavy interface was rejected by the operator. Current controls are in README; the UX change and scoped validation are in [UX review](Review/ux/REPORT.md). Initial delivery fingerprints/results remain preserved as the original `29ea41a` snapshot, not claims about the changed candidate. Human acceptance of this UX revision is pending.

## Implemented

- Twenty linked campaign missions and five optional contracts, with four named companions, dialogue, rewards, discoveries and two consequential endings. See [mission overview](CAMPAIGN.md).
- Four Earth regions on a 796,375 m radius sphere; lunar and Martian settlements; a walkable derelict. Local curved collision terrain uses double-precision authored global origins. Regional loading transitions were expressly approved.
- Conner's supplied character, exported from a separate Blender game copy; first/third person, CharacterMotor movement, jumping/mantling and optional framewalk equipment.
- Rifle/recoil, launcher, grenades, melee, physical explosions, ranged pirates and aggressive zombies. Enemy deaths use real native ragdolls.
- Finite deposits and physical supply collection; inventory, equipment, purchases, shelter/generator/defense construction, fueled outpost healing, repaired ship travel and a transmission upgrade.
- A physical wheel/combustion/gearbox/differential rover; a walkable rigid-body spacecraft with hinged door and console control transfer.
- M79 bridge with physical approach ramps, an accepted M84 cave collision product, and a streamed M83 salvage assembly. Its native plastic damage is followed by deliberate physical dismantling through `Fracture.remove`; reward follows committed removal. No hit counter pretends to be mechanical damage.
- Normal save slots, scene transitions, snapshot-owned streaming, pause UI, audio and particles. Save input waits for a pending write rather than silently discarding the load request. Camera mode and campaign facts restore.

All game semantics live in `Assets/scripts`. Existing Judas primitives own collision, gravity, animation/pose resolution, articulated physics, wheel mechanics, fracture and persistence. No hidden game mode or game-specific C++ was added.

## Assets and provenance

Reuses the existing consumer-game foundation and original low-poly pack. Claude's finished parser/backend generated dimension-checked houses, research structures, vehicles, trees, shop/roads/railings/stairs and a mining launch. Some whole-prompt calls selected inappropriate library substitutes; those results were rejected/preserved, and validated parsed parameters were supplied to the existing procedural backend. Claude's tool/source was not modified.

The supplied original Conner Blender file was not overwritten. Runtime/player copy and animation details are in [ANIMATIONS.md](ANIMATIONS.md).

## Focused Linux validation

Release-configured native application tests exercise actual game input and public JS behaviour. Fixtures seed particular campaign chapters and reposition actors for bounded scenarios; these are not an automated uninterrupted campaign playthrough. Pure rule checks are reported separately.

Final focused scenarios: **151 checks, 0 failures**, plus 96 pure campaign checks and JavaScript parse checks. Save/package follow-up results are in Review. No full production-suite rerun was needed because runtime C++ was unchanged. Windows gameplay validation was not performed.

| Scenario | Checks | Bodies at exit | Fixed p95 ms | Render p95 ms | Frame work p95 ms | Maximum ms |
|---|---:|---:|---:|---:|---:|---:|
| avatar | 9 | 1177 | 6.45 | 10.99 | 18.08 | 64.67 |
| cold | 12 | 1177 | 6.57 | 10.38 | 17.25 | 58.58 |
| combat | 17 | 1209 | 13.34 | 8.41 | 22.45 | 62.49 |
| derelict | 8 | 37 | 1.22 | 4.71 | 6.80 | 56.92 |
| earth | 12 | 1178 | 6.93 | 11.96 | 18.87 | 61.92 |
| flight-block | 11 | 1177 | 5.92 | 12.78 | 19.29 | 71.13 |
| flight | 14 | 1178 | 8.33 | 22.28 | 33.21 | 512.78 |
| mars | 9 | 1177 | 6.37 | 9.65 | 16.70 | 60.32 |
| moon | 9 | 1178 | 6.38 | 10.06 | 17.10 | 62.41 |
| pirates | 18 | 1218 | 13.59 | 7.56 | 23.04 | 60.74 |
| roof | 13 | 1176 | 8.15 | 6.34 | 15.56 | 59.86 |
| stream | 19 | 1181 | 12.21 | 5.24 | 18.81 | 62.17 |

The native fixture advances authored time at 60 Hz. Frame-work measurements include the fixture; they are **not a promise of real-time FPS**. The moved standalone uses real wall-clock time: 117 measured fixed-active frame intervals, median 17.90 ms (~55.9 FPS), p95 31.01 ms, maximum 520.29 ms, one catch-up cap during the startup capture. This is an idle startup observation, not combat certification. Region publication can stall around half a second; this is reported rather than hidden in an average.

### Ragdoll content choice

The same five-pirate lunar workload measured 26.49 ms fixed-step p95 with eleven bodies, versus 12.34 ms with seven mapped bodies and the original 51 kg total mass. Cadence, native contacts/constraints and engine solver settings were unchanged. Arms/legs are single physical segments; elbows/knees retain their captured local visual pose rather than independent physical articulation. Eleven-body reference definitions and measured profiles remain in Research/Review. A first comparison had an incorrect authored mass and was rejected before accepting the mass-checked result.

## Genuine findings and follow-ups

- Compound stair lips can cause the motor's support query to select a rounded non-supporting side before the floor, blocking automatic step-up. Original probes/failures remain in Review. Engine code/thresholds were not changed; the research objective has a real service ramp and landing. Jump/mantle remain available elsewhere. This scoped engine limitation is not claimed fixed.
- An early roof fixture released movement in a shared screenshot callback; the corrected arrival-driven fixture walks to the roof and physically recovers its recorder.
- The rear cabin really occludes blasts. Freely accelerated vehicles need not break immediately. Native permanent deformation is observed independently, then an actual damaged section is dismantled for salvage. The earlier over-strong break expectation is preserved as failure evidence.
- A corpse assertion inspected only baseline entity records. Canonical script objects include runtime prefabs; the corrected proof finds four/five actual rigs and live physical chest bodies.
- A load key arriving during an asynchronous write was lost by game policy. The game now queues it; actual resource, physical endpoint and third-person restoration pass.
- Export correctly rejected an unreferenced empty product from an early scenery bake. It was preserved outside runtime assets and removed from the asset database; the standalone export then succeeded.

## Limitations

This is a regional authored adventure, not a fully populated geographic Earth, seamless global rebasing or orbital flight simulation. Pirates are fought on foot/aboard the derelict; there is no orbital dogfight system. Region travel is ship-gated loading, while local ship motion is physical.

Campaign/base facts persist between regions. Manual saves retain the current physical world, including owned streamed damage. Arbitrary physics changes across every previously visited region are not a persistent-universe database. Development content changes can invalidate strict save compatibility; use Escape > Save game before quitting.

Hero/enemy animations are prototype-quality. Some decorative furniture has simplified/no collision; walls, floors, terrain, vehicles, interactive props and the tested assemblies are physical. Seven-body corpse fidelity is deliberately modest. Construction offers authored upgrades and physical barricade placement, not a free-form architectural editor. Full mission pacing, balance and human gameplay acceptance remain outstanding.

Protected historical evidence, existing games, independent fluid research and runtime engine source remain unchanged. No new engine milestone was started.

## Human checklist

1. E with Mara, build shelter, open Tab > Journal and follow the campaign marker.
2. Move/jump/mantle, switch V, and inspect Conner's character.
3. E board the rover; drive/brake/steer and cross the raised bridge.
4. Open the ship door, walk inside, sit at the console, fly locally and use Tab > Map / Travel for charted travel.
5. Mine actual deposits with E; collect physical supplies with E; build/upkeep the outpost.
6. Fight zombies and ranged pirates; inspect physical corpses and explosion reactions.
7. Use Journal > Next side contract for Scarred Bearings; damage a section, approach it, E dismantle and check actual removal/reward.
8. Walk the service ramp and recover the physical roof recorder.
9. Explore the cave and derelict; complete the blocked-passage rescue.
10. Visit the differently oriented Earth regions, Moon and Mars; inspect local gravity and optional framewalk boots.
11. Escape > Save game, move away, Escape > Load game; quit/relaunch/load and check resources, physical position and view mode.
12. Pause/resume, inventory/purchases, and test the moved standalone.
13. Play through all twenty chapters; save before the final council to compare both choices.

Project: `projects/planetfall/planetfall.judasproj`. Standalone: `.cache/Planetfall-UX-Package`. Rebuild/export/reproduction instructions are in Tools and README. Commit/push state is reported by the final handoff, not inferred from this document.
