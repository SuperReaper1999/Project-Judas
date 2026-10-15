# PR #15 — portal camera review and M85 integration

Linux review passed; human acceptance and Windows validation are not claimed.
PR: https://github.com/SuperReaper1999/Project-Judas/pull/15
Reviewed head: `80f6c0970050d8216a0987820a4f711047056e43`.
Integration parent: M85 `b9bd655485fe07182e68b9c78bd3d4987fba2985`.
The PR target was changed from main to Experimental to preserve release policy.

## Source review

The PR provides optional camera-space off-axis frusta and a world-space oblique
near plane for existing secondary/render-target cameras. Both options are runtime
presentation state. Missing keys retain existing options; null clears. Ordinary
near/far behaviour and symmetric projection remain intact. No portal gameplay,
teleportation, recursive rendering or new physics capability is introduced.

Math and rendered-pixel assertions verify real occluder removal/off-axis framing,
not just symbol presence. Existing render-camera resource/lifetime checks pass.
Native validation is completed before camera mutation; invalid nested frusta,
zero normals and nonfinite points reject atomically. Returned objects are detached.
The new public example and CameraInfo result-shape drift checks supplement the
PR's types/docs. No new top-level export or native dispatcher operation was needed.

## Integration correction

M85 world origin shifts now translate a configured clip-plane point alongside its
camera. Normals and camera-space frusta are unchanged. The test shifts both X and
Z by 512 m, verifies the same green beyond-plane image, and deliberately restores
the old plane as a negative control: the red near occluder reappears. This detects
an absent translation mechanism, rather than testing a direction perpendicular to
the clipping normal. No terrain source, physics or collision rule changed.

One documentation correction describes planetSample.distance accurately as signed
projection onto the sampled normal, rather than radial or exact closest distance.
No implementation change was made for that wording.

## Focused results

- Portal: final 21 checks, zero failures, twice consecutively. Includes original
  projection/pixel checks, actual origin shift and visible negative control,
  public JS set/read/keep/clear, no-camera false, detached snapshots and bad inputs.
- Existing render-camera checks: 52/0, including resource deletion and save/lifecycle.
- M85 world 220/0, origin physics 13/0, streaming 20/0, real application 37/0.
  Actual saved/restored physical endpoints and mixed-LOD grids are exercised.
- API/TypeScript 5.9.3: 373 symbols, 32 exports, 261 operations, 43 declaration
  examples, five result-shape rows and four negative drift controls passed.
- Release runtime/editor/export and focused targets built. Two baseline material
  indentation warnings remain. No full production-suite rerun was warranted.
- Final moved standalone: see export/moved-package/save logs and results.json;
  normal asset closure, bounded input/probe/travel/pause and save/load services.

The combined app run records CPU frame median/p95/max 10.635/27.651/297.376 ms,
fixed p95/max 1.636/11.648 ms, cold cover 270.367 ms, same 384/500/34 peak tile
counts and 1.31708901597e-9 m measured seam. This variability is not an improvement
claim relative to the pre-PR run; no synchronous cold hitch or LOD budget limitation
was removed by a portal feature. Linux is the measured reference; Windows is not
measured here.

## Preserved failure

My newly added temporary JS fixture initially passed but failed on reuse: its
asset database Scan already registered the prior run's .judasmeta, so Track
correctly rejected duplicate registration. The fixture then dereferenced a null
ScriptSystem after Build rejected its empty asset ID. The core backtrace identifies
ScriptSystem::Capture called from main. This was a test-harness bug, not a portal
runtime failure. The fixture now creates its own clean build/pr15-portal-script
workspace and checks Build before proceeding. Original failed output, exact fixture
bytes/hash and backtrace are retained. No production API was changed to hide it.

## Evidence identity

M85's original fingerprints/receipts remain immutable at its checkpoint. This
folder's fingerprints.json records the current combined source set and binaries
actually rebuilt for this integration, rather than relabelling old binaries as
final-source tests. Export closure hashes are in package-fingerprints.json.
Changed files are listed in FILES.txt. Unrelated untracked historical work remains
unmodified. main remains the accepted release; no tag or release promotion.

M85 human checklist remains in the lab README: terrain editing, orbit/ground and
face-edge traversal, actual probes, save/load, streamed station and moved package.
Portal users should additionally inspect their own two linked views and opening
orientation; the engine tests do not certify Claude's entire separate game.
