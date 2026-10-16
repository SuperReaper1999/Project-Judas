# Provenance and reproduction

Base: `7f7b617907a81d65bcbdd931f9989f01c4c72dd9`, accepted M89. Development
branch: Experimental. See `linux/environment.json` for UTC/toolchain/hardware.
Release, native Linux OpenGL/NVIDIA; no software GPU or Windows claim. Build
parallelism bounded at four; physics/graphics workloads run serially.

## Commands

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
python3 scripts/validate_m90.py --build --graphics --output .cache/m90/repro
# The driver intentionally reports the two preserved stale-bound failures below.
build/judas_save_storage_tests .cache/m90/m61-storage-repro
# Requires a FRESH owned directory:
build/judas_save_exclusion_tests .cache/m90/exclusion-repro .
node scripts/check_judasjs_api.mjs --typescript /path/to/typescript --runtime .cache/m89/script-probe/surface.json
JUDAS_EDITOR_AUTOTEST=/absolute/owned/editor-output build/judas_editor projects/travelling_lab/travelling_lab.judasproj
build/judas_export projects/travelling_lab/travelling_lab.judasproj /absolute/owned/package build/judas
# Move the package, change to an unrelated directory, launch its own executable.
```

The API checker used cached TypeScript 5.9.3, with tarball integrity verified;
no permanent dependency or download was introduced. Live surface enumeration
comes from `judas_local_multiplayer_script_tests` (the exact published surface
example), not an old copied enumeration. Callback behaviour is additionally
executed by `judas_origin_script_tests`.

`linux/initial-focused-commands.json` is the exact original serial command log.
Its navigation-dependency and save-exclusion invocations omitted required CLI
fixture arguments (exit 2). Corrected receipts are separate and pass. The
navigation dependency fixture was an owned copy of the existing M78 prepared
landscape-fork fixture, never modified in place. That authoring suite is optional
in the driver through `--navigation-project`; the ordinary navigation runtime
suite and new actual translated Detour path check require no historical cache.

## Preserved failures and controls

- The baseline library/executable was copied before rebuilding the candidate.
  The same OriginAssembly test object linked against that library rejects real
  vehicle/navigation shifts with the old explicit unsupported-component guard.
  `failures/baseline-assembly-rejection.log` is an actual baseline run. There is
  no invented before-speedup figure: the previous engine could not perform this
  workload. Reproduce with an isolated clean baseline build and the new fixture.
- M77 foundation and M82 powertrain lifecycle suites retain old envelope limits
  (17/19 respectively); current SaveStorage supports 21. Both original binaries
  relinked against the copied baseline library fail the same one assertion.
  These are preserved, unmodified fixtures, not M90 physics failures. The current
  save storage suite proves 19/20/21 positive and 22 negative boundaries.
- The initial watch tour used open-loop steering and drifted into a real shelter
  post near 1 km. Screenshot/logs preserve the failure. Ordinary JS course
  feedback fixes the tour; the engine does not move or free the car specially.
  A temporary too-early progress guard also stopped a healthy follow-up; its log
  is retained and the final test uses the unchanged 2,080 m endpoint.
- Initial stream-test mistakes omitted ResourceManager pumping, then omitted the
  separate streaming participant during a direct WorldPersistence cold fixture.
  Logs are labelled test construction failures. The corrected fixture serializes
  both normal participants and checks actual restored nodes/sections/commands.
- Audio test construction initially referenced nonexistent diagnostic fields and
  a nonexistent header; build logs are preserved. The passing test checks the
  actual production voice Doppler ratio, cursor, backend positions and teardown.

No fluids, solver budgets, controller settings or physical acceptance tolerances
were retuned. No full production-suite run or human acceptance is claimed.
