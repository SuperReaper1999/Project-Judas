# Planetfall — The Fracture

An ordinary JudasJS consumer game on Experimental. Human playthrough acceptance is pending.

## Play

Open `planetfall.judasproj`, then play `Scenes/earth.judas`. Follow the gold marker to Mara. **E** uses the exact action shown on screen. **Tab** opens Equipment, Journal, Map / Travel and Controls.

## Controls

| Action | Control |
|---|---|
| Move / look | WASD / mouse |
| Move faster | Shift |
| Jump / mantle a low ledge | Space |
| Interact | E — follow the contextual prompt |
| Shoot / aim | Left / right mouse |
| Reload / melee / shove / grenade | R / F / Q / G |
| First / third person | V |
| Equipment, journal, contracts, rations and travel | Tab |
| Pause, save and load | Escape, then click a button |

E talks, builds, recovers items, mines, collects salvage, picks up or puts down a light physical prop, trades, boards/leaves the rover, or uses the ship door/console. Only the displayed action executes. Equipment selects the weapon; 1/2 are optional shortcuts.

- **Rover:** WASD drive/steer; Space brake; E leave. Journal offers a nearby transmission upgrade for 8 iron + 6 scrap.
- **Wayfarer:** E opens its door and seats you at the console. WASD thrust; mouse steer; Space/right mouse rise/descend; Shift boost; R level; V cockpit/chase; E leave after stopping. Release controls to hover. Tab > Map / Travel selects a destination and offers Depart / auto-land. Interplanetary travel needs the repaired drive and fuel.
- **Framewalk equipment:** a second airborne Space switches personal gravity. Tab > Controls releases it and rotates a carried prop.
- **Final council:** E opens Journal with two explicitly labelled decision buttons. No hidden Shift combination.

Menus pause gameplay and release the pointer. Tab or Escape closes them. Menu clicks cannot fire the weapon when gameplay resumes. Save/load requests execute after the menu releases authoritative updates.

## World and precision

Earth has a physical radius of 796,375 metres, one eighth of 6,371 km. Detailed 640-metre regions use double-precision authored global origins with local physics. Four Earth regions occupy different surface orientations. Moon and Mars have their own radii, origins and RadicalGravity fields. The derelict uses ordinary artificial uniform gravity.

Travel between detailed regions is an explicit loading transition, as agreed for this project. This is not a seamless, completely authored globe, real orbital simulation or a geographic Earth dataset.

## Ownership

Movement, combat, quests, inventory, construction, ship controls and AI are project JavaScript. Native CharacterMotor, gravity, navigation, rigid bodies, ragdolls, wheels, powertrains, structural fracture and save/streaming services provide the mechanisms.

The abandoned salvage rover is an ordinary snapshot-owned streamed region. Its damage comes from native plastic deformation and physical loads, not a health counter. The salvage contract then dismantles a genuinely damaged section through the ordinary fracture API and rewards only committed removal. The cave uses accepted volumetric-terrain collision products. Deposit mining consumes finite authored resource quantities; this project does not invent a runtime volumetric brush API.

## Persistence boundaries

Campaign resources, collected supplies, upgrades, mission progress and equipment carry between regions. Built outpost structures are reconstructed from those game facts when returning to Lantern. A fueled generator provides slow healing inside the outpost; its game-side fuel accounting remains distinct from the vehicle’s native mechanical powertrain. Escape > Save game stores the current physical world and script state through the normal save service. Arbitrary physical changes in every previously visited region are not yet a persistent universe database; do not assume all loose objects survive a scene change.

## Art

Uses the original low-poly starter assets and Claude's Prompt3D parser/procedural generator, with explicit validated backend parameters where library substitution selected an unsuitable object. Conner's supplied character is exported from a separate game copy with procedural locomotion/melee clips. The supplied original Blender file is untouched.

See `ArtSource/validated-streets/generation.json` for model dimensions and generation route. Current hero animation is functional prototype animation, not polished motion capture.
