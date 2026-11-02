Fluid transient investigation and representation review — 2 November 2026
Report: ../../FLUID_TRANSIENT_AND_REPRESENTATION_REVIEW.txt
Investigated source: 05b19870fbe3c1dbed961dec9a5a5782206bb313, Experimental.
Investigation only: no production implementation, acceptance test or threshold
changed. Main, published releases, Basic Water, M54/M55 and the Alpha fluid lock
remain unchanged. Independent research directories were not accessed or edited.

raw-evidence.tar.xz preserves disposable instrumentation, exact captured contact
rows, sources, logs, causal controls and source/binary fingerprints. Each member
was extracted in memory and verified against MEMBERS_SHA256.json. Executables,
objects and libraries are omitted; their measured binary hashes are retained.
The containing checkpoint is an evidence checkpoint, not a new physics binary.

Selected evidence under .cache/fluid-transient-20261102/:
- FluidWorld-feature.cpp: passive phase/contact/finite-feature logging added to
  a disposable copy, linked ahead of the unchanged production engine archive.
- pool_probe.cpp: original rectangular-pool gameplay recipe. Production scene
  projects/fluid_comparison/Scenes/rectangular_pool.judas is used unchanged.
- feature.log: complete pool run, 1440 particles, mass 180000 kg retained exactly,
  no endpoint escapes; particle 217's contact-response spike is retained.
- EPISODE_SUMMARY.json: phase velocities/speeds from fluid call 236.
- contact-review/wedge.cpp and wedge.log: actual SolveParticleContacts with no
  PBF/gravity, prescribed wedge, absent floor, stationary and finite-mass controls.
- contact-review/captured-rows.json, gap_probe.cpp, gap_probe.log: all six actual
  captured states, zero-gap versus real-gap response, independent touching and
  crossing controls. FAILURES 0. Later inputs already contain earlier energy.
- contact-review/REVIEW.txt: preliminary independent review, followed by
  GAP_REVIEW.txt: final causal micro-prototype and explicit qualification limits.
- SOURCE_FINGERPRINTS.json and BINARY_FINGERPRINTS.json: actual investigated
  sources and native probe binaries. Embedded build metadata was not relabelled.
- reproduce.py: rebuild/run micro-probes from an existing matching Linux build;
  --full-pool additionally links the disposable instrumented fluid source.

Reproduction from the repository root at the investigated checkpoint:
  tar -xf docs/evidence/fluid-transient-review-20261102/raw-evidence.tar.xz
  cmake -S . -B .cache/fluid-transient-rebuild -DCMAKE_BUILD_TYPE=Release \
    -DJUDAS_BUILD_CHANNEL=Experimental -DJUDAS_ENABLE_VALIDATION=ON
  cmake --build .cache/fluid-transient-rebuild \
    --target judas_rendering_architecture_tests -j2
  python3 .cache/fluid-transient-20261102/reproduce.py \
    .cache/fluid-transient-rebuild --full-pool
The script consumes this target's ordinary Unix Makefiles flags/link libraries;
it does not run the renderer or need a GL context. The recorded build was
.cache/2d-stage1-build, GNU 15.2, Release -O3, on the Ryzen 5 5600 Linux host.
The default two-micro-probe reproduction was rebuilt and rerun successfully.
Full-pool instrumentation was run for feature.log; the script's optional wrapper
was not separately rerun after packaging. Floating-point platform differences
may move a chaotic long-run event; the exact captured-row micro remains available.

Limits: gap controls use frozen normals and constant wall velocities. They do
not validate rotating finite-box CCD, a production soft-clearance policy, a
replacement backend or a grid performance improvement. No such claim is made.
