# Reproducing the CCD evidence

Run from a separate checkout of
`f647d7fd95f60d3a37dba70a51f4e45dc41c2922` for the frozen behavior. Do not discard
another developer's local changes. No third-party game assets are needed: the
fixture cooks its own exact trough.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUDAS_ENABLE_VALIDATION=ON -DJUDAS_BUILD_CHANNEL=Experimental
cmake --build build --target judas_impact_mesh_cost_tests -j2
build/judas_impact_mesh_cost_tests 1 trough
build/judas_impact_mesh_cost_tests 1 trough 1.2 entry
build/judas_impact_mesh_cost_tests 60 trough
```

The first and third commands must report **four failed assertions** on the frozen
and this Pass 3 candidate. The second is the separately passing initial-arrival
contract. Passing arrival does not mean full continuation passes.

`fixtures/ImpactMeshCostTests.cpp` preserves the exact source used. Verify its
hash against the tracked test before treating a later repository as equivalent.
`BASELINE_FROZEN.json` gives the original executable/archive identities; binaries
are deliberately not part of this evidence directory.

## Diagnostic probes

`probes/BaselineObserver.cpp` and `probes/ContinuationObserver.cpp` preserve the
actual compiled observation sources. Build them against the engine archive from
the checkout under investigation, with the same dependencies as
`build/CMakeFiles/judas_impact_mesh_cost_tests.dir/link.txt`, replacing the test
entrypoint objects with the probe source. The observer does not alter simulation
state. The expanded observer evaluates an already resolved authoritative path
and observes real native narrowphase at 25 positions on its last segment.

`probes/MinimalContinuationRepro.cpp` includes the preserved fixture helper;
add this evidence directory's `fixtures` to the compiler include path. Its two
worlds compare the original 104-triangle fixture with a 16-triangle strip made
from exactly the same authored source rows. It retains the body, launch, gravity,
fixed step and strict 5 cm gate. A reproduced crossing returns a failure exit
code; this is deliberately not another passing test.

The original build/run commands and absolute paths are retained in JSON/raw logs
as execution receipts. Adapt output paths to a fresh owned directory rather than
rerunning an old evidence script over historical outputs.

`probes/verify_final.py` is likewise the original cache-dependent comparison
runner, not a clean-checkout test entrypoint. It expects a separately rebuilt
frozen baseline executable under `.cache/prealpha-pass3/baseline/bin`, a current
built executable, and a fresh `.cache/prealpha-pass3/ccd/final` output directory.
Frozen binaries are omitted from this evidence package. Adapt these paths to
fresh owned baseline/candidate builds before reproducing the comparison.

## Rejected experiments

The two `experiments/` directories contain exact rejected sources and raw results,
not production implementations. `probes/run_variant.py`,
`probes/prepare_finite_current.py` and `probes/run_finite_current.py` are original
execution records referring to this pass's scratch directories and frozen engine
archive. Use a fresh owned output directory if adapting them. They must not be
copied into `src/` as a fix: both fail the original contract, and the second adds
substantial event work.

## Mechanical formatting proof

```sh
python3 docs/evidence/prealpha-pass3/ccd/tools/verify_format.py
python3 docs/evidence/prealpha-pass3/ccd/tools/verify_format.py --current src/PhysicsWorld.cpp
```

The first proves frozen before/after token identity and two deliberately altered
negative controls. `--current` additionally verifies this particular candidate's
source equals the recorded formatted output. Future legitimate functional edits
need not match that historical source; this is an evidence verifier, not a
permanent API constraint or replacement for native trajectory comparisons.

`formatting/format-only.patch` isolates mechanical whitespace from the origin
admission and exact event-membership functional changes already present in both
recorded source copies. No new formatting tool is a build/runtime dependency.
