# Rendering checkpoint review

The three canonical mesh-vertex/fragment/shadow-vertex identities deliberately
change for shared terrain height reconstruction and source-local material rules.
The preserved shader-stale-identities.log reports the original pin failures;
shader-identity-review.log identifies the exact changed sources. Only those
three pins were advanced after source-diff review, actual GPU colour/shadow
controls and ordinary runtime renderer controls. All negative preparation and
context-ownership controls remain. This is an identity update, not a suppression
of a rendering failure.

unflushed-test-oracle-failure.log is the first test harness reading ordinary
geometry before flushing the normal deferred batch. The final test flushes it,
then verifies actual state restoration. No production workaround was added for
that oracle error.

GPU controls pass978 assertions each on desktop4.6 and portable3.3. Actual
rendered height error <=7.2e-6m, actual shadow-depth difference zero against the
independent CPU-height mesh, including normal shadow bias. Existing runtime
render controls54, shader47 and particle collision37 pass. See raw logs.

The camera LOD target uses projected geometric error, not image equality.
Physical collision remains at authored source detail. A real motor traverses20m
of seams/corners with300/300 supported steps and zero observed difference from
the monolithic-collider trajectory. Whole game and terrain-source cold saves
pass; tests are automated and do not imply human acceptance.

The initial flat terrain still retains every physical chunk. Native asset pages,
independent physical residency, temporal LOD transitions, planetary excavation,
broader authoring and final SDK/consumer qualification remain. This is not the
completed five-stage delivery. Main/release/fluid lock are unchanged.
