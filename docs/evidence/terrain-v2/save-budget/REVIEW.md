# Runtime terrain continuation budget correction

Based on Experimental f4e8335b. A real runtime-created513x513 source at2m pitch
(one kilometre,1024physical chunks) was excavated and captured through normal
WorldPersistence. Its native continuation is5,271,385bytes, within the existing
8MiB capture/SaveArchive string budget. World restore failed with
`invalid runtime terrain record` because it still imposed2MiB. The same old
limit remained on retained-region serialization. This is a verified contract
mismatch, not a numerical/physics defect or a new larger terrain allowance.

Capture, direct restore, world restore and retained regions now share
TerrainV2::MaximumSnapshotBytes=8MiB. Direct restore rejects an oversized record
before changing support. Existing string, asset, source geometry,1024chunk,
8owner,64MiB whole-save and regional memory budgets remain; none is removed.
Valid old snapshots keep their representation and participant versions.

The focused test passes30write/18independent-process read checks. It covers exact
samples/journal, actual excavated collision, full-world cold reconstruction,
suspending a large runtime-created regional source, retiring all1024generated
colliders, writing the inactive archive and cold-publishing it again. A rejected
8MiB+1byte direct continuation preserves exact live ray results and body count.
The regional old-limit defect is source-confirmed; the archived executed failure
is specifically the world path. Do not claim a separate old regional executable
was run. The final test genuinely exercises both corrected paths.

Related components40, projected streaming37, ordinary components35/surface59/
streaming69 and legacy volumetric LivingWorld58 pass after this change. Main,
AdvancedFluidPolicy, gameplay projects, SDK selection and research remain untouched.

Reproduce with one -j2 build:

    cmake -S . -B build
    cmake --build build -j2 --target judas_terrain_v2_persistence_budget_tests
    build/judas_terrain_v2_persistence_budget_tests --write /tmp/FRESH-budget
    build/judas_terrain_v2_persistence_budget_tests --read /tmp/FRESH-budget

The initial assertion concatenated the diagnostic before the Restore call,
hiding its error due to argument-evaluation order. The preserved cold failure
uses a separate result variable to capture the actual returned diagnostic.
