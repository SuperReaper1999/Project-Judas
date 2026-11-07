# Water Workshop — pickup and control follow-up

Experimental, 7 November 2026; parent checkpoint
`45b0cf3298a75338a54652f28425a86f9178e3db`. Main and the Alpha fluid lock are unchanged.

## Confirmed defect and correction

The Workshop's pickup ray started inside the CharacterMotor capsule without
excluding that capsule. It therefore hit the player before the aimed prop. G
could not acquire the prop, and H consequently had nothing to throw. This was
a project-controller defect, not a failure of MK2 buoyancy or the grab-force API.

The actual authored controller now excludes its player from the pickup ray.
`before.log` records three failed physical pickup/throw endpoints;
`self-filter-only.log` records all twelve original checks passing with just that
query-filter correction. Physics settings, collision shapes and acceptance
thresholds were unchanged between those two runs.

The controller also provides a centred reticle, target/held feedback, release
and throw confirmations, and explanations for unreachable/fixed targets and
empty-handed throws. Gameplay interactions use fixed-step input, including taps
between simulation steps. F remains a paired circulation pump/drain, with explicit
HUD feedback; it intentionally does not keep raising the pool level. B now
disturbs nearby water along the view ray, stops at solid obstructions, and reports
when no water is targeted. Its game-owned local impulse is 350 N s over a 0.6 m
radius, replacing the old barely visible 65 N s impulse at a hidden fixed point.
No fluid solver or rigid-body implementation changed.

The project generator preserves the authored controller instead of rebuilding
it through fragile substitutions of the Basic Water example. Regeneration was
checked to preserve the controller and reproduce the authored UI byte-for-byte.

## Verification

- `judas_water_workshop_tests`: **25 checks pass**, using the actual project,
  actual JudasJS controller, ordinary input bindings, InteractivePlay and MK2.
  This checks lifting, held-state feedback, real throwing, release/falling,
  empty-handed feedback, circulation, short taps across zero-step/catch-up frames,
  walking, jumping, diving/swimming, actual splash disturbance, conserved water,
  reticle alignment, pause/resume and absence of script faults.
- The same gameplay checks passed on Mesa llvmpipe and native GTX 970 / NVIDIA
  OpenGL 4.6. Synthetic input is automated verification, not human acceptance.
- Four focused Linux gates pass: grid units, water runtime, Water scripting and
  the Workshop controller. Raw machine/build identity and per-case logs are retained.
- A fresh standalone export was relocated to a path containing spaces and run
  from a foreign directory. Separate-process save/load retained the pump and
  nonzero source/drain receipts. Actual 1/2/3/4 bindings selected every scene;
  R reloaded the scene; Escape paused/resumed. Explicit service waits and scene
  assertions passed. See `export/RESULTS.json` and the three run logs.
- Validation-policy Python tests: **17 pass**. CTest-registration tests: **9 pass,
  2 skipped** because Ninja is unavailable. The new gameplay test is explicitly
  unavailable in Alpha while the fluid lock remains, rather than falsely passing
  without water. Registration expectations also include the two existing grid
  runtime/script tests.

`expanded-controls.log` retains an intermediate test failure: the first dive
fixture put the capsule almost against the pool floor, so its final downward
velocity was an invalid oracle. The final test starts at 1.4 m in real water and
checks actual descent, then upward swimming. It also caught an attempted HUD
label using a nonexistent Entity.name property; the final controller uses only
documented entity APIs. Neither issue is represented as an engine defect.

The first gate attempt rejected stale executable provenance after source edits.
The source was reconfigured/rebuilt and the qualified run passes; stale binaries
were not admitted by weakening the gate.

## Reproduce / human retry

```sh
cmake -S . -B build
cmake --build build --target judas_water_workshop_tests -j2
build/judas_water_workshop_tests
python3 scripts/prealpha_validate.py --build-dir build --test water-workshop-controls
python3 tools/validation/qualify_water_workshop.py --runtime build/judas --exporter build/judas_export --project projects/water_v2_workshop/water_v2_workshop.judasproj --output /tmp/water-controls-fresh-proof
```

The output directory must be fresh. Open the updated Workshop, aim at the brown
crate on the deck or the green floating box, and press G. Check that the HUD says
it is held, then try G to drop and H to throw. Q works while immersed; B needs
nearby water under the crosshair. This follow-up does not unlock Alpha, qualify
additional platforms or change the existing water support boundaries.
