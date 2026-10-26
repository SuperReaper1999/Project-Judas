# Lunar Island — player guide

## Start a first expedition

Select **Begin your island**. Mouse looks; WASD walks; Shift sprints; Space jumps.
Escape pauses and releases the mouse. Controls opens a short on-foot guide; its
**Vehicles & travel** button explains the aircraft separately.

1. Look at loose sticks, stones and green fibre plants near camp. Press E to gather.
2. Tab → Crafting → Stone axe → Craft. Equip with 2. Three cuts fell a tree;
   let its actual trunk fall, then cut it three more times for timber.
3. Craft a pickaxe, equip with 3 and mine a rock. It takes repeated hits, not E.
4. Gather berries. Stand beside the campfire to craft a cooked meal. Equip/use
   food through Backpack. E at the well fills your flask; 5 then click drinks.
5. Talk to Mara or Ivo. Trading and quests are in their E menus. Carry the requested
   items back to the requesting survivor; rewards are not granted merely for visiting.
6. Enter a cottage, E on a chest to transfer supplies. B opens construction.
   Aim the placement preview at a valid support, R rotates and click places.
   Red/blocked placement is refused without charging materials. Escape cancels.
7. E on furniture offers its relevant actions. Resting at a bed claims a recovery
   home. Openings have barricade actions. Doors must be closed before securing them.
8. F6 saves. Wait for **Expedition saved**. Quit completely and F9 after reopening
   to continue. **Save and quit** waits for the save; failure keeps the game open.

## Explore beyond camp

M opens the destination list. Select a place and **Track location** for a persistent
compass/distance guide. **Stop tracking** returns the journal to your current task.
The reference image is a geographical concept, not an interactive teleport map.

Follow the roads to Whisperwood, Moonbridge, Blackwater Works, Tidemark harbour,
the civic quarter and Eastwatch. Lantern Islet needs a boat or aircraft. The whole
island is traversable, but detailed content is concentrated in these quarters and
roadside patches. Some landmarks are sealed façades; furnished modular houses
have real interiors. There is no planet travel or fast travel in Lunar Island.

## Vehicles

Walk south behind camp to the hatchback, pickup, motorcycle and supply locker.
The yellow helicopter is west of the apron; the plane sits on the grass strip
farther south. The first skiff is at the spring. Ocean skiffs are at the harbour
and Lantern Islet.

- E on a vehicle → Drive/Pilot. The same menu offers cargo, refuelling and repair.
- Tab opens cargo while stopped. Transfer one item or a whole selected stack.
- Cars/bike: W/S forward/reverse, A/D steer, Space brake.
- Boats: W/S forward/reverse, A/D steer. Let the boat slow before leaving.
- Helicopter: allow rotor spin-up; Space climbs, Ctrl descends, W/S flies and A/D
  yaws. Release horizontal controls for assisted physical braking toward hover.
  Mouse look is independent of flight direction. V changes the camera.
- Plane: W/S changes throttle; give it runway and speed before Space climbs.
  A/D turns, Ctrl descends, S reduces throttle and Shift brakes the wheels.
- E exits at low speed. Aircraft need ground nearby; land first. A skiff tries a
  dry pier before allowing a swimming exit. Never stop its engine by editing a save.

Sealed fuel cans add 5 litres when tank space permits. Repair kits restore condition.
Take both before long trips. Cargo capacity is a gameplay weight limit; cargo does
not change physical chassis inertia. Flight and motorcycle balance are assisted.

## Combat and survival

Equip purchased/looted weapons through Backpack. Left click attacks/fires, right
mouse aims, R reloads. Different guns use their specified ammunition. Enemy death
and loot are persistent. Barricades can be physically damaged and repaired.

Food, water, stamina, drowning, falls and injury are active. A field dressing helps
injuries. Death is deliberately forgiving: recovery retains your pack and returns
to a claimed bed, or camp. This is not a hardcore corpse-retrieval system.

## Saves and troubleshooting

This Stage 6 candidate has its own save identity. Older-stage slots remain untouched;
there is no claimed automatic migration. Use the older exported game to resume an
older-stage expedition. Runtime source/art changes may invalidate a save's native
content compatibility; preserve a matching game build alongside important saves.

There is no automatic periodic save. A native expedition save differs from an editor
scene save: Play/Stop restores authored content; F6/F9 preserves the player's world.

- No crafting result: read the notice; check ingredients, pack capacity and station.
- Cannot drive: get close, stop it and repair a destroyed vehicle.
- Cannot exit: slow down and find clear ground. An aircraft in the air is refused.
- Full pack: store or drop supplies before looting, harvesting or collecting rewards.
- Lost: M → destination → Track location. F9 replaces unsaved progress.
- Linux launch problem: use `Run Lunar Island.sh` in the standalone folder. It
  handles spaces and the working directory. Read RUNTIME_REQUIREMENTS.txt for the
  actual native-library and graphics requirements; this is not a universal Linux binary.
