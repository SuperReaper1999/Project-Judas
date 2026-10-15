# Planetfall — The Fracture

An ordinary JudasJS consumer game on Experimental. Human playthrough acceptance is pending.

## Play

Open `planetfall.judasproj`, then play `Scenes/earth.judas`. Find Mara at the outpost and press H. J opens the journal; a gold marker identifies the current objective when it is in this region.

## Controls

- WASD move, mouse look, Shift faster, Space jump, E mantle or interact with the ship door/console.
- V first/third person. Left mouse fire, Ctrl aim, R reload, 1 rifle, 2 launcher, G grenade.
- Right mouse punch, F heavy punch, Q shove. B grab/drop a physical crate, T rotate it, C salvage nearby supply crates.
- H objective interaction, M mine the actual deposit under the reticle, J journal, P cycle side contracts, O flight chart, Y eat a ration.
- K board/leave the rover near its chassis. WASD drive/steer; release throttle to brake. The engine, gearbox, differential and tyres are physical. Shift+H beside the rover spends 8 iron and 6 scrap on taller transmission gearing.
- Enter the Wayfarer through its physical door, walk to the console and E to sit. WASD thrust, mouse attitude, Space/Ctrl rise/descend, Shift boost, V cockpit/chase, R level. E leaves the chair after stopping.
- While piloting: T selects a destination; N spools a regional travel transition. Earth-region travel is available before drive repair; interplanetary travel requires the repaired drive. It costs real campaign fuel.
- F5 save, F9 load; Escape pause. I opens equipment. Framewalk boots allow airborne Space surface switching; X releases the personal gravity override.
- At the final council: H contains the mineral; Shift+H reconnects the reference network.

## World and precision

Earth has a physical radius of 796,375 metres, one eighth of 6,371 km. Detailed 640-metre regions use double-precision authored global origins with local physics. Four Earth regions occupy different surface orientations. Moon and Mars have their own radii, origins and RadicalGravity fields. The derelict uses ordinary artificial uniform gravity.

Travel between detailed regions is an explicit loading transition, as agreed for this project. This is not a seamless, completely authored globe, real orbital simulation or a geographic Earth dataset.

## Ownership

Movement, combat, quests, inventory, construction, ship controls and AI are project JavaScript. Native CharacterMotor, gravity, navigation, rigid bodies, ragdolls, wheels, powertrains, structural fracture and save/streaming services provide the mechanisms.

The abandoned salvage rover is an ordinary snapshot-owned streamed region. Its damage comes from native plastic deformation and physical loads, not a health counter. The salvage contract then dismantles a genuinely damaged section through the ordinary fracture API and rewards only committed removal. The cave uses accepted volumetric-terrain collision products. Deposit mining consumes finite authored resource quantities; this project does not invent a runtime volumetric brush API.

## Persistence boundaries

Campaign resources, collected supplies, upgrades, mission progress and equipment carry between regions. Built outpost structures are reconstructed from those game facts when returning to Lantern. A fueled generator provides slow healing inside the outpost; its game-side fuel accounting remains distinct from the vehicle’s native mechanical powertrain. F5 stores the current physical world and script state through the normal save service. Arbitrary physical changes in every previously visited region are not yet a persistent universe database; do not assume all loose objects survive a scene change.

## Art

Uses the original low-poly starter assets and Claude's Prompt3D parser/procedural generator, with explicit validated backend parameters where library substitution selected an unsuitable object. Conner's supplied character is exported from a separate game copy with procedural locomotion/melee clips. The supplied original Blender file is untouched.

See `ArtSource/validated-streets/generation.json` for model dimensions and generation route. Current hero animation is functional prototype animation, not polished motion capture.
