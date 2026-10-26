# Consumer-discovered physical persistence corrections

2026-10-26, Experimental. Discovered during Lunar Island Stage 2; no release/main
promotion or platform certification implied. Original reproductions and binary
contexts: projects/lunar_island/Review/stage2/{cold-failure,mass-failure}.

1. Joint/contact continuation compared serialized physical contexts byte-for-byte.
   IEEE +0 and -0 are equal numeric states but were rejected as different inertia.
   The unchanged binary fast path remains; a mismatch uses the same field traversal
   for exact typed equality. All nonzero fields, strings, counts, geometry and
   settings remain exact. No epsilon, damping, body repair or solver-budget change.
2. Ordinary entity snapshots omitted actual inverse mass and local inverse inertia.
   A static-to-dynamic change uses double shape integration, whereas simple-body
   reconstruction can use float primitive formulas. A one-ULP mismatch is genuine
   lost state, and custom native SetMassDistribution values were also lost.
   Entity participant **version 2** now records durable mass/inertia beside body
   forces, before restored solver/contact continuation. Version 1 remains readable
   but cannot guarantee exact reconstruction of state it never separately captured.
   Original affected version-1 save remains preserved and explicitly unqualified.

Mass records validate finite positive inverse mass, positive-definite approximately
symmetric inertia for dynamic bodies, and zero mass/tensor for static/query/kinematic
bodies. Decoding validates temporaries before assigning. The symmetry tolerance is
input validity consistent with authored tensors; it is not a context-equality
relaxation. Geometry, motion authority, pose and velocities do not change. Save
migrations accept entity versions 1 and 2 and preserve the opaque native tail;
unknown versions continue to reject. No JudasJS binding/type/API additions.

Validation: focused continuation includes signed-zero positive control, exact next
physical boundary, one-ULP/velocity/type/trailing-data negatives, invalid/truncated
mass records, authority rejection and a custom off-diagonal tensor cold restore.
Original validator fails the signed-zero positive control. Existing joint,
WorldPersistence and migration checks rerun; actual consumer write/fresh-process
read verifies bodies, inventory, attachments, lamp and 120 resumed hinge steps.

A sleeping hinge's cached solver `coordinate` starts unpublished at zero after
restore because no solve is needed. The consumer observer therefore checks its
actual body/frame geometry for the unchanged >1-radian opening requirement;
source poses and saved endpoint remain independently checked. No physical test
threshold was relaxed to accept an engine observation cache.
