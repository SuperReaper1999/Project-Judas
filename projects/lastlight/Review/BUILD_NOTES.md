# Lastlight build notes

Built against accepted Judas HEAD `18650ba23c23eaaab0542d6e0055e33cc14d1049`.
Only `projects/lastlight/` is added; no engine/API implementation change.
The operator accepted the playable revision and requested this project checkpoint.
No new engine milestone or engine/API changes belong to this project.

## CURRENT — physical deaths / reaching punches / blast forces, 10 October 2026

The human-played inventory/Framewalk candidate was accepted before these final
requested additions. This revision adds only game-side JS, original animation
content and project documentation/evidence. It does not add an engine weapon,
health, inventory, gravity, explosion or corpse system. The operator explicitly
requested checkpoint/push after implementing the additions.

### Current behaviour

- Zombies approach with arms extended. The original nine clips and 17-bone
  skeleton/skin/bind identity remain intact. `Attack` is a right-fist windup,
  contact and recovery; runner/stalker/brute playback rates 1.05 / 0.90 / 0.70
  align its extension with each variant's actual scripted strike timing.
  Reach, vertical separation, facing and fresh real line-of-sight still gate
  damage. This is a simple visual punch, not articulated fist collision scoring.
- Death disables motor/navigation and enters the existing passive eleven-body
  articulation at a fixed script boundary, using the current resolved pose and
  the engine's observed world/bone motion. Only the game-authored killing impact
  is added. An already tripped enemy retains the exact existing body handles.
  No corpse presentation transform, animation collapse or standing drive fights
  physics. Damage/score are guarded once; mapped hits still find the owning enemy.
- Eight corpses maximum; the oldest whole owning entity is removed on overflow.
  Ordinary 18-second expiry and scene destruction remove the mapped bodies and
  constraints. Living trips retain their existing six-articulation limit.
  Wave completion clears wave tracking without deleting the final pending death.
  Unexpected activation errors are logged/retried at a bounded half-second rate;
  expiry still ends a malformed content instance safely.
- Blasts use normal broadphase overlap, nearest collider surface queries,
  ordinary cover rays and finite mass-scaled impulses at actual points. Untagged
  dynamic props and dead bones participate. Each body is processed once per
  blast; static destructibles can take damage without dynamic-only API reads.
  Damage, points and owner meaning remain project JS. Cover is a single visible
  surface-point approximation, not pressure transport through geometry.

### Focused actual application evidence

`death-ragdoll-proof.log` / `.txt`, `death-ragdoll-fixture.py.txt` and screenshots
record the final source in the existing Release application. Disposable copies
use public prefab/weapon/signal APIs to arrange reachable cases; no solver mocks
or capture driver is shipped in runtime content.

- Normal nearby runner pursues/attacks; player health **1000 → 989** in the fixture
  (extra health is observation setup, not a changed game health rule).
- Real rifle fire kills: score **100**, eleven valid dynamic mapped bodies.
  Positions change under normal gravity/contact response; the physical corpse
  remains visible. A later blast changes its actual bone velocities and leaves
  score **100**. Fallen owner routing stays live without scoring dead enemies.
- Living trip → killed: all eleven mapped body IDs remain identical.
- Ten deaths retain exactly **eight owners / 88 bodies**, retiring the first two.
  The settled group's maximum linear speed falls below **0.043 m/s** in this case.
  Destroying those owners invalidates every captured body handle.
- The real round director clears the wave while its last victim stays valid and
  physically active. The body owner becomes invalid after normal 18-second expiry.
- `death-explosion-physical-proof.log`: an untagged crate receives **4.61538 m/s**
  outward motion and **2.76924 rad/s** off-centre rotation. A barrel behind an
  actual wall remains at zero velocity. A static wooden door breaks through its
  ordinary debris script. No score is awarded and no script errors occur.
- A content review caught that the old wave-clear loop deleted dead owners before
  their pending activation. The fix and actual final-victim proof are recorded
  here; no artificial failure log was invented for this source-review finding.

No full production suite, unrelated subsystem checks or engine rebuild was
needed for this project-only extension. Existing engine/current demos, protected
historical evidence/research, and the original low-poly backup remain unchanged.
Windows/controller hardware gameplay review remains outstanding. The final
requested additions received application/asset checks; no new human visual
acceptance is claimed beyond the operator's prior game review.

Performance and final package/fingerprints: see `death-performance.json` and
`physical-handoff.json`. Historical capture manifests remain intact, including
`framewalk-sha256.json`; `content-sha256.json` describes this current revision.

## HISTORICAL — accepted inventory / Judas Framewalk revision, 10 October 2026

This extends the town-defence candidate without changing engine/runtime source.
The current guides are `README.md`, `FRAMEWALK.md` and `ANIMATION_GUIDE.md`.
Earlier logs, performance samples, content manifests and genuine failures remain
historical, including `town-defence-sha256.json`.

### Content and engine boundary

- Eight stored stacks; ordinary weapon/feet equipment and reusable item metadata.
  C collects real physical pickups; I is an authored modal pack. Equip/unequip
  uses plain script state; drop queues one actual prefab for the resumed fixed
  step. Rockets/grenades retain one shared stock with the vendor/weapon.
- Judas Framewalk Boots start unequipped. A supported Space jump arms a second
  airborne Space press toward an actual clear static/kinematic surface. The
  optional script uses public gravity selection, casts and CharacterMotor state;
  it never assigns a player transform or velocity to switch/clear a frame.
- Gravity acceleration smoothstep-blends to the selected face normal. A 180°
  change passes briefly through zero instead of applying a full lateral force.
  Actual motor orientation and the presented camera follow the existing engine
  path. Existing locomotion, physical contacts and fall damage still change motion.
- References retain local point/normal and generation-safe entity handles. Lost
  reference/no landing triggers a bounded nearest-surface/14-ray search with
  visibility, approach and real capsule clearance. It can fail in cramped or
  unsampled geometry; failure restores pre-boot gravity intent. A missing prior
  source falls back to spatial fields. No universal world-up or global override.
- Only an explicitly held prop inherits effective player gravity; release restores
  its previous selection. Unheld bodies remain under their own gravity.
- Three real interior/roof blockouts, 7.4 / 10 / 8.5 m, join the eight houses.
  Open entries, columns, galleries, roof hatches, two canopies and three breakable
  wooden doors support traversal. Roof supplies are actual inventory pickups.
  Current canonical scene: **598 entities, 331 static and 26 dynamic bodies**
  (22 breakable props plus four inventory pickups). Navigation: **222 layers,
  547 polygons**, bake source
  `4ee8e889fe09a2c4ac2901ec4f8274f279e465b644968218be54c4cf7831c33f`.

### Actual application captures

Existing Release input/capture runner and public APIs; no new engine tests,
production-suite reruns or runtime builds. Disposable fixture copies author
reachable starting positions and pre-own boots for focused motion captures;
the normal shipped project requires collection/equipment and contains no driver.
`framewalk-capture-inputs.json` and `framewalk-fixture-observers.json` describe
those captures; fixture files are evidence, not runtime game scripts.

| Record | Actual observation |
|---|---|
| `framewalk-inventory-followup.log` | Normal start: C transfers the one real boots pickup, pack 2 → 3 stacks with feet still null; actual I/UI selects/equips; Drop closes the pack, creates a normal inert physical pickup and returns pack 3 → 2. |
| `framewalk-wall.log` | First jump then airborne selection supports on wall x31.843 m under +X gravity; walking up leaves the wall and automatically recovers the roof's actual +Y face. Roof support y7.417 m; X resumes spatial gravity. |
| `framewalk-ceiling-floor.log` | Same character supports on ceiling y7.143 m with up -Y, then a second supported-jump/airborne selection returns it to floor y0.137 m/up +Y. Two switches, zero failsafes; ordinary landing damage 100 → 89 → 78. |
| `framewalk-near-wall.log` | Previously troublesome near-wall initial position now reaches the ceiling with zero failsafes. |
| `framewalk-held.log` | Real crate700 gravity matches the turning player's effective acceleration; other body702 stays -Y. Dropping restores crate700 to spatial -Y and it falls/settles through normal physics. |
| `framewalk-lifecycle.log` | Destroying the selected wall recovers the ground. Removing all 330 remaining suitable nearby bodies causes exactly one no-reference fallback; player falls under the surviving spatial field, with no stale-handle fault. |
| `framewalk-unequip.log` | Unequip during active wall support clears to spatial gravity on the resumed fixed step and ordinary ground support returns. |
| `framewalk-prior-gravity.log` | A pre-existing -4.905 m/s² uniform owner selection survives release after wall traversal; the spatial field and unheld body remain -9.81 m/s². |
| `framewalk-door.log` | Rifle damage takes the real static door from 100 → 16 → broken; six physical planks replace it and the player enters its real opening. Score stays zero. |
| `framewalk-package.log` | Normal moved package: collect/equip/drop through actual authored UI, view switching, pause-menu reload, then fresh pack2/feet ordinary and player100 health. |

Selection and clear/restore observations report **0 m/s immediate velocity
change**. This does not mean velocity stays constant through gravity, normal
locomotion, capsule rotation/contact resolution or landing. All final captures
completed without script diagnostics. Visual/gameplay acceptance remains human.

### Genuine development failures retained

- `framewalk-edge-normal-initial.log`: nearest-point edge separation normal was
  used as a face reference, producing tilted roof gravity. The project now refines
  it with an actual raycast face normal; no native query data is fabricated.
- `framewalk-ceiling-near-wall-initial.log`, `framewalk-ceiling-arc-initial.log`:
  full-strength 180° gravity rotation induced a sideways pull and unwanted wall
  recovery. Acceleration blending removes that artificial arc policy. Outward
  initial contacts are allowed only with clear final capsule space; blocked inward
  approaches are still rejected. The original near-wall position was rechecked.
- `framewalk-static-door-initial.log`: the reusable breakup script read
  `angularVelocity` on a static door, which the documented API rejects. Static
  doors now use their actual zero angular motion; other bodies retain real angular
  and point velocity. Door destruction was rechecked. This was a project-script
  misuse, not an engine/runtime API defect.

### Lightweight performance and package

`framewalk-performance.json`: 120 retained real-time Release exploration frames,
active wall gravity/third-person camera, no living enemies: mean **7.37 ms
(~136 FPS)**, p95 **10.27 ms**, max **17.04 ms**; fixed step mean **2.23 ms**.
`framewalk-active-performance.json`: same expanded town/view, eight living enemies;
only the disposable copy's initial countdown was shortened to 2 s. Mean **8.62 ms
(~116 FPS)**, p95 **11.51 ms**, max **16.72 ms**; fixed step mean **3.83 ms**,
JS fixed mean **1.71 ms**. No fixed caps or dropped profiler events in either.
This wall-facing view submits ~808/825–833 draws and is not comparable to the
prior wider combat-camera sample or a worst-case 16-enemy/debris/ragdoll workload.
Full profiler reports remain ignored under `.cache/lastlight-framewalk-evidence`.

Final export: **76 assets, one scene, 57,028,943 bytes**. Compared all 77
exported asset/scene payloads and 76 asset IDs/types with the project; package
and moved-copy payloads match the fresh export. **180 source-content hashes**
record this revision; earlier fingerprints remain retained.

Current standalone: `.cache/Lastlight-Package`; moved review copy:
`/tmp/judas-lastlight-framewalk-review-20261010`. Final source fingerprints and
package comparison are recorded in `framewalk-handoff.json` and
`content-sha256.json`. Human review, controller hardware and Windows game review
remain pending. No commit/push/tag; protected engine evidence/research and the
original low-poly backup were not modified.

## HISTORICAL — expanded town-defence revision, 10 October 2026

This supersedes the original gun/climb and melee-only candidate descriptions
below. Earlier captures, fingerprints and failures remain historical evidence.
The current handoff is `README.md` and `ANIMATION_GUIDE.md` in the project root.

- 96 × 108 m town, eight enterable two-room houses, front/rear doors, yards and
  backstreets. Current canonical scene has 486 entities, 259 static bodies and
  22 authored dynamic breakable props. Ordinary navigation bake: 200 layers,
  439 polygons; source hash
  `dbe8bfb42347843201a5c8db88bf63324d97a7169e94f01027a4b02126b4fa97`.
- Only zombies are active: runner/brute/stalker variants, sight/hearing/search,
  flanking approaches, attack windup/lunges and obstruction bashing. The original
  unused soldier prefab is retained. Maximum final wave is 16 living enemies.
- Rifle aiming, recoil/sway/reload cues, physical RPG/grenades, points vendor,
  force/torque crate pickup/drag/turn/drop, house defence and updated zombie clips.
  All are ordinary project JS/assets using existing public engine APIs.
- Editable original Blender files and a practical manual-animation handoff are
  provided. Hand grips, magazine reload, grenade throw, mantle hand placement,
  bite/death/get-up and foot planting remain explicit presentation gaps.

### Human-reported invincible enemy: demonstrated cause and correction

`human-invincible-enemy.log` preserves the actual failure:
`RangeError: too many interrupted fade contributors` in enemy fixedUpdate.
Repeated Idle/Run threshold changes interrupted fades faster than contributors
could retire, faulting the whole script and consequently its damage callbacks.

The game now uses gait-speed hysteresis and minimum dwell longer than fade time
(enemy 0.28 / 0.12 seconds; player locomotion 0.24 / 0.15). Cosmetic selection is
isolated with an immediate-play fallback so animation rejection cannot disable
combat. No engine contributor budget was raised and no engine source was changed.

### Other concrete game/content corrections

- SDL modifier names are `Left Ctrl` and `Left Shift`; incorrect original names
  have been corrected in the project map and generator. Existing synthetic action
  injection alone did not prove physical-key spelling; it was checked against SDL.
- Grip direction turns through a bounded arc. While held, only the holder's motor
  ignores that body through the existing CharacterMotor API; release restores
  its normal collision. The held body remains physical against the world/enemies.
- Zombie near-obstruction attacks take precedence over a lunge with clear chest
  sight; attack reach also checks vertical distance and fresh line of sight.
- Explosive query ignores use actual safe entity wrappers, not JSON ID strings.
  Other characters do not become blast-proof cover; actual walls/props still do.
- Vendor two-unit bundles require room for both, avoiding payment for a truncated
  stock increment. Empty RPG attempts have a 0.35-second feedback interval.

### Actual application captures for this revision

Used the existing Release runtime/input/capture runner, not new engine tests.
Disposable copied projects positioned reachable fixtures and observed public
script state. Those fixtures/drivers are not part of the shipped human game.

| Capture | Observed result |
|---|---|
| `defence-startup.log` | Current arena, movement, camera switching, aiming/fire/reload/melee and first wave loaded without script diagnostics. |
| `defence-physical-carry.log` | B/T and movement carried then released the actual dynamic prop; it retained motion and settled. |
| `defence-house-barricade.log` | Entered a house with the held crate through its real front door, dropped it in the doorway; zombie reduced its health 60 → 37 and physically pushed it inside. No crate transform teleport. |
| `defence-vendor-rocket.log` | Actual N/UI purchase changed points 300 → 50 and rockets 0 → 2; firing consumed one and produced a normal physical projectile with bounded fuse cleanup. |
| `defence-rocket-target.log` | Purchased/fired rocket killed a 70-health runner (→ -52), awarded 100 score, produced self-damage and retired the projectile. |
| `defence-grenade-target.log` | Purchased/thrown grenade bounced/rested as a physical body, detonated after 2.4 s, killed a runner (70 → -18), awarded 100 score and caused nearby self-damage. |
| `defence-five-waves.log` | Actual director and weapon/damage logic completed all 8/10/12/14/16 waves: 60 kills, 7400 score, 7700 spendable points, 100 health, victory. Ordinary weapon casts/damage; no enemy-health or score assignment in the driver. |
| `defence-moved-package.log` | Moved package launched from unrelated `/tmp` directory; normal gameplay, camera switching, pause/UI restart and equipment selection loaded without script diagnostics. |
| `defence-final-equipment.log` | Latest one-line empty-RPG feedback interval received a short moved-package held-fire/no-ammunition capture with no script diagnostics. |

The five-wave capture uses a disposable automated aim/motion driver. This proves
end-to-end state/lifetime, not human difficulty, fun or animation acceptance.
One transient unready script-state observation at prefab creation was not a fault;
no persistent faulted enemy was observed. Original melee/trip/debris captures
below remain proof of those earlier paths, not a claim that every combination
was replayed against the revised town. No broad production suites were rerun.

### Lightweight current performance

`defence-performance.json`: M56 profiler, current Release Linux runtime, active
combat in the same arena/rules/assets with a disposable aim/motion driver; only
initial countdown shortened to 2 s to capture active workload. Most recent 120
unpaused frame records, 3–5 living enemies and 22 authored dynamic props:

- Mean frame 13.52 ms (~74 FPS), p95 17.63 ms, maximum 54.93 ms. This sample includes
  PNG screenshot capture; it is not a guarantee that all stalls are eliminated.
- 97 fixed steps: mean 2.81 ms / maximum 8.63 ms; JS fixed callbacks mean 1.39 ms;
  navigation 0.83 ms; rigid physics 0.09 ms. No fixed-step cap/catch-up loss.
- World render submission mean 10.14 ms. Original ~94 FPS sample below used a
  smaller different arena; it is not a comparable before/after optimization result.
- This is not the worst-case sixteen-enemy/six-ragdoll/debris workload. Motor crowds
  can overlap; barricades are pushed/attacked rather than navigation-carved;
  simplified animation/physical recovery and controller/Windows review remain.

Final export: **67 assets, one scene, 56,853,772 bytes**. Compared 69
reachable asset/scene payloads byte-for-byte with the project, plus 67 asset IDs/
types. Sidecar source paths are intentionally normalized by the existing exporter.
The final package launch and content comparison report no script diagnostics or
payload mismatches.

Current standalone package is `.cache/Lastlight-Package`, with a moved launch copy
at `/tmp/judas-lastlight-defense-review-20261010`. Editable sources are in the
project, not required by the runtime package. `content-sha256.json` records current
non-Review project content; original and melee-only manifests are retained.
Human review of the revision remains pending. Nothing was committed/pushed.

## HISTORICAL — original gun/climb candidate: actual application previews

These records describe the original candidate before the melee/destructible
extension below; they are retained as evidence rather than claimed as proof of
the new combat paths.

Used the installed current Release runtime and its existing input/capture runner:
normal project startup, movement/jump, first/third view, firing, pause/resume;
reachable ledge climbing; pursuing enemies, real attacks and the defeat menu.
The climb capture records foot height rising from 0.017 m to 1.357 m and settling
supported on the authored 1.34 m platform through CharacterMotor motion.

A **disposable copied project**, with an automated aim/motion capture driver, ran
all five waves through the actual weapon/raycast/signals/enemy scripts. It did not
assign enemy health or scores, skip waves or grant invulnerability. Its victory
capture shows 5/5, 0 hostiles, 5800 score and 55 health. The shipped project contains
neither this driver nor auto-aim; it is human-controlled. The final local-avoidance
correction received a corresponding complete gameplay capture. This proves game flow,
not human fun/balance acceptance.

The exported package launched with `/tmp` as its working directory and normal
package discovery. Audio/assets/scripts/UI/navigation loaded, soldiers attacked,
and a rendered screenshot was captured. The pause-menu restart also reconstructed
a fresh game with 100 health, zero score and the initial countdown. Package is ~55.4 MB, 42 assets, one scene.

## HISTORICAL — original candidate: lightweight live rendering sample

M56 profiler, Release Linux package, 1600 real-time rendered frames, six enemies.
Most recent 120 frame records: mean **10.63 ms (~94 FPS)**, p95 **11.49 ms**,
maximum **53.68 ms** (sample includes PNG screenshot capture). No fixed-step cap
was reached. Mean fixed simulation step **1.47 ms**, JS fixed callbacks **0.66 ms**,
navigation update **0.31 ms**. These are one host/sample, not a hardware guarantee;
the game has at most 14 living enemies in a wave. Full profile remains in ignored
`.cache/lastlight-review/profile.json`.

## HISTORICAL — original integration issues found and corrected in content

1. Navigation prefab serialization requires an explicit costs field, even when
   the map is empty (`nav.agent.costs ""`). The first authored prefab omitted it;
   the next attempt mistakenly supplied an unpaired zero. Both were content errors.
2. Transform writes on a motor are teleports and reset its motion. Initially turning
   each enemy root every fixed step erased desired movement. Facing now belongs to
   a body-free visual child, using the motor's presented pose; roots are never
   teleported for facing. This is documented existing API behaviour, not a new bug.
3. Navigation avoidance returns a speed-bearing steering vector. The final script
   preserves its magnitude instead of normalizing every result to full speed.

No confirmed engine defect was encountered. No engine test suites/build were rerun;
only the small existing navigation-bake tool was built and game application previews
were run. Human gameplay, controller hardware and Windows review remain outstanding.
Original low-poly assets/backups and all historical engine evidence are untouched.

## HISTORICAL — melee / destructible extension: actual application previews

- Project logical actions: RMB quick punch, F heavy punch, Q shove. Quick/heavy/
  shove damage is 25/55/5, with separate impact timings and short nearest-hit
  sphere casts. Cover participates in the cast; game rules stay in JavaScript.
- Enemy motor motion handles pushes/stun. Zombie trips are conditional on low
  collision geometry or an unsupported ledge; soldiers do not trip. The fall uses
  ordinary passive ragdolls, bounded to six simultaneous articulations. Recovery
  checks for supported clear placement and crossfades to the ordinary animation
  pose; this is not physical balancing or an active get-up controller.
- Crate/barrel health is 60/90. Breaking replaces the intact body/mesh with six/ten
  separately simulated debris bodies. Each set retains total mass (18/24 kg),
  actual contact-point velocity and angular velocity from the original body.
  Cleanup is 12 seconds, with twelve active debris groups maximum. There is no
  engine shooting-target, injury-scoring or destructible-prop subsystem.
- New content includes `melee.js`, `combat.js`, `destructible.js`, reusable
  breakable/debris prefabs and `Tools/build_breakables.py`. Original model backups
  are unchanged. No engine/runtime API change is required.

Ran short application input/capture previews, not engine suites or new tests.
Disposable project copies placed reachable fixtures and recorded actual script
state; the delivered project contains no preview drivers or automatic combat.

- Nearest-query light punch reduced zombie health 75 -> 50; heavy punch reduced
  it to -5 and awarded exactly 100 score. Both camera-mode animations rendered.
- An open-ground shove reduced health to 70 and displaced the motor backward
  (z 16.76 -> 15.20 m), with zero trips. It then resumed its normal pursuit.
- The same shove into a real 0.5 m obstacle activated the 11-body passive rig from
  its current pose. Its physical origin moved from z 16.66 -> 13.74 m as it fell;
  the disabled motor stayed behind until clear standing placement was found.
  Bodies retired and pursuit resumed. Captures show the fall and return fade.
- Actual shots against mapped falling bodies reached the owning enemy through
  the project's safe bone-owner map, killed it, and retired the articulation.
- Crate heavy/light hits left 5 health then broke it. Tagged dynamic-body count
  increased 10 -> 15 (one body replaced by six); total tagged mass stayed 206 kg.
  Barrel heavy/light/light hits left 35, then 10 health, then broke it; count rose
  10 -> 19 and total mass again stayed 206 kg. After the 12-second cleanup, exactly
  the respective six/ten pieces retired. Logs/captures accompany this note.
- Final standalone export: **51 assets, one scene, 55,761,561 bytes**. It launched
  from `/tmp`, used melee, view switching, movement/jump, shooting, pause/resume,
  and rendered the wave-one town without script faults. The package contains the
  human-controlled game, not the disposable fixture drivers.

Content corrections during this extension: hinge endpoint frames were aligned
with the runtime child/parent ordering; the fallen visual origin now blends toward
standing placement during the return fade; old motor knockback is not reapplied
when an already-physical zombie recovers. No confirmed engine bug was found.
A disposable diagnostic initially read mass from body-free visual entities;
restricting that observer to actual dynamic bodies corrected the observer only.

The original live rendering measurements above describe the earlier gun/climb
candidate; they are not a new performance claim for six simultaneous ragdolls.
The game bounds physical falls/debris and still needs human balance/gameplay review.
No controller hardware or Windows verification is claimed.

## Files

- `lastlight.judasproj`, `Scenes/town.judas`;
- `Assets/models/`: original GLBs and army/undead palette variants;
- `Assets/scripts/`: player, camera, climb, weapon, melee, enemy, destructible,
  combat routing, HUD, facts and maths;
- `Assets/prefabs/`: soldier/zombie with independent motor/visual/navigation state,
  plus breakable crate/barrel and bounded dynamic debris groups;
- `Assets/navigation/`: baked surface plus metadata;
- `Assets/ui`, `audio`, `fonts`, `textures`: authored presentation/resources;
- metadata, stable ID map and provenance;
- `Tools/build_town.py`, `Tools/build_breakables.py`, README and these preview captures/logs.
