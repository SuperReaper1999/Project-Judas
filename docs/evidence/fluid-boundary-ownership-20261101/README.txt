Owned water-boundary evidence — 2026-11-01

Read ../../FLUID_BOUNDARY_OWNERSHIP.md first. Base source is
7a377c8cad65cfce26dce4d4fc1d6313d3c1550e. The containing checkpoint
identifies the implementation. Main and the Alpha fluid lock are unchanged.

FINGERPRINTS.json identifies the final tested source/binaries, including engine
archive and application. The evidence commit changes embedded dirty/provenance
identity; binary hashes refer to the tested pre-checkpoint builds, not a claim
that those binaries were rebuilt from the later evidence commit.

ARCHIVE_MANIFEST.json lists every archived file with bytes and SHA-256. Each was
read back from the archive and compared to its original. No executable/build
artifacts or independent research are included. Raw .bin files are physical
state dumps, not executables. RAW_OUTPUT_VALIDATION.json rejects empty-output
parity and records the native original-scene particle/executed-step proof.

Final evidence:
 SHIP_RESULTS.json, ship-*.log, ship-production/player/demos/containers/*.csv
 OWNED_RESULTS.json, KnownOwned.log, owned-known.csv, AdditionalOwned.log
 native-capture.csv, NATIVE_FRAMES.json, MAPPED_FRAMES.json
 PHYSICAL_FRAME_PARITY.json, physical-before/after-data/physical.bin
 COST.json, paired-*.log/.bin/.bin.csv, INDEPENDENT.json
 verified-original-flat/radial.log, original-*.log, APPS.json
 python-final.log, build-ship.log, ENVIRONMENT.txt

Preserved failures/experiments:
 rejected-advected-domain/: adds 20 acceptance failures; do not ship
 cap-only-native/: fails coarse native dome sphere accuracy
 centre-only-first/: misses contact at preferred clearance
 final-* logs/results: REJECTED advected-domain run, NOT final shipping evidence
 cell-* logs: cap-only intermediate run
 ship-legacy-known.csv: unchanged old metadata-free fixture (36 engine failures)
 KnownOwned.log still exits 1: 80 original observer failures remain
 old-coupling-lifecycle.log: negative control catches stale wet field after Clear
 native-frames.csv: 5 separately initialized frame comparisons fail
 GATES.txt and STATE.txt retain chronology, including superseded interim notes

Reproduction (Linux source checkout of this containing commit):
  Verify SHA256SUMS.txt. Extract evidence.tar.gz into a DISPOSABLE directory.
  To run supplied drivers as recorded, put its contents under
  .cache/fluid-boundary-ownership-20261101 in a disposable checkout.
  Configure Unix Makefiles Release at .cache/2d-stage1-build using the current
  docs/BUILD_RELEASES.md prerequisites. Build judas, judas_production_fluid_tests and the
  targets listed in SHIP_RESULTS.json with cmake --build ... -j2 --target ... .
  This is an Experimental build; never change the Alpha lock for these checks.
  python3 .cache/fluid-boundary-ownership-20261101/ship_checks.py
  python3 .cache/fluid-boundary-ownership-20261101/native_probe.py
  python3 .cache/fluid-boundary-ownership-20261101/native_frames.py
  python3 .cache/fluid-boundary-ownership-20261101/mapped_frames.py
  python3 .cache/fluid-boundary-ownership-20261101/independent.py
  python3 .cache/fluid-boundary-ownership-20261101/post.py
  python3 .cache/fluid-boundary-ownership-20261101/original_scenes.py
Run sequentially, without concurrent builds/tests for matched timings. post.py
also uses a current desktop DISPLAY/XDG_RUNTIME_DIR for hidden native graphics;
original_scenes.py is a no-render native physical probe with explicit counters.
These scripts overwrite their output files: reproduce in a disposable copy.
The drivers use Makefiles' generated flags.make/link.txt, not Ninja/MSVC files.
Original flags/link inputs are recorded-*.txt; regenerate paths on a new machine.
known_owned.py has an archived AdditionalShapes fixture fallback. independent.py
has archived baseline-source fallback if the old Git commit is unavailable.

Do not link stale objects compiled against old public class layouts. All before
objects in the controlled physical comparison are rebuilt from archived old .cpp
files against the SAME final headers before linking. Rejected snapshots require
an isolated, internally consistent full source/header build if rerun; they are
not drop-in object replacements. Archived failed source is evidence, not adoption.

The original production test and old known-geometry executable intentionally
remain failed. SHIP_RESULTS records their exit 1 rather than suppressing it.
No Windows/Android qualification or human acceptance is asserted here.
