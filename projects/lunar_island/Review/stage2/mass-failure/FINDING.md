# LI-SAVE-002: actual mass/inertia absent from ordinary body persistence

The strict signed-zero correction reveals another difference in a completed
contact body's inverse inertia. Two diagonal entries are 0.0014324915828183293
in the saved physical context and 0.0014324914664030075 after reconstruction.
This is one float32 ULP, not merely a zero sign, and is correctly rejected.
The original failed save is retained unchanged under ../cold-failure/island.save.

Mechanism verified in actual source:
- PhysicsWorld::SetMotionType uses double ShapeMassProperties + matrix inversion.
- PhysicsWorld::CreateShape uses that same path.
- RuntimeWorld::InstantiateEntityBody chooses CreateDynamicBox/Sphere for simple
  shapes; their AddBody path uses float SolidBox/SphereInverseInertia formulas.
- WorldPersistence::Entities preserves pose, velocities, force accumulators and
  scene body settings, but not actual inverse mass/inertia. A felled static tree
  is therefore reconstructed by a different calculation. Including real hinges
  enables the extended contact continuation that correctly detects this loss.
- Native SetMassDistribution also permits a genuinely custom tensor/mass; those
  physical values are likewise not captured by the entity participant.

The generic proposed correction is to retain the actual physical mass/tensor
in a versioned entity participant, restored before joint/contact continuation.
Do not change body dynamics or relax nonzero equality. Old records lacking these
fields cannot promise exact reconstruction for the affected bodies; preserve their
explicit rejection rather than inventing missing state. Operator authorization
for this additional bounded repair was requested separately.

Corrected in the Stage 2 checkpoint: entity participant v2 preserves actual inverse mass/inertia before continuation. The independent custom tensor fixture now passes; the original version-1 input remains unchanged and unsupported for exact reconstruction. See ../../STAGE2_REPORT.md and docs/PREALPHA_LUNAR_PERSISTENCE_CORRECTIONS.md.
