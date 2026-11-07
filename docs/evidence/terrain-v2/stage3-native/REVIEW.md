# Native terrain checkpoint qualification

The native source/scene/persistence path is integrated, not a parallel terrain
engine. A native scene no longer imports sample arrays into JavaScript or needs a
bootstrap script. Native files expose independently validated pages; runtime
currently loads all arrays and keeps all collision chunks resident. That final
limitation is explicit rather than called streaming.

The final five-run warm-file benchmark has scene-build median/p95/worst
502.038/561.287/561.287 ms and complete local-edit 26.9926/30.2151/30.2151 ms.
Publication median/p95/worst is0.182795/0.285408/0.285408 ms. At five samples,
nearest-rank p95 is worst. Source payload5271331bytes; edit snapshot594bytes.
Final process peakRSS150212KiB; estimated source/geometry28396588bytes excludes
BVH, allocator, GPU and process overhead. Initial earlier runs are also retained;
no speedup claim is made from the small timing changes.

Geometry416, world93, LOD339, nativeasset32, component35, coldread3,
GPU978checks per modern/portable profile, nativeeditor15, PlanetWorld220,
PlanetTerrain10074 and originalLivingWorld58 pass. Component coverage includes
no-script startup, actual excavation, asset-reference restore, changed-base
rejection, regional private placement/publication/remove, physical-material
propagation and restore-budget rejection preserving live support.

The ordinary game/export/cold-save driver runs a real generated package after
relocation and from a foreign cwd, moves the real motor through the tunnel,
saves/quits, starts a separate process and validates its public restore report.
Native receipt hashes are checked against actual products. Screenshots were
inspected: the tunnel is open through the hillside, not a portal or visual prop.

A separate1km native authoring fixture cooked an adaptive editor preview512tris
instead of trying to cook one524288triangle collider. Its real runtime support
still contains524288triangles across1024chunks. Native export contains16assets,
including source and material dependencies; relocated desktop play succeeds.
This is a workflow/control fixture, not final landscape art acceptance.

The sparse-range failed experiment is preserved. Its exact exhaustive oracle was
retained and the candidate range corrected for float-rounded tangent boundaries.
No failure expectation was weakened. The larger runtime profiler capture yielded
no resolved GPUtimestamps; no GPU timing claim comes from it. Stage2 native GPU
measurements/controls remain separate. Automated checks are not human acceptance.

Next work remains the approved TerrainV2 programme: physical residency/budgets,
source creation/import/multisource authoring, planetary excavation, runtime raster
editing and cross-system geometry publication, visual/consumer/SDK qualification.
Main/releases/fluidlock and unrelated projects/research remain unchanged.
