# Pass 3 curved-mesh continuation investigation

Baseline: f647d7fd95f60d3a37dba70a51f4e45dc41c2922, original tests/ImpactMeshCostTests.cpp.
No threshold, speed, geometry, timestep or solver budget changes are proposed.

1. Preserve the unchanged 90-step continuation failure and separate two-step entry pass.
2. Trace first physical divergence for indices 0/1/2/5 against each mesh resolution with actual endpoint contacts and authoritative motion segments.
3. Isolated hypothesis A: concave support pairs currently suppress all mesh CCD unless angular travel exceeds the feature. Translation into a new curved patch has no corresponding test. Route such support to the existing penetration-target query, without changing contact truth.
4. If A establishes the mechanism but cost/correctness is unsuitable, investigate finite feature ownership. Any selective witness predicate must be a strict subset of the full, same-time response query. The Pass 2 front-reference variant violated this by accepting geometry rejected by current-pose response. Do not revive that mismatch.
5. Retain original negative controls for collider absence, legal flight, finite edges, one-sided rear entry and arbitrary rotation.

Hypotheses are not established fixes. Production source remains unchanged until baseline reproduction and a demonstrated mechanism.
