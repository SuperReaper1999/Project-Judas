# Hearthworld — Pre-Alpha consumer survival sandbox

Hearthworld is an ordinary Judas project, not an engine milestone or hidden demo
mode. Development starts from Experimental `3c5119ca707f6f287cfd9e2213d1c62a9ae91602`.
Main remains unchanged. Automated evidence is not human gameplay acceptance.

## Playable project

[Project, controls and game rules](../projects/hearthworld/README.md) ·
[Editable project](../projects/hearthworld/hearthworld.judasproj) ·
[Authored world](../projects/hearthworld/Authoring/WORLD_DESIGN.md) ·
[Asset provenance](../projects/hearthworld/Assets/ART_PROVENANCE.md).

A 120 m radius world has four enterable cottages, 42 harvestable trees, eight
stone deposits, three loose physical props and a starter home. The homesteading
loop connects tool collection, actual tree felling, logs, stone, construction,
excavation and a durable save. It remains free play after the first-home goals.

### Ownership and mechanisms

| Game behaviour | Existing engine primitive | Project owner |
|---|---|---|
| Surface walking, jump, sprint | CharacterMotor; ordinary RadicalGravity field | `movement.js` |
| First/third person, tool views | Main view; interpolated presentation transforms | `camera.js`, `player.js` |
| Tool selection and gathering | Logical input, nearest physics rays, tags, entity lifetime | `harvest.js`, `inventory.js` |
| Tree falling | Static-to-dynamic authority, off-centre impulse, rigid collision | `harvest.js` |
| Log processing | Ordinary dynamic prefab spawning; actual fallen-pose gate | `harvest.js` |
| Gravity gun | Bounded force/torque, world inertia, real collision | `gravity-gun.js` |
| Construction | Dynamic prefabs, real fixed joints with local anchors, overlap casts | `build.js` |
| Excavation | Existing bounded M97 remove brush and async geometry/collision publication | `player.js` |
| Excavation removes a foundation | Ray recheck after terrain revision; joint destruction | `build.js` |
| Save/load and safe quit | Normal save requests, safe boundary, native participants and JSON state | `player.js`, `hud.js` |
| HUD, pack, pause, progress | Ordinary runtime UI, original WAVs and bounded particle emitter | `hud.js`, native assets |

No game-specific C++, new binding or runtime semantic change is required.
`docs/ARCHITECTURE.md` corrects one existing prose typo: the actual public getter
is `Entity.presentedTransform`, not `Entity.presentationTransform`.

### Terrain composition

The planet's visual **and cooked collision** omit the north cap. The garden's
32 × 12 × 32 m density volume supplies the missing real surface; its 5,819 samples
produce 12,040 initial triangles. Digging is restricted inside its marked area,
with edge and depth boundaries. No hidden solid sphere seals the hole. The pool
is a dry excavation; water filling and planet-wide digging are not claimed.

Terrain remains the owner of its native collider during recooks. Construction
uses normal fixed constraints, then checks its terrain anchor after a committed
revision so excavation can release an unsupported assembly. This is project
support policy, not a new engine structural material model.

### Asset workflow

Claude's local `ClaudesNewML-AI-Thing` tool really exists: a trained prompt
classifier drives procedural geometry generators. Two tree species were generated
through that actual workflow. Original cottage/tool/construction meshes and
reused starter props complete a coherent low-poly palette. No invented service,
neural mesh model or external runtime dependency is claimed. The original generator
and asset packs were read only; game-ready outputs are self-contained GLBs.

Imported model materials require the existing linear-rendering scene path. The
planet uses an explicit grass tint because its vertex `COLOR_0` stream is not
supported by the current runtime mesh path. Neither requires an engine change.

## Verification and evidence

The application observer injects normal logical input. It fixtures **only player
placement and aim**, never target transforms, resource counts, gameplay callbacks,
terrain fields or object health. It checks actual physics, not just counters.

Reproduction and final measured results are recorded in
[evidence](evidence/hearthworld/README.md). Failed attempts and their actual causes
are retained separately. Original failed source, tests, thresholds and physical
observations are not relabelled as passes.

## Measured Linux result

The combined native application observer passes **169 assertions**: 66 gameplay,
41 fresh-process restore, 20 actual save/UI controls and 42 profile-process
checks. Six dynamic-body endpoints and the motor match the actual saved capture
boundary exactly. The editor Play/Stop preserves authored content; an exported
52-asset / one-scene package starts, moves, switches view, opens inventory,
pauses, saves and loads after byte-verified relocation to a read-only installation.

On the i7-4790 / GTX 970 host, 600 rendered application frames at a controlled
60 Hz simulation clock have median **3.206 ms**, p95 **4.102 ms**, and maximum
**152.093 ms**. Physics p95 is **0.446 ms**, JS p95 **0.558 ms**, process RSS
**222.6 MiB**. These are measured costs, not a displayed-FPS certification. The
maximum is a retained cold-load reconstruction hitch: World build takes 104 ms
and restoration about 34 ms. It is not represented as solved or hidden by a
warmup exclusion. See the raw M56 report for separately labelled steady samples.

Release targets rebuilt without warnings. No full production-suite rerun was
needed for unchanged runtime source. API/type checks retain 473 symbols, 40
exports, 51 examples and seven drift negatives; all 11 project modules link.
The current-doc link check passes.

## Human checklist

- Enter the home; collect all three tools with E. Confirm tool and pack labels.
- Chop a tree three times; watch it physically fall. Chop the fallen trunk twice,
  carry a log with the gravity gun, then collect all three logs for nine wood.
- Mine a rock. Confirm the wrong tool earns nothing and the deposit exhausts.
- Grab a crate, walk and turn, let it collide with a wall, then release/launch it.
- Build a floor, wall and fence; rotate a piece. Dismantle a supporting piece and
  watch the attached assembly respond physically.
- Dig the marked backyard. Step into the excavation and test object collision.
  Excavate beneath a foundation and check its support is released.
- Switch view, jump, walk to another cottage and explore the far side of the planet.
- Pause, inspect the pack, resume; confirm no action leaks through menus.
- F5 save or use Save & Quit. Relaunch and Continue Home; verify inventory, harvested
  tree/deposit, buildings, objects and the actual excavation remain.
- Open the source project in the editor, Play/Stop, change a recipe in ordinary
  JavaScript and try the relocated standalone.

## Known boundaries

The whole compact world is resident; no streaming machinery is needed or claimed.
Trees use trunk-box collision and visual foliage, rocks use simple conservative
colliders, and resources are finite. Construction has one support constraint per
piece and a 64-piece cap; calibrated material-strength failure is not claimed.
Saving releases a gravity grip without changing the object's velocity. Restoring
reacquires fresh handles and preserves the physical object. There is no water
fill, hunger, combat, farming or infinite procedural world. Some directional
shadow banding remains visible; it is recorded rather than disguised as a geometry
or physics fix. Windows, Android and physical controller acceptance are not claimed.
