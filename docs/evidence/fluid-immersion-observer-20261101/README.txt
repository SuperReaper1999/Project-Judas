Experimental immersion-observer audit, 2026-11-01
Read ../../FLUID_IMMERSION_OBSERVER_AUDIT.md.
This checkpoint does NOT repair the remaining production immersion failure.
The original 0.6341740926 observation still fails its unchanged +/-0.10 gate.

native-capture.tar.gz contains exact native before/after body and particle
snapshots, scene, normal CSV and observed fractions for all 300 measured frames.
SNAPSHOT_FILES.json hashes each uncompressed file. Archive member bytes were
verified against that manifest before checkpointing. Extract only into a new,
disposable directory; it contains ordinary filenames, no system paths.

source/ contains final test/capture and standalone replay sources.
run/ contains raw runs, immutable-physics replay comparison and disposable
prototype sources. Text is gzip with mtime=0. PROVENANCE.json fingerprints
source and the actual native binary; binaries/objects/build trees are excluded.
No independent research was copied or modified.

The harmonic prototype is REJECTED: it fails a known curved-surface control
and the actual late window. Its results are not production acceptance.
The frozen 84-configuration plane sweep is measured evidence, not a newly
predeclared release gate. The convergence probe's initializer warnings are
exploratory; the final native target build was warning-free.
The initial build-organization discovery used the wrong filename and ran zero
tests; the subsequent correct module passed 18. Both logs are preserved.

Replay from the repo with standard Python (no third-party packages):
  python3 tools/validation/audit_fluid_immersion.py EXTRACTED_DIRECTORY \
    --case half_025 --spacing .25
  python3 -m unittest discover -s tests -p test_fluid_immersion_observer.py -v
Exit 0 from the replay means diagnostic completion, not qualification. The
original window's original_tolerance_pass remains false.

Native reproduction (configured Release/Experimental build):
  cmake --build BUILD --target judas_production_fluid_tests -j2
  BUILD/judas_production_fluid_tests --case half_025 --capture-immersion \
    --output NEW_DIRECTORY
Expect exit 1: 2176 checks, the one original failure. Capture adds disk I/O;
never use captured-run timings as isolated performance measurements.

run/capture.py and run/build_probe.py record exploratory commands against
this checkout's existing configured .cache/2d-stage1-build. They are NOT a
portable SDK build recipe; adapt paths only in a disposable reproduction.
No main promotion, fluid unlock, tags, publication or human acceptance.
