# LI-SAVE-001: signed-zero joint continuation mismatch

Observed 2026-10-26 on Linux, accepted engine baseline c2cdd376f1c20a9d6a8a75d8257f9ac44327ec09.
This is a generic persistence defect discovered by an actual consumer-game loop,
not a failure of inventory accounting. The operator subsequently authorized a narrow generic correction. Exact typed context comparison now handles signed zero; the additional durable mass issue is recorded separately in ../mass-failure.

The shipped Application runs the game, gathers/crafts real materials, moves an
authored crate (static → dynamic, rotated 90 degrees), constructs pieces, uses a
bed/lamp and barricades. The save completes; a genuinely fresh-process load rejects
with `joint continuation physical context differs`. Native transactional staging
keeps the live world intact. The 18 downstream read assertions fail because no
saved world was published; they are not 18 independent defects.

`island.save` preserves the exact failed input; `expected.json` records the independently
observed save boundary. `read.log` preserves the original rejection and downstream
checks. `saved-context.bin` and `restored-context.bin` are 618 bytes each, dumped by
an isolated diagnostic executable replacing only the WorldPersistence object.
The repository's engine source, static library and ordinary executables were not
modified by that probe. Its source-generation/link command is retained in
`diagnose_context.py` (requires a current observer object and Release build).

The contexts differ at byte offsets 163, 171, 179 and 187 only: high sign bytes of
four zero off-diagonal entries in body A's local inverse-inertia matrix. Saved
values have IEEE float32 -0; reconstructed values have +0. All nonzero fields and
other context bytes are identical. Diagnostic joint handle 4 has native body A
243 and body B 2 (handles are process-local, not persistent gameplay IDs).

Root: `src/physics/WorldPersistence.cpp`, `PersistJointContinuation`, compares
serialized context **bytes** for equality. Numerically equal signed zeros are
therefore rejected after valid reconstruction. This should not be relaxed with
an epsilon, removed context checks, cache deletion, modified tests or changed
physical state. A proposed generic correction is typed exact field comparison
when byte comparison differs, using the existing context field traversal. That
admits only numerically identical signed zero while continuing to reject any
changed nonzero physical state, geometry, topology or settings. Preserve binary
format and validate old input directly, plus independent changed-inertia,
velocity, geometry and setting negative controls.

Reproduction after building the observer:

1. Copy island.save into a fresh XDG data home's
   judas/games/9ac05e8350116f67dc3ba285539ad502e32f9ae780c0afc00264bb5ba6806e29/Saves/Slots/island.save.
2. Copy expected.json into an observer output directory.
3. SDL_AUDIODRIVER=dummy XDG_DATA_HOME=<that directory> /tmp/lunar-stage2-observer-final \
   projects/lunar_island/lunar_island.judasproj read <observer output>.

No save should be described as passing cold continuation until the original input
and a newly generated save pass physical endpoints and 120 unpaused hinge steps.
