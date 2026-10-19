ISOLATED IMPACT LOOKUP / LAZY ROTATION EXPERIMENT

Base: safe Experimental checkpoint 8f6bd18b876158c131bee61fa0dc71625d912c30,
using the frozen candidate-fastpaths-final archive and immutable source bytes.
No production source, public API, object layout or physics settings are changed.

ImpactSolver.cpp changes only:
- A Solve-local pointer membership lookup replaces repeated linear m_saved scans.
- That same per-event lookup lazily retains ContactRotation for local-anchor
  closing-speed classification while body orientations remain immutable.
- m_saved encounter order still determines all budgets, restore order and result
  sums. Contact order, response arithmetic, energy guards, retry and ten velocity
  iterations are unchanged. The lookup is never traversed to perform physics.
- No unused body orientation is eagerly validated. Nothing survives Solve.

Build (only when host timing slot is free):
  python3 .cache/dense-scalability-20261019/impact-memo-prototype/build.py
Run bounded controls (only when timing slot is free):
  python3 .cache/dense-scalability-20261019/impact-memo-prototype/run.py

The exact captured event3470 input has 969 bodies and 2379 contact rows.
MemoReplay runs the unchanged actual solver and compares every saved body field,
prepared/solved contact field and ImpactResult bit against the original capture,
then repeats the immutable input 32 times. JSON comparison and restoration are
outside measured Solve spans. These local repeated-event timings are not FPS or
whole-world acceptance evidence.

MemoControls compares complete small-case state/diagnostic output between the
same source linked against original and candidate archives. It covers repeated
rows and first-encounter order, repeated calls with changed poses, actual hard
joint coupling and a joint-only participant, static skips, unused joint-only
orientations, and exact input-failure messages. An eager-unused-rotation mutant
must fail, proving that the lazy validation control detects that regression.
The original ImpactSolverTests.cpp and thresholds are also linked unchanged.

All commands, real exits, source/archive/binary hashes and exact parity outputs
are recorded in BUILD.json and RESULTS.json. Absence of RESULTS.json means this
experiment has not yet been executed; do not infer a pass from this README.
