# Lunar Island Stage 4 — Roads, Water and Sky

Stage 4 is implemented on Experimental; human gameplay acceptance remains pending.
The operator approved advancement after reviewing Stages 1–3. Stage 5 world
population and Stage 6 final polish have not begun. No engine implementation,
public API, independent research, historical evidence or unrelated project changed.

## Playable result

Six transport types: hatchback, pickup, motorcycle, skiff, helicopter and plane.
Vehicles are editable ordinary scene assemblies and ordinary prefab assets. Each
has native physical collision/mass, persistent fuel/condition, shared bag storage,
entry/exit, third-person chase and optional bonnet view. A finite supply locker,
Mara's finite fuel/kit stock and a workbench repair recipe connect transport with
existing gathering, trade, crafting, housing and combat.

The connected tested loop gathers real timber, loads the pickup, drives, brakes,
exits, unloads and constructs an actual foundation. A real pistol shot damages the
pickup; a consumed repair kit restores condition and a consumed can restores 5 L.
Overweight cargo refuses atomically. A fresh process restores personal inventory,
vehicle cargo/fuel/condition, actual body positions and pilot ownership, then drives
again. No inventory, vehicle pose/velocity, damage or fuel is injected to obtain
these passes. The test fixtures position/aim the player to avoid long walks.

Native wheels/suspension and powertrain shafts/combustion/gearbox/clutch/differential
move car and truck; motorcycle uses the same wheel system with an electric-style
source and explicit game-side balance assistance. Boat/aircraft retain native
rotating power sources with finite inertia/load; JS supplies sampled-water buoyancy,
power-bounded thrust, lift, drag and assisted control torques. Actual forces and
collision move all vehicles; no prescribed trajectory/RPM display substitutes for
simulation. Airborne tests independently inspect real physical height and position.

The small transport garage actually unloads its undriven pickup and reconstructs
native body, four wheels, powertrain and depleted cargo. On boarding, the complete
assembly is adopted to root ownership, keeping its stable regional reference.
Its adopted reference and native mechanical/body state survive a fresh-process
save/load and continued driving. The original six vehicles are root-resident.

## Project organization and exact engine mechanisms

- `Authoring/vehicles.json`: content, dimensions, capacities, mass, fuel and stock.
- `tools/author_stage4.py`: native assemblies, public component definitions, runway,
  dock, original fallback models/audio, generator receipts and prefab outputs.
- `Assets/scripts/vehicle.js`: per-vehicle durable state, real actuation, collision
  damage, shaft-driven visuals/audio; lifecycle registration and destruction.
- `transport.js`: same-player control transfer, safe exit, cargo/services, chase
  camera, ordinary streaming interest and complete-assembly adoption.
- `vehicle-rules.js`, `vehicle-ui.js`, `vehicle-session.js`: pure service accounting,
  existing-HUD menus and transient lifecycle registry respectively.
- `inventory.js`: reused stack/slot/weight transaction authority for every cargo bag.
- `Scenes/island.judas`, `Scenes/transport_garage.judas` and world composition:
  editor-owned assemblies plus the optional regional demonstration.

Public JudasJS primitives consumed include Entity transform/presentedTransform,
velocity/angularVelocity/pointVelocity, applyForce/applyTorque/applyImpulseAtPoint, CharacterMotor.enabled/velocity, wheel and powertrain
settings/state, audio state plus setAudio/playAudio/stopAudio, world.fluidSample, physics
raycast/sphereCast/capsuleCast with ignored entities, input axis/held/pressed,
scenes.setInterest/owner/adopt, saves.reference and existing save slots/lifecycle
hooks, UI document/list/focus and view/camera APIs. These are existing documented
bindings. No internal native dispatch or game-specific C++ component was added.
Native mass is deliberately not mutated through an invented script API.

## Assets and authoring

The real installed Prompt3D trained parser and procedural templates generated car,
pickup, boat and plane sources (seeds 91400–91403). It has no motorcycle/helicopter
template: those, rotating visual parts and audio are original project authoring.
Raw generated GLBs, parsed prompts, parameters and source/output hashes are retained
in `ArtSource/stage4-prompt3d` and `asset_catalog.json`. Existing CC0 material scans
are reused, and all source tools/research remain read-only. No new downloaded asset,
voice or third-party sound was introduced. See `ArtSource/PROVENANCE.md`.

README describes adding vehicles, capacities, stock and rebuilding the reserved
scene range. Included runtime art and exports do not depend on the generator,
Blender, Python or an ML service. Generated car wheel geometry is separated from
body rendering; native wheel owners remain unit scale with scaled visual children.

## Corrected development failures and limitations

All original failed runs remain under `Review/stage4`, including:

- Native component values initially emitted with wrong JSON types; author corrected.
- Scaled wheel-owner transform correctly rejected by engine preflight; moved mesh
  scale onto a visual child, preserving the native wheel invariant.
- Empty material groups after wheel removal failed export; removed empty groups,
  retained raw original models and failure log.
- Garage content referenced a template with the wrong mapping; author corrected.
- Observer captured a short-lived definition by reference; corrected capture lifetime.
- First helicopter source had inadequate spin-up/hover authority; revised the game's
  actual motor power/torque/load and hover control, retaining failed takeoff evidence.
- Initial cargo observer assumed Move stack took all 12 cans. Actual item stack is
  four; corrected the fixture and retained the genuine overweight negative control.
- Immediate regional `active` observation returned `{}` because script instantiation
  had not completed. Physics publication precedes async script readiness. Pending
  state was preserved and actual UI exposed restored cargo correctly. The corrected
  observer waits for presence of `bag`, then compares its exact value; it never waits
  for the expected count. No native correction or arbitrary settling delay added.

No genuine new engine defect was established in this stage. The previous accepted
Stage 2 signed-zero/inertia persistence corrections remain intact and are exercised
by new cold saves. This stage uses a new save/content identity; old stages' saves
are preserved, not migrated. Compatibility still rejects mismatched runtime content.

Flight and motorcycle balance are assisted game models, not aircraft/rider research.
Stored cargo enforces weight/slot limits but does not change chassis inertia/mass.
Damage changes condition and available propulsion; repairs do not reconstruct
crumpled panels. Body collision uses coarse compound proxies, rotor blades are
visual, and driver avatar is hidden (no seated/entry animation). No passengers,
NPC vehicle tactics, full traffic, populated towns or universal vehicle re-suspension.
Driven assemblies stay root-resident after adoption. The runway and spring dock
are small homestead facilities, not Stage 5 island expansion. Basic water remains
prescribed/nonconserved and advanced fluid dynamics stays disabled.

## Final-source focused verification

All tests run on Linux with the existing Release native engine. No engine source
changed, so there was no ceremonial clean engine rebuild/full production-suite run.
The project observer was compiled/linked against that exact native build. Raw logs,
screenshots, timings and expected endpoints remain under `Review/stage4`.

- Pure inventory/crafting: 83 checks; encounter rules: 36; new vehicle service/fuel:
  19, including invalid/nonfinite state, full-tank refusal, bounded burn, kit refusal,
  conservation and weight/slot transaction negatives. All passed.
- JudasJS preflight: 25 modules compile/link, no failures. Root/regional scenes and
  all eight vehicle/service prefab files pass native validation; navigation rebuilt
  from final geometry (105 layers, 796 polygons, source hash in navigation-final.log).
- Connected truck cargo/build/services/firearm-damage: 55 checks. Fresh-process
  restore and continued driving: 44. Both passed.
- Actual regional unload/reload, remapped wheel/powertrain reconstruction, ownership
  adoption and cargo: 34 checks. Fresh-process adopted regional vehicle: 48. Passed.
- Car/truck/motorcycle: 18 each; boat/heli/plane: 19 each. Actual physical endpoints,
  native shafts, independent fuel and required flight height/flotation passed.
- Both aircraft takeoff → controlled landing → safe exit: 17 each, passed.
- Actual prefab instantiation/remapped drivetrain/four wheels, ordinary entry and
  physical driving, own fuel use and independent original car: 19 checks, passed.
- Existing Stage 3 combat/trading/quest/death scenario: 80 checks; actual fresh-process
  continuation: 33. Passed against the combined Stage 4 content.

See the final logs for any additional housing/editor/export qualification outcomes;
these are reported separately below rather than inferred from the vehicle tests.

## Performance scope

Ryzen 5 5600, GTX 970 4 GB, 8 GB RAM; raw host record in stage4/hardware.txt.
At 1024×768, 600 real rendered application frames with a supplied fixed 60 Hz
simulation clock, hidden window, vsync disabled and dummy audio measured:

| Cost | Mean | Median | p95 | Worst |
| --- | ---: | ---: | ---: | ---: |
| Whole frame | 8.166 ms | 8.042 ms | 9.531 ms | 14.434 ms |
| Native physics step | 0.142 ms | 0.138 ms | 0.169 ms | 0.240 ms |

318 physical bodies were present. The recorded throughput is about 122 frames/s,
not a claim about human displayed FPS, full-island scale or a before/after speedup.
The last 120 retained profiler frames measured JS fixedUpdate mean 1.139 ms,
update 0.046 ms and presentation 0.383 ms. Nested/inclusive scopes are not summed.
Resident process memory was about 539 MiB, including about 67.5 MiB reserved by the
enabled evidence profiler. Raw M56 profile is losslessly compressed as
`final-performance/profile.json.gz`; derived costs and raw frame CSV are retained.
Six distinct driving routes' final 120-frame mean frame costs ranged 5.99–9.82 ms;
physics means ranged 0.088–0.189 ms. These routes are not equivalent workloads and
are not compared as a speedup. `DRIVING_PERFORMANCE.json` includes p95 and maximum.

## Reproduction, source identity and acceptance

`stage4/REPRODUCE.sh` uses fresh evidence/user-data paths and an existing Release
build. Pair each writer/reader without editing runtime content. Native harness
fixtures and failure logs are explicit; no human acceptance is fabricated.
`FINAL_SOURCE_FINGERPRINTS.sha256`, `FINAL_BINARY_FINGERPRINTS.sha256`,
`FINAL_EVIDENCE_FINGERPRINTS.sha256` and `CHANGED_FILES.txt` identify the candidate.
The unchanged runtime's embedded build provenance is retained honestly in
`PACKAGE_BUILD_PROVENANCE.json`; it is not relabelled as a fresh clean Stage 4
engine build. The Stage 2 persistence repairs are already in that native build.

Use the project README controls and `stage4/HUMAN_CHECKLIST.md`. Review realistic
flight feel, vehicle/camera presentation, engine audio, cargo menus and landing
usability yourself. Windows/Android, physical controllers and listening tests were
not run. Stage 5 requires another authorization. Main remains untouched.

### Additional combined qualification

Existing harvesting/crafting/felling/mining/swimming/gravity gun/housing/construction/
physical support-loss/doors/storage/survival scenario: **133/133**, followed by
**42/42** in a fresh process, including restored actual open hinge geometry after
120 continued steps. Both passed on final Stage 4 runtime content.

Editor qualification used an isolated project copy to avoid saving over authored
content. Duplicate/undo and Play/Stop reported **IDENTICAL** authored scene; scene
save succeeded, resource failures were zero and background jobs reported no failures.
The automated editor debug view adds diagnostic geometry, so its timing is not
used as a normal gameplay performance number.

Export succeeded: **144 assets, 2 scenes, 205,943,744 bytes, 6.261 seconds**. The
complete package was copied to `/tmp/Lunar Island Stage 4 Relocated` and launched
from unrelated `/tmp` working directory. A genuine native save-service request
loaded the final combined-content encounter expedition, with no script error, failed service
or harness assertion; screenshot and log retained. The repository-local replayable
package is `.cache/lunar-island-stage4/Lunar Island/` (untracked), executable `judas`.
Export contents and native tool bytes are separately fingerprinted. All native
runtime/tool binaries match the previously used Stage 3 binaries; this stage is
project-only work.

The final regional, encounter and housing native save bytes are retained losslessly
compressed beside their expected endpoint records. The cargo-write record retains
its observed expected endpoints; that slot was later replaced by the subsequent
regional/encounter tests, so a separate final cargo-slot binary is not claimed.

The archived regional save was additionally restored in an independently scoped
user-data directory: **48/48**, including physical continued driving. The relocated
standalone then loaded those same archived bytes as well; `relocated-regional.log`
and screenshot distinguish this from the earlier relocated encounter-save check.
