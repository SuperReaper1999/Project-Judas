# Terrain V2 — engineering complete, human review pending

Final engine/SDK source: `23dbefdec8aa5f5ac5a466af85032186357db700`.
The following documentation checkpoint records qualification without changing
compiled inputs. Development remains Experimental. Main and supported packages,
the Advanced Fluids lock, existing projects and the RPG SDK selection are unchanged.

The consolidated results, limits and ordered human checklist are in
`docs/TERRAIN_V2_QUALIFICATION.md`; cache ownership and failure policy are in
`docs/TERRAIN_V2_RESIDENCY.md`. Machine-readable receipts, compressed measurements,
failures and selected views are in `docs/evidence/terrain-v2/residency-final/`.

## Implemented checkpoints

- `376b0a8a`: independent source, exact collision and GPU paging; durable edits,
  checked prepared cave storage, physical leases and source-local recipes.
- `970c8296`: local publication preserves unaffected GPU resources instead of
  rebuilding every visible page for each brush.
- `a9a98c96`: private source readers close before their temporary directory is
  removed; borrowed-source file lifetime is regression-tested.
- `23dbefde`: explicit graphics-policy forwarding in qualification, plus an owned
  render target for the strict colour/shadow transition oracle. No engine shader
  change or relaxed pixel expectation is claimed by that test correction.

## Completed automated qualification

Clean CI run42507574497 completed successfully on both configurations. Experimental
passes199registered cases. Alpha passes187, with12predeclared locked-feature cases
not exercised and0failures; skipped cases are not counted as passes. Both SDKs
pass Ubuntu24.04/GLIBC2.39 inspection,17tool probes and844file verification.
Earlier superseded/cancelled runs remain distinguishable from this successful run.

Local31-gate groups pass on actual NVIDIA OpenGL4.6modern and3.3portable contexts,
verified in logs, together with actual editor, cold process and sustained support
controls. Source paging3556, collision400803, render177, height/shadow978,
transition98839 and sustained support9293checks pass. Wrapper controls19pass.
The final documentation-only commit is subject to its ordinary current-docs CI.

The matching local SDK is `.cache/terrain-v2/sdk-native-preview-23dbefde`.
It passes17tool probes,844file verification,8standard consumer operations and
three6-operation terrain export/cold-save runs (flat/particles, planet and actual
portable flat). All17executables match the already qualified candidate byte for
byte; five refreshed terrain/support documents match the checkout. This local
binary is a glibc2.43 host preview, not the separate Ubuntu CI SDK. No new Windows,
Android or browser certification is claimed. Dirty documentation/PNG provenance
and separately observed untracked research remain truthful; compiled inputs match
23dbefde. No unrelated research headers were added to the SDK.

## Measurements and preserved observations

The24576-frame/3-lap actual renderer traversal has frame median1.374ms, p952.465ms,
worst16.574ms; collision query p951.023ms,worst3.249ms. CPU terrain maximum is
77399240bytes, with exactly77291844bytes at every lap end. GPU pages peak at
6359920bytes. Excavation p95 is2.309–2.656ms versus78–80ms before local invalidation;
final geometry, draws and payload are identical. Startup1.317–1.348s is slower
than the retained whole-resident~1.035s baseline; this is documented, not hidden.
Five edited region visits load asynchronously in1.316–1.536s, largest observed
integration unit3.88ms. These are scoped measurements, not FPS promises.

Original hidden-window framebuffer observations and incorrectly inherited portable
group labels are archived explicitly. The precise original native failure trigger
is unconfirmed; the final owned-target oracle and actual profile checks pass.
No failed attempt or physical fixture was weakened or silently relabelled.

## Human launch

`/home/conner/Documents/JudasTerrainV2Acceptance-23dbefde/launch-editor.sh flat`
opens the final packaged editor and a fresh material-enabled editable review copy.
Use `planet` for the planetary copy or append `--play` for direct gameplay. The
launcher isolates editor preferences and per-project user data and removes inherited
harness/save overrides. Original projects and saved games remain untouched.

All large disposable builds, full projects, screenshots and original logs remain
locally under `.cache/terrain-v2/` and the dedicated acceptance folder. No purge
or archive move was performed. Human terrain acceptance is the remaining boundary;
do not promote main or publish on the operator's behalf.
