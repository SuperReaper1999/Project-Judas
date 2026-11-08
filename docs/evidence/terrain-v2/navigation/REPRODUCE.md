# Native terrain navigation integration

Base: `4905918a`. Experimental / Release / GCC15.2 / Ryzen5 5600,8GB.
Application/GPU checks used GTX970, driver580.178.04, modern4.6 and portable3.3.
No main/release/fluid-lock/installed-RPG-SDK changes.

```sh
cmake -S . -B build
cmake --build build -j2 --target judas_terrain_v2_navigation_tests judas_terrain_v2_world_tests judas_navigation_tests judas_navigation_game_tests
build/judas_terrain_v2_navigation_tests --write /tmp/native-nav-FRESH
build/judas_terrain_v2_navigation_tests --read /tmp/native-nav-FRESH
build/judas_terrain_v2_world_tests
build/judas_navigation_tests
build/judas_navigation_game_tests
python3 tools/validation/qualify_terrain_v2.py --build build --output /tmp/native-nav-export-FRESH
```

Native64 controls +genuine cold7: real source height (Box negative control),
original route, pending revision isolation, excavation, longer reroute, actual
agent replanning, paint preserving nav revision, collider disable/reenable, rigid
pose/origin, whole-source removal and restoration, private worker handoff,
settings mismatch rejection, complete cleanup, cancellation, oversized/invalid
preflight, native export validation, and real tunnel under a hillside. The last
fixture preserves two walkable layers and routes below the hill across chunks;
a physical cast independently confirms that the tunnel is open.

Retained regressions: world93 with300-step exact monolithic seam trajectory;
component35, surface59, handoff25, fluid/particle interaction85, navigation92,
actual Spring Range game26, save exclusions77, export18, creation46,
GPU978 modern +978 portable. API/type checker passes; no new JudasJS symbols.
Registration11 checks passed with2 existing environment-specific skips.

Performance driver (no concurrent compilers/benchmarks):

```sh
cmake --build build -j2 --target judas_terrain_v2_native_benchmark
/usr/bin/time -v build/judas_terrain_v2_native_benchmark /tmp/nav-off-FRESH
/usr/bin/time -v build/judas_terrain_v2_native_benchmark /tmp/nav-on-FRESH --navigation
```

This holds the1km513² physical source and1,024colliders constant. Optional
navigation covers128m at0.5m cells, NOT the entire kilometre. Five warm-file
samples per mode run off then on, so these diagnose added work; no independent
percentile, CPU speedup or FPS claim. Full navigation coverage/tile scalability
still needs profiling. No silent resolution/coverage reduction occurs.

Preserved failures and fixes:

- Cold03 skipped the normal `WorldPersistence::Prepare` phase after saved-scene
  construction. Corrected fixture follows build/prepare/restore; its expected
  participant creation is not an engine failure or a relaxed assertion.
- First relocated application walked/saved before project brush queue completion.
  Queue was outside persistent script state: a REAL Workshop defect. The queue
  now persists; dabs are removed only after acceptance. Corrected full test saved
  with8queued/busy, cold-restored7after the in-flight dab committed, finished and
  resaved0, traversed the complete tunnel and cold-restored beyond its far exit.
  Original position/geometry/save requirements remain and navigation is also
  required. It uses normal game actions and source-parked relocated packages;
  no native injection, teleport or test-only game mode. Bounded repeated cold
  settlement passes inspect actual project queue state instead of guessing a
  render-frame deadline. Screenshots were inspected.
- Compiler temporary-lifetime warning in terrain source publication was made
  unambiguous using named owned outputs; the synchronous function never retains
  the former temporary. No actual dangling ownership was established.

Native surfaces reuse existing Navigation Surface settings with empty asset.
Other props need normal obstacles/separate surfaces. Static external bakes cannot
silently follow geometric edits; explicit rejection and useful diagnostic remain.
Current navigation rebuilds configured coverage as a whole. Keep sampled heights
relative to a landscape and place it with its transform rather than encoding a
large absolute elevation into local height samples; Recast's finite vertical
span/resolution applies. Planetary excavation, finer physical residency and final
SDK/consumer qualification remain work. Human acceptance is pending.
