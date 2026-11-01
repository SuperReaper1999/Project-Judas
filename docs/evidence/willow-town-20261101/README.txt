Willow Town — native Linux qualification, 1 November 2026

Base: bc65f6f079d22a7c5789a99759dce028972c8c50, Experimental.
Main remains 49c4f3d9e29debe669411f01501e0276dea23231.
No release, fluid unlock or new human acceptance.
Report: projects/willow_town/Tests/QUALIFICATION.md at repository root.

Final authoritative results
- raw/application-final.log: 115 checks. Six real native brains, camera-based
  world clicks, correct visible stats, direct control, real WASD movement,
  pause/speed, 600 game seconds, per-resident progress, economy, closed-shop
  rejection, reopened purchases and modern same-process save restoration.
- raw/cold-qualified.log: 56 checks in a genuinely fresh process. Six physical
  endpoints and native clocks/choices/cooldowns match before stepping; restored
  actions then move the actual bodies.
- raw/FINAL_APPLICATION_RESULTS.json: final export, isolated editor and standalone
  runs all exit 0. Standalone: 1,200 frames, cwd /tmp, engine-root variable unset.
- raw/editor-final.log: authored scene IDENTICAL after Play/Stop, save succeeds,
  zero failed resources. User preferences and source project are untouched.
- raw/build-final.log and build-package-final.log: new compilation warning-free.
- raw/manifest.log and registration.log: 106 entries, 98 native targets; registration
  module passes nine tests with two existing skips.

Preserved failures and incomplete earlier passes
- application-first.log / application-second.log: generated scene omitted required
  settings or render fields. Native loading rejected it; generator was corrected.
- application-third.log: earlier 96-check pass. Its short watch was insufficient.
- long-before.log: forward pedestrian avoidance; 88 services at 600 seconds with
  shop congestion. This did not establish a permanently frozen entire town.
- long-after.log: pedestrian passing improved total services to 270, but its
  aggregate 103-check pass concealed Mara being individually stuck.
- retained-corner-negative-control.json reapplies the stronger per-person gate to
  those original observations. Mara remains at six services from 400 to 600 and
  has never rested: the expected negative control fails.
- The final run reaches 323 services and EVERY resident progresses between 400
  and 600, with work, meals and rest completed. Chat rewards include both people;
  they are not a count of unique conversations. These are behavioural outcomes
  after game-policy corrections, not a physics speedup claim.
- pre-passing-town.js and pre-grid-corner-town.js preserve the prior sources.
  pedestrian-deadlock.json and grid-corner-reconstruction.json record exact
  snapshot/source hashes and independent geometric derivations. They are not
  native profiler traces. Actual casts remained authoritative throughout.
- fixtures.tar.gz contains the exact first/second/third/long-after/final project
  copies, expected records and final modern .save bytes. No binaries or build
  caches are included.

Reproduce from repository root, using fresh build/output directories
  cmake -S . -B /tmp/judas-willow-build -DCMAKE_BUILD_TYPE=Release
  cmake --build /tmp/judas-willow-build -j2 --target judas judas_editor judas_export judas_willow_town_application_tests
  JUDAS_WINDOW_SIZE=1280x800 /tmp/judas-willow-build/judas_willow_town_application_tests projects/willow_town/Willow_Town.judasproj /tmp/willow-check
  JUDAS_WINDOW_SIZE=1280x800 /tmp/judas-willow-build/judas_willow_town_application_tests projects/willow_town/Willow_Town.judasproj /tmp/willow-check cold
  /tmp/judas-willow-build/judas_export projects/willow_town/Willow_Town.judasproj /tmp/willow-export /tmp/judas-willow-build/judas

Move the WHOLE exported package, then run judas from an unrelated directory with
JUDAS_ENGINE_ROOT unset. Use a copy of raw/standalone-final.txt with its screenshot
path adjusted as JUDAS_TEST_SCRIPT. For editor smoke tests, use an isolated project
copy, separate XDG_DATA_HOME and JUDAS_EDITOR_AUTOTEST output prefix. Graphics runs
need a working GL desktop. Do not overwrite the operator's projects/preferences.

Art regeneration: python3 tools/CreateWillowTown.py. This intentionally replaces
the generated scene/layout/UI, so preserve editing work first. Pillow and DejaVu
are authoring dependencies; neither is needed to run the game.

Live qualified standalone: .cache/willow-town/Willow-Town-standalone/judas.
Pedestrians pass one another, collide with scenery and exclusively claim service
stations. No universal pathfinder, crowd simulation, full production economy,
new device qualification or human acceptance is claimed. FINGERPRINTS.json and
PACKAGE_PROVENANCE.json identify the actual source and binary bytes. Binaries
honestly embed a base-plus-dirty development identity, not the later checkpoint.
SHA256SUMS verifies every evidence member.
