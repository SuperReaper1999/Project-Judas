# M83 reproduction

Run from the repository root. Linux Release is the automated acceptance platform.
Keep a matching source/library/header build; do not mix historical engine
archives with current headers. The checked-in cooked lab assets are intentional
authoring products, not disposable build outputs.

## Build and focused checks

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2 --target judas judas_editor judas_export \
  judas_mechanical_damage_tests judas_mechanical_connection_tests \
  judas_mechanical_damage_script_tests judas_mechanical_damage_application_tests \
  judas_structural_tests judas_bending_tests judas_wheel_tests \
  judas_wheel_lifecycle_tests judas_powertrain_tests judas_powertrain_lifecycle_tests \
  judas_powertrain_script_tests judas_deformable_tests judas_fracture_tests \
  judas_collision_geometry_tests judas_physics_tests judas_collision_tests \
  judas_physics_cast_tests judas_joint_tests judas_contact_lifecycle_tests \
  judas_physics_continuation_tests judas_save_storage_tests judas_streaming_tests
```

`RUN_FOCUSED_CHECKS.py.txt` records the executed sequential 21-suite driver,
including exact arguments in `linux/final-focused-cases.json`. For a repeat,
copy that driver to a scratch file and change `out` and `XDG_DATA_HOME` to fresh
scratch directories; its recorded form otherwise overwrites accepted receipts.
Script fixtures require fresh writable scratch paths. Run expensive workloads
sequentially, with builds finished before taking performance measurements.

The three new mechanisms can also be run directly without replaying unrelated
systems:

```sh
m83_out=$(mktemp -d /tmp/judas-m83-reproduce.XXXXXX)
build/judas_mechanical_damage_tests
build/judas_mechanical_connection_tests
build/judas_mechanical_damage_script_tests "$m83_out/js"
```

The public-script fixture executes the exact `mechanical-damage.js` cookbook.
Original failures/fixtures and the independent old-COM negative are preserved
in `ATTACHMENT_NUMERICS.md` and the referenced logs; they are not expected passes.

## Normal application and cold restoration

The application test accepts exactly:

```text
judas_mechanical_damage_application_tests PROJECT MODE OUTPUT_DIRECTORY
```

Modes are `probe`, `write`, `read`, `lifecycle` and `oblique`. `probe` exercises
the flat physical sequence without saving; `write` creates slot `damage-current`;
`read` must run afterward in a **separate fresh process**, with the same project
identity and user-data root. It checks published physical participant bytes
before resumed stepping and actual detached-body response afterward. `lifecycle`
exercises prefab/remapping, retained-region damage/revisit, destruction and reload.
`oblique` selects the registered oblique scene and runs the same physical checks.

```sh
m83_user="$m83_out/user"
env SDL_AUDIODRIVER=dummy XDG_DATA_HOME="$m83_user" \
  build/judas_mechanical_damage_application_tests \
  projects/damage_lab/damage_lab.judasproj write "$m83_out/write"
env SDL_AUDIODRIVER=dummy XDG_DATA_HOME="$m83_user" \
  build/judas_mechanical_damage_application_tests \
  projects/damage_lab/damage_lab.judasproj read "$m83_out/read"
env SDL_AUDIODRIVER=dummy XDG_DATA_HOME="$m83_user" \
  build/judas_mechanical_damage_application_tests \
  projects/damage_lab/damage_lab.judasproj lifecycle "$m83_out/lifecycle"
env SDL_AUDIODRIVER=dummy XDG_DATA_HOME="$m83_user" \
  build/judas_mechanical_damage_application_tests \
  projects/damage_lab/damage_lab.judasproj oblique "$m83_out/oblique"
```

The final native graphics runs retain the host desktop SDL/OpenGL configuration
(no `SDL_VIDEODRIVER=offscreen` override), use dummy audio and disable VSync in the
fixture. The hidden fixture advances synthetic 1/60 s frames and prints measured
wall work plus actual counts. Offscreen software runs may reproduce correctness
but are not comparable native-rendering performance samples. No network ports or
external service are required. `XDG_DATA_HOME` isolates saves/settings; do not
delete an existing user-data tree merely to rerun tests.

## Editor, standalone and moved package

`RUN_PACKAGE_CHECKS.py.txt` records the executed editor/export/moved-package
driver. Its editor fixture copies the entire project into a **fresh scratch
directory**, makes the ordinary root powertrain first in hierarchy order for
generic inspector automation, and runs `JUDAS_EDITOR_AUTOTEST`. Edit, duplicate/
undo, Play/Stop and save operate on that copy; the accepted project stays intact.
Use fresh paths and a scratch output directory when repeating that driver.

The final package paths are `/tmp/judas-m83-damage-lab-final-20261013` and
`/tmp/Judas M83 Final Package`; build products remain outside Git. Equivalent
fresh-path commands are:

```sh
m83_root="$PWD"
m83_package="$m83_out/export"
m83_moved="$m83_out/Moved Package"
build/judas_export projects/damage_lab/damage_lab.judasproj \
  "$m83_package" build/judas
cp -a "$m83_package" "$m83_moved"
(cd /tmp && env XDG_DATA_HOME="$m83_out/package-user" \
  "$m83_moved/judas")
```

The moved runtime launches **without a project argument** from an unrelated
working directory, finding normal `game.judasproj` content beside its executable.
For deterministic smoke, copy `export-smoke.txt` and replace its absolute
screenshot destinations with scratch output paths, then pass that file through
`JUDAS_TEST_SCRIPT`. It runs the 36-second V watch tour, pause/resume, oblique
transition/reload and screenshots. The harness is test automation, not a hidden
damage mode. Omit it and dummy-audio settings for interactive play.

The exact moved package can also be passed to the ordinary application fixture:

```sh
(cd /tmp && env SDL_AUDIODRIVER=dummy XDG_DATA_HOME="$m83_out/package-user" \
  "$m83_root/build/judas_mechanical_damage_application_tests" \
  "$m83_moved/game.judasproj" write "$m83_out/moved-write")
(cd /tmp && env SDL_AUDIODRIVER=dummy XDG_DATA_HOME="$m83_out/package-user" \
  "$m83_root/build/judas_mechanical_damage_application_tests" \
  "$m83_moved/game.judasproj" read "$m83_out/moved-read")
(cd /tmp && env SDL_AUDIODRIVER=dummy XDG_DATA_HOME="$m83_out/package-user" \
  "$m83_root/build/judas_mechanical_damage_application_tests" \
  "$m83_moved/game.judasproj" lifecycle "$m83_out/moved-lifecycle")
```

Controls: Tab assembly; W/S source input; A/D steering; Space brake; B equal
sustained load; I real ram; H overload; V watch; P prefab; F3 annex; 1/2 scenes;
F5/F8 save/load; R reload; Escape pause. After F3 publishes the annex, Tab selects
it for ordinary controls. The annex is authored flat; the oblique scene separately
rotates its five initial assemblies, environment and actual gravity field.

## Types, runtime surface and fingerprints

TypeScript 5.9.3 is temporary developer tooling, not a Judas runtime dependency.
Use an available local module path; this host used
`/tmp/judas-m80-typecheck/node_modules/typescript`. The powertrain-script test
publishes `surface.json` for actual VM export/member enumeration:

```sh
build/judas_powertrain_script_tests "$m83_out/powertrain-js"
node scripts/check_judasjs_api.mjs \
  --typescript /tmp/judas-m80-typecheck/node_modules/typescript \
  --runtime "$m83_out/powertrain-js/surface.json"
sha256sum -c docs/evidence/m83/FINAL_SOURCE_SHA256.txt
sha256sum -c docs/evidence/m83/FINAL_BINARIES_SHA256.txt
```

The type checker also checks the declaration file and 42 cookbook scripts;
missing/phantom/accessor negative controls test the checker itself. Structured
value semantics require the actual engine/application tests, not enumeration.
`VALIDATED_INPUTS_SHA256.txt` and `linux/PACKAGE_FINGERPRINTS.json` preserve input/
packaging identity separately. Binary paths identify the locally tested outputs;
rebuilt binaries on another host need not have identical hashes. The integrating
agent supplies refreshed final-source/binary verification and checkpoint identity.

These commands describe focused Linux evidence. They do not constitute a native
Windows run or human validation; both remain separately recorded statuses.
