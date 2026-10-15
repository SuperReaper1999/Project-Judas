# M85 development and failure record

The development directory preserves actual build/test stdout, including unsuccessful
attempts. These are development receipts, not the final-source validation result.
No historical milestone or fluid research file was edited.

## Mechanisms corrected during implementation

- Absolute float radius addition was replaced by binary64 source sampling and
  subtraction before float tile upload/cooking; old RadialTerrain remains intact.
- Child geomorphs start on the actual parent triangle. Canonical dyadic sampling
  removed independent floating expression disagreement at fine levels.
- Mixed-LOD corner cracks required interpolating the coarser neighbour's already
  resolved grid, rather than its uncorrected analytic points. Both sides now share
  the same coarse triangle including corner propagation and morph state.
- First implementation rebuilt/uploaded every visible tile and searched leaves
  linearly. Morton indexing and explicit seam-dependency revisions retain unchanged
  meshes. Development timing logs use evolving configurations and must not be
  advertised as a controlled before/after benchmark.
- Contact cooks initially computed unused render normals/parent grids. Collision
  now skips those calculations while preserving the exact source positions.
- SaveService's participant count and streaming archive version acceptance needed
  explicit registration for the new planet participant and origin-aware version 7.
- Authored reset must preserve the terrain owner; only world destruction retires it.
  Destroy callbacks execute before terrain teardown. Runtime tags propagate to the
  actual generated tile bodies, including future cooks and authored reset.

## Preserved fixture/build failures

- Shared physics-only CMake source membership accidentally pulled GL renderer code
  into M23 tests; new world/planet sources now belong only to judas_engine.
- Initial compilation exposed missing includes, const archive IDs, editor call
  signatures and incomplete forward types. Original compiler output is retained.
- Initial project omitted required lighting data and its relative JS module metadata;
  actual startup failed until the ordinary project assets were correctly registered.
- One streaming negative requested beyond the documented absolute origin bound;
  the fixture now uses 9e11 m. Another assumed a queued request was already in flight;
  the corrected negative uses an explicit asynchronous gate before requesting rebase.
- A collision-budget negative required exactly four tiles with a four-tile budget;
  increasing the requested physical neighbourhood made that negative meaningful.
- A rolling sphere save/load assertion incorrectly expected it to stay within one
  metre after half a second despite ~2.47 m/s physical velocity. The diagnostic
  records 1.3066 m displacement and 0.3482 m surface distance for its 0.35 m radius.
  The corrected test compares actual restored velocity/time against motion and
  verifies continued physical contact. No physics friction/damping was altered.

Final results are in final/results.json and its per-test logs. No failed numerical
acceptance check is hidden by overwriting a historical result.
