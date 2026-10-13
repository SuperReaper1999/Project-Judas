# M83 material/body attachment numerical evidence

## Mechanism

The existing M80 bilateral group constraint still solves each material node against an ordinary finite-mass rigid body. M83 records the **signed body impulses actually applied**, sums them per material substep, and divides by that substep duration. Force is the vector resultant, not the sum of impulse magnitudes. Moment is the sum of `cross(destination - patchCentroid, reaction)` about the active authored patch centroid. Peak force/torque are the maximum norms across the completed fixed step. An authored positive force/torque limit schedules release; zero remains unlimited. Release is published only after every substep succeeds and introduces no extra impulse or pose correction.

Finite strengths are valid only for Body attachments to dynamic bodies; prescribed world/bone/static supports have no measured finite reaction in this solver. Invalid runtime finite/static claims stop before geometry changes. Rigid fracture retains M79 section bonds rather than claiming deformable patch strength.

## Preserved first failures and corrections

`linux/damage-first.txt` preserves the initial 31-check run with two failures.

1. The claimed pure-couple fixture used `MakeDeformableSheet`, which places its sheet below its top-edge pivot. The patch centroid was 0.2 m from the mount COM, so spinning it caused a genuine 44.7251328 N resultant alongside 16.0905945 Nm moment. The test fixture, not the force formula, was incorrect. Centring the actual test sheet about the body COM produces 0.0803219489 N resultant and 20.9579836 Nm torque. The original bound was retained.
2. Translating the original fixture to `(10000,-20000,30000)` changed measured moment by 0.00574625582 Nm. The constraint denominator and point velocity used a double lever arm, but its actual reaction passed an absolute point through `glm::vec3` before COM subtraction. M83 applies the same reaction through existing linear/angular impulse primitives and computes `cross(destination - actualCOM, reaction)` in double before converting the angular impulse. The final translated-centred fixture error is 3.99201703e-8 Nm against the unchanged 1e-5 Nm check. This removes an avoidable point-quantization inconsistency; it does not promise unlimited world-coordinate precision or bit-identical historic trajectories.
3. Source inspection also established that `GetTransform().position` is the authored pivot, whereas `GetCenterOfMass()` is the physical COM for generic offset shapes. The local deformable effective-mass and point-velocity helpers now use the same actual COM as impulse application. A one-child offset compound and an otherwise identical centred box now produce a torque error of 8.55138182e-12 Nm and identical final angular velocity.

`linux/old-com-negative.txt` and `.json` preserve an independently compiled control: only the two local helper lookups are reverted to authored pivots. The final 33-check test then fails exactly the offset-mount comparison, with 0.0099592175 Nm torque discrepancy and 0.000796914101 rad/s angular-velocity discrepancy. All other checks pass. The JSON records source hashes and exact compile/link arguments; the temporary binary is outside the repository.

The first JS build failure remains in its original log. Its attachment prerequisite had been added to the source while the first compile was in progress; a fresh narrow compile executes the seeded fixture. `damage-script-build-error.txt` records the successful diagnostic rerun, and the test now prints actual runtime build errors.

## Final focused checks

- `linux/damage-com-followup.txt`: 33 checks, zero failures. Real impact exchange linear-momentum error 2.0615491e-7 kg m/s; weak/strong/unlimited controls retain identical solved motion until the successful release boundary. Sustained load, pure rotation, arbitrary orientation, translation, authored pivot, detached later impulse and cold restore are exercised.
- `linux/damage-script-followup.txt`: 18 checks pass in actual QuickJS. Positive physical load/release and actual transmission disconnect; invalid strengths/targets/phase cannot mutate configuration; returned snapshots are copies; destroyed material handles invalidate/throw; the exact public cookbook runs through registered assets/input.
- Strength-aware malformed/truncated snapshots and duplicate/missing/negative authoring are rejected.
- An intentionally inverted element records 87.0633064 N demand and stops outside its envelope without publishing pending release.

## Persistence and compatibility

`DeformableAttachment::Save` retains its old bytes. Optional authoring uses `deformable.attachmentLimits` only when nonzero. Entity residency snapshots use State.2 only when strengths exist; State.1/raw legacy decode to zero/unlimited strength. Whole-world deformables remain participant version 3 without strengths and use version 4 when strengths exist; versions 1 through 3 retain their historical interpretation. Released flags are already durable; reaction observations are derived and deliberately not persisted. Powertrain connection availability is republished from restored material topology before scripts/callbacks resume.

## Limits

The scalar force/torque strengths are isotropic patch capacities, not industrial damage/fatigue or anisotropic fastener analysis. The M80 bounded Gauss-Seidel solve and fixed world-axis iteration order remain approximate. The detector does not alter solver budgets. An out-of-envelope deformable solve preserves its existing stop semantics: it is not a full body/material transaction rollback. M83 prevents irreversible pending release publication on such a failed step, but does not invent rollback of already applied M80 impulses. Diagnostics describe the last evaluated step, which may be partial on error, not a serialized solver history. Patch-centroid material-group evaluation adds bounded work proportional to authored attachment nodes.
