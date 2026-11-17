# Mechanical connection damage

## Reused foundations and ownership

structure already supplies authored cohesive/rigid fracture, section stress, irreversible
bonds and current connected components. bending supplies real tetrahedral elastic and
plastic material, boundary collision and finite-mass Body attachments. wheel wheel
mounts and traction use an ordinary chassis body's current transform. powertrain networks
already resolve shaft inertia, contact load and bearing reactions.

mechanical-damage connects those mechanisms instead of creating a second damage simulator:

1. A material arm attaches named groups to an ordinary root and a finite carrier.
2. Existing node/body point constraints exchange actual impulses; a noncollinear
   patch constrains orientation as well as translation.
3. Elastic/plastic geometry changes the **physical carrier's** pose and thus its
   wheel contact axis, suspension and mechanical reactions.
4. Optional finite patch strengths can release that real connection under load.
5. An explicitly authored transmission prerequisite can require the attachment
   and a surviving material path to its root. Loss of that path opens the link.

The material entity does not own a competing rigid body. Each carrier retains its
normal body mass, collision, inertia, gravity and velocity. Normal PhysicsWorld
integrates bodies; deformables retain the existing partitioned impulse exchange.
Renderer only presents current geometry. JavaScript supplies input, loads, driver
policy, camera, repair/reconstruction requests and game meaning.

## Actual load and failure

For the impulses actually applied to a target body in substep duration `h`:

```text
J = sum(actual signed body reaction impulses)
K = sum((applicationPoint - patchCentroid) cross reactionImpulse)
F = J / h                  [N]
M = K / h                  [N m]
fail = (forceLimit > 0 and length(F) > forceLimit)
    or (torqueLimit > 0 and length(M) > torqueLimit)
```

The centroid is the mean of surviving attachment rest nodes transformed by the
sampled target frame. Signed resultants distinguish a pure moment from opposing
forces; individual magnitudes are not accumulated as fake damage. Peak demand
is observed across the completed fixed step. Limits are **patch capacities**,
not Pa or a material fatigue/health value. structure remains the section-stress authority.

All substeps use the same existing physical constraints as an unlimited connection.
Pending overload releases publish only after a successfully completed material
step. Release applies no extra impulse, correction, teleport or velocity reset.
There is no settling delay, budget increase or damage-specific physics cadence.
The existing bending out-of-envelope solver stops with partial physical state;
this change prevents pending release on a failed step, and does not promise
transactional rollback of the whole physical solve.

Finite strength requires a **dynamic Body** target. Prescribed world, bone,
static and kinematic targets have no measured finite-mass reaction in this path
and reject finite strengths. Their default unlimited compatibility path remains.
Rigid fracture supports use their existing section bonds instead of these limits.

## Mechanical continuity

A Powertrain link optionally contains:

```json
"connection": {"owner":"102", "attachment":"right", "rootAttachment":"left"}
```

The owner and groups use existing authored identities. Selected attachments must
be enabled, unreleased and have live targets; the material must be enabled/healthy.
Optional root attachment requires continuous material between both patches.
For a cohesive cook, all surviving nodes in both patches must share an actual
current connected component. A removed whole patch fails closed; an alternate
intact material path continues working. Without a root, only the local attachment
is the prerequisite. Unpartitioned material has no authored internal break topology.

A prerequisite describes the developer's physical wiring; it does not infer a
vehicle or arbitrary global joint graph. Lost continuity suppresses only the
selected transmission row. Independent sources, shaft inertia, wheels and brakes
remain physical; held driver commands are not overwritten. `connected` is derived,
separate from JS `enabled`. Enabling a link cannot heal a released patch/bond.
The next ordinary mechanical interval observes committed damage. A narrow final
publication refresh reports committed continuity at the completed boundary without
repeating impulses, body integration or shaft-angle advancement.

## Authoring and integration

The existing Deformable inspector exposes named attachments, target-local offsets,
force/moment strengths and actual demand. Powertrain link authoring has an optional
material-owner/terminal/root picker. Default strengths are zero (unlimited) and
absent prerequisites retain powertrain mechanics. Change material compliance/yield,
patch strength or the physical root to change outcomes in ordinary content.

Optional canonical `deformable.attachmentLimits` stores group/capacity rows; default
attachment rows and fingerprints are unchanged. Optional connection JSON likewise
omits defaults. Existing scene validation, entity references, prefab remapping,
dependency retention and export carry the graph. Qualified region references use
`powertrain-link:<link-name>:connection` and validate the foreign material/groups.

Finite-strength saves use deformables participant 4 and retained region state 2.
Original attachment binary rows remain unchanged; participant 1–3 and old retained
records decode their original layouts with unlimited historical strengths.
Material positions/velocities, plastic rest, released patches and structure broken bonds
remain authoritative. Transient reactions and availability are reconstructed.
This does not invent absent historical state or create a new identity system.

JudasJS extends existing [Deformable](judasjs/deformables.md) options/observations
and [Powertrain](judasjs/powertrains.md) snapshots/configuration; no new public
namespace, gameplay damage function or unsafe node memory is exposed.

## Limits

- Small game-oriented tetrahedral/cloth patches, not industrial crash FEM,
  continuum-calibrated metals, arbitrary mesh cutting or contact-exact tyre solids.
- Existing XPBD/rigid splitting and world-axis point iterations are approximate;
  arbitrary orientation is supported, not bitwise rotational equivalence.
- wheel retains query suspension/scalar spin. Damage changes its real mounting
  body, not a deformable pneumatic tyre or simulated engine block.
- Failure uses instantaneous signed resultant thresholds; no fatigue, thermal
  degradation or probabilistic damage law. Distributed patch stress peaks can
  differ from resultants; use authored structure sections for material stress failure.
- No automatic assembly inference, repair, mesh recooking, strength degradation
  curve or material-to-joint arbitrary-joint capacity mapping.
- Current deformed boundary uses existing contact/raycast capabilities; this does
  not add exact soft CCD, shape-query shape casts, motor blocking or deformable collision-event events.
- Extreme folding/inversion and disconnected/unavailable dependencies retain
  explicit diagnostics and existing bounded envelopes. No hidden stabilization.
- Native Windows mechanical-damage checks and operator acceptance remain outstanding unless
  separately recorded in the evidence. Existing deferred fluid/research and
  unrelated physics tails are unchanged.

## Current practical crash representation
