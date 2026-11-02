Fluid clearance / contact-episode repair — 2 November 2026
Report: ../../FLUID_CLEARANCE_REPAIR_AND_GRID_PROTOTYPE.txt
Parent investigation35db07d0; isolated grid checkpointd0f1f359. The containing
commit supplies the final selected source. Main/releases/Alpha lock unchanged.
No independent research, existing project/scene or accepted test was modified.

raw-evidence.tar.xz contains disposable experiments, exact sources, unchanged
fixtures, successful/failed numerical results, commands and fingerprints.
Every member was extracted in memory and SHA256 verified against
MEMBERS_SHA256.json. ELF binaries, object files and archives/build libraries are
excluded; their actual hashes remain. No unrelated project/assets are included.

Authoritative final evidence under .cache/fluid-clearance-repair-20261102/:
- final-source/: exact implementation/header/test/registration snapshots and
  original project/scene inputs. FINAL_SOURCE_FINGERPRINTS.json ties them to
  final3. Earlier CANDIDATE_SOURCE_FINGERPRINTS is a withdrawn intermediate.
- final3/RESULTS.json:13 sequential final-source suites exit0. Detailed logs
  distinguish acceptance from retained failed diagnostics; no quiet relabelling.
- coherent-position-controls.log:513 focused assertions,0 failures, including
  explicit KNOWN_LIMIT residual/cap diagnostics, not physical passes for that
  stress case. final3/clearance.log is the authoritative combined run.
- coherent_pool.log: ORIGINAL unmodified recipe/acceptance expression passes,
  including0 escapes,1440 particles,180000 kg, swimming and finite state.
- final-coherent-passive/: two ordinary digest repeats and a passive phase run
  have identical trajectory/endpoint digests. Published peak9.6519 m/s; actual
  intermediate10.6298 comes from represented PBF displacement, not the false
  near-opposed contact. Source/binary/log hashes and exact commands are retained.
- APPLICATION_RESULT.json / relocated-application.log / relocated-frame.png /
  RELOCATED_PACKAGE_SHA256.json: real relocated standalone, scene switching,
  pause/resume/camera/frame and lightweight GPU logging, exit0. This is local
  Experimental validation, not clean-machine distribution qualification.
- *-build-info.json: actual parent/dirty/untracked-source build identities.
  These binaries are not relabelled with a future commit or release identity.
- python-checks.json / test_*.log:15 validation-helper and22 CMake tests pass.

Preserved failed / superseded experiments:
- position-fit-only/: incomplete velocity policy, later false closing-wedge jet.
- competing-all/ + final/: blanket opposing-normal policy;13801-check production
  run has1 real acceptance failure and6 diagnostic failures. Other runs retained.
- available-certificate/ and compatible*.log: intermediate compatibility policy.
- pre-position-replay/ + final2/ + final-passive/: all13 suite exits0 yet the
  actual ORIGINAL pool probe reproducibly fails2 escapes. Both ordinary repeats
  and passive traces agree, proving that a suite pass alone missed the defect.
  HISTORY_HARD shows a true initial floor hit combined with a synthetic body
  episode caused by optional floor lift; final floor gap0.1474 m, contact kick
 21.879 m/s. Escaping IDs651/678 are separately traced; no false common cause is
  asserted merely because both problems occur in the same pool.
- contact-tests/: prior259/291/375/... control versions, exact source/logs,
  failed fallback response, narrow-wedge physical residual, corrected NEW oracle
  premises and the22.0728 m/s/34525.6 N s false-body known-answer episode.
  Inputs and tolerances for Squeeze are unchanged; its zero-velocity expectation
  is independently disproved by actual gap/h. No accepted engine test was weakened.
- .cache/fluid-transient-20261102/original-mechanism-control/: original git35db07d0
  sources compiled against CURRENT structural layout with two override objects
  before the current archive:425 checks/78 failures. Explicitly not an old release
  binary or a mixed old-binary/new-header ABI. Commands and all hashes included.
- initial setup logs retain the missing newly configured target and mistaken
  judas_export_game target spelling. Correct registered tool is judas_export;
  those setup failures are not counted as numerical tests or production defects.

Reproduction (from repository root, fresh output directories):
  cmake -S . -B BUILD -DCMAKE_BUILD_TYPE=Release \
    -DJUDAS_BUILD_CHANNEL=Experimental -DJUDAS_ENABLE_VALIDATION=ON
  cmake --build BUILD --target judas_fluid_contact_clearance_tests \
    judas_fluid_boundary_contact_tests judas_particle_contact_batch_tests \
    judas_fluid_cadence_motion_tests judas_production_fluid_tests \
    judas_fluid_container_integration_tests judas_player_fluid_integration_tests \
    judas_fluid_demo_integration_tests -j2
  BUILD/judas_fluid_contact_clearance_tests
  BUILD/judas_production_fluid_tests --output FRESH_PRODUCTION_DIRECTORY
  BUILD/judas_fluid_container_integration_tests --output FRESH_CONTAINER_DIRECTORY
  BUILD/judas_player_fluid_integration_tests --output FRESH_PLAYER_DIRECTORY
  BUILD/judas_fluid_demo_integration_tests --output FRESH_DEMO_DIRECTORY
Recorded build: .cache/2d-stage1-build, GNU15.2 Release -O3. The archived
run_final_checks.py records all13 exact commands and this host's desktop settings.
For another host use its own display/session environment, not a copied Xauthority
path. BasicWaterPreviewTests requires a real GL context; other listed checks are
CPU-side. final-flags.make/final-link.txt record the native probe link environment.
Passive harnesses have explicit build commands and exact source/header hashes.

The standalone package at /tmp/Judas Fluid Clearance Verification 20261102 is a
temporary qualification artifact; the archived check_relocated.py reproduces it.
Live active-fluid suspension/origin/save admission limitations are preserved.
No universal active-particle cold-save, continuous rotating-feature CCD, bounded
fluid energy/speed, GPU crash resolution or other-platform certification is claimed.

The separate grid prototype/evidence is already checkpointed independently:
  docs/evidence/fluid-volume-grid-20261102/README.txt
Its measured costs are not a same-geometry PBF comparison or a backend replacement.
