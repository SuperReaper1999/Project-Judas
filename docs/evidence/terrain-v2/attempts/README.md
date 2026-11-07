# Sparse vertical-band join correction

The first sparse-band implementation used binary32 interpolation through two
paths: clipping the heightfield to a band and reconstructing tetrahedral edge
crossings. Equivalent intersections differed at the final bits. The native
collision cooker rejected the first runtime excavation with a T-junction;
edge subdivision alone then exposed a nonmanifold microscopic edge. Neither
failure was suppressed or counted as passing.

The retained source and output record the failed float construction. The final
construction performs density differences and edge interpolation in binary64,
uses the same global integer vertical sample coordinate on both sides, and
narrows only the completed vertex. Collinear edge subdivision makes both
surface representations share intermediate vertices. The native collision
cooker and all its topology checks remain unchanged. Runtime material fields
and GPU/native vertex storage remain binary32; no exact-arithmetic solver or
new physics backend is involved.

The original full-column prototype is also preserved at
`/tmp/terrain-v2-full-column-mesher-reference.cpp` during this run. It passed the
initial tunnel but coupled work to global vertical bounds. The sparse-band
mountain case explicitly prevents restoring that scaling constraint.
