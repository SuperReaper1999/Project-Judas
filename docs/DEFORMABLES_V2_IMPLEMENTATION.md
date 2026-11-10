# Deformables V2 — Stages 2–4 implementation record

The operator human-accepted revised Stage 1 at
`6a1d49b2add0e45bab6933f544797a8faa6e3d21`: imported Challenger wheels,
local bonnet bending, progressive mounting-sheet failure and physical detachment.
Stages 2–4 are now authorised on Experimental. This record is **work in progress**,
not full-milestone qualification or human acceptance. Main, the fluid lock and the
operator's RPG SDK remain protected. Third-party assets stay private.

## Contact and sleep correction

- `deformables/MaterialContacts.cpp` owns soft/rigid contact sampling and response.
  Capsules use the existing authoritative segment evaluator; direct radial terrain
  uses the existing analytic surface. Cooked Terrain V2 contacts remain available.
- `deformables/RigidResponse.h` shares effective point mass and live rigid velocity
  with attachments. Reactions retain a binary64 local lever arm before crossing
  MK2's binary32 impulse API, including at large simulation coordinates.
- `deformables/SleepContacts.*` owns instance-local collision publication signatures.
  Candidate identity, immutable geometry, pose and applicable filtering detect new,
  moved, removed and replaced nearby geometry. No world-wide edit epoch wakes
  unrelated materials. Terrain geometry pages are not acquired just to check sleep.
- Dynamic attachments now test actual pose/velocity motion, rather than treating
  every dynamic body as permanently moving. Genuine target impulses still wake.
  The accepted released-target wake correction is preserved.

The current soft test executable passes **86 checks**, including 18 new controls.
Vehicle dynamics (97) and structural/crash controls (174) also pass with the accepted
rigid cadence restored. The build-registration controls pass 27 tests.
The new tests cover static/rotated capsule contact, analytic radial contact, local
static creation/movement, same-body cooked collision replacement, quiet dynamic
attachments, wake impulses and an unrelated-geometry negative control.

An attempted dynamic/kinematic capsule fixture exposed an existing MK2 limitation:
its general capsule mass/motion factory is unsupported. Static capsules are tested;
query-only character proxies are not silently advertised as physical rigid actors.
Dynamic reactions remain exercised using supported rigid shapes. No capsule mass
implementation or unsupported interaction is invented by these tests.

## Rejected scheduling shortcut — retained evidence

Removing the structural-quality maximum's outer subdivisions while retaining MK2's
native four substeps improved ordinary timing, but did not qualify as a final fix.
On identical authored geometry/controls, the 30 m/s crash changed drivetrain failure
and worsened the worst measured interval from about 115 to 196 ms. Its trajectory
was not physically identical. The faster mild/four-car numbers are **not** claimed
as an accepted improvement or permission to reduce fidelity.

The accepted cadence is restored while native coupling/local scheduling is developed.
Raw private results and the tested source are retained under
`/tmp/deformables-v2-stage2/` during development and must be included in the durable
final qualification evidence before cleanup. The original accepted private binary
and assets remain unchanged.

## Remaining authorised work

Reusable multi-panel settings/cooking and detailed bindings; native attachment part
references and physical tearing; normal editor presets/visual authoring; scripting,
prefabs and persistence; correct soft/rigid scheduling without unrelated whole-world
subdivision; sustained terrain/streaming/origin/platform/SDK qualification and actual
performance/visual checks. These are not yet reported complete.
