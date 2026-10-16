# Development findings retained

- `BASE_INDEX_FAILURE.log` and `BASE_INDEX_DIAGNOSIS.txt`: the triangle codec
  cyclically rotated 63 of 16,384 base triangles. Geometry was equivalent but the
  exact index/barycentric ordering contract was not. Switched to the lossless,
  order-preserving index-sequence codec; base assertions were retained.
- `SECONDARY_RESIDENCY_FAILURE.log`: a cached secondary camera lost ordinary
  static scenery between updates. The prepass iterated managed rigid entities,
  which excludes static renderables. Corrected it to use the renderer's actual
  static/dynamic presentation owners, retaining independent view/mask checks.
- `TEXTURE_INITIALIZER_BUILD_FAILURE.log`: the first TextureData extension put
  new fields before the legacy aggregate fields. Existing material tests no
  longer compiled. Preserved the old field order and appended the new fields;
  did not modify the old material fixture initializers.
- `fixture-followup/`: the added imported-material test accidentally expected
  opaque BC1 after explicitly giving that fixture alpha=40 (correct output BC3).
  The first application observer looked for `.save` bytes under the project
  instead of SaveService's platform user-data root and its directory iterator
  aborted. Corrected those observers. These were test-tool failures, not evidence
  that runtime save/load or alpha compression was broken.

The older async residency assertion assumed a never-reused numeric GPU slot.
M93 deliberately reuses slots with generations. Its revised assertion compares
full identity (slot or generation changes), and the stale-completion assertion
now checks that both stay unchanged. New GPU churn tests independently reject
stale reads/destroys. This is a contract adaptation, not an observed physics bug.

Early incremental build scripts also used a misspelled benchmark target and ran
before explicitly reconfiguring the new lab target. Final validation uses the
registered target names; these build-dispatch errors do not establish runtime
behavior. Final-source receipts supersede development binaries.

## Final integration/quality follow-up

`lod-publication/REGRESSION_RED.log` establishes the real resource path's defect:
49,152 uploaded vertices instead of 8,385 prepared vertices. Direct upload and
CPU LOD bounds/area were correct; both instanced and non-instanced diagnostics
were exercised. ResourceManager regenerated tangents on material-less cooked
meshes, expanding/reindexing only the base stream and orphaning LOD indices.
It now respects prepared LOD topology, and GenerateMeshTangents rejects partial
reindexing of final LOD geometry before mutation. The unchanged red assertion
passes, comparing actual published GPU positions/normals/UV/tangents/base/tails.
Earlier invalid publication measurements are preserved under `lod-publication/`
and are not acceptance performance data.

After that correction, the first 1-pixel/weak-normal quality defaults still made
smooth shading visibly coarse (foreground RMS 23.76, threshold-mask difference
12.49%). The rejected default measurement is retained under `coarse-default/`.
Final cooking strengthens normal attributes and defaults to 0.5 pixel tolerance;
final foreground RMS is 5.06, mask difference 1.45%. Final timings deliberately use
the better-quality result rather than the earlier faster setting.

`BASELINE_SHUTDOWN_FAILURE.log` reproduces the old shutdown observer failure on
unmodified M92 source/engine. Application already exits immediately on window
close, so the test's after-frame setup never ran. Setup now runs at the actual
before-shutdown boundary; all real queued/read/CPU-ready cancellation and owner
cleanup assertions remain. The 12-case/246-check suite passes. The first storage
runner dispatch also used an invalid output basename; the storage executable
correctly rejected it before work. The corrected runner uses m61-storage*.
